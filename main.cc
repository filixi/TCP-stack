
#include "state.h"

#include <future>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <type_traits>
#include <condition_variable>

#include <pthread.h>

template <class Ret, class... Args>
struct CallableBase {
  virtual ~CallableBase() = default;
  virtual Ret operator()(Args... args) = 0;
  
  virtual CallableBase *CopyConstruct(const CallableBase *from, void *to) = 0;
  virtual CallableBase *MoveConstruct(CallableBase *from, void *to) = 0;
};

template <class Fn, class Ret, class... Args>
struct Callable final : CallableBase<Ret, Args...> {
  using Base = CallableBase<Ret, Args...>;

  Callable(const Fn &fn) : fn_(fn) {}
  Callable(Fn &&fn) : fn_(std::move(fn)) {}

  Ret operator()(Args... args) override {
    return fn_(std::forward<Args>(args)...);
  }

  Base *CopyConstruct(const Base *from, void *to) override {
    return new(to) Callable(static_cast<const Callable *>(from)->fn_);
  }

  Base *MoveConstruct(Base *from, void *to) override {
    return new(to) Callable(std::move(static_cast<Callable *>(from)->fn_));
  }

  Fn fn_;
};

class DynamicStorage {
public:
  DynamicStorage(size_t size)
      : size_(size), p_(size!=0 ? new unsigned char[size] : nullptr) {}

  DynamicStorage(const DynamicStorage &) = delete;
  DynamicStorage(DynamicStorage &&x) : size_(x.size_), p_(x.p_) {
    x.size_ = 0;
    x.p_ = nullptr;
  }

  ~DynamicStorage() {
    delete[] p_;
  }

  DynamicStorage &operator=(const DynamicStorage &) = delete;

  DynamicStorage &operator=(DynamicStorage &&x) {
    std::swap(size_, x.size_);
    std::swap(p_, x.p_);
 
    return *this;
  }

  void NewStorage(size_t size) {
    if (size > size_) {
      const auto new_buffer_ = new unsigned char[size];
      delete[] p_;
      p_ = new_buffer_;
      size_ = size;
    }
  }

  void *Get() const { return p_; }

  size_t Size() const { return size_; }

  operator bool() const {
    return p_;
  }

private:
  size_t size_;
  unsigned char *p_;
};

template <class Fn>
class StackFunction;

template <class Ret, class... Args>
class StackFunction<Ret(Args...)> {
public:
  using CallableBaseType = CallableBase<Ret, Args...>;
  template <class Fn>
  using CallableType =
      Callable<std::remove_const_t<std::remove_reference_t<Fn>>, Ret, Args...>;
  static constexpr size_t kStaticStorageSize = 64;
  static constexpr size_t kStaticStorageAlign = 8;
  using StaticBufferType =
      std::aligned_storage_t<kStaticStorageSize, kStaticStorageAlign>;

  template <class Fn>
  explicit StackFunction(Fn &&fn)
      noexcept (std::is_nothrow_constructible_v<
                    std::remove_const_t<std::remove_reference_t<Fn>>, Fn> &&
                sizeof(CallableType<Fn>) <= sizeof(StaticBufferType))
      : dynamic_buffer_(sizeof(CallableType<Fn>) <= sizeof(StaticBufferType) ?
                        0 : sizeof(CallableType<Fn>)) {
    if constexpr (sizeof(CallableType<Fn>) <= sizeof(StaticBufferType)) {
      callable_ = new(&static_buffer_) CallableType<Fn>(std::forward<Fn>(fn));
    } else {
      callable_ =
          new(dynamic_buffer_.Get()) CallableType<Fn>(std::forward<Fn>(fn));
    }
  }

  StackFunction(const StackFunction &) = delete;
  
  // Fn is erased, no way to know whether it is nothrow_move_constructible
  StackFunction(StackFunction &&x) noexcept
      : dynamic_buffer_(std::move(x.dynamic_buffer_)) {
    if (dynamic_buffer_) {
      std::swap(callable_, x.callable_);
    } else {
      callable_ = x.callable_->MoveConstruct(x.callable_, &static_buffer_);
    }
  }

  ~StackFunction() noexcept {
    if (callable_)
      callable_->~CallableBaseType();
  }

  StackFunction &operator=(const StackFunction &) = delete;
  StackFunction &operator=(StackFunction &&) = delete;

  template <class Fn>
  StackFunction &operator=(Fn &&fn)
      noexcept (std::is_nothrow_constructible_v<
                    std::remove_const_t<std::remove_reference_t<Fn>>, Fn> &&
                sizeof(CallableType<Fn>) <= sizeof(StaticBufferType)) {
    if (callable_)
      callable_->CallableBaseType();

    if constexpr (sizeof(CallableType<Fn>) <= sizeof(StaticBufferType)) {
      callable_ = new(&static_buffer_) CallableType<Fn>(std::forward<Fn>(fn));
    } else {
      dynamic_buffer_.NewStorage(sizeof(CallableType<Fn>));
      callable_ =
          new(dynamic_buffer_.Get()) CallableType<Fn>(std::forward<Fn>(fn));
    }
    return *this;
  }

  Ret operator()(Args... args) const {
    return callable_->operator()(std::forward<Args>(args)...);
  }

private:
  StaticBufferType static_buffer_;
  DynamicStorage dynamic_buffer_;
  CallableBaseType *callable_ = nullptr;
};

class TimeOutQueue {
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Event = StackFunction<void()>;
  using EventQueue = std::multimap<TimePoint, Event>;

  template <class T>
  class Handle {
  public:
    friend class TimeOutQueue;
    auto &Future() {
      return future_;
    }

    void Cancel() {
      cancel_.store(true);
    }

  private:
    std::promise<T> promise_;
    std::future<T> future_ = promise_.get_future();
    std::atomic<bool> cancel_{false};
  };

  TimeOutQueue() = default;

  ~TimeOutQueue() {
    Quit();
    for (auto &thread : threads_)
      thread.join();
  }

  TimeOutQueue(const TimeOutQueue &) = delete;
  TimeOutQueue &operator=(const TimeOutQueue &) = delete;

  void AsyncRun() {
    threads_.emplace_back(&TimeOutQueue::Worker, this);
  }

  template <class Fn, class Rep, class Period>
  void PushEvent(Fn fn, std::chrono::duration<Rep, Period> timeout_duration) {
    const auto timeout_point = Clock::now() + timeout_duration;
    auto refined_event = [event = std::move(fn)] {event();};

    std::lock_guard guard(mtx_);
    time_out_queue_.emplace(timeout_point, std::move(refined_event));
    new_event_.notify_one();
  }

  template <class Fn, class Rep, class Period>
  auto PushEventWithFeedBack(
      Fn fn, std::chrono::duration<Rep, Period> timeout_duration) {
    using ResultType = std::invoke_result_t<Fn>;
    const auto timeout_point = Clock::now() + timeout_duration;

    auto handle = std::make_shared<Handle<ResultType>>();

    auto refined_event =
        [event = std::move(fn), handle] {
          if (!handle->cancel_.load()) {
            if constexpr (std::is_same_v<ResultType, void>) {
              event();
              handle->promise_.set_value();
            } else {
              handle->promise_.set_value(event());
            }
          }
        };

    {
      std::lock_guard guard(mtx_);
      time_out_queue_.emplace(timeout_point, std::move(refined_event));
      new_event_.notify_one();
    }

    return handle;
  }

  void Quit() {
    quit_.store(true);
    new_event_.notify_all();
  }

  auto WaitUntilAllDone() {
    std::unique_lock lock(mtx_);
    all_done_.wait(lock, [this] {
          return time_out_queue_.empty() && events_out_of_queue_.load()==0;
        });

    return lock;
  }

private:
  void Worker() {
    while (!quit_.load()) {
      std::unique_lock lock(mtx_);
      if (time_out_queue_.empty()) {
        if (events_out_of_queue_.load() == 0)
          all_done_.notify_all();
        new_event_.wait(lock, [this] {
              return quit_.load() || !time_out_queue_.empty();
            });
      }

      if (quit_.load())
        break;
      
      ++events_out_of_queue_;
      auto node = time_out_queue_.extract(time_out_queue_.begin());
      

      auto [next_timeout, next_event] = std::tie(node.key(), node.mapped());

      new_event_.wait_until(lock, next_timeout, [this] {return quit_.load();});
      if (!quit_.load() && next_timeout <= Clock::now()) {
        lock.unlock();
        next_event();
        --events_out_of_queue_;
      } else {
        time_out_queue_.insert(std::move(node));
      }
    }
  }

  EventQueue time_out_queue_;

  std::mutex mtx_;
  std::condition_variable new_event_;

  std::condition_variable all_done_;
  std::atomic<int> events_out_of_queue_{0};

  std::atomic<bool> quit_{false};

  std::vector<std::thread> threads_;
};

int main() {
  TimeOutQueue queue;
  for (int i=0; i<8; ++i)
    queue.AsyncRun();

  std::function<void()> fn;
  fn = [&] {
        static int i = 0;
        std::clog << ++i << std::endl;
        if (i<5)
          queue.PushEvent(fn, std::chrono::seconds(1));
      };

  queue.PushEventWithFeedBack(fn, std::chrono::seconds(1));
  
  queue.WaitUntilAllDone();
  return 0;
}

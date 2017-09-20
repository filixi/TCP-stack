
#include "state.h"

#include <future>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>

#include <pthread.h>

class Mutex {
  
};

class ConditionVariable {

};

class TimeOutQueue {
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Event = std::function<void()>;
  using EventQueue = std::multimap<TimePoint, Event>;

  template <class T>
  class Handle {
  public:
    Handle(std::future<T> &future) : future_(future) {}
    auto &Future() {
      return future_;
    }

    void Cancel() {
      cancel_.store(true);
    }

  private:
    std::future<T> future_;
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
    const auto refined_event = [fn] {fn();};

    std::lock_guard guard(mtx_);
    time_out_queue_.emplace(timeout_point, refined_event);
    new_event_.notify_one();
  }

  template <class T, class Rep, class Period>
  auto PushEventWithFeedBack(
      std::function<T()> event,
      std::chrono::duration<Rep, Period> timeout_duration) {
    std::promise<T> result_promise;

    auto handle = std::make_shared<Handle>(result_promise.get());

    const auto timeout_point = Clock::now() + timeout_duration;
    const auto refined_event =
        [result_promise = std::move(result_promise),
         event = std::move(event),
         handle] {
          if (!handle.cancel_.load())
            result_promise.set(event());
        };

    {
      std::lock_guard guard(mtx_);
      time_out_queue_.emplace(timeout_point, refined_event);
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
      
      auto node = time_out_queue_.extract(time_out_queue_.begin());
      ++events_out_of_queue_;

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

  queue.PushEvent(fn, std::chrono::seconds(1));
  
  queue.WaitUntilAllDone();
  return 0;
}

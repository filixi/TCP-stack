#ifndef _TCP_STACK_TIMEOUT_QUEUE_H_
#define _TCP_STACK_TIMEOUT_QUEUE_H_

#include <chrono>
#include <condition_variable>
#include <future>
#include <map>
#include <mutex>
#include <thread>

#include "stack-function.h"

class TimeoutQueue {
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Event = StackFunction<void()>;
  using EventQueue = std::multimap<TimePoint, Event>;

  using MutexType = std::mutex;
  template <class T>
  using UniqueLockType = std::unique_lock<T>;
  using ConditionVariableType = std::condition_variable;

  template <class T>
  class Handle {
  public:
    friend class TimeoutQueue;
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

  TimeoutQueue() = default;

  ~TimeoutQueue() {
    Quit();
    for (auto &thread : threads_)
      thread.join();
  }

  TimeoutQueue(const TimeoutQueue &) = delete;
  TimeoutQueue &operator=(const TimeoutQueue &) = delete;

  void AsyncRun() {
    threads_.emplace_back(&TimeoutQueue::Worker, this);
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

    auto refined_event = [event = std::move(fn), handle] {
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
    UniqueLockType<MutexType> lock(mtx_);
    all_done_.wait(lock, [this] {
          return time_out_queue_.empty() && events_out_of_queue_.load()==0;
        });
    return lock;
  }

private:
  void Worker();

  EventQueue time_out_queue_;

  MutexType mtx_;
  ConditionVariableType new_event_;

  ConditionVariableType all_done_;
  std::atomic<int> events_out_of_queue_{0};

  std::atomic<bool> quit_{false};

  std::vector<std::thread> threads_;
};

#endif // _TCP_STACK_TIMEOUT_QUEUE_H_

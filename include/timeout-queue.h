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
  using FunctionType = StackFunction<bool()>;

  struct Event {
    FunctionType function;
    std::chrono::nanoseconds period;
  };

  using EventQueue = std::multimap<TimePoint, Event>;

  using MutexType = std::mutex;
  template <class T>
  using UniqueLockType = std::unique_lock<T>;
  using ConditionVariableType = std::condition_variable;

  TimeoutQueue() = default;

  explicit TimeoutQueue(size_t n) {
    for (size_t i=0; i<n; ++i)
      AsyncRun();
  }

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
    std::lock_guard<MutexType> guard(mtx_);
    time_out_queue_.emplace(Clock::now() + timeout_duration,
                            Event{std::move(fn), timeout_duration});
    new_event_.notify_one();
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

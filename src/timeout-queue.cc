#include "timeout-queue.h"

template <class Fn>
struct AtExit : Fn {
  AtExit(Fn &&fn) noexcept : Fn(std::move(fn)) {}
  ~AtExit() noexcept { Fn::operator()(); }
};

void TimeoutQueue::Worker() {
  while (!quit_.load()) {
    UniqueLockType<MutexType> lock(mtx_);
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
    AtExit at_exit([this]() {--events_out_of_queue_;});
    auto node = time_out_queue_.extract(time_out_queue_.begin());
    auto [next_timeout, next_event] = std::tie(node.key(), node.mapped());
    
    new_event_.wait_until(lock, next_timeout, [this] {return quit_.load();});
    if (!quit_.load() && next_timeout <= Clock::now()) {
      lock.unlock();
      const auto is_repeat = next_event.function();

      if (is_repeat) {
        next_timeout += next_event.period;

        lock.lock();
        time_out_queue_.insert(std::move(node));
        new_event_.notify_one();
      }
    } else {
      time_out_queue_.insert(std::move(node));
    }
  }
}

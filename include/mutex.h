#ifndef _TCP_STACK_MUTEX_H_
#define _TCP_STACK_MUTEX_H_

#include <exception>

#include <pthread.h>

template <class Fn>
struct AtExit : Fn {
  AtExit(const Fn &fn) : Fn(fn) {}
  AtExit(Fn &&fn) : Fn(std::move(fn)) {}
  ~AtExit() { Fn::operator()(); }
};

class Mutex {
public:
  friend class ConditionVariable;

  Mutex() noexcept {
    if (pthread_mutex_init(&mtx_, nullptr))
      std::terminate();
  }
    
  Mutex(const Mutex &) = delete;

  ~Mutex() noexcept {
    pthread_mutex_destroy(&mtx_);
  }
  
  Mutex &operator=(const Mutex &) = delete;

  void lock() noexcept {
    pthread_mutex_lock(&mtx_);
  }

  bool try_lock() noexcept {
    return !pthread_mutex_trylock(&mtx_);
  }

  void unlock() noexcept {
    pthread_mutex_unlock(&mtx_);
  }

private:
  pthread_mutex_t mtx_;
};

template <class T>
class UniqueLock;

template <>
class UniqueLock<Mutex> {
public:
  friend class ConditionVariable;

  UniqueLock(Mutex &mtx) noexcept : mtx_(&mtx) {
    mtx_->lock();
  }

  UniqueLock(const UniqueLock &) = delete;
  UniqueLock(UniqueLock &&x) : mtx_(x.mtx_), is_locked_(x.is_locked_) {
    x.is_locked_ = false;
    x.mtx_ = nullptr;
  }

  ~UniqueLock() noexcept {
    if (is_locked_)
      mtx_->unlock();
  }

  UniqueLock &operator=(const UniqueLock &) = delete;
  UniqueLock &operator=(UniqueLock &&x) noexcept {
    std::swap(mtx_, x.mtx_);
    std::swap(is_locked_, x.is_locked_);

    return *this;
  }

  void lock() noexcept {
    mtx_->lock();
    is_locked_ = true;
  }

  bool try_lock() noexcept {
    return is_locked_ = is_locked_ || mtx_->try_lock(); // !?
  }

  void unlock() noexcept {
    is_locked_ = false;
    mtx_->unlock();
  }

private:
  Mutex *mtx_;
  bool is_locked_ = true;
};

class ConditionVariable {
public:
  ConditionVariable() noexcept {
    if(pthread_cond_init(&cv_, nullptr))
      std::terminate();
  }
    
  ConditionVariable(const ConditionVariable &) = delete;

  ~ConditionVariable() noexcept {
    pthread_cond_destroy(&cv_);
  }

  ConditionVariable &operator=(const ConditionVariable &) = delete;

  void wait(UniqueLock<Mutex> &lock) noexcept {
    lock.is_locked_ = false;
    AtExit([&lock]() {lock.is_locked_ = true;});
    pthread_cond_wait(&cv_, &lock.mtx_->mtx_);
  }

  template <class Predicate>
  bool wait(UniqueLock<Mutex> &lock, Predicate pred) noexcept {
    lock.is_locked_ = false;
    AtExit([&lock]() {lock.is_locked_ = true;});
    while (!pred())
      pthread_cond_wait(&cv_, &lock.mtx_->mtx_);
    return pred();
  }

  template <class Rep, class Period>
  std::cv_status wait_for(
      UniqueLock<Mutex> &lock,
      const std::chrono::duration<Rep, Period> &rel_time) noexcept {
    timespec abstime = Duration2TimeSpec(rel_time);

    lock.is_locked_ = false;
    AtExit([&lock]() {lock.is_locked_ = true;});
    if (pthread_cond_timedwait(&cv_, &lock.mtx_->mtx_, &abstime) == ETIMEDOUT)
      return std::cv_status::timeout;
    return std::cv_status::no_timeout;
  }

  template <class Rep, class Period, class Predicate>
  bool wait_for(UniqueLock<Mutex> &lock,
                const std::chrono::duration<Rep, Period> &rel_time,
                Predicate pred) noexcept {
    timespec abstime = Duration2TimeSpec(rel_time);

    lock.is_locked_ = false;
    AtExit([&lock]() {lock.is_locked_ = true;});
    while (!pred()) {
      if (pthread_cond_timedwait(&cv_, &lock.mtx_->mtx_, &abstime) == ETIMEDOUT)
        return pred();
    }
    return pred();
  }

  template <class Clock, class Duration>
  auto wait_until(UniqueLock<Mutex> &lock,
                  const std::chrono::time_point<Clock, Duration> &abs_time) {
    return wait_for(lock, abs_time - Clock::now());
  }

  template <class Clock, class Duration, class Predicate>
  bool wait_until(UniqueLock<Mutex> &lock,
                  const std::chrono::time_point<Clock, Duration> &abs_time,
                  Predicate pred) {
    return wait_for(lock, abs_time - Clock::now(), pred);
  }

  void notify_one() noexcept {
    pthread_cond_signal(&cv_);
  }

  void notify_all() noexcept {
    pthread_cond_broadcast(&cv_);
  }

private:
  template <class Rep, class Period>
  static auto Duration2TimeSpec(
      const std::chrono::duration<Rep, Period> &rel_time) {
    const auto seconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            rel_time).count()/1000;
    const auto nano_seconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            rel_time - std::chrono::seconds(seconds)).count();
    timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);

    int64_t tv_sec = abstime.tv_sec + seconds;
    int64_t tv_nsec = abstime.tv_nsec + nano_seconds;

    abstime.tv_sec = tv_sec + tv_nsec / 1000000000;
    abstime.tv_nsec = tv_nsec % 1000000000;

    return abstime;
  }

  pthread_cond_t cv_;
};

#endif // _TCP_STACK_MUTEX_H_

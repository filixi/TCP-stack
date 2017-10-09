#ifndef _TCP_STACK_STACK_FUNCTION_H_
#define _TCP_STACK_STACK_FUNCTION_H_

#include <type_traits>
#include <utility>
#include <vector>

template <class Ret, class... Args>
class CallableBase {
public:
  virtual ~CallableBase() = default;
  virtual Ret operator()(Args... args) = 0;
  
  virtual CallableBase *CopyConstruct(const CallableBase *from, void *to) = 0;
  virtual CallableBase *MoveConstruct(CallableBase *from, void *to) = 0;
};

template <class Fn, class Ret, class... Args>
class Callable final : public CallableBase<Ret, Args...> {
public:
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

private:
  Fn fn_;
};

class DynamicStorage {
public:
  explicit DynamicStorage(size_t size) : buff_(size) {}

  DynamicStorage(const DynamicStorage &) = delete;
  DynamicStorage(DynamicStorage &&x) noexcept : buff_(std::move(x.buff_)) {}

  DynamicStorage &operator=(const DynamicStorage &) = delete;

  DynamicStorage &operator=(DynamicStorage &&x) noexcept {
    buff_.swap(x.buff_);
    return *this;
  }

  void NewStorage(size_t size) {
    buff_.resize(size);
  }

  void *Get() noexcept {
    return buff_.data();
  }

  size_t Size() const noexcept {
    return buff_.size();
  }

  explicit operator bool() const noexcept {
    return Size();
  }

private:
  std::vector<unsigned char> buff_;
};

template <class Fn, size_t size = 64,
          size_t align = alignof(std::aligned_storage_t<size>)>
class StackFunction;

template <class Ret, class... Args, size_t size, size_t align>
class StackFunction<Ret(Args...), size, align> {
public:
  using CallableBaseType = CallableBase<Ret, Args...>;
  template <class Fn>
  using CallableType = Callable<
      std::remove_const_t<std::remove_reference_t<Fn>>, Ret, Args...>;
  using StaticBufferType = std::aligned_storage_t<size, align>;

  template <class Fn>
  StackFunction(Fn &&fn)
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
    callable_ = nullptr;

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

#endif // _TCP_STACK_STACK_FUNCTION_H_

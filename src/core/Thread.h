#pragma once
// std::thread and std::async where the C++ library has threads. AmigaOS (libnix) has neither,
// and its stdio and its sockets belong to the task that opened them: there the work is done at
// once, by the caller, and what waits for it finds it finished.
#include <optional>
#include <utility>

#if defined(__amigaos__) && !defined(_GLIBCXX_HAS_GTHREADS)
#define ENCORE_NO_THREADS 1
#else
#include <future>
#include <thread>
#endif

namespace encore {

#ifdef ENCORE_NO_THREADS
class Thread {
 public:
  Thread() = default;
  template <class F>
  explicit Thread(F&& f) {
    std::forward<F>(f)();
  }
  bool joinable() const { return false; }
  void join() {}
  void detach() {}
};

enum class FutureStatus { ready, timeout };

template <class T>
class Future {
 public:
  Future() = default;
  explicit Future(T value) : value_(std::move(value)) {}
  bool valid() const { return value_.has_value(); }
  template <class Duration>
  FutureStatus wait_for(const Duration&) const {
    return FutureStatus::ready;
  }
  T get() {
    T v = std::move(*value_);
    value_.reset();
    return v;
  }

 private:
  std::optional<T> value_;
};

template <class F>
auto runAsync(F&& f) -> Future<decltype(f())> {
  return Future<decltype(f())>(std::forward<F>(f)());
}
#else
using Thread = std::thread;
template <class T>
using Future = std::future<T>;
using FutureStatus = std::future_status;

template <class F>
auto runAsync(F&& f) {
  return std::async(std::launch::async, std::forward<F>(f));
}
#endif

}  // namespace encore

#pragma once
// std::mutex where the C++ library has threads. AmigaOS (libnix) has none, but SDL plays the
// sound from a task of its own while the game asks for sounds from the main one: there the lock
// is one of exec's signal semaphores.
#include <mutex>

#if defined(__amigaos__) && !defined(_GLIBCXX_HAS_GTHREADS)
#include <exec/semaphores.h>
#include <proto/exec.h>
#endif

namespace encore {

#if defined(__amigaos__) && !defined(_GLIBCXX_HAS_GTHREADS)
class Mutex {
 public:
  Mutex() { InitSemaphore(&semaphore_); }
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;
  void lock() { ObtainSemaphore(&semaphore_); }
  void unlock() { ReleaseSemaphore(&semaphore_); }
  bool try_lock() { return AttemptSemaphore(&semaphore_) != 0; }

 private:
  SignalSemaphore semaphore_{};
};
#else
using Mutex = std::mutex;
#endif

}  // namespace encore

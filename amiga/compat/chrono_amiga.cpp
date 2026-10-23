// std::chrono's clocks for the Amiga port, on timer.device.
//
// GCC 16 rc18's libstdc++ reads both clocks with time(): whole seconds. The game moves on by
// the time between frames (App::run), so with that clock most frames saw no time pass at all
// and the game crawled. These are the same functions, so the linker takes them from here and
// leaves libstdc++'s chrono.o out (amiga/Makefile checks the map): steady_clock from the
// E-clock (ReadEClock, microseconds or better), system_clock from GetSysTime.
#include <chrono>
#include <cstdint>
#include <cstdlib>

#include <devices/timer.h>
#include <exec/memory.h>
#include <proto/exec.h>

// (a base of our own: SDL has its TimerBase, opened only once SDL starts)
static struct Device* g_timer = nullptr;
#define TIMER_BASE_NAME g_timer
#include <proto/timer.h>

namespace {

struct timerequest* g_request = nullptr;

void closeTimer() {
  if (g_timer) CloseDevice(reinterpret_cast<IORequest*>(g_request));
  if (g_request) FreeVec(g_request);
  g_timer = nullptr;
  g_request = nullptr;
}

bool openTimer() {
  if (g_timer) return true;
  g_request = static_cast<struct timerequest*>(AllocVec(sizeof(struct timerequest), MEMF_PUBLIC | MEMF_CLEAR));
  if (!g_request) return false;
  if (OpenDevice(reinterpret_cast<CONST_STRPTR>(TIMERNAME), UNIT_MICROHZ, reinterpret_cast<IORequest*>(g_request), 0) != 0) {
    FreeVec(g_request);
    g_request = nullptr;
    return false;
  }
  g_timer = g_request->tr_node.io_Device;
  std::atexit(closeTimer);
  return true;
}

}  // namespace

namespace std::chrono::_V2 {

steady_clock::time_point steady_clock::now() noexcept {
  if (!openTimer()) return time_point();
  EClockVal e;
  const std::uint64_t freq = ReadEClock(&e);
  const std::uint64_t ticks = (static_cast<std::uint64_t>(e.ev_hi) << 32) | e.ev_lo;
  // (whole seconds, then what is left of one: ticks times a billion would not fit)
  const std::uint64_t seconds = ticks / freq, rest = ticks % freq;
  return time_point(duration(static_cast<std::int64_t>(seconds * 1000000000ull + rest * 1000000000ull / freq)));
}

system_clock::time_point system_clock::now() noexcept {
  if (!openTimer()) return time_point();
  struct timeval tv;
  GetSysTime(&tv);
  // (the Amiga counts from 1978, the system clock from 1970: 2922 days; local time, as the Amiga keeps it)
  const std::int64_t seconds = static_cast<std::int64_t>(tv.tv_secs) + 2922LL * 86400;
  return time_point(duration(seconds * 1000000000LL + static_cast<std::int64_t>(tv.tv_micro) * 1000));
}

}  // namespace std::chrono::_V2

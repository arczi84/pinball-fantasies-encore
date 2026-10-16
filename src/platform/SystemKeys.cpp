#include "platform/SystemKeys.h"

#include <string>

#include "core/Log.h"

#ifdef __APPLE__
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace encore {
namespace {

#ifdef __APPLE__
void apply(bool off) {
  // The window server's switch for its global shortcuts: 0 lets them act, 1 stands them down
  // for this connection. Private, so looked up rather than linked against.
  using Connection = int (*)();
  using SetMode = int (*)(int connection, int mode);
  static const auto connection = [] {
    auto f = reinterpret_cast<Connection>(dlsym(RTLD_DEFAULT, "_CGSDefaultConnection"));
    return f ? f : reinterpret_cast<Connection>(dlsym(RTLD_DEFAULT, "CGSMainConnectionID"));
  }();
  static const auto setMode = reinterpret_cast<SetMode>(dlsym(RTLD_DEFAULT, "CGSSetGlobalHotKeyOperatingMode"));
  if (!connection || !setMode) return;
  if (setMode(connection(), off ? 1 : 0) != 0)
    log::info(std::string("the window server would not ") + (off ? "stand its shortcuts down" : "give its shortcuts back"));
}
#elif defined(_WIN32)
// The player's settings as they were when the shortcuts were last turned off, to put back.
STICKYKEYS sticky{sizeof(STICKYKEYS), 0};
FILTERKEYS filter{sizeof(FILTERKEYS), 0, 0, 0, 0, 0};
TOGGLEKEYS toggle{sizeof(TOGGLEKEYS), 0};
bool kept = false;

// As Microsoft has games do it ("Disabling Shortcut Keys in Games"): only for the session, never
// written to the player's profile.
void apply(bool off) {
  if (off) {
    kept = SystemParametersInfoW(SPI_GETSTICKYKEYS, static_cast<UINT>(sizeof sticky), &sticky, 0) &&
           SystemParametersInfoW(SPI_GETFILTERKEYS, static_cast<UINT>(sizeof filter), &filter, 0) &&
           SystemParametersInfoW(SPI_GETTOGGLEKEYS, static_cast<UINT>(sizeof toggle), &toggle, 0);
    if (!kept) return;
    if (!(sticky.dwFlags & SKF_STICKYKEYSON)) {
      STICKYKEYS s = sticky;
      s.dwFlags &= ~(SKF_HOTKEYACTIVE | SKF_CONFIRMHOTKEY);
      SystemParametersInfoW(SPI_SETSTICKYKEYS, static_cast<UINT>(sizeof s), &s, 0);
    }
    if (!(filter.dwFlags & FKF_FILTERKEYSON)) {
      FILTERKEYS f = filter;
      f.dwFlags &= ~(FKF_HOTKEYACTIVE | FKF_CONFIRMHOTKEY);
      SystemParametersInfoW(SPI_SETFILTERKEYS, static_cast<UINT>(sizeof f), &f, 0);
    }
    if (!(toggle.dwFlags & TKF_TOGGLEKEYSON)) {
      TOGGLEKEYS t = toggle;
      t.dwFlags &= ~(TKF_HOTKEYACTIVE | TKF_CONFIRMHOTKEY);
      SystemParametersInfoW(SPI_SETTOGGLEKEYS, static_cast<UINT>(sizeof t), &t, 0);
    }
  } else if (kept) {
    SystemParametersInfoW(SPI_SETSTICKYKEYS, static_cast<UINT>(sizeof sticky), &sticky, 0);
    SystemParametersInfoW(SPI_SETFILTERKEYS, static_cast<UINT>(sizeof filter), &filter, 0);
    SystemParametersInfoW(SPI_SETTOGGLEKEYS, static_cast<UINT>(sizeof toggle), &toggle, 0);
    kept = false;
  }
}
#else
void apply(bool) {}
#endif

}  // namespace

void systemShortcutsOff(bool off) {
  // (only the changes are passed on: the system is asked nothing in between)
  static bool held = false;
  if (off == held) return;
  held = off;
  apply(off);
}

}  // namespace encore

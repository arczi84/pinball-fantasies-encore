#include "platform/Window.h"

#include "core/Log.h"
#include "platform/AppIcon.h"
#ifdef __APPLE__
#include "platform/MacMenu.h"
#include "platform/MacPointer.h"
#endif

namespace encore {

Window::~Window() {
  log::debug("leaving: the window object");
  destroy();
}

void Window::destroy() {
  if (context_) SDL_GL_DestroyContext(context_);
  if (window_) SDL_DestroyWindow(window_);
  context_ = nullptr;
  window_ = nullptr;
}

bool Window::create(const std::string& title, int width, int height, bool fullscreen, int screenW, int screenH) {
#ifdef ENCORE_GLES2
  // (on the Amiga, asking for ES 2.0 is what gives ogles2.library rather than MiniGL)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  window_ = SDL_CreateWindow(title.c_str(), width, height,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                 (fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
  if (!window_) {
    log::error(std::string("SDL_CreateWindow: ") + SDL_GetError());
    return false;
  }
  fullscreen_ = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
  if (fullscreen) log::info(fullscreen_ ? "full screen" : std::string("no full screen: ") + SDL_GetError());
  // A screen mode of its own (--screen): set before the GL context, which on the Amiga takes the
  // screen as it is then.
  if (fullscreen_ && screenW > 0 && screenH > 0) {
    SDL_DisplayMode mode;
    if (SDL_GetClosestFullscreenDisplayMode(SDL_GetDisplayForWindow(window_), screenW, screenH, 0.0f, false, &mode) &&
        SDL_SetWindowFullscreenMode(window_, &mode)) {
      SDL_SyncWindow(window_);
      log::info("screen mode " + std::to_string(mode.w) + "x" + std::to_string(mode.h) + " (asked for " +
                std::to_string(screenW) + "x" + std::to_string(screenH) + ")");
    } else {
      log::info(std::string("no screen mode near ") + std::to_string(screenW) + "x" + std::to_string(screenH) +
                ": the desktop's (" + SDL_GetError() + ")");
    }
  }
#ifndef __APPLE__
  // The window's icon, for the taskbar and the window list. Windows also carries one as a
  // resource for Explorer to show on the .exe; macOS takes it from the bundle, at every size,
  // which a 128-pixel one set here would replace in the Dock.
  if (SDL_Surface* icon = SDL_CreateSurfaceFrom(kAppIconSize, kAppIconSize, SDL_PIXELFORMAT_RGBA32,
                                                const_cast<std::uint8_t*>(kAppIcon), kAppIconSize * 4)) {
    SDL_SetWindowIcon(window_, icon);
    SDL_DestroySurface(icon);
  }
#else
  plainWindowMenu();
#endif
  context_ = SDL_GL_CreateContext(window_);
  if (!context_) {
    log::error(std::string("SDL_GL_CreateContext: ") + SDL_GetError());
    return false;
  }
  SDL_GL_MakeCurrent(window_, context_);
  // Each frame shown at the screen's refresh (vsync), or, where the driver will not, at the
  // refresh unless it is late (adaptive); a driver's own setting can still override either.
  // What it came to is said, with the screen's refresh rate, for when the scrolling stutters.
  if (!SDL_GL_SetSwapInterval(1) && !SDL_GL_SetSwapInterval(-1))
    log::error(std::string("the screen's refresh could not be waited for (vsync): ") + SDL_GetError());
  int interval = 0;
  SDL_GL_GetSwapInterval(&interval);
  const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_));
  log::info("vsync " + std::string(interval == 1 ? "on" : interval == -1 ? "adaptive" : "off") +
            (mode ? ", the screen at " + std::to_string(static_cast<int>(mode->refresh_rate + 0.5f)) + " Hz" : std::string()));
  return true;
}

void Window::swap() { SDL_GL_SwapWindow(window_); }

void Window::setFullscreen(bool on) {
  fullscreen_ = on;
  SDL_SetWindowFullscreen(window_, on);
  // Fullscreen, the keys go to the game rather than to the system: no window switching on
  // Alt-Tab, no menu on the Windows or Super key. Alt-Tab still works, so nobody is trapped,
  // and in a window the system keeps its shortcuts, where the player expects them, but while
  // a ball is in play.
  SDL_SetWindowKeyboardGrab(window_, on || playing_);
  // Fullscreen there is nothing else on the screen for the pointer to do.
  if (on) hidePointer(true);
}

void Window::setPlaying(bool on) {
  if (on == playing_) return;
  playing_ = on;
  SDL_SetWindowKeyboardGrab(window_, fullscreen_ || on);
}

void Window::hidePointer(bool hide) {
#ifdef __APPLE__
  macHidePointer(hide);
#else
  if (hide)
    SDL_HideCursor();
  else
    SDL_ShowCursor();
#endif
}

void Window::drawableSize(int& w, int& h) const { SDL_GetWindowSizeInPixels(window_, &w, &h); }

}  // namespace encore

#include "platform/Window.h"

#include "core/Log.h"
#include "platform/AppIcon.h"
#ifdef __APPLE__
#include "platform/MacMenu.h"
#include "platform/MacPointer.h"
#endif

namespace encore {

Window::~Window() {
  if (context_) SDL_GL_DestroyContext(context_);
  if (window_) SDL_DestroyWindow(window_);
}

bool Window::create(const std::string& title, int width, int height) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  window_ = SDL_CreateWindow(title.c_str(), width, height,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window_) {
    log::error(std::string("SDL_CreateWindow: ") + SDL_GetError());
    return false;
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
  SDL_GL_SetSwapInterval(1);
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

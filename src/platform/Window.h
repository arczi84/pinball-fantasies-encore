#pragma once
#include <SDL3/SDL.h>

#include <string>

namespace encore {

/// SDL3 window with an OpenGL 4.1 core context.
class Window {
 public:
  Window() = default;
  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  /// `fullscreen`: made so from the start. (SDL3_68k cannot take a window with a GL context
  /// to full screen later: on the Amiga it is now or never.)
  /// screenW x screenH (not 0): full screen in the screen mode nearest that size, not the desktop's.
  bool create(const std::string& title, int width, int height, bool fullscreen = false, int screenW = 0, int screenH = 0);
  void swap();
  /// The context and the window gone now (before SDL_Quit); the destructor then does nothing.
  void destroy();
  void setFullscreen(bool on);
  bool fullscreen() const { return fullscreen_; }
  /// While a ball is in play the keys go to the game, in a window too (see setFullscreen).
  void setPlaying(bool on);
  /// The pointer out of the way over the game (nothing in it is played with the mouse), or back.
  void hidePointer(bool hide);
  void drawableSize(int& w, int& h) const;
  SDL_Window* handle() const { return window_; }

 private:
  SDL_Window* window_ = nullptr;
  SDL_GLContext context_ = nullptr;
  bool fullscreen_ = false;
  bool playing_ = false;
};

}  // namespace encore

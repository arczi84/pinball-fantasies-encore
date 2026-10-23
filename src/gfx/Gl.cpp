#include "gfx/Gl.h"

#ifndef ENCORE_GLES2

#include <SDL3/SDL_video.h>

#include "core/Log.h"

namespace encore {

#define ENCORE_GL_DEFINE(type, name) type name = nullptr;
ENCORE_GL_FUNCTIONS(ENCORE_GL_DEFINE)
#undef ENCORE_GL_DEFINE

bool loadGlFunctions() {
  bool ok = true;
#define ENCORE_GL_LOAD(type, name)                                          \
  name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name));           \
  if (!name) {                                                           \
    log::error("this OpenGL driver has no " #name);                      \
    ok = false;                                                          \
  }
  ENCORE_GL_FUNCTIONS(ENCORE_GL_LOAD)
#undef ENCORE_GL_LOAD
  return ok;
}

}  // namespace encore

#endif  // ENCORE_GLES2

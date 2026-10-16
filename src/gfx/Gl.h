#pragma once
// The OpenGL functions this renderer uses beyond version 1.1, as pointers fetched from the
// driver.
//
// Only macOS lets a program link the 3.2+ entry points; Linux and Windows expect it to ask the
// driver for each one at run time. SDL knows how to ask on all of them, so they are looked up
// once, after the context is made, and called through these pointers. The names match OpenGL's
// own, and they live in this project's namespace, so the renderer's calls read as they always
// did. The 1.1 functions (textures, clearing, drawing, the viewport) come from the platform's
// own library, and `SDL_opengl.h` brings the types and the constants for all of them.
#include <SDL3/SDL_opengl.h>

namespace encore {

/// Fetches every pointer below; false, with the name logged, if the driver lacks one.
bool loadGlFunctions();

#define ENCORE_GL_FUNCTIONS(X)                                     \
  X(PFNGLACTIVETEXTUREPROC, glActiveTexture)                    \
  X(PFNGLATTACHSHADERPROC, glAttachShader)                      \
  X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                \
  X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                \
  X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)  \
  X(PFNGLCOMPILESHADERPROC, glCompileShader)                    \
  X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                    \
  X(PFNGLCREATESHADERPROC, glCreateShader)                      \
  X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)          \
  X(PFNGLDELETEPROGRAMPROC, glDeleteProgram)                    \
  X(PFNGLDELETESHADERPROC, glDeleteShader)                      \
  X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)          \
  X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)      \
  X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                \
  X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                \
  X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap)                  \
  X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)            \
  X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                      \
  X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)              \
  X(PFNGLGETSHADERIVPROC, glGetShaderiv)                        \
  X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)          \
  X(PFNGLLINKPROGRAMPROC, glLinkProgram)                        \
  X(PFNGLSHADERSOURCEPROC, glShaderSource)                      \
  X(PFNGLUNIFORM1FPROC, glUniform1f)                            \
  X(PFNGLUNIFORM1IPROC, glUniform1i)                            \
  X(PFNGLUNIFORM1UIPROC, glUniform1ui)                          \
  X(PFNGLUNIFORM2FPROC, glUniform2f)                            \
  X(PFNGLUNIFORM3FPROC, glUniform3f)                            \
  X(PFNGLUNIFORM4FPROC, glUniform4f)                            \
  X(PFNGLUSEPROGRAMPROC, glUseProgram)

#define ENCORE_GL_DECLARE(type, name) extern type name;
ENCORE_GL_FUNCTIONS(ENCORE_GL_DECLARE)
#undef ENCORE_GL_DECLARE

}  // namespace encore

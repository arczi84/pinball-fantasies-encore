#pragma once
#include "gfx/Gl.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace encore {

/// A vertex+fragment program loaded from files, with source-change hot reloading.
class ShaderProgram {
 public:
  ShaderProgram() = default;
  ~ShaderProgram();
  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  bool load(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
  /// Recompiles if either source file changed on disk. Returns true if reloaded.
  bool reloadIfChanged();
  void use() const;
  /// Deletes the program now, while its context is still there; the destructor then does nothing.
  void release();
  GLuint id() const { return program_; }
  /// A uniform's location, asked of the driver once per name (and per program linked): ogles2
  /// is a library call away, and the renderer asks for some thirty a frame.
  GLint uniform(const char* name) const;

 private:
  bool compileFromFiles();
  GLuint program_ = 0;
  std::filesystem::path vertexPath_, fragmentPath_;
  std::filesystem::file_time_type vertexTime_{}, fragmentTime_{};
  std::string error_;
  mutable std::vector<std::pair<std::string, GLint>> uniforms_;
};

}  // namespace encore

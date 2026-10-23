#include "platform/DataLocator.h"

#include <SDL3/SDL.h>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <string>
#include <system_error>

#include "core/DataMove.h"
#include "core/File.h"
#include "core/Log.h"
#include "data/GameVersion.h"

namespace encore {
namespace {

bool holdsGameFiles(const std::filesystem::path& dir) {
  return file::findCaseInsensitive(dir, "INTRO.PRG").has_value() &&
         file::findCaseInsensitive(dir, "TABLE1.PRG").has_value();
}

/// The files are there and are the version the engine supports.
bool holdsGame(const std::filesystem::path& dir) {
  if (!holdsGameFiles(dir)) return false;
  const auto bad = unsupportedGameFiles(dir);
  if (bad.empty()) return true;
  if (tablesOnlyRelease(dir)) {
    log::info(dir.string() + " holds the Pinball Power Pack (1996): its tables, without the intro and the menu");
    return true;
  }
  log::info(dir.string() + " holds a different release of the game; skipping it");
  return false;
}

}  // namespace

std::filesystem::path gameDataDir() {
  const std::filesystem::path prefs = preferencesDir();
#ifdef __amigaos__
  return prefs / "FANTASY";  // (relative: beside the program)
#endif
  return prefs.empty() ? prefs : prefs / "FANTASY";
}

std::optional<std::filesystem::path> locateGameData(const std::optional<std::filesystem::path>& explicitDir) {
  // One place, known in advance: no folder is searched, nothing around the application is
  // stepped through, and nothing outside this version's own folder is read.
  if (explicitDir) return holdsGame(*explicitDir) ? std::optional(*explicitDir) : std::nullopt;
  const std::filesystem::path dir = gameDataDir();
  if (!dir.empty() && holdsGame(dir)) return dir;
  return std::nullopt;
}

std::filesystem::path preferencesDir() { return portableMode() ? programDir() : osPreferencesDir(); }

std::filesystem::path osPreferencesDir() {
#ifdef __amigaos__
  // (everything beside the program, as Amiga programs keep it: see programDir)
  return programDir();
#endif
  char* base = SDL_GetPrefPath("Encore", "Pinball Fantasies");
  if (!base) return {};
  std::filesystem::path path(base);
  SDL_free(base);
  return path;
}

std::filesystem::path programDir() {
#ifdef __amigaos__
  // The current folder, which main() makes the program's own (PROGDIR:). An empty path, and
  // so relative ones from it: std::filesystem joins "PROGDIR:" and "x" as "PROGDIR:/x", which
  // AmigaDOS reads as the folder above.
  return {};
#endif
  const char* base = SDL_GetBasePath();
  std::filesystem::path dir = base ? std::filesystem::path(base) : std::filesystem::current_path();
  if (!dir.has_filename()) dir = dir.parent_path();  // (SDL's ends in a separator)
#ifdef __APPLE__
  // In an application bundle the program's own folder is Contents/Resources (the shaders and
  // the font are there); the folder the player sees is the one the bundle is in.
  if (dir.filename() == "Resources" && dir.parent_path().filename() == "Contents" &&
      dir.parent_path().parent_path().extension() == ".app")
    return dir.parent_path().parent_path().parent_path();
#endif
  return dir;
}

std::filesystem::path portableMarker() { return programDir() / "portable.txt"; }
#ifdef __amigaos__
// (the program's folder is the only one there is: never portable, nor anything to move)
bool portableMode() { return false; }
#else

bool portableMode() {
  std::error_code ec;
  return std::filesystem::exists(portableMarker(), ec);
}
#endif

namespace {

/// Whether `dir` is `base` or inside it (the two as the system spells them).
bool under(const std::filesystem::path& dir, const std::filesystem::path& base) {
  if (base.empty()) return false;
  std::error_code ec;
  auto d = std::filesystem::weakly_canonical(dir, ec).generic_string();
  auto b = std::filesystem::weakly_canonical(base, ec).generic_string();
#ifdef _WIN32
  for (char& c : d) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (char& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
#endif
  if (!b.empty() && b.back() == '/') b.pop_back();
  return d == b || d.compare(0, b.size() + 1, b + "/") == 0;
}

/// Whether the program is somewhere applications are installed, where its files should not go.
bool inAppFolder(const std::filesystem::path& dir) {
#if defined(__APPLE__)
  const char* home = std::getenv("HOME");
  return under(dir, "/Applications") || (home && under(dir, std::filesystem::path(home) / "Applications"));
#elif defined(_WIN32)
  for (const char* var : {"ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"})
    if (const char* base = std::getenv(var); base && under(dir, base)) return true;
  return false;
#else
  return under(dir, "/usr") || under(dir, "/opt");
#endif
}

}  // namespace

PortableLock portableLock(bool turningOn, std::uintmax_t bytes) {
#ifdef __amigaos__
  // (the things are beside the program already, and there is no other place to move them to)
  (void)turningOn, (void)bytes;
  return PortableLock::ReadOnly;
#endif
  const std::filesystem::path program = programDir();
#ifdef __APPLE__
  // A quarantined application is run from a hidden copy, read-only and somewhere else each
  // time: a portable.txt beside the real one would never be seen. Moving the application once
  // with Finder ends that.
  if (program.generic_string().find("/AppTranslocation/") != std::string::npos) return PortableLock::Translocated;
#endif
  if (inAppFolder(program)) return PortableLock::AppFolder;
  // (the program's folder is written either way: the things go there, or the marker there goes)
  if (!writable(program)) return PortableLock::ReadOnly;
  const std::filesystem::path target = turningOn ? program : osPreferencesDir();
  if (!turningOn && !writable(target)) return PortableLock::ReadOnly;
  // (the copy stands beside the original while it is checked, on the same volume or another:
  // room for it, and some to spare)
  if (const auto free = freeSpace(target); free && *free < bytes + bytes / 20 + (16u << 20)) return PortableLock::NoSpace;
  return PortableLock::None;
}

void reportMissingGameData() {
  const std::string msg =
      "The original Pinball Fantasies files were not found.\n\n"
      "This game reads the artwork, music and tables from your own copy of the 1994\n"
      "MS-DOS release. Put INTRO.PRG, TABLE1..4.PRG and the MOD files in:\n\n" +
      gameDataDir().string() + "\n\nor start the game with:  --data /path/to/pinball";
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pinball Fantasies", msg.c_str(), nullptr);
}

void reportError(const std::string& message) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pinball Fantasies: Encore!", message.c_str(), nullptr);
}

}  // namespace encore

#pragma once
// Finds the player's copy of the original game files, so the app can be launched by
// double-clicking it rather than only from a terminal with an argument.
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "game/Config.h"

namespace encore {

/// The folder the game files are read from: FANTASY, inside this version's own folder
/// (see preferencesDir), and nowhere else -- or the folder given with --data, when there
/// is one. Nothing is searched for, so nothing outside this version's own things is read.
std::optional<std::filesystem::path> locateGameData(const std::optional<std::filesystem::path>& explicitDir);

/// Where the game files belong: <preferences>/FANTASY.
std::filesystem::path gameDataDir();

/// Where the app keeps its own files: the game files, options, high scores. The system's
/// folder for them, or, portable, the program's own folder (portableMode).
std::filesystem::path preferencesDir();
/// The system's folder for them, whether or not they are there now.
std::filesystem::path osPreferencesDir();

/// The program's own folder: the one the executable is in, or on macOS the one the application
/// bundle is in.
std::filesystem::path programDir();
/// A portable.txt there says the app's files are kept there (Options::portable).
std::filesystem::path portableMarker();
bool portableMode();
/// Whether PORTABLE can be turned on (or off) in the options, and if not why not: whether the
/// folder the files would go to can be written, with `bytes` of room to spare, and is not one
/// applications are installed in.
PortableLock portableLock(bool turningOn, std::uintmax_t bytes);

/// Tells the player what is missing.
void reportMissingGameData();

/// Says what went wrong, for a failure that would otherwise end the program in silence.
void reportError(const std::string& message);

}  // namespace encore

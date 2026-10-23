#pragma once
// The engine reads the game's data at fixed addresses, so it needs exactly the file
// version it was written from: the 1994 disk release. Other releases (for
// example cracked re-releases) are laid out differently and are refused up front.
#include <filesystem>
#include <string>
#include <vector>

namespace encore {

/// The files that differ from the supported version, or cannot be read (empty: all good).
/// INTRO.MOD is not checked: the original game rewrites it as part of its copy protection.
std::vector<std::string> unsupportedGameFiles(const std::filesystem::path& dir);

/// The 1996 Pinball Power Pack (21st Century's CD): its tables and music are the 1994 release's,
/// byte for byte, but its INTRO.PRG is another program, which the slideshow and the menu cannot
/// be read from. True when that is the one file that differs, and it is that INTRO.PRG: the
/// tables can then be played without the intro and the menu.
bool tablesOnlyRelease(const std::filesystem::path& dir);

}  // namespace encore

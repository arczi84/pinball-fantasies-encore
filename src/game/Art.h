#pragma once
// The HD pictures do not travel with the application: the server keeps the current set
// (server/publish-art.sh puts it there) and the game fetches it, when the player says yes,
// into its preferences folder as art/<version>. A set is fetched into art/<version>.part, every
// picture checked against its SHA-256, and only renamed into place once all of it is there, so
// an interrupted fetch never leaves half a set in use. The sets before it are then removed.
#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Types.h"

namespace encore {

/// What this game understands of a set. A set of a higher format needs code this game does
/// not have, and is left alone; raise it with the pictures that need it (server/README.md).
/// The server is told it, and gives the newest set of this format or older. 2: one picture of
/// a bat for every flipper (flipper.png), where 1 had a picture of each flipper.
constexpr int kArtFormat = 2;

struct ArtFile {
  std::string sha256;
  u64 size = 0;
  std::string name, url;
};

struct ArtSet {
  int version = 0;
  int format = 0;
  std::vector<ArtFile> files;
};

/// A set as the server describes it (GET /v1/art), or nothing if the text is not one.
std::optional<ArtSet> parseArtSet(std::string_view text);

/// The current set on the server. It blocks, so it is asked for from a thread of its own.
std::optional<ArtSet> fetchArtSet(std::string* error = nullptr);

/// A set fetched before, complete: where it is, and what it holds.
struct InstalledArt {
  std::filesystem::path dir;
  ArtSet set;
};
/// The newest one, or nothing if no set was ever fetched.
std::optional<InstalledArt> installedArt(const std::filesystem::path& saveDir);

/// What fetching `set` would download, given what is there already: a picture the installed
/// set has the same is copied instead.
u64 artBytesToFetch(const ArtSet& set, const std::optional<InstalledArt>& have);

/// The version the player last turned down, so that it is not offered again; 0 for none.
int declinedArt(const std::filesystem::path& saveDir);
void declineArt(const std::filesystem::path& saveDir, int version);

/// How far a fetch has got, for the screen showing it, and a way to stop it.
struct ArtProgress {
  std::atomic<u64> done{0};
  u64 total = 0;
  std::atomic<bool> cancel{false};
};

/// Fetches `set` into the preferences folder, taking the pictures `have` holds the same rather
/// than downloading them again. True once the set is complete and in place, the older ones
/// gone; false, leaving everything as it was, on any failure or when cancelled. It blocks.
bool fetchArt(const std::filesystem::path& saveDir, const ArtSet& set, const std::optional<InstalledArt>& have,
              ArtProgress& progress, std::string* error = nullptr);

}  // namespace encore

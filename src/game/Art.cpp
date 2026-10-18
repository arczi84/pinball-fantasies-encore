#include "game/Art.h"

#include <algorithm>
#include <charconv>
#include <system_error>

#include "core/File.h"
#include "core/Log.h"
#include "core/Sha256.h"
#include "game/Online.h"
#include "platform/Http.h"

namespace encore {
namespace {

/// Where the sets are kept, and what the set in a folder holds (the server's text, as fetched).
std::filesystem::path artDir(const std::filesystem::path& saveDir) { return saveDir / "art"; }
constexpr const char* kSetFile = "set.txt";

/// As the game names its pictures: playfield1_on.png. Nothing else may be written, so a name
/// cannot reach outside the set's folder.
bool pictureName(std::string_view name) {
  if (name.size() < 5 || name.size() > 68 || !name.ends_with(".png")) return false;
  return std::all_of(name.begin(), name.end() - 4,
                     [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

bool hexDigest(std::string_view s) {
  return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

template <typename T>
bool number(std::string_view s, T& out) {
  const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
  return ec == std::errc() && end == s.data() + s.size();
}

/// The words of a line, split at spaces.
std::vector<std::string_view> words(std::string_view line) {
  std::vector<std::string_view> out;
  std::size_t at = 0;
  while (at < line.size()) {
    const std::size_t space = line.find(' ', at);
    const std::size_t end = space == std::string_view::npos ? line.size() : space;
    if (end > at) out.push_back(line.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

std::string text(const Bytes& bytes) { return std::string(bytes.begin(), bytes.end()); }

/// A folder's set, if it is a complete one: named after its version, and holding its text.
std::optional<ArtSet> setIn(const std::filesystem::path& dir) {
  int version = 0;
  if (!number(dir.filename().string(), version) || version <= 0) return std::nullopt;
  const auto saved = file::readAll(dir / kSetFile);
  if (!saved) return std::nullopt;
  auto set = parseArtSet(text(*saved));
  if (!set || set->version != version) return std::nullopt;
  return set;
}

/// The picture of that name in the installed set, if it is the same picture.
const ArtFile* sameIn(const std::optional<InstalledArt>& have, const ArtFile& f) {
  if (!have) return nullptr;
  for (const auto& h : have->set.files)
    if (h.name == f.name && h.sha256 == f.sha256) return &h;
  return nullptr;
}

}  // namespace

std::optional<ArtSet> parseArtSet(std::string_view body) {
  ArtSet set;
  std::size_t at = 0;
  while (at < body.size()) {
    const std::size_t newline = body.find('\n', at);
    std::string_view line = body.substr(at, newline == std::string_view::npos ? std::string_view::npos : newline - at);
    at = newline == std::string_view::npos ? body.size() : newline + 1;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    const auto w = words(line);
    if (w.empty()) continue;
    if (w.size() == 2 && w[0] == "version") {
      if (!number(w[1], set.version)) return std::nullopt;
    } else if (w.size() == 2 && w[0] == "format") {
      if (!number(w[1], set.format)) return std::nullopt;
    } else if (w.size() == 4) {
      ArtFile f{std::string(w[0]), 0, std::string(w[2]), std::string(w[3])};
      if (!hexDigest(f.sha256) || !number(w[1], f.size) || !pictureName(f.name)) return std::nullopt;
      if (!f.url.starts_with("https://") && !f.url.starts_with("http://")) return std::nullopt;
      for (const auto& other : set.files)
        if (other.name == f.name) return std::nullopt;
      set.files.push_back(std::move(f));
    } else {
      return std::nullopt;
    }
  }
  if (set.version <= 0 || set.format <= 0 || set.files.empty()) return std::nullopt;
  return set;
}

std::optional<ArtSet> fetchArtSet(std::string* error) {
  const auto body = httpDownload(onlineApi() + "/v1/art?format=" + std::to_string(kArtFormat), error);
  if (!body) return std::nullopt;
  auto set = parseArtSet(text(*body));
  if (!set && error) *error = "the server's answer is not a set of pictures";
  return set;
}

std::optional<InstalledArt> installedArt(const std::filesystem::path& saveDir) {
  std::optional<InstalledArt> newest;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(artDir(saveDir), ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_directory(ec)) continue;
    auto set = setIn(it->path());
    if (set && (!newest || set->version > newest->set.version)) newest = InstalledArt{it->path(), std::move(*set)};
  }
  return newest;
}

u64 artBytesToFetch(const ArtSet& set, const std::optional<InstalledArt>& have) {
  u64 bytes = 0;
  for (const auto& f : set.files)
    if (!sameIn(have, f)) bytes += f.size;
  return bytes;
}

int declinedArt(const std::filesystem::path& saveDir) {
  const auto saved = file::readAll(artDir(saveDir) / "declined.txt");
  int version = 0;
  if (saved) number(text(*saved), version);
  return version;
}

void declineArt(const std::filesystem::path& saveDir, int version) {
  std::error_code ec;
  std::filesystem::create_directories(artDir(saveDir), ec);
  const std::string v = std::to_string(version);
  file::writeAll(artDir(saveDir) / "declined.txt", ByteView(reinterpret_cast<const u8*>(v.data()), v.size()));
}

bool fetchArt(const std::filesystem::path& saveDir, const ArtSet& set, const std::optional<InstalledArt>& have,
              ArtProgress& progress, std::string* error) {
  const auto fail = [&](const std::string& why, const std::filesystem::path& part) {
    std::error_code ec;
    std::filesystem::remove_all(part, ec);
    if (error) *error = why;
    return false;
  };
  const auto dir = artDir(saveDir);
  const auto part = dir / (std::to_string(set.version) + ".part");
  const auto done = dir / std::to_string(set.version);
  std::error_code ec;
  std::filesystem::remove_all(part, ec);
  std::filesystem::create_directories(part, ec);
  if (ec) return fail("cannot make " + part.string() + ": " + ec.message(), part);

  for (const auto& f : set.files) {
    if (progress.cancel) return fail("cancelled", part);
    std::optional<Bytes> data;
    // A picture the set before had the same is taken from there, if it is still what it was.
    if (const ArtFile* same = sameIn(have, f)) {
      data = file::readAll(have->dir / same->name);
      if (data && sha256Hex(*data) != f.sha256) data.reset();
    }
    if (!data) {
      std::string why;
      data = httpDownload(f.url, &why);
      if (!data) return fail(f.name + ": " + why, part);
      if (data->size() != f.size || sha256Hex(*data) != f.sha256) return fail(f.name + " is not what the set says", part);
      progress.done += f.size;
    }
    if (!file::writeAll(part / f.name, *data)) return fail("cannot write " + (part / f.name).string(), part);
  }
  // The set's own description goes in last: a folder without it is never taken for a set.
  std::string description = "version " + std::to_string(set.version) + "\nformat " + std::to_string(set.format) + "\n";
  for (const auto& f : set.files) description += f.sha256 + " " + std::to_string(f.size) + " " + f.name + " " + f.url + "\n";
  if (!file::writeAll(part / kSetFile, ByteView(reinterpret_cast<const u8*>(description.data()), description.size())))
    return fail("cannot write the set's description", part);

  std::filesystem::remove_all(done, ec);
  std::filesystem::rename(part, done, ec);
  if (ec) return fail("cannot put the set in place: " + ec.message(), part);
  // Only now are the older sets, and anything left from fetches that did not finish, removed.
  std::vector<std::filesystem::path> old;
  for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_directory(ec) && it->path() != done) old.push_back(it->path());
  for (const auto& path : old) std::filesystem::remove_all(path, ec);
  log::info("HD pictures: version " + std::to_string(set.version) + " in place");
  return true;
}

}  // namespace encore

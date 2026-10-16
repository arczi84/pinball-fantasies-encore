#include "core/DataMove.h"

#include <system_error>
#include <utility>
#include <vector>

#include "core/File.h"
#include "core/Log.h"

namespace encore {
namespace {

namespace fs = std::filesystem;

/// The target's folder for a move under way: the items copied so far under new/, what the
/// target had of the same names under old/.
constexpr const char* kStaging = ".encore-move";

struct Failure {
  std::string step, why;
};

/// One item copied, file by file, so that the bytes done can be told and a failure named.
std::optional<Failure> copyItem(const fs::path& from, const fs::path& to, std::uintmax_t& done,
                                const std::function<void(std::uintmax_t)>& progress) {
  std::error_code ec;
  if (fs::is_directory(from, ec)) {
    fs::create_directory(to, ec);
    if (ec) return Failure{"making " + to.string(), ec.message()};
    for (const auto& entry : fs::directory_iterator(from, ec)) {
      if (auto f = copyItem(entry.path(), to / entry.path().filename(), done, progress)) return f;
    }
    if (ec) return Failure{"reading " + from.string(), ec.message()};
    return std::nullopt;
  }
  if (ec) return Failure{"looking at " + from.string(), ec.message()};
  fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
  if (ec) return Failure{"copying " + from.string(), ec.message()};
  done += fs::file_size(to, ec);
  if (progress) progress(done);
  return std::nullopt;
}

/// Whether a copy has every file of the original, each of the same size.
std::optional<Failure> checkCopy(const fs::path& from, const fs::path& to) {
  std::error_code ec;
  if (fs::is_directory(from, ec)) {
    for (const auto& entry : fs::directory_iterator(from, ec)) {
      if (auto f = checkCopy(entry.path(), to / entry.path().filename())) return f;
    }
    return std::nullopt;
  }
  const auto a = fs::file_size(from, ec), b = fs::file_size(to, ec);
  if (ec || a != b) return Failure{"checking " + to.string(), ec ? ec.message() : "the copy is not the same size"};
  return std::nullopt;
}

}  // namespace

std::uintmax_t dataSize(const fs::path& from, std::span<const std::string_view> items) {
  std::uintmax_t total = 0;
  std::error_code ec;
  for (const std::string_view name : items) {
    const auto item = file::findCaseInsensitive(from, name);
    if (!item) continue;
    if (fs::is_directory(*item, ec)) {
      for (const auto& entry : fs::recursive_directory_iterator(*item, ec))
        if (entry.is_regular_file(ec)) total += entry.file_size(ec);
    } else if (fs::is_regular_file(*item, ec)) {
      total += fs::file_size(*item, ec);
    }
  }
  return total;
}

bool writable(const fs::path& dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  const fs::path probe = dir / ".encore-write-test";
  const u8 byte = 0;
  if (!file::writeAll(probe, ByteView(&byte, 1))) return false;
  fs::remove(probe, ec);
  return true;
}

std::optional<std::uintmax_t> freeSpace(const fs::path& dir) {
  std::error_code ec;
  const auto info = fs::space(dir, ec);
  if (ec) return std::nullopt;
  return info.available;
}

MoveReport moveData(const fs::path& from, const fs::path& to, const std::function<std::string()>& commit,
                    const std::function<void(std::uintmax_t)>& progress, std::span<const std::string_view> items) {
  std::error_code ec;
  const fs::path staging = to / kStaging, fresh = staging / "new", aside = staging / "old";
  const auto fail = [&](Failure f) {
    fs::remove_all(staging, ec);
    return MoveReport{false, std::move(f.step), std::move(f.why)};
  };
  fs::create_directories(to, ec);
  if (ec) return fail({"making " + to.string(), ec.message()});
  // (what an earlier move left there, had the program ended in the middle of it)
  fs::remove_all(staging, ec);
  fs::create_directories(fresh, ec);
  if (!ec) fs::create_directories(aside, ec);
  if (ec) return fail({"making " + staging.string(), ec.message()});

  // 1. the items, as the source has them (in whatever case), copied into the staging folder
  std::vector<fs::path> found;  // their paths in the source
  std::uintmax_t done = 0;
  for (const std::string_view name : items) {
    const auto item = file::findCaseInsensitive(from, name);
    if (!item) continue;
    if (auto f = copyItem(*item, fresh / item->filename(), done, progress)) return fail(*f);
    if (auto f = checkCopy(*item, fresh / item->filename())) return fail(*f);
    found.push_back(*item);
  }

  // 2. into their places, the target's own of the same names put aside: renames within the
  // target, undone in reverse if one fails
  struct Placed {
    fs::path name;
    bool hadOld;
  };
  std::vector<Placed> placed;
  const auto undo = [&] {
    for (auto it = placed.rbegin(); it != placed.rend(); ++it) {
      std::error_code e;
      fs::rename(to / it->name, fresh / it->name, e);
      if (it->hadOld) fs::rename(aside / it->name, to / it->name, e);
    }
    placed.clear();
  };
  for (const fs::path& item : found) {
    const fs::path name = item.filename();
    const bool hadOld = fs::exists(to / name, ec);
    if (hadOld) {
      fs::rename(to / name, aside / name, ec);
      if (ec) {
        undo();
        return fail({"putting aside " + (to / name).string(), ec.message()});
      }
    }
    fs::rename(fresh / name, to / name, ec);
    if (ec) {
      if (hadOld) fs::rename(aside / name, to / name, ec);
      undo();
      return fail({"placing " + (to / name).string(), ec.message()});
    }
    placed.push_back({name, hadOld});
  }

  // 3. the word that the game's things are now here
  if (std::string why = commit(); !why.empty()) {
    undo();
    return fail({"marking the move", std::move(why)});
  }

  // 4. From here the target is the game's folder: what is left over goes, and nothing failing
  // to go is a failure.
  fs::remove_all(staging, ec);
  if (ec) log::error("after moving the game's files: " + staging.string() + " could not be removed: " + ec.message());
  for (const fs::path& item : found) {
    fs::remove_all(item, ec);
    if (ec) log::error("after moving the game's files: " + item.string() + " could not be removed: " + ec.message());
  }
  return MoveReport{true, {}, {}};
}

void discardStaleMove(const fs::path& dir) {
  std::error_code ec;
  const fs::path staging = dir / kStaging;
  if (!fs::exists(staging, ec)) return;
  fs::remove_all(staging, ec);
  log::info(ec ? "a move of the game's files left " + staging.string() + ", which could not be removed: " + ec.message()
               : "a move of the game's files was left half done; what it left in " + dir.string() + " is removed");
}

}  // namespace encore

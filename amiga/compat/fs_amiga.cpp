// std::filesystem's operations for the Amiga port, on AmigaDOS.
//
// GCC 16 rc18's libstdc++ has them built for newlib with most of them missing: its fs_dir.o
// cannot read a folder (no opendir), its fs_ops.o knows only stat() in newlib's layout (with
// libnix every folder looks like nothing) and makes no folders. The game's code is left as it
// is: these are the same functions, with the same names, so the linker takes them from here and
// never pulls fs_ops.o or fs_dir.o out of the library (amiga/Makefile checks the map).
//
// Only what the game calls is here (the error_code overloads), plus what directory_iterator
// and recursive_directory_iterator need. Paths go to AmigaDOS as they are: relative ones from
// the current folder, which main() makes the program's own.
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>

namespace std::filesystem {
namespace {

std::error_code dosError(LONG code) {
  switch (code) {
    case ERROR_OBJECT_NOT_FOUND:
    case ERROR_DIR_NOT_FOUND:
    case ERROR_DEVICE_NOT_MOUNTED:
      return std::make_error_code(std::errc::no_such_file_or_directory);
    case ERROR_OBJECT_EXISTS: return std::make_error_code(std::errc::file_exists);
    case ERROR_OBJECT_IN_USE: return std::make_error_code(std::errc::device_or_resource_busy);
    case ERROR_DISK_WRITE_PROTECTED:
    case ERROR_WRITE_PROTECTED:
    case ERROR_DELETE_PROTECTED:
    case ERROR_READ_PROTECTED:
      return std::make_error_code(std::errc::permission_denied);
    case ERROR_DIRECTORY_NOT_EMPTY: return std::make_error_code(std::errc::directory_not_empty);
    case ERROR_DISK_FULL: return std::make_error_code(std::errc::no_space_on_device);
    case ERROR_NO_FREE_STORE: return std::make_error_code(std::errc::not_enough_memory);
    case ERROR_OBJECT_WRONG_TYPE: return std::make_error_code(std::errc::not_a_directory);
    default: return std::make_error_code(std::errc::io_error);
  }
}

bool notFound(LONG code) {
  return code == ERROR_OBJECT_NOT_FOUND || code == ERROR_DIR_NOT_FOUND || code == ERROR_DEVICE_NOT_MOUNTED;
}

/// A lock on the object, and what Examine says of it.
struct Examined {
  BPTR lock = 0;
  FileInfoBlock* fib = nullptr;
  LONG error = 0;
  explicit Examined(const path& p) {
    const std::string name = p.string();
    if (name.empty()) {
      error = ERROR_OBJECT_NOT_FOUND;
      return;
    }
    if (!(lock = Lock(reinterpret_cast<CONST_STRPTR>(name.c_str()), SHARED_LOCK))) {
      error = IoErr();
      return;
    }
    fib = static_cast<FileInfoBlock*>(AllocDosObject(DOS_FIB, nullptr));
    if (!fib || !Examine(lock, fib)) error = fib ? IoErr() : ERROR_NO_FREE_STORE;
  }
  ~Examined() {
    if (fib) FreeDosObject(DOS_FIB, fib);
    if (lock) UnLock(lock);
  }
  bool ok() const { return error == 0; }
  bool directory() const { return fib->fib_DirEntryType > 0; }
};

file_type typeOf(const FileInfoBlock* fib) { return fib->fib_DirEntryType > 0 ? file_type::directory : file_type::regular; }

}  // namespace

file_status status(const path& p, std::error_code& ec) noexcept {
  const Examined e(p);
  if (!e.ok()) {
    ec = dosError(e.error);
    return file_status(notFound(e.error) ? file_type::not_found : file_type::none);
  }
  ec.clear();
  return file_status(typeOf(e.fib), perms::all);
}

std::uintmax_t file_size(const path& p, std::error_code& ec) noexcept {
  const Examined e(p);
  if (!e.ok()) {
    ec = dosError(e.error);
    return static_cast<std::uintmax_t>(-1);
  }
  if (e.directory()) {
    ec = std::make_error_code(std::errc::is_a_directory);
    return static_cast<std::uintmax_t>(-1);
  }
  ec.clear();
  return static_cast<std::uintmax_t>(static_cast<ULONG>(e.fib->fib_Size));
}

file_time_type last_write_time(const path& p, std::error_code& ec) noexcept {
  const Examined e(p);
  if (!e.ok()) {
    ec = dosError(e.error);
    return file_time_type::min();
  }
  ec.clear();
  // (AmigaDOS counts from 1978, the system clock from 1970: 2922 days; local time, as the Amiga keeps it)
  const struct DateStamp& d = e.fib->fib_Date;
  const long long seconds = (static_cast<long long>(d.ds_Days) + 2922) * 86400 + d.ds_Minute * 60LL + d.ds_Tick / TICKS_PER_SECOND;
  return std::chrono::file_clock::from_sys(std::chrono::sys_seconds(std::chrono::seconds(seconds)));
}

bool create_directory(const path& p, std::error_code& ec) noexcept {
  {
    const Examined e(p);
    if (e.ok()) {
      if (e.directory()) {
        ec.clear();
        return false;
      }
      ec = std::make_error_code(std::errc::file_exists);
      return false;
    }
  }
  const std::string name = p.string();
  const BPTR lock = CreateDir(reinterpret_cast<CONST_STRPTR>(name.c_str()));
  if (!lock) {
    ec = dosError(IoErr());
    return false;
  }
  UnLock(lock);
  ec.clear();
  return true;
}

bool create_directories(const path& p, std::error_code& ec) {
  // Each folder on the way, as far as the separators go ("A/B/C": A, A/B, A/B/C); a device or
  // assign ("Work:") is there already.
  std::string name = p.string();
  while (name.size() > 1 && name.back() == '/') name.pop_back();
  if (name.empty()) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return false;
  }
  bool made = false;
  std::size_t from = name.find(':');
  from = from == std::string::npos ? 0 : from + 1;
  for (std::size_t at = name.find('/', from);; at = name.find('/', at + 1)) {
    const std::string part = at == std::string::npos ? name : name.substr(0, at);
    if (!part.empty() && part.back() != ':') {
      made = create_directory(part, ec) || made;
      if (ec) return false;
    }
    if (at == std::string::npos) break;
  }
  return made;
}

bool remove(const path& p, std::error_code& ec) noexcept {
  const std::string name = p.string();
  if (!name.empty() && DeleteFile(reinterpret_cast<CONST_STRPTR>(name.c_str()))) {
    ec.clear();
    return true;
  }
  const LONG error = name.empty() ? ERROR_OBJECT_NOT_FOUND : IoErr();
  if (notFound(error)) {
    ec.clear();
    return false;
  }
  ec = dosError(error);
  return false;
}

std::uintmax_t remove_all(const path& p, std::error_code& ec) {
  std::vector<path> inside;
  {
    const Examined e(p);
    if (!e.ok()) {
      if (notFound(e.error)) {
        ec.clear();
        return 0;
      }
      ec = dosError(e.error);
      return static_cast<std::uintmax_t>(-1);
    }
    if (e.directory())
      while (ExNext(e.lock, e.fib)) inside.push_back(p / reinterpret_cast<const char*>(e.fib->fib_FileName));
  }
  std::uintmax_t count = 0;
  for (const path& one : inside) {
    const std::uintmax_t n = remove_all(one, ec);
    if (ec) return static_cast<std::uintmax_t>(-1);
    count += n;
  }
  if (!remove(p, ec)) return ec ? static_cast<std::uintmax_t>(-1) : count;
  return count + 1;
}

void rename(const path& from, const path& to, std::error_code& ec) noexcept {
  const std::string a = from.string(), b = to.string();
  if (Rename(reinterpret_cast<CONST_STRPTR>(a.c_str()), reinterpret_cast<CONST_STRPTR>(b.c_str()))) {
    ec.clear();
    return;
  }
  LONG error = IoErr();
  // (a file there already is replaced, as rename() does elsewhere)
  if (error == ERROR_OBJECT_EXISTS) {
    const Examined e(to);
    if (e.ok() && !e.directory() && DeleteFile(reinterpret_cast<CONST_STRPTR>(b.c_str())) &&
        Rename(reinterpret_cast<CONST_STRPTR>(a.c_str()), reinterpret_cast<CONST_STRPTR>(b.c_str()))) {
      ec.clear();
      return;
    }
    error = IoErr();
  }
  ec = dosError(error);
}

bool copy_file(const path& from, const path& to, copy_options options, std::error_code& ec) {
  {
    const Examined target(to);
    if (target.ok()) {
      if ((options & copy_options::skip_existing) != copy_options::none) {
        ec.clear();
        return false;
      }
      if ((options & (copy_options::overwrite_existing | copy_options::update_existing)) == copy_options::none) {
        ec = std::make_error_code(std::errc::file_exists);
        return false;
      }
    }
  }
  const std::string a = from.string(), b = to.string();
  const BPTR in = Open(reinterpret_cast<CONST_STRPTR>(a.c_str()), MODE_OLDFILE);
  if (!in) {
    ec = dosError(IoErr());
    return false;
  }
  const BPTR out = Open(reinterpret_cast<CONST_STRPTR>(b.c_str()), MODE_NEWFILE);
  if (!out) {
    ec = dosError(IoErr());
    Close(in);
    return false;
  }
  std::vector<char> buffer(65536);
  bool ok = true;
  for (;;) {
    const LONG got = Read(in, buffer.data(), static_cast<LONG>(buffer.size()));
    if (got < 0) {
      ok = false;
      break;
    }
    if (got == 0) break;
    if (Write(out, buffer.data(), got) != got) {
      ok = false;
      break;
    }
  }
  const LONG error = ok ? 0 : IoErr();
  Close(out);
  Close(in);
  if (!ok) {
    DeleteFile(reinterpret_cast<CONST_STRPTR>(b.c_str()));
    ec = dosError(error);
    return false;
  }
  ec.clear();
  return true;
}

space_info space(const path& p, std::error_code& ec) noexcept {
  space_info s{static_cast<std::uintmax_t>(-1), static_cast<std::uintmax_t>(-1), static_cast<std::uintmax_t>(-1)};
  const Examined e(p);
  if (!e.ok()) {
    ec = dosError(e.error);
    return s;
  }
  InfoData* info = static_cast<InfoData*>(AllocVec(sizeof(InfoData), MEMF_PUBLIC | MEMF_CLEAR));
  if (!info || !Info(e.lock, info)) {
    ec = dosError(info ? IoErr() : ERROR_NO_FREE_STORE);
    if (info) FreeVec(info);
    return s;
  }
  const std::uintmax_t block = static_cast<ULONG>(info->id_BytesPerBlock);
  s.capacity = block * static_cast<ULONG>(info->id_NumBlocks);
  s.free = s.available = block * static_cast<ULONG>(info->id_NumBlocks - info->id_NumBlocksUsed);
  FreeVec(info);
  ec.clear();
  return s;
}

// ------------------------------------------------------------------------------------------
// directory_iterator: a lock on the folder and ExNext through it.

inline namespace __cxx11 {
struct _Dir {
  BPTR lock = 0;
  FileInfoBlock* fib = nullptr;
  path dir;
  directory_entry entry;

  ~_Dir() {
    if (fib) FreeDosObject(DOS_FIB, fib);
    if (lock) UnLock(lock);
  }
  /// The next entry; false at the end, or with `ec` set when the folder could not be read.
  bool next(std::error_code& ec) {
    if (!ExNext(lock, fib)) {
      const LONG error = IoErr();
      if (error != ERROR_NO_MORE_ENTRIES) ec = dosError(error);
      return false;
    }
    entry._M_path = dir / reinterpret_cast<const char*>(fib->fib_FileName);
    entry._M_type = typeOf(fib);
    return true;
  }
};
}  // namespace __cxx11

directory_iterator::directory_iterator(const path& p, directory_options, std::error_code* ecptr) {
  std::error_code ec;
  auto d = std::__make_shared<_Dir>();
  const std::string name = p.string();
  if (name.empty() || !(d->lock = Lock(reinterpret_cast<CONST_STRPTR>(name.c_str()), SHARED_LOCK)))
    ec = dosError(name.empty() ? ERROR_OBJECT_NOT_FOUND : IoErr());
  else if (!(d->fib = static_cast<FileInfoBlock*>(AllocDosObject(DOS_FIB, nullptr))) || !Examine(d->lock, d->fib))
    ec = dosError(d->fib ? IoErr() : ERROR_NO_FREE_STORE);
  else if (d->fib->fib_DirEntryType <= 0)
    ec = std::make_error_code(std::errc::not_a_directory);
  if (!ec) {
    d->dir = p;
    if (d->next(ec)) _M_dir = std::move(d);
  }
  if (ecptr) *ecptr = ec;
}

const directory_entry& directory_iterator::operator*() const noexcept { return _M_dir->entry; }

directory_iterator& directory_iterator::increment(std::error_code& ec) {
  ec.clear();
  if (_M_dir && !_M_dir->next(ec)) _M_dir.reset();
  return *this;
}

directory_iterator& directory_iterator::operator++() {
  std::error_code ec;
  return increment(ec);
}

// recursive_directory_iterator: only for moving the game's files between folders, which the
// Amiga never does (platform/DataLocator.cpp: no portable mode). It is the end at once, and
// says so.

struct recursive_directory_iterator::_Dir_stack {
  directory_entry entry;
};

recursive_directory_iterator::recursive_directory_iterator(const path&, directory_options, std::error_code* ecptr) {
  if (ecptr) *ecptr = std::make_error_code(std::errc::not_supported);
}

recursive_directory_iterator::~recursive_directory_iterator() = default;

const directory_entry& recursive_directory_iterator::operator*() const noexcept { return _M_dirs->entry; }

recursive_directory_iterator& recursive_directory_iterator::increment(std::error_code& ec) {
  ec.clear();
  _M_dirs.reset();
  return *this;
}

recursive_directory_iterator& recursive_directory_iterator::operator++() {
  std::error_code ec;
  return increment(ec);
}

}  // namespace std::filesystem

// (the header has them extern; with fs_dir.o left out, they are made here)
template class std::__shared_ptr<std::filesystem::__cxx11::_Dir>;
template class std::__shared_ptr<std::filesystem::recursive_directory_iterator::_Dir_stack>;

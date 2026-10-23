#include "core/Log.h"

#include <cstdio>
#include <string>
#include "core/Mutex.h"

namespace encore::log {
namespace {
Level g_minimum = Level::Info;
Mutex g_mutex;
const char* prefix(Level level) {
  switch (level) {
    case Level::Debug: return "debug";
    case Level::Info: return "info ";
    case Level::Warn: return "warn ";
    case Level::Error: return "error";
  }
  return "?";
}
}  // namespace

void setMinimumLevel(Level level) { g_minimum = level; }

#ifdef __amigaos__
namespace {
std::string g_stats;  ///< stats() lines not yet written
std::FILE* g_file = nullptr;
bool g_closed = false;
bool g_tried = false;  ///< opening was tried (and failed for every name: no more tries)
// Into pinball.log beside the program (the current folder: main.cpp), line by line, so that
// what came before a crash is there: AmigaDOS's > takes no stderr.
void logFile(Level level, std::string_view message) {
  if (g_closed) return;
  // (a pinball.log some earlier run left open is in use until a reboot: then another name)
  if (!g_file && !g_tried) {
    g_tried = true;
    for (const char* name : {"pinball.log", "pinball-1.log", "pinball-2.log", "pinball-3.log"})
      if ((g_file = std::fopen(name, "w")) != nullptr) break;
  }
  if (g_file) {
    std::fprintf(g_file, "[%s] %.*s\n", prefix(level), static_cast<int>(message.size()), message.data());
    std::fflush(g_file);
  }
}
}  // namespace
#endif

void write(Level level, std::string_view message) {
  if (level < g_minimum) return;
  std::scoped_lock lock(g_mutex);
  std::fprintf(stderr, "[%s] %.*s\n", prefix(level), static_cast<int>(message.size()), message.data());
#ifdef __amigaos__
  logFile(level, message);
#endif
}

void debug(std::string_view m) { write(Level::Debug, m); }
void info(std::string_view m) { write(Level::Info, m); }

void stats(std::string_view m) {
#ifdef __amigaos__
  // Kept in memory and written when the program ends (flushStats): a line to the disk every
  // second held the main task up for tens of milliseconds, the very frames it measures.
  std::scoped_lock lock(g_mutex);
  g_stats.append("[info ] ").append(m).append("\n");
#else
  write(Level::Info, m);
#endif
}
void warn(std::string_view m) { write(Level::Warn, m); }
void error(std::string_view m) { write(Level::Error, m); }

void flushStats() {
#ifdef __amigaos__
  std::scoped_lock lock(g_mutex);
  if (!g_stats.empty()) logFile(Level::Info, std::string_view(g_stats).substr(0, g_stats.size() - 1).substr(8));
  g_stats.clear();
#endif
}

// (closed by the program itself, not left to the C library's exit: on the Amiga the file was
// found still in use after the game had ended, and could not be read until a reboot)
void closeFile() {
#ifdef __amigaos__
  flushStats();
  std::scoped_lock lock(g_mutex);
  if (g_file) std::fclose(g_file);
  g_file = nullptr;
  g_closed = true;
#endif
}

}  // namespace encore::log

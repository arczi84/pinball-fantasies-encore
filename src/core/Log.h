#pragma once
#include <string_view>

namespace encore::log {

enum class Level { Debug, Info, Warn, Error };

void setMinimumLevel(Level level);
void write(Level level, std::string_view message);

void debug(std::string_view message);
void info(std::string_view message);
/// As info, but on the Amiga into pinball.log only: its console is slow to write to, and a
/// line a second there (--stats) costs the frames it is measuring.
void stats(std::string_view message);
/// The stats lines kept so far, into the log (on the Amiga; elsewhere they are written at once).
void flushStats();
/// The Amiga's pinball.log closed (written lines after it are dropped): the end of main, and at exit.
void closeFile();
void warn(std::string_view message);
void error(std::string_view message);

}  // namespace encore::log

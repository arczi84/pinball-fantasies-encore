#include "data/GameVersion.h"

#include <optional>
#include <utility>

#include "core/File.h"
#include "core/Sha256.h"

namespace encore {

std::vector<std::string> unsupportedGameFiles(const std::filesystem::path& dir) {
  static constexpr std::pair<const char*, const char*> kFiles[] = {
      {"INTRO.PRG", "619723e39acc003c64ae5f10159ae9da6192a28642c348f455bac447a1184967"},
      {"TABLE1.PRG", "3b897533f11163934b8e4da038143e8f7339224421803ab4108c9d10f0a7bb4a"},
      {"TABLE2.PRG", "37019f7bd41d896a8f5a6383a2dffc3b3e4fc63fdf1aec1cc15db99110e581eb"},
      {"TABLE3.PRG", "da83ef5a7a471e6a6ad759126907076c81e92ffde6dec8e3de8e6052c6a98858"},
      {"TABLE4.PRG", "88f63edd4c7b50bd057397016d7aa962f0ed1c858f4a746f1ccf976f67494ebf"},
      {"MOD2.MOD", "aa5003c275b494062f37f44e8c77105b8a420555f4bd6ff53d7698f89c540f21"},
      {"TABLE1.MOD", "a0877e4372abe64b70d9e361bf257ea5a84c948771f0eace3433d5f6399060b5"},
      {"TABLE2.MOD", "728629c54311386781271308e181ac0435f0582e90870accff0a42270d467529"},
      {"TABLE3.MOD", "fb7bfd1c96a462cb03999d2e6f843a20d3de69ba05fcbd384a9f1c131b9a563a"},
      {"TABLE4.MOD", "31ad7e671ae77c07c3d075e2f1fecd3d918fd921fa23acd9a1b0b6fc07fbbcea"},
  };
  std::vector<std::string> bad;
  for (const auto& [name, sum] : kFiles) {
    const auto path = file::findCaseInsensitive(dir, name);
    const auto bytes = path ? file::readAll(*path) : std::nullopt;
    if (!bytes || sha256Hex(*bytes) != sum) bad.emplace_back(name);
  }
  return bad;
}

bool tablesOnlyRelease(const std::filesystem::path& dir) {
  const auto bad = unsupportedGameFiles(dir);
  if (bad.size() != 1 || bad[0] != "INTRO.PRG") return false;
  const auto path = file::findCaseInsensitive(dir, "INTRO.PRG");
  const auto bytes = path ? file::readAll(*path) : std::nullopt;
  return bytes && sha256Hex(*bytes) == "2f6addcbf410e60abf585a3af1bbad0f6cb5a85c84af01a56a046c72eca6f98a";
}

}  // namespace encore

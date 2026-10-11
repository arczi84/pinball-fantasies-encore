#pragma once
// Billion Dollar Gameshow's own rules (TABLE3.PRG, cs:0000 to cs:2228), on the engine the
// tables share. Addresses here are this table's own. Routines are named by where they are in
// the program until what they are for is clearer.
#include <array>

#include "engine/table/Flow.h"

namespace encore {

class Gameshow : public Flow {
 public:
  explicit Gameshow(ByteView prg);

 private:
  u8& b(u16 a) { return nativeB(a); }
  Word w(u16 a) { return nativeW(a); }
  void sfx(u16 record, u8 how = 0) { sound->effect(b(record), b(static_cast<u16>(record + 1)), how, static_cast<u8>(b(static_cast<u16>(record + 3)) + 1)); }
  /// Where in the blinks' common period (cs:1e15: twenty frames) a light begins: in step with
  /// the others, or half a period out.
  u8 inStep() { return b(0x0060); }
  u8 outOfStep() {
    const u8 start = static_cast<u8>(b(0x0060) + 0x0a);
    return start >= 0x14 ? static_cast<u8>(start - 0x14) : start;
  }
  void wait1() {
    w(0x2c7b) = 1;
    w(0x2c79) = 0x4513;
  }
  void copy12(u16 to, u16 from) {
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(to + i)) = b(static_cast<u16>(from + i));
  }
  void zero12(u16 at) {
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(at + i)) = 0;
  }
  /// The ball kept out of sight somewhere.
  void hold(u16 x, u16 y) {
    b(0x29ec) = 0xff;
    placeBall(x, y);
  }

  void savePlayer(u16 player) override;   // cs:0c97
  void restorePlayer() override;          // cs:0be9
  void clearBall() override;              // cs:0008
  bool extraBallOwed() override { return b(0x2e7e) == 0xff; }
  void matchPace() override { nativeCW(0x0762) = 0x0e; }
  u16 matchLength() override { return 0x0f; }
  u16 matchRepeated(u16 digit) override { return static_cast<u16>(9 - digit); }
  void matchWon() override { b(0x03bd) = 0xff; }
  void gameOverLights() override {}
  bool restoresAtTurn() override { return false; }
  void afterRestore() override {
    gate(false);
    lane(false);
  }
  void serveMusic() override { music(0x08de); }
  void sameBallAgain() override;

  // --- the pieces of the collision masks that come and go (cs:0d0f to cs:0dea)
  static constexpr u16 kMask = 0x2aef + kLoadSegment, kUpperMask = 0x362f + kLoadSegment;
  void door(bool open) { copyShape(kUpperMask, 0x0f00, open ? 0x6190 : 0x61e0, 3, 0x19, 3); }
  void plungerLane(bool open) { copyShape(kMask, 0x2fcb, open ? 0x6230 : 0x6280, 2, 0x22, 2); }
  void target(int which, bool down);
  void gate(bool open) { copyShape(kUpperMask, 0x198e, static_cast<u16>((open ? 0x6390 : 0x6480) + 4), 4, 0x14, 12); }
  void lane(bool open) { copyShape(kMask, 0x4fd8, static_cast<u16>((open ? 0x6570 : 0x6620) + 4), 4, 0x0e, 12); }

  void bindRules();
  void bindSteps();
  void everyFrame();     // cs:1d9d
  void at10be();
  void at11a0();
  void at1342();
  void at1517();
  void at16b9();
  void at1848();
  void at1beb();
  void at1f63() { addScore(0x0617, 0x0155); }
  void at1f6d() { copy12(0x0617, 0x0631); }
  void at1f7b() { addScore(0x06cb, 0x019d); }
  void at1f8f();
  void at1fb8();
  /// One of the six prizes is lit to be collected (cs:17d0 and its like): its light blinks,
  /// and with all three of its row lit the way to collect them opens.
  void prizeLit(u16 flag, u16 record, u8 light, bool outOfStepLight, u16 other1, u16 other2);
  void threeDigits(u16 value, u16 at);
  void maniaAt();
  bool bothShown_ = false;  ///< the pair of door targets is flashing for both having been hit
  std::array<u8, 4> maniaAt_{};  ///< the loop's message's number as the program begins it (ds:129f)
};

}  // namespace encore

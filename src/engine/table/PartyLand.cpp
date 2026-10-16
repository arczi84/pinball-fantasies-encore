#include "engine/table/PartyLand.h"

namespace encore {
namespace {

/// The lights each player keeps from ball to ball, in the order they are kept (cs:0d58).
constexpr u8 kKeptLights[] = {0x01, 0x02, 0x04, 0x05, 0x06, 0x08, 0x09, 0x29, 0x26, 0x22, 0x1f, 0x1c, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e};

}  // namespace

PartyLand::PartyLand(ByteView prg) : Flow(prg, 0) {
  // (amended) The top of the table, right of the tower: a soft shot that dies there balances on
  // the steel edge, at (187,15) or (183,14), and a shake sends it on to the right, down the
  // side, as the original's sums would.
  traps_.push_back({182, 12, 190, 17, 128});
  bind(0x000b, [this] { newGameTable(); });
  bind(0x0202, [this] {  // the music a game opens with
    music(0x0c87);
    B(0x230c) = B(0x0c71);
    B(0x230a) = 0;
  });
  bindRules();
  bindSteps();
  bind(0x0004, [this] {  // at start-up: the top of the left ramp's mask is kept, for putting back
    u16 to = 0x1356;
    for (u16 row = 0; row < 0x12; ++row)
      for (u16 b = 0; b < 2; ++b) B(to++) = farB(S(0x46b4), static_cast<u16>(0x0266 + row * 0x28 + b));
    lightsOut();
  });
  bind(0x01cd, [this] {  // the music while nobody plays
    if (B(0x3712) != 0xff && B(0x3711) != 0xff) {
      music(0x0c75);
      B(0x230c) = B(0x0c77);
      B(0x230a) = 9;
      B(0x230d) = 1;
    }
    B(0x3712) = 0;
  });
  bind(0x0fa0, [this] {  // tilt
    B(0x33cf) = 0;
    music(0x0c7b);
    B(0x230a) = 0x3e;
    startScript(0x1bf0);
    stopBlinks();
    lightsDark();
    B(0x33f8) = 0xff;
  });
  bind(0x0fc5, [this] { music(0x0c78); });  // the warning before it
  bind(0x02ba, [this] {
    if (!countTo(0x35ca, 0x1e)) return;
    B(0x00d0) = 0xff;
    serve();
  });
  bind(0x02f1, [this] {
    if (!countTo(0x35cc, 5)) return;
    effect(0x0c31);
    endTimer();
  });
  bind(0x52cd, [this] {  // until the music has come round (cs:52cd; Party Land's has a flag of its own in it)
    si = 0xff;
    if (B(0x338e) != 0xff) return;
    // (after the side lane's extra ball a lost ball is not taken as lost for ten seconds, or,
    // in the original, until this wait finds the jingle over; amended: the ten seconds)
    if (!amended) B(0x00d2) = 0;
    si = 0;
  });
  bind(0x2ab1, [this] { everyFrame(); });
  bind(0x0215, [this] { drained(); });
  bind(0x0fcc, [this] {  // a flipper pressed: the four lane lights move along one (cs:0fcc)
    if (B(0x338b) != 0xff) return;
    // (the original keeps a press made while the lights are held, and moves them when they
    // are let go; amended: such a press is lost)
    if (amended) B(0x338b) = 0;
    if (B(0x00cf) == 0xff) return;
    B(0x338b) = 0;
    const u8 fourth = B(0x3596);
    auto put = [this](u8 light, u8 state) { state == 0xff ? setLight(light) : clearLight(light); };
    put(5, B(0x3593));
    put(2, B(0x3592));
    put(1, B(0x3595));
    put(4, fourth);
  });
  bind(0x2bd1, [this] {  // after a bumper's score (cs:2bd1)
    at2b9d();
    W(0x36f6) = 0;
    if (W(0x36f8) == 0xff) {
      startScript(0x1acc);
      W(0x36f8) = 0;
    }
  });
}

void PartyLand::newGameTable() {
  B(0x06ce) = 0;
  lightsOut();
  stopBlinks();
  for (u16 i = 0; i < 12; ++i) B(0x010c, i) = B(0x0124, i);
  clearScores();
  clearBall();
  for (u16 player = 0; player < 8; ++player) savePlayer(static_cast<u16>(player * 0x74));
  B(0x00ce) = 0;
  B(0x00cd) = 0;
  B(0x00d1) = 0;
  for (u16 i = 0; i < 0x10; ++i) B(0x1c16, i) = 0x20;
  for (u16 i = 0; i < 0x10; ++i) B(0x1c28, i) = 0x20;
}

void PartyLand::clearScores() {
  for (u16 at : {u16{0x45b6}, u16{0x3399}, u16{0x00b0}, u16{0x00bc}, u16{0x00dc}, u16{0x00e8}})
    for (u16 i = 0; i < 12; ++i) B(at, i) = 0;
  W(0x00ae) = 0;
  for (u16 i = 0; i < 0x10; ++i) B(0x1c16, i) = 0x20;
}

void PartyLand::clearBall() {
  B(0x2238) = 0x38;
  for (u16 i = 0; i < 12; ++i) B(0x00f4, i) = 0;
  for (u16 i = 0; i < 12; ++i) B(0x0100, i) = 0;
  B(at::layer) = 0;
  for (u16 a : {u16{0x86}, u16{0x9b}, u16{0xa4}, u16{0xa5}, u16{0x8a}, u16{0x87}, u16{0x88}, u16{0x89}, u16{0x8e}, u16{0x97}}) B(a) = 0;
  W(0x33b1) = 1;
  for (u16 a : {u16{0x98}, u16{0x99}, u16{0x9a}, u16{0xa0}, u16{0xa1}, u16{0xa2}, u16{0xa3}, u16{0x8b}, u16{0x8c}, u16{0x8d}, u16{0x94},
                u16{0x95}, u16{0xc9}, u16{0xca}})
    B(a) = 0;
  W(0x05a9) = 0;
  B(0x00cf) = 0;
  B(0x00d4) = 0;
  W(0x00a8) = 0;
  W(0x05ad) = 0;
  W(0x05ab) = 0;
  B(0x0096) = 0;
  W(0x009c) = 0;
  W(0x009e) = 0;
  B(0x00c8) = 0;
  B(0x05b1) = 0;
  B(0x05b2) = 0;
  B(0x05b3) = 0;
  B(0x33f8) = 0xff;
  stopBlinks();
  lightsOut();
  setLight(0x34);
  setLight(0x35);
  setLight(0x36);
  blink(0x0e, 0, 8);
  blink(0x1a, 0, 9);
  flushColours();
  // the three drop targets stand
  patchMask(0x3b74, 0x2b5a, 0x68b0, 2, 0x0f);
  patchMask(0x3b74, 0x2e2b, 0x68f0, 2, 0x0f);
  patchMask(0x3b74, 0x30fc, 0x6940, 1, 0x0f);
  B(0x01fc) = 1;
}

void PartyLand::savePlayer(u16 player) {
  u16 i = 0;
  for (u8 light : kKeptLights) nativeB(static_cast<u16>(player + 0x209 + i++)) = B(0x3591, light) == 0xff ? 0xff : 0;
  flushColours();
  nativeW(static_cast<u16>(player + 0x27a)) = W(0x00ae);
  nativeB(static_cast<u16>(player + 0x27c)) = B(0x06ce);
  struct Kept { u16 at, from; };
  for (const Kept k : {Kept{0x21a, 0x45b6}, {0x226, 0x3399}, {0x232, 0x00dc}, {0x23e, 0x00e8}, {0x262, 0x00b0}, {0x26e, 0x00bc}})
    for (u16 b = 0; b < 12; ++b) nativeB(static_cast<u16>(player + k.at + b)) = B(k.from, b);
}

void PartyLand::restorePlayer() {
  const u16 player = static_cast<u16>((B(0x371a) - 1) * 0x74);
  u16 i = 0;
  for (u8 light : kKeptLights) {
    if (nativeB(static_cast<u16>(player + 0x209 + i++)) == 0xff) setLight(light);
    else clearLight(light);
  }
  flushColours();
  W(0x00ae) = nativeW(static_cast<u16>(player + 0x27a));
  B(0x06ce) = nativeB(static_cast<u16>(player + 0x27c));
  struct Kept { u16 at, to; };
  for (const Kept k : {Kept{0x21a, 0x45b6}, {0x226, 0x3399}, {0x232, 0x00dc}, {0x23e, 0x00e8}, {0x262, 0x00b0}, {0x26e, 0x00bc}})
    for (u16 b = 0; b < 12; ++b) B(k.to, b) = nativeB(static_cast<u16>(player + k.at + b));
}

void PartyLand::everyFrame() {
  if (B(0x33ce) == 0xff) return;
  ++B(0x0093);
  for (u16 a : {u16{0x00aa}, u16{0x05ad}, u16{0x05ab}, u16{0x05a9}})
    if (W(a) != 0) --W(a);
  struct Wheel { u16 at; u8 size; };
  for (const Wheel w : {Wheel{0x86, 0x10}, {0x9b, 0x18}, {0xa4, 0x04}, {0xa5, 0x1c}})
    if (++B(w.at) == w.size) B(w.at) = 0;
  if (W(0x00a8) == 0) return;
  --W(0x00a8);
  if (W(0x00a8) == 0x2d0) {
    stopBlink(0x0a);
    lightOff(0x0a);
    B(0x359d) = 0;
    blink(0x0c, 0, 8);
  } else if (W(0x00a8) == 0) {
    stopBlink(0x0c);
    lightOff(0x0c);
    B(0x359f) = 0;
    blink(0x0e, 0, 8);
  }
}

void PartyLand::drained() {
  B(at::ballHidden) = 0xff;
  W(0x3385) = high() ? 0x103 : 0x171;
  placeBall(0x0f, 0x2f);
  B(0x33cf) = 0;
  B(0x33e2) = 0;
  B(0x05b1) = 0;
  B(0x05b2) = 0;
  if (B(0x00d2) == 0xff) return;
  B(0x33ce) = 0xff;
  B(0x3389) = 0;
  if (B(0x33de) == 0) {  // nothing was scored: the ball is given again
    startScript(0x1477);
    music(0x0c6f);
    B(0x00d1) = 0xff;
    addTimer(0x02ba);
    return;
  }
  award(0x06d5);
  // (amended: the lost ball's jingle keeps its priority while it plays, so nothing cuts into it)
  if (!amended) B(0x3389) = 0;
  B(0x230a) = 0x3e;
  addTimer(0x02f1);
}

}  // namespace encore

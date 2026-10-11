#include "engine/table/Gameshow.h"

#include <algorithm>

namespace encore {

Gameshow::Gameshow(ByteView prg) : Flow(prg, 2) {
  for (u16 i = 0; i < 4; ++i) maniaAt_[i] = b(static_cast<u16>(0x129f + i));
  bindRules();
  bindSteps();
  bindNative(0x0004, [this] { lightsOut(); });
  bindNative(0x00c6, [this] {  // a new game
    b(0x03bd) = 0;
    zero12(0x03a5);
    w(0x03a3) = 0;
    b(0x01be) = 0;
    b(0x01bf) = 0;
    lightsOut();
    stopBlinks();
    clearBall();
    for (u16 player = 0; player < 8; ++player) savePlayer(static_cast<u16>(player * 0x28));
    at1f6d();
    for (u16 i = 0; i < 0x10; ++i) b(static_cast<u16>(0x1a2e + i)) = 0x20;
    for (u16 i = 0; i < 0x10; ++i) b(static_cast<u16>(0x1a40 + i)) = 0x20;
  });
  bindNative(0x0124, [this] {  // the music while nobody plays
    music(0x08e1);
    b(0x1dcd) = b(0x08e3);
    b(0x1dcb) = 9;
  });
  bindNative(0x013d, [this] {  // the music a game opens with
    music(0x08de);
    b(0x1dcd) = b(0x08e0);
    b(0x1dcb) = 0;
  });
  bindNative(0x0150, [this] {  // the ball is lost
    b(0x2c60) = 0xff;
    w(0x2c17) = high() ? 0x103 : 0x171;
    placeBall(0x87, 0x1c);
    b(0x29ec) = 0xff;
    b(0x2c61) = 0;
    b(0x2c74) = 0;
    b(0x2c1b) = 0;
    award(0x0838);
    b(0x1dcb) = 0x37;
    addTimer(0x01d2);
  });
  bindNative(0x01d2, [this] {
    if (!countTo(0x2e86, 5)) return;
    sfx(0x0871);
    endTimer();
  });
  bindNative(0x0bd5, [this] {
    w(0x2fb2) = 0;
    if (b(0x2c72) == 0) endTimer();
  });
  bindNative(0x0cd7, [this] {  // tilt
    b(0x2c61) = 0;
    music(0x08b3);
    b(0x1dcb) = 0x37;
    startScript(0x1a0a);
    stopBlinks();
    lightsDark();
  });
  bindNative(0x0cf6, [this] { music(0x08b0); });
  bindNative(0x0cfd, [this] {  // a flipper pressed
    if (b(0x2c1d) == 0xff) b(0x2c1d) = 0;
  });
  bindNative(0x1d9d, [this] { everyFrame(); });
  bindNative(0x1f40, [this] {  // after a bumper's score
    at1f8f();
    w(0x2fb2) = 0;
    if (w(0x2fb4) == 0xff) {
      startScript(0x19dc);
      w(0x2fb4) = 0;
    }
  });
  bindNative(0x21a4, [this] {  // each second of a count-down: at nought the mode ends
    if (b(0x2faf) != 0x2a || b(0x2fb0) != 0x37) return;
    for (u16 mode : {u16{0x01b5}, u16{0x01b6}})
      if (b(mode) == 0xff) {
        b(0x1dce) = 1;
        music(0x08fd);
        b(0x1dcb) = 3;
      }
    b(0x01b6) = 0;
    b(0x01b5) = 0;
    b(0x2c1b) = 0;
    b(0x1dcb) = 3;
    b(0x1dce) = 1;
  });
}

void Gameshow::target(int which, bool down) {
  struct Piece { u16 at, down, up, width; };
  static constexpr Piece kPieces[] = {{0x2389, 0x62d0, 0x6370, 2}, {0x26a9, 0x62f0, 0x6360, 1}, {0x2994, 0x6300, 0x6350, 1}, {0x2cb3, 0x6310, 0x6330, 2}};
  const Piece& p = kPieces[which];
  copyShape(kMask, p.at, down ? p.down : p.up, p.width, 0x10, p.width);
}

void Gameshow::clearBall() {
  zero12(0x3e90);
  zero12(0x2c2b);
  zero12(0x15fc);
  b(0x2be8) = 0;
  copy12(0x06cb, 0x016d);
  for (u16 a : {u16{0x01b7}, u16{0x01b9}, u16{0x01ba}, u16{0x01bc}, u16{0x01bd}, u16{0x01bb}}) b(a) = 0;
  for (u16 a : {u16{0x006a}, u16{0x0072}, u16{0x0074}, u16{0x006c}, u16{0x0070}, u16{0x006e}}) w(a) = 0;
  b(0x01b8) = 0;
  b(0x01c0) = 0;
  b(0x0854) = 1;
  w(0x1cf7) = 0x3820;
  stopBlinks();
  lightsOut();
  door(false);
  for (int i = 0; i < 4; ++i) {
    setLight(static_cast<u8>(7 + i));
    target(i, false);
  }
  stopBlink(0x01);
  blink(0x01, 0, 0x0f);
}

void Gameshow::savePlayer(u16 player) {
  copy12(static_cast<u16>(player + 0x0263), 0x3e90);
  copy12(static_cast<u16>(player + 0x026f), 0x2c2b);
  copy12(static_cast<u16>(player + 0x027d), 0x03a5);
  w(static_cast<u16>(player + 0x0289)) = w(0x03a3);
  b(static_cast<u16>(player + 0x027b)) = b(0x01be);
  b(static_cast<u16>(player + 0x027c)) = b(0x01bf);
}

void Gameshow::restorePlayer() {
  const u16 player = static_cast<u16>((b(0x2fd6) - 1) * 0x28);
  copy12(0x3e90, static_cast<u16>(player + 0x0263));
  copy12(0x2c2b, static_cast<u16>(player + 0x026f));
  copy12(0x03a5, static_cast<u16>(player + 0x027d));
  w(0x03a3) = w(static_cast<u16>(player + 0x0289));
  b(0x01be) = b(static_cast<u16>(player + 0x027b));
  b(0x01bf) = b(static_cast<u16>(player + 0x027c));
  if (amended) maniaAt();
  if (b(0x01be) == 0) return;
  b(0x01b7) = b(0x01b9) = b(0x01ba) = 2;
  for (u8 light : {u8{0x0d}, u8{0x0e}, u8{0x0f}}) setLight(light);
  if (b(0x01bf) == 0) return;
  b(0x01bc) = b(0x01bb) = b(0x01bd) = 2;
  for (u8 light : {u8{0x1c}, u8{0x1d}, u8{0x1e}}) setLight(light);
}

void Gameshow::sameBallAgain() {
  w(0x2fb2) = 0;
  for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(0x3eb4 + i)) = 0x12;
  w(0x3ec0) = 0;
  bx = 0x1738;
  call(nativeW(bx));
}

void Gameshow::at1f8f() {
  if (b(0x2c74) != 0xff || b(0x01b5) != 0xff || b(0x005e) == 0xff) return;
  addScore(0x15fc, 0x016d);
}

void Gameshow::at1fb8() {
  if (b(0x2c74) != 0xff || b(0x01b5) != 0xff || b(0x005e) == 0) return;
  addScore(0x15fc, 0x0179);
}

void Gameshow::threeDigits(u16 value, u16 at) {
  for (int i = 2; i >= 0; --i) {
    u16 digit = value % 10;
    value /= 10;
    if (digit == 0 && i == 0) digit = 0xfff3;
    b(static_cast<u16>(at + i)) = static_cast<u8>(digit + 0x37);
  }
}

/// (amended) The loop's message, MONEY MANIA AT, with the number this player's own trips round
/// it make: cs:1beb writes the next one into it past the twelfth trip, but it is one message for
/// all the players and the original never writes it back, so a player could be shown the number
/// of whoever passed twelve last.
void Gameshow::maniaAt() {
  for (u16 i = 0; i < 4; ++i) b(static_cast<u16>(0x129f + i)) = maniaAt_[i];
  const u16 trips = w(0x03a3);
  if (trips <= 12) return;
  // as cs:1beb last wrote it: at this trip, or at the one before it on a sixth
  const u16 next = static_cast<u16>(trips % 6 != 0 ? (trips / 6 + 1) * 6 : trips);
  threeDigits(std::min<u16>(next, 96), 0x129f);  // (a hundred or more is written one place on,
  if (next >= 0x64) threeDigits(next, 0x12a0);     // over the end of the one before it)
}

void Gameshow::prizeLit(u16 flag, u16 record, u8 light, bool outOfStepLight, u16 other1, u16 other2) {
  b(flag) = 1;
  award(record);
  blink(light, outOfStepLight ? outOfStep() : inStep(), 0x0a);
  if (b(other1) == 0 || b(other2) == 0) return;
  blink(0x11, outOfStep(), 0x0a);
  b(0x2e70) = 1;
  door(true);
}

void Gameshow::at10be() {
  b(0x01b8) = 0;
  award(0x03da);
  stopBlink(0x1b);
  blink(0x1b, 0, 4);
  addTimer(0x111e);
  hold(4, 0x211);
}

void Gameshow::at11a0() {
  hold(4, 0x211);
  addTimer(0x11e7);
}

/// cs:1342: the prize wheel moves on one, slower each time, until its list of waits ends.
void Gameshow::at1342() {
  w(0x0078) += 2;
  const u16 wait = nativeW(w(0x0078));
  if (wait == 0xffff) {
    startScript(0x12e7);
    addTimer(b(0x2e70) == 1 ? 0x13f7 : 0x13a8);
    return;
  }
  w(0x0122) = wait;
  clearLight(static_cast<u8>(0x13 + b(0x0124)));
  b(0x0124) = static_cast<u8>((b(0x0124) + 1) & 7);
  setLight(static_cast<u8>(0x13 + b(0x0124)));
  w(0x130b) = static_cast<u16>(0x0c * b(0x0124) + 0x013d);
  startScript(0x1309);
}

void Gameshow::at1517() {
  setLight(0x06);
  blink(0x1b, inStep(), 0x0a);
  award(0x03be);
  b(0x1dcb) = 0;
  placeBall(0x130, 0x217);
  w(0x29ae) = 0x0a;
  b(0x29ec) = 0;
  b(0x2c1b) = 0;
  b(0x1dcb) = 0;
  door(true);
  b(0x01b8) = 0xff;
}

void Gameshow::at16b9() {
  clearLight(0x06);
  sfx(0x0881);
  placeBall(0x67, 0xe9);
  w(0x29b0) = 0x588;
  w(0x29ae) = 0x53;
  b(0x29ec) = 0;
}

/// cs:1848: the bonus multiplier goes up a step, or, with nothing else to give, a count does.
void Gameshow::at1848() {
  if (w(0x0068) == 0) {
    if (w(0x0074) == 0) return;
    ++b(0x012a);
    ++b(0x067c);
    award(0x0675);
    return;
  }
  if (b(0x01c0) == 6) return;
  ++b(0x01c0);
  const u16 step = static_cast<u16>(b(0x01c0) - 1);
  if (b(0x2c1b) <= b(0x08ea) && b(0x2c74) != 0xff) {
    startScript(nativeW(static_cast<u16>(0x01c7 + step * 2)));
    music(0x08e8);
  }
  u8 times = b(static_cast<u16>(0x01c1 + step));
  b(0x0854) = times;
  b(0x1cf7) = 0x20;
  if (times == 0x0a) {
    b(0x1cf7) = 0x38;
    times = 0;
  }
  b(0x1cf8) = static_cast<u8>(times + 0x37);
  setLight(static_cast<u8>(b(0x01c0) + 0x20));
}

/// cs:1beb: a trip round the loop is counted, with something at every sixth.
void Gameshow::at1beb() {
  do {
    addScore(0x03a5, 0x01d3);
    ++w(0x03a3);
  } while (w(0x03a3) == 1);
  const u8 sixes = static_cast<u8>(w(0x03a3) / 6), over = static_cast<u8>(w(0x03a3) % 6);
  if (over != 0) {
    if (sixes == 1) {
      award(0x06ad);
      return;
    }
    if (sixes > 1) {
      const u16 next = static_cast<u16>((sixes + 1) * 6);
      threeDigits(next, next >= 0x64 ? 0x12a0 : 0x129f);
    }
    award(0x0691);
    return;
  }
  if (sixes == 2) {
    music(0x0918);
    blink(0x0b, outOfStep(), 0x0a);
    b(0x2e6a) = 1;
    return;
  }
  const bool second = sixes != 1 && (sixes & 1);
  award(second ? 0x05f9 : 0x05dd);
  b(0x01b5) = 0xff;
  b(0x2c74) = 0xff;
  b(0x005e) = second ? 0xff : 0;
  setLight(0x20);
}

void Gameshow::everyFrame() {
  auto down = [this](u16 at) { return w(at) != 0 && --w(at) == 0; };
  auto hurry = [this](u16 at, u8 light, u8 half) {
    if (w(at) != 0x78) return;
    stopBlink(light);
    blink(light, 0, half);
  };
  if (down(0x0064)) {
    stopBlink(0x05);
    clearLight(0x05);
  }
  hurry(0x0064, 0x05, 2);
  if (down(0x0066)) {
    stopBlink(0x10);
    clearLight(0x10);
  }
  hurry(0x0066, 0x10, 2);
  if (++b(0x0060) == 0x14) b(0x0060) = 0;
  if (down(0x0122)) at1342();
  for (u16 at : {u16{0x0068}, u16{0x006a}, u16{0x006c}, u16{0x006e}, u16{0x0070}, u16{0x0072}, u16{0x0074}}) down(at);
  if (down(0x0076)) {
    stopBlink(0x0c);
    clearLight(0x0c);
  }
  hurry(0x0076, 0x0c, 2);
  if (down(0x0062)) {
    stopBlink(0x04);
    lightOff(0x04);
  }
  hurry(0x0062, 0x04, 3);
}

void Gameshow::bindRules() {
  auto after = [this](u16 at, u16 counter, u16 limit, std::function<void()> then) {
    bindNative(at, [this, counter, limit, then] {
      if (countTo(counter, limit)) then();
    });
  };

  // the two pairs of targets that drop
  struct Drop { u16 hit, flag, other, record; int piece; u8 light; u16 timer; };
  for (const Drop d : {Drop{0x0deb, 0x2e66, 0x2e67, 0x0701, 0, 0x07, 0x0e6b}, {0x0e2b, 0x2e67, 0x2e66, 0x0701, 1, 0x08, 0x0e6b},
                       {0x0ea1, 0x2e68, 0x2e69, 0x071e, 2, 0x09, 0x0f21}, {0x0ee1, 0x2e69, 0x2e68, 0x071e, 3, 0x0a, 0x0f21}}) {
    bindNative(d.hit, [this, d] {
      at1f8f();
      if (b(d.flag) == 0) return;
      award(d.record);
      sfx(0x089d);
      target(d.piece, true);
      clearLight(d.light);
      if (b(d.other) == 0) addTimer(d.timer);
    });
  }
  after(0x0e6b, 0x2e90, 0x3c, [this] {
    sfx(0x0865);
    setLight(0x07);
    setLight(0x08);
    target(0, false);
    target(1, false);
    endTimer();
  });
  after(0x0f21, 0x2e92, 0x3c, [this] {
    sfx(0x0865);
    setLight(0x09);
    setLight(0x0a);
    target(2, false);
    target(3, false);
    endTimer();
  });

  // the pair of targets that open the door when both are hit
  auto both = [this] {
    blink(0x02, 0, 2);
    blink(0x03, 0, 2);
    if (!(amended && bothShown_)) addTimer(0x1051);
    bothShown_ = true;
    award(0x0758);
    stopBlink(0x12);
    blink(0x12, outOfStep(), 0x0a);
    b(0x2e71) = 0xff;
    door(true);
  };
  bindNative(0x0f57, [=, this] {
    at1f8f();
    sfx(0x089d);
    // (amended: for the second that the pair flashes after both were hit, another hit on
    // either counts as both again)
    if (b(0x2e62) == 0xff || (amended && bothShown_)) return both();
    blink(0x02, 0, 6);
    b(0x2e61) = 0xff;
    addTimer(0x1019);
    award(0x073b);
  });
  bindNative(0x0f95, [=, this] {
    at1f8f();
    sfx(0x089d);
    if (b(0x2e61) == 0xff || (amended && bothShown_)) return both();
    blink(0x03, 0, 6);
    b(0x2e62) = 0xff;
    addTimer(0x1035);
    award(0x073b);
  });
  after(0x1019, 0x2e94, 0x19, [this] {
    stopBlink(0x02);
    setLight(0x02);
    endTimer();
  });
  after(0x1035, 0x2e96, 0x19, [this] {
    stopBlink(0x03);
    setLight(0x03);
    endTimer();
  });
  after(0x1051, 0x2e98, 0x3c, [this] {
    bothShown_ = false;
    stopBlink(0x02);
    clearLight(0x02);
    stopBlink(0x03);
    clearLight(0x03);
    endTimer();
  });

  bindNative(0x1077, [this] {  // the ball came back down the lane: it is given again
    if (w(0x2be6) != 0x10ad) return;
    plungerLane(false);
    b(0x2c75) = 0;
    music(0x08d2);
    b(0x1dcb) = 1;
    startScript(0x0e9e);
    b(0x2c72) = 0;
    b(0x005f) = 0;
  });
  bindNative(0x10a9, [this] { lane(false); });
  bindNative(0x10ad, [this] {
    b(0x2c1c) = 0;
    plungerLane(true);
  });
  bindNative(0x10b7, [this] { b(0x2c1c) = 0xff; });

  after(0x111e, 0x2e9a, 0xfa, [this] {
    stopBlink(0x1b);
    for (u8 light : {u8{0x1b}, u8{0x0d}, u8{0x0e}, u8{0x0f}, u8{0x1c}, u8{0x1d}, u8{0x1e}}) clearLight(light);
    for (u16 a : {u16{0x01b7}, u16{0x01b9}, u16{0x01ba}, u16{0x01bc}, u16{0x01bd}, u16{0x01bb}, u16{0x01be}, u16{0x01bf}}) b(a) = 0;
    w(0x29b0) = 0xf254;
    sfx(0x0875);
    b(0x29ec) = 0;
    endTimer();
  });
  after(0x11e7, 0x2e9c, 0x1e, [this] {
    w(0x29b0) = 0xf254;
    sfx(0x0875);
    b(0x29ec) = 0;
    endTimer();
  });
  bindNative(0x1219, [this] {  // through the door: the prize wheel turns
    lane(true);
    if (b(0x2c74) == 0xff || b(0x1dd2) == 0xff) return at11a0();
    if (b(0x01b8) == 0xff) return at10be();
    b(0x29ec) = 0xff;
    if (b(0x2e70) == 0) door(false);
    placeBall(4, 0x211);
    music(0x08c0);
    w(0x0078) = high() ? 0x7a : 0xd0;
    w(0x0122) = nativeW(w(0x0078));
    startScript(0x12ff);
    clearLight(static_cast<u8>(0x13 + b(0x0124)));
    u8 start = static_cast<u8>(w(0x2c7f));
    if (drawsChance() && b(0x2e70) != 1) {
      // by chance proper: where the wheel is to stop, counted back by the moves it will make
      u16 moves = 0;
      for (u16 at = w(0x0078); nativeW(at) != 0xffff; at = static_cast<u16>(at + 2)) ++moves;
      start = static_cast<u8>(chance(8, 0) - moves);
    }
    if (b(0x2e70) == 1) {  // a prize is to be collected: the wheel is made to stop on it
      struct Prize { u16 flag; u8 start; };
      for (const Prize p : {Prize{0x01b7, 0}, {0x01b9, 1}, {0x01ba, 2}, {0x01bc, 6}, {0x01bb, 5}, {0x01bd, 4}})
        if (b(p.flag) == 1) {
          start = p.start;
          break;
        }
    }
    if (high()) start = static_cast<u8>(start - 2);
    b(0x0124) = start & 7;
    w(0x2c17) = high() ? 0xdc : 0x10e;
  });
  after(0x13a8, 0x2e9e, 0x64, [this] {
    w(0x2c17) = 0xffff;
    if (w(0x130b) == 0x013d) w(0x130b) = 0x0131;
    addScore(0x3e90, w(0x130b));
    startScript(0x12f9);
    b(0x29ec) = 0;
    w(0x29b0) = 0xf49c;
    stopBlink(0x12);
    clearLight(0x12);
    endTimer();
  });
  bindNative(0x13f7, [this] {  // the next prize of the row is collected
    auto collect = [this](u16 flag, u8 light, u16 record) {
      stopBlink(light);
      setLight(light);
      if (record == 0x056d || record == 0x05c1) {
        stopBlink(0x11);
        clearLight(0x11);
      }
      b(flag) = 2;
      b(0x2c1b) = 0;
      award(record);
    };
    if (b(0x01be) != 0xff) {
      if (b(0x01b7) != 2) collect(0x01b7, 0x0d, 0x0535);
      else if (b(0x01b9) != 2) collect(0x01b9, 0x0e, 0x0551);
      else {
        collect(0x01ba, 0x0f, 0x056d);
        blink(0x10, inStep(), 0x0a);
        b(0x2e6f) = 1;
        w(0x0066) = 0x5dc;
        b(0x01be) = 0xff;
        door(false);
      }
    } else if (b(0x01bc) != 2) {
      collect(0x01bc, 0x1c, 0x0589);
    } else if (b(0x01bb) != 2) {
      collect(0x01bb, 0x1d, 0x05a5);
    } else {
      collect(0x01bd, 0x1e, 0x05c1);
      b(0x01bf) = 0xff;
      door(false);
    }
    endTimer();
  });

  bindNative(0x1587, [this] {  // the hole
    b(0x29ec) = 0xff;
    if (b(0x2c74) == 0xff) {
      b(0x29ec) = 0xff;
      setLight(0x06);
      addTimer(0x166a);
      return;
    }
    if (b(0x01bf) == 0xff) return at1517();
    at1f63();
    if (w(0x0076) == 0) {
      award(0x06c9);
    } else {
      w(0x0076) = 0x0a;
      for (int i = 0; i < 5; ++i) addScore(0x06e7, 0x06cb);
      award(0x06e5);
    }
    addTimer(0x15d8);
  });
  bindNative(0x15d8, [this] {
    hold(0x67, 0xe9);
    if (!countTo(0x2ea0, 0xa0)) return;
    setLight(0x06);
    addTimer(0x1643);
    zero12(0x06e7);
    endTimer();
  });
  after(0x1643, 0x2ea2, 0x28, [this] {
    at16b9();
    endTimer();
  });
  bindNative(0x166a, [this] {
    placeBall(0x67, 0xe9);
    if (!countTo(0x2ea4, 0x1e)) return;
    at16b9();
    endTimer();
  });

  bindNative(0x1713, [this] {
    at1f7b();
    at1fb8();
    award(0x0430);
    w(0x0074) = 0xf0;
    gate(true);
    if (w(0x0066) != 0) {
      w(0x0066) = 1;
      award(0x0615);
      at1f6d();
      w(0x0064) = 0x12c;
      blink(0x05, 0, 0x0a);
      music(0x08eb);
    }
    w(0x006a) = 0xf0;
  });
  bindNative(0x1768, [this] {
    at1f7b();
    at1f63();
    at1fb8();
    award(0x046a);
    if (w(0x0064) != 0) {
      award(0x063d);
      w(0x0064) = 1;
    }
    w(0x0076) = 0x294;
    stopBlink(0x0c);
    blink(0x0c, outOfStep(), 0x0a);
    if (w(0x0074) != 0 && b(0x01ba) < 1) {
      w(0x0074) = 0;
      return prizeLit(0x01ba, 0x07c8, 0x0f, false, 0x01b9, 0x01b7);
    }
    if (b(0x01bd) == 0 && w(0x0074) != 0 && b(0x01be) == 0xff) {
      w(0x0074) = 0;
      if (b(0x01bf) != 0xff) {
        if (b(0x01bd) != 0) return;
        if (b(0x2c74) != 0xff) startScript(0x10fd);
        w(0x0070) = 0x258;
        return;
      }
    }
    at1848();
  });
  bindNative(0x18d6, [this] {
    award(0x04de);
    if (w(0x2be6) != 0x19bd) return;
    if (w(0x006c) != 0 && b(0x01bc) == 0) {
      w(0x006c) = 0;
      return prizeLit(0x01bc, 0x07e4, 0x1c, false, 0x01bb, 0x01bd);
    }
    if (w(0x006e) == 0 || b(0x01bb) != 0) return;
    prizeLit(0x01bb, 0x0800, 0x1d, true, 0x01bd, 0x01bc);
  });
  bindNative(0x19bd, [this] {
    at1fb8();
    award(0x04fb);
    if (w(0x2be6) != 0x18d6) return;
    if (b(0x2e6a) != 0) {  // the ball more that was lit
      award(0x0659);
      stopBlink(0x0b);
      clearLight(0x0b);
      setLight(0x1f);
    }
    if (w(0x0070) != 0 && b(0x01bd) == 0) {
      w(0x0070) = 0;
      return prizeLit(0x01bd, 0x081c, 0x1e, false, 0x01bb, 0x01bc);
    }
    gate(false);
    w(0x0068) = 0xf0;
    w(0x0072) = 0xf0;
  });
  bindNative(0x1a66, [] {});
  bindNative(0x1a67, [this] {
    award(0x04c1);
    if (w(0x2be6) != 0x1a66) return;
    at1f63();
    at1f7b();
    at1fb8();
    at1beb();
    if (w(0x006a) != 0) {
      if (b(0x01bf) == 0xff) return;
      if (b(0x01be) == 0xff) {
        if (b(0x01bc) != 0) return;
        if (b(0x2c74) != 0xff) startScript(0x10ef);
        w(0x006c) = 0x258;
        return;
      }
      if (b(0x01b7) == 0) {
        w(0x006a) = 0;
        return prizeLit(0x01b7, 0x0790, 0x0d, false, 0x01ba, 0x01b9);
      }
    }
    if (w(0x0072) == 0 || b(0x01bf) == 0xff) return;
    if (b(0x01be) == 0xff) {
      if (b(0x01bb) != 0) return;
      if (b(0x2c74) != 0xff) startScript(0x10ef);
      w(0x006e) = 0x258;
      return;
    }
    w(0x0072) = 0;
    if (b(0x01b9) != 0) return;
    prizeLit(0x01b9, 0x07ac, 0x0e, true, 0x01ba, 0x01b7);
  });

  for (u16 at : {u16{0x1cd9}, u16{0x1cf4}})
    bindNative(at, [this] {
      award(0x03f6);
      sfx(0x0885, 0x20);
    });
  for (u16 at : {u16{0x1d0f}, u16{0x1d2a}})
    bindNative(at, [this] {
      award(0x0413);
      sfx(0x0889);
    });
  bindNative(0x1d45, [this] { award(0x0487); });
  bindNative(0x1d4c, [this] { award(0x04a4); });
  bindNative(0x1d53, [this] { award(0x0518); });
  bindNative(0x1d5a, [this] { at1f63(); });
  bindNative(0x1d5e, [this] {
    at1f7b();
    award(0x044d);
    const bool already = w(0x0062) != 0;
    w(0x0062) = 0x258;
    if (already) award(0x0774);
    if (b(0x2e70) == 1) music(0x091e);
    stopBlink(0x04);
    blink(0x04, inStep(), 0x0a);
  });
  bindNative(0x1f85, [this] { addScore(0x06cb, 0x01a9); });
}

/// The steps the Gameshow adds to the display's scripts.
void Gameshow::bindSteps() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  bindNative(0x0678, [this] {
    wait1();
    const u16 step = bx;
    addTimer(0x13a8);
    bx = step;
    nextStep(4);
  });
  bindNative(0x068f, [this] {  // a mode is over
    wait1();
    b(0x2c74) = 0;
    stopBlink(0x20);
    clearLight(0x20);
    b(0x2c1b) = 0;
    nextStep(4);
  });
  bindNative(0x06b6, [this] {
    wait1();
    blink(0x20, 0, 3);
    nextStep(4);
  });
  bindNative(0x06d0, [this] {  // the match begins
    w(0x2c79) = 0x06e5;
    w(0x2c7b) = 1;
    b(0x2fd6) = 1;
    nextStep(4);
    if (amended) {  // in this frame, not the next, as on the other tables
      si = 1;
      call(0x06e5);
      w(0x2c7b) = si;
    }
  });
  bindNative(0x1fe1, [this] {
    b(0x1a7b) = b(0x1aa8) = static_cast<u8>(b(0x2fd6) + 0x37);
    wait1();
    nextStep(4);
  });
  bindNative(0x1fff, [this] {
    b(0x1a93) = static_cast<u8>(b(0x2fd6) + 0x37);
    wait1();
    nextStep(4);
  });
  bindNative(0x455a, [] {});
  bindNative(0x201a, [this] {
    b(0x005f) = 0xff;
    w(0x2c7b) = 1;
    w(0x2c79) = 0x455a;
    nextStep(4);
  });
  bindNative(0x202e, [this] {
    wait1();
    nextStep(4);
  });
  bindNative(0x203d, [this] {
    w(0x2c79) = 0x204c;
    w(0x2c7b) = 0x2c;
    nextStep(4);
  });
  bindNative(0x204c, [this] { si = b(0x2c75) == 0xff ? 0x22 : 0; });
  bindNative(0x205f, [=, this] {  // go elsewhere if a score is nought
    bool nought = true;
    for (u16 i = 0; i < 12; ++i)
      if (nativeB(static_cast<u16>(arg(2) + i)) != 0) nought = false;
    bx = nought ? arg(4) : static_cast<u16>(bx + 6);
    if (nativeW(bx) == 0) {
      w(0x2c77) = 0;
      return;
    }
    w(0x2c77) = bx;
    call(nativeW(bx));
  });
  bindNative(0x20a3, [] {});
  bindNative(0x20a4, [this] {  // the bonus times its multiplier
    copy12(0x0855, 0x2c2b);
    for (u8 n = static_cast<u8>(b(0x0854) - 1); n > 0; --n) addScore(0x2c2b, 0x0855);
    b(0x1ad0) = 0x38;
    b(0x1ad1) = 0x37;
    if (const u8 digit = static_cast<u8>(b(0x0854) + 0x37); digit < 0x41) {
      b(0x1ad0) = digit;
      b(0x1ad1) = 0x20;
    }
    wait1();
    nextStep(4);
  });
  bindNative(0x20fc, [=, this] {
    if (b(0x0854) == 1) return goTo(arg(2));
    wait1();
    nextStep(4);
  });
  bindNative(0x2120, [this] {
    wait1();
    addScore(0x2c2b, 0x15fc);
    nextStep(2);
  });
  bindNative(0x214a, [=, this] { goTo(arg(2)); });
  bindNative(0x2155, [] {});
  bindNative(0x2156, [this] { w(0x2c77) = bx; });
  bindNative(0x215b, [this] {
    zero12(0x03b1);
    wait1();
    u32 times = w(0x03a3);
    if (times == 0) times = 0x10000;  // as the original's loop does with a count of nought
    for (u32 i = 0; i < times; ++i) addScore(0x03b1, 0x01df);
    addScore(0x2c2b, 0x03b1);
    nextStep(2);
  });
  bindNative(0x2211, [this] {
    wait1();
    setLight(0x1f);
    nextStep(4);
  });
}

}  // namespace encore

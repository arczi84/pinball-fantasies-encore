#version 100
// Draws a high-resolution replacement over the pixels a screen drew from the original picture
// (shaders/hd.frag in GLSL ES 1.00). The map comes as the HdPixels are in memory: each pixel's
// four words (x8, y8, picture, flags) are two RGBA texels side by side, put together here in
// the machine's byte order. textureGrad becomes texture2D with a bias: the hardware's level of
// detail, read off neighbours that may belong to another picture, is turned into the one the
// picture's own movement under the screen asks for.
#extension GL_OES_standard_derivatives : enable
precision highp float;
varying vec2 vUv;
uniform sampler2D uMap;      // per screen pixel: x8, y8 | picture, flags (RGBA, two texels a pixel)
uniform vec2 uMapSize;       // the map's size in screen pixels
uniform float uBigEndian;    // 1: a word's high byte comes first
uniform sampler2D uPicture;  // the replacement, top row first
uniform sampler2D uPictureLit;  // the same with every lamp lit, when uHasLit
uniform vec2 uPictureSize;   // the replacement's size in its own pixels
uniform float uHasLit;       // the picture comes in an unlit and a lit version
uniform float uId;           // the picture this pass draws
uniform vec2 uSourceSize;    // the original picture's size in pixels
uniform float uFade;         // 1 = as drawn, 0 = all uFadeColor
uniform vec3 uFadeColor;
uniform float uRowStep;      // not 0: a strip repeated downwards, this many of its rows to a screen row
uniform float uFlat;         // 1: the picture is magnified (level 0 only, filtered LINEAR): no level sums
uniform float uCovered[32];  // (for uFallback) 1: picture n has a pass of its own after this one
uniform float uFallback;     // 1: this pass is the base picture's, over the whole frame: where the map
                             // names another picture or none, the frame's own colour (palette.frag)
uniform sampler2D uIndices;  // (for uFallback) palette.frag's: four indices a texel,
uniform sampler2D uPalette;  // 256 x rows RGB,
uniform float uPaletteRows;
uniform float uDisplayTop;   // and the dot display's rows
uniform float uDisplayRows;
uniform sampler2D uLit;      // when uLitPlane: the table's lit levels (0 to 63), one dot a texel, filtered
uniform float uLitPlane;
uniform vec2 uLitSize;
uniform float uLitRowOffset; // a map row y is the lit plane's row y + this

// The bytes of the pixel at p: its first texel (x8, y8) or its second (picture, flags).
vec4 mapBytes(vec2 p, float second) {
  vec2 uv = vec2((p.x * 2.0 + second + 0.5) / (uMapSize.x * 2.0), (p.y + 0.5) / uMapSize.y);
  return floor(texture2D(uMap, uv) * 255.0 + 0.5);
}
float word(float first, float second) { return uBigEndian > 0.5 ? first * 256.0 + second : second * 256.0 + first; }
float lowByte(float first, float second) { return uBigEndian > 0.5 ? second : first; }
float highByte(float first, float second) { return uBigEndian > 0.5 ? first : second; }
float bit(float byte, float value) { return mod(floor(byte / value), 2.0); }
/// The lamp level of the pixel at p (the picture's bits 10-15), or `here` if it is another picture's.
float litAt(vec2 p, float here) {
  vec4 b = mapBytes(clamp(p, vec2(0.0), uMapSize - 1.0), 1.0);
  return lowByte(b.r, b.g) == uId ? floor(highByte(b.r, b.g) / 4.0) : here;
}
// The frame's own colour at p, and the dot display drawn at the screen's resolution
// (palette.frag, whose pass the base picture's stands in for).
vec3 colourAt(vec2 p) {
  p = clamp(p, vec2(0.0), uMapSize - 1.0);
  vec4 four = texture2D(uIndices, vec2((floor(p.x / 4.0) + 0.5) / (uMapSize.x / 4.0), (p.y + 0.5) / uMapSize.y));
  float k = mod(p.x, 4.0);
  float index = floor(dot(four, vec4(k == 0.0 ? 1.0 : 0.0, k == 1.0 ? 1.0 : 0.0, k == 2.0 ? 1.0 : 0.0, k == 3.0 ? 1.0 : 0.0)) * 255.0 + 0.5);
  float row = uPaletteRows > 1.0 ? clamp(p.y, 0.0, uPaletteRows - 1.0) : 0.0;
  return texture2D(uPalette, vec2((index + 0.5) / 256.0, (row + 0.5) / uPaletteRows)).rgb;
}
vec3 frameColour(vec2 f) {
  if (uDisplayTop >= 0.0 && f.y >= uDisplayTop && f.y < uDisplayTop + uDisplayRows) {
    vec2 screen = 1.0 / max(fwidth(f), vec2(1e-6));
    vec2 local = f - vec2(0.0, uDisplayTop);
    vec2 cell = floor((local - 0.5) / 2.0 + 0.5);
    vec2 lit = cell * 2.0 + vec2(0.0, uDisplayTop);
    vec2 away = abs(local - (cell * 2.0 + 0.5)) * screen - 0.5 * screen;
    float on = clamp(0.5 - max(away.x, away.y), 0.0, 1.0);
    // (inside a dot or between dots, one colour: only at a dot's edge both are read)
    if (on >= 1.0) return colourAt(lit);
    if (on <= 0.0) return colourAt(lit + vec2(1.0, 0.0));
    return mix(colourAt(lit + vec2(1.0, 0.0)), colourAt(lit), on);
  }
  return colourAt(floor(f));
}

// (the larger of the two lengths, as a level: half the log of its square, no square root)
float lod(vec2 gx, vec2 gy) {
  vec2 a = gx * uPictureSize, b = gy * uPictureSize;
  return 0.5 * log2(max(max(dot(a, a), dot(b, b)), 1e-12));
}

void main() {
  vec2 f = vec2(vUv.x, 1.0 - vUv.y) * uMapSize;  // screen rows are top-down
  vec2 p = clamp(floor(f), vec2(0.0), uMapSize - 1.0);
  vec4 a = mapBytes(p, 0.0), b = mapBytes(p, 1.0);
  if (lowByte(b.r, b.g) != uId) {
    if (uFallback < 0.5) discard;
    // (a pixel the pass of its own picture draws next: the frame's colour here would be drawn
    // over, two texture reads and the dots' sums for nothing)
    float other = lowByte(b.r, b.g);
    if (other < 32.0 && uCovered[int(other)] > 0.5) discard;
    gl_FragColor = vec4(frameColour(f), 1.0);
    return;
  }
  float flags = highByte(b.r, b.g);  // the picture's bits 8-15
  vec2 step = vec2(bit(flags, 1.0) != 0.0 ? 0.5 : 1.0, bit(flags, 2.0) != 0.0 ? 0.5 : 1.0);
  if (uHasLit == 0.0 && bit(flags, 4.0) != 0.0) step.y = 0.0;  // one line of the picture, drawn out
  if (uRowStep > 0.0) step.y = uRowStep;
  vec2 src = vec2(word(a.r, a.g), word(a.b, a.a)) / 8.0 + fract(f) * step;
  vec2 uv = src / uSourceSize;
  float bias = 0.0;
  if (uFlat < 0.5) {
    vec2 gx = dFdx(f) * step / uSourceSize, gy = dFdy(f) * step / uSourceSize;
    bias = lod(gx, gy) - lod(dFdx(uv), dFdy(uv));
  }
  vec3 c = texture2D(uPicture, uv, bias).rgb;
  if (uHasLit != 0.0 && uLitPlane != 0.0) {
    // How lit this spot is, read smoothly between the dots by the texture unit (the same blend
    // the four reads below make, at once)
    float amount = texture2D(uLit, vec2(f.x, f.y + uLitRowOffset) / uLitSize).r * (255.0 / 63.0);
    c = mix(c, texture2D(uPictureLit, uv, bias).rgb, amount);
  } else if (uHasLit != 0.0) {
    // How lit this spot is, read smoothly between the screen's pixels; only pixels of this same
    // picture count (shaders/hd.frag).
    float here = floor(flags / 4.0);
    float amount = here / 63.0;
    // (the four around are read only where the level is between unlit and lit, at a lamp's
    // edge: elsewhere it is the same all round, near enough, and the GPU has four reads fewer)
    if (here > 0.0 && here < 63.0) {
      vec2 q = f - 0.5;
      vec2 base = floor(q);
      vec2 t = fract(q);
      float l00 = litAt(base, here), l10 = litAt(base + vec2(1.0, 0.0), here);
      float l01 = litAt(base + vec2(0.0, 1.0), here), l11 = litAt(base + vec2(1.0, 1.0), here);
      amount = mix(mix(l00, l10, t.x), mix(l01, l11, t.x), t.y) / 63.0;
    }
    // (the lit picture read only where a lamp shows: most of a table is unlit, and the read
    // is a texture fetch a pixel the GPU can do without)
    if (amount > 0.0) c = mix(c, texture2D(uPictureLit, uv, bias).rgb, amount);
  }
  gl_FragColor = vec4(mix(uFadeColor, c, uFade), 1.0);
}

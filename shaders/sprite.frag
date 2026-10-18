#version 410 core
// Draws a flipper as one picture turned to the angle the table computes, instead of the
// original's twenty-one drawn positions, and the ball as a picture of its own. Runs over the
// scene, at the window's resolution.
out vec4 fragColor;
uniform vec2 uTargetSize;    // the scene's size in its own pixels
uniform sampler2D uSprite;   // the flipper at rest, RGBA, top row first
uniform usampler2D uMap;     // the replacement-picture map: its low bit marks what covers the flipper
uniform vec2 uFrameSize;     // screen pixels
uniform vec2 uPivotFrame;    // the hinge, in screen pixels
uniform vec2 uPivotSprite;   // the hinge, as a fraction of the picture
uniform vec2 uScale;         // fractions of the picture per screen pixel
uniform float uAngle;        // radians away from the resting position
uniform float uTint;         // the table's fade
uniform vec2 uClip;          // the screen rows it may be drawn in
uniform uint uHiddenBy;      // pixels whose flags match these are left alone (the ball only)
uniform float uOpacity;      // 1 for the ball itself, less for the ghosts behind it
uniform sampler2D uCover;    // what hides the ball, drawn to the replacement playfield (App::loadFlipperPictures)
uniform uint uHasCover;      // there is such a picture for where the ball is (on the playfield or the ramps)
uniform vec2 uCoverSource;   // the original playfield's size, which the picture covers edge to edge
uniform uint uFirstPlayfield, uLastPlayfield;  // the playfield pictures' numbers
uniform vec3 uRecolour;      // the hue turned by x radians, the saturation and brightness by y and z (HdSprite)

vec3 toHsv(vec3 c) {
  float hi = max(c.r, max(c.g, c.b)), lo = min(c.r, min(c.g, c.b)), d = hi - lo;
  float h = d <= 0.0 ? 0.0 : hi == c.r ? mod((c.g - c.b) / d, 6.0) : hi == c.g ? (c.b - c.r) / d + 2.0 : (c.r - c.g) / d + 4.0;
  return vec3(h / 6.0, hi > 0.0 ? d / hi : 0.0, hi);
}
vec3 toRgb(vec3 c) {
  vec3 k = clamp(abs(mod(c.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
  return c.z * mix(vec3(1.0), k, c.y);
}
// The colour turned as uRecolour says, as much as it is a colour: a strong one wholly, the
// cream and the whites of a picture hardly.
vec3 recolour(vec3 c) {
  if (uRecolour == vec3(0.0, 1.0, 1.0)) return c;
  vec3 h = toHsv(c);
  float strong = smoothstep(0.25, 0.6, h.y);
  h.x = fract(h.x + uRecolour.x / 6.2831853 * strong);
  h.y = clamp(h.y * mix(1.0, uRecolour.y, strong), 0.0, 1.0);
  h.z = clamp(h.z * mix(1.0, uRecolour.z, strong), 0.0, 1.0);
  return toRgb(h);
}

// Under clear plastic the ball is seen whole, but greyed: this much of its light and shade
// gives way to an even grey.
const float kHaze = 0.42;
const vec3 kHazeGrey = vec3(0.55);
// In a picture of what hides the ball, the levels taken as black and as white.
const float kMaskBlack = 0.12, kMaskWhite = 0.92;

void main() {
  // (where this pixel is, exactly, whatever shape is drawn: sprite.vert draws only a box)
  vec2 uv = gl_FragCoord.xy / uTargetSize;
  vec2 f = vec2(uv.x, 1.0 - uv.y) * uFrameSize;  // screen rows are top-down
  if (f.y < uClip.x || f.y >= uClip.y) discard;    // never over the dot matrix
  ivec2 p = clamp(ivec2(floor(f)), ivec2(0), ivec2(uFrameSize) - 1);
  // How much artwork is in front: all of it hides the picture, a dithered cover lets it
  // through. Read smoothly between the screen's pixels, so no trace of the dither is left.
  float cover = 0.0, plastic = 0.0;
  if (uHiddenBy != 0u) {
    vec2 c = f - 0.5;
    ivec2 q = ivec2(floor(c));
    vec2 t = fract(c);
    ivec2 hi = ivec2(uFrameSize) - 1;
    float c00 = float((texelFetch(uMap, clamp(q, ivec2(0), hi), 0).a >> 8u) & 0xffu);
    float c10 = float((texelFetch(uMap, clamp(q + ivec2(1, 0), ivec2(0), hi), 0).a >> 8u) & 0xffu);
    float c01 = float((texelFetch(uMap, clamp(q + ivec2(0, 1), ivec2(0), hi), 0).a >> 8u) & 0xffu);
    float c11 = float((texelFetch(uMap, clamp(q + ivec2(1, 1), ivec2(0), hi), 0).a >> 8u) & 0xffu);
    cover = mix(mix(c00, c10, t.x), mix(c01, c11, t.x), t.y) / 255.0;
    // A picture of what hides the ball, where there is one, says it instead, at its own
    // resolution: white shows the ball, black hides it, and pure red is clear plastic, which
    // shows it greyed. Its red is how much of the ball shows, and red less green how much
    // plastic it is seen through, so the soft edges between the three read as they look.
    uvec4 m = texelFetch(uMap, p, 0);
    uint id = m.b & 0xffu;
    if (uHasCover != 0u && id >= uFirstPlayfield && id <= uLastPlayfield) {
      vec2 step = vec2((m.b & 0x100u) != 0u ? 0.5 : 1.0, (m.b & 0x200u) != 0u ? 0.5 : 1.0);
      vec3 drawn = texture(uCover, (vec2(m.rg) / 8.0 + fract(f) * step) / uCoverSource).rgb;
      // (what is nearly black is black, and nearly white white: a picture's black is seldom 0)
      float shows = clamp((drawn.r - kMaskBlack) / (kMaskWhite - kMaskBlack), 0.0, 1.0);
      cover = 1.0 - shows;
      plastic = clamp((drawn.r - drawn.g - kMaskBlack) / (kMaskWhite - kMaskBlack), 0.0, 1.0);
    }
    if (cover > 0.99) discard;
  }
  vec2 d = f - uPivotFrame;
  float c = cos(uAngle), s = sin(uAngle);
  vec2 sp = vec2(c * d.x + s * d.y, -s * d.x + c * d.y) * uScale + uPivotSprite;
  if (any(lessThan(sp, vec2(0.0))) || any(greaterThanEqual(sp, vec2(1.0)))) discard;
  vec4 t = texture(uSprite, sp);
  float alpha = t.a * (1.0 - cover) * uOpacity;
  if (alpha < 0.02) discard;
  fragColor = vec4(mix(recolour(t.rgb), kHazeGrey, kHaze * plastic) * uTint, alpha);
}

#version 100
// Draws a flipper as one picture turned to the angle the table computes, and the ball as a
// picture of its own (shaders/sprite.frag in GLSL ES 1.00; the map as in hd.frag).
precision highp float;
uniform vec2 uTargetSize;    // the scene's size in its own pixels
uniform vec2 uTargetOrigin;  // and where it begins in the framebuffer (the screen's letterbox, drawn straight to it)
uniform float uRotate;       // as fullscreen.vert's: the framebuffer's pixels turned back into the scene's
uniform sampler2D uSprite;   // the flipper at rest, RGBA, top row first
uniform sampler2D uMap;      // the replacement-picture map (hd.frag): its flags' high byte is the cover
uniform float uBigEndian;    // 1: a word's high byte comes first
uniform vec2 uFrameSize;     // screen pixels (the map's size)
uniform vec2 uPivotFrame;    // the hinge, in screen pixels
uniform vec2 uPivotSprite;   // the hinge, as a fraction of the picture
uniform vec2 uScale;         // fractions of the picture per screen pixel
uniform float uAngle;        // radians away from the resting position
uniform float uTint;         // the table's fade
uniform vec2 uClip;          // the screen rows it may be drawn in
uniform float uHiddenBy;     // 1: pixels the artwork covers are left alone (the ball only)
uniform float uOpacity;      // 1 for the ball itself, less for the ghosts behind it
uniform sampler2D uCover;    // what hides the ball, drawn to the replacement playfield (App::loadFlipperPictures)
uniform float uHasCover;     // there is such a picture for where the ball is (on the playfield or the ramps)
uniform vec2 uCoverSource;   // the original playfield's size, which the picture covers edge to edge
uniform float uFirstPlayfield, uLastPlayfield;  // the playfield pictures' numbers
uniform vec3 uRecolour;      // the hue turned by x radians, the saturation and brightness by y and z (HdSprite)

vec4 mapBytes(vec2 p, float second) {
  vec2 uv = vec2((p.x * 2.0 + second + 0.5) / (uFrameSize.x * 2.0), (p.y + 0.5) / uFrameSize.y);
  return floor(texture2D(uMap, uv) * 255.0 + 0.5);
}
float word(float first, float second) { return uBigEndian > 0.5 ? first * 256.0 + second : second * 256.0 + first; }
float lowByte(float first, float second) { return uBigEndian > 0.5 ? second : first; }
float highByte(float first, float second) { return uBigEndian > 0.5 ? first : second; }
float bit(float byte, float value) { return mod(floor(byte / value), 2.0); }
/// How much artwork covers the pixel at p, 0 to 255: its flags' high byte.
float coverAt(vec2 p) {
  vec4 b = mapBytes(clamp(p, vec2(0.0), uFrameSize - 1.0), 1.0);
  return highByte(b.b, b.a);
}

vec3 toHsv(vec3 c) {
  float hi = max(c.r, max(c.g, c.b)), lo = min(c.r, min(c.g, c.b)), d = hi - lo;
  float h = d <= 0.0 ? 0.0 : hi == c.r ? mod((c.g - c.b) / d, 6.0) : hi == c.g ? (c.b - c.r) / d + 2.0 : (c.r - c.g) / d + 4.0;
  return vec3(h / 6.0, hi > 0.0 ? d / hi : 0.0, hi);
}
vec3 toRgb(vec3 c) {
  vec3 k = clamp(abs(mod(c.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
  return c.z * mix(vec3(1.0), k, c.y);
}
vec3 recolour(vec3 c) {
  if (uRecolour == vec3(0.0, 1.0, 1.0)) return c;
  vec3 h = toHsv(c);
  float strong = smoothstep(0.25, 0.6, h.y);
  h.x = fract(h.x + uRecolour.x / 6.2831853 * strong);
  h.y = clamp(h.y * mix(1.0, uRecolour.y, strong), 0.0, 1.0);
  h.z = clamp(h.z * mix(1.0, uRecolour.z, strong), 0.0, 1.0);
  return toRgb(h);
}

const float kHaze = 0.42;
const vec3 kHazeGrey = vec3(0.55);
const float kMaskBlack = 0.12, kMaskWhite = 0.92;

void main() {
  vec2 q = gl_FragCoord.xy - uTargetOrigin;
  if (uRotate > 0.5) q = vec2(uTargetSize.x - q.y, q.x);
  else if (uRotate < -0.5) q = vec2(q.y, uTargetSize.y - q.x);
  vec2 uv = q / uTargetSize;
  vec2 f = vec2(uv.x, 1.0 - uv.y) * uFrameSize;  // screen rows are top-down
  if (f.y < uClip.x || f.y >= uClip.y) discard;    // never over the dot matrix
  vec2 p = clamp(floor(f), vec2(0.0), uFrameSize - 1.0);
  float cover = 0.0, plastic = 0.0;
  if (uHiddenBy != 0.0) {
    vec2 c = f - 0.5;
    vec2 q = floor(c);
    vec2 t = fract(c);
    float c00 = coverAt(q), c10 = coverAt(q + vec2(1.0, 0.0));
    float c01 = coverAt(q + vec2(0.0, 1.0)), c11 = coverAt(q + vec2(1.0, 1.0));
    cover = mix(mix(c00, c10, t.x), mix(c01, c11, t.x), t.y) / 255.0;
    vec4 a = mapBytes(p, 0.0), b = mapBytes(p, 1.0);
    float id = lowByte(b.r, b.g), flags = highByte(b.r, b.g);
    if (uHasCover != 0.0 && id >= uFirstPlayfield && id <= uLastPlayfield) {
      vec2 step = vec2(bit(flags, 1.0) != 0.0 ? 0.5 : 1.0, bit(flags, 2.0) != 0.0 ? 0.5 : 1.0);
      vec2 at = vec2(word(a.r, a.g), word(a.b, a.a)) / 8.0 + fract(f) * step;
      vec3 drawn = texture2D(uCover, at / uCoverSource).rgb;
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
  vec4 t = texture2D(uSprite, sp);
  float alpha = t.a * (1.0 - cover) * uOpacity;
  if (alpha < 0.02) discard;
  gl_FragColor = vec4(mix(recolour(t.rgb), kHazeGrey, kHaze * plastic) * uTint, alpha);
}

#version 100
// Pass 1: palette lookup (shaders/palette.frag in GLSL ES 1.00). The indices are an RGBA
// texture, four to a texel, and the palette 256 x rows RGB, their sizes given, as ES 1.00
// cannot ask a texture.
#extension GL_OES_standard_derivatives : enable
precision highp float;
varying vec2 vUv;
uniform sampler2D uIndices;    // RGBA, four indices a texel (r, g, b, a: x, x+1, x+2, x+3)
uniform sampler2D uPalette;    // 256 x rows RGB
uniform vec2 uIndexSize;       // the frame's size in pixels
uniform float uPaletteRows;    // rows in uPalette
uniform float uDisplayTop;     // the frame's row where the dot display begins (its last rows, or its first); -1 for none
uniform float uDisplayRows;    // and how many rows it has
uniform float uDots;           // 1: this pass is at the screen's resolution, and draws the dots

// (see shaders/palette.frag for the dot display)

vec3 colourAt(vec2 p) {
  p = clamp(p, vec2(0.0), uIndexSize - 1.0);
  vec4 four = texture2D(uIndices, vec2((floor(p.x / 4.0) + 0.5) / (uIndexSize.x / 4.0), (p.y + 0.5) / uIndexSize.y));
  float k = mod(p.x, 4.0);
  float index = floor(dot(four, vec4(k == 0.0 ? 1.0 : 0.0, k == 1.0 ? 1.0 : 0.0, k == 2.0 ? 1.0 : 0.0, k == 3.0 ? 1.0 : 0.0)) * 255.0 + 0.5);
  float row = uPaletteRows > 1.0 ? clamp(p.y, 0.0, uPaletteRows - 1.0) : 0.0;
  return texture2D(uPalette, vec2((index + 0.5) / 256.0, (row + 0.5) / uPaletteRows)).rgb;
}

void main() {
  vec2 f = vec2(vUv.x, 1.0 - vUv.y) * uIndexSize;  // in the frame's pixels, rows top-down
  vec2 screen = 1.0 / max(fwidth(f), vec2(1e-6));   // screen pixels to a frame pixel
  if (uDots != 0.0 && uDisplayTop >= 0.0 && f.y >= uDisplayTop && f.y < uDisplayTop + uDisplayRows) {
    vec2 local = f - vec2(0.0, uDisplayTop);
    vec2 cell = floor((local - 0.5) / 2.0 + 0.5);    // the nearest dot
    vec2 lit = cell * 2.0 + vec2(0.0, uDisplayTop);
    vec2 away = abs(local - (cell * 2.0 + 0.5)) * screen - 0.5 * screen;
    float on = clamp(0.5 - max(away.x, away.y), 0.0, 1.0);
    gl_FragColor = vec4(mix(colourAt(lit + vec2(1.0, 0.0)), colourAt(lit), on), 1.0);
    return;
  }
  gl_FragColor = vec4(colourAt(floor(f)), 1.0);
}

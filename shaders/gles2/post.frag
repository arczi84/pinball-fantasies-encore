#version 100
// Pass 2: presentation (shaders/post.frag in GLSL ES 1.00).
#extension GL_OES_standard_derivatives : enable
precision highp float;
varying vec2 vUv;
uniform sampler2D uScene;      // native-resolution RGB scene
uniform vec2 uSceneSize;       // pixels
uniform vec2 uOutputSize;      // pixels of the viewport this pass renders into
uniform float uFilter;         // 0 = nearest neighbour, 1 = sharp with antialiased edges
uniform float uDisplayTop;     // the scene's row where the dot display begins (its last rows, or its first); -1 for none
uniform float uDisplayRows;    // and how many rows it has
uniform float uDots;           // 1: the scene is at the frame's resolution, and the dots are drawn here

vec3 sceneAt(vec2 p) { return texture2D(uScene, (clamp(p, vec2(0.0), uSceneSize - 1.0) + 0.5) / uSceneSize).rgb; }

vec2 sharpUv(vec2 uv) {
  vec2 texel = uv * uSceneSize;
  vec2 scale = max(uOutputSize / uSceneSize, vec2(1.0));
  vec2 centre = fract(texel) - 0.5;
  vec2 flat_ = 0.5 - 0.5 / scale;
  vec2 offset = (centre - clamp(centre, -flat_, flat_)) * scale + 0.5;
  return (floor(texel) + offset) / uSceneSize;
}

void main() {
  vec2 f = vec2(vUv.x, 1.0 - vUv.y) * uSceneSize;  // in the scene's pixels, rows top-down
  vec2 screen = 1.0 / max(fwidth(f), vec2(1e-6));   // screen pixels to a scene pixel
  if (uDots != 0.0 && uDisplayTop >= 0.0 && f.y >= uDisplayTop && f.y < uDisplayTop + uDisplayRows) {
    vec2 local = f - vec2(0.0, uDisplayTop);
    vec2 cell = floor((local - 0.5) / 2.0 + 0.5);    // the nearest dot
    vec2 lit = cell * 2.0 + vec2(0.0, uDisplayTop);
    vec2 lit0 = vec2(lit.x, uSceneSize.y - 1.0 - lit.y);  // (the texture's rows are bottom-up)
    vec2 away = abs(local - (cell * 2.0 + 0.5)) * screen - 0.5 * screen;
    float on = clamp(0.5 - max(away.x, away.y), 0.0, 1.0);
    gl_FragColor = vec4(mix(sceneAt(lit0 + vec2(1.0, 0.0)), sceneAt(lit0), on), 1.0);
    return;
  }
  vec2 uv = uFilter < 0.5 ? (floor(vUv * uSceneSize) + 0.5) / uSceneSize : sharpUv(vUv);
  gl_FragColor = vec4(texture2D(uScene, uv).rgb, 1.0);
}

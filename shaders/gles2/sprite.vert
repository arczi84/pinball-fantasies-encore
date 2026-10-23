#version 100
// The box a flipper or the ball can be drawn in (shaders/sprite.vert): the unit square's six
// corners from a vertex buffer, spread over uBox.
attribute vec2 aPos;
uniform vec4 uBox;  // left, bottom, right, top, as fractions of the scene, bottom-up
uniform float uRotate;  // as fullscreen.vert's
void main() {
  vec2 p = mix(uBox.xy, uBox.zw, aPos) * 2.0 - 1.0;
  if (uRotate > 0.5) p = vec2(p.y, -p.x);
  else if (uRotate < -0.5) p = vec2(-p.y, p.x);
  gl_Position = vec4(p, 0.0, 1.0);
}

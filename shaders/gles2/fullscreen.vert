#version 100
// Fullscreen triangle (shaders/fullscreen.vert), its corners from a vertex buffer.
attribute vec2 aPos;
varying vec2 vUv;
uniform float uRotate;  // 1: the picture turned a quarter clockwise on the screen, -1: anticlockwise (--rotate)
void main() {
  vUv = aPos * 0.5 + 0.5;
  vec2 p = aPos;
  if (uRotate > 0.5) p = vec2(p.y, -p.x);
  else if (uRotate < -0.5) p = vec2(-p.y, p.x);
  gl_Position = vec4(p, 0.0, 1.0);
}

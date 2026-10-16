#version 410 core
// The box a flipper or the ball can be drawn in, rather than the whole screen: only the pixels
// sprite.frag could draw are run (it finds each pixel's place from gl_FragCoord, so the shape
// drawn changes nothing of what it draws). Two triangles; no vertex buffer needed.
uniform vec4 uBox;  // left, bottom, right, top, as fractions of the scene, bottom-up
void main() {
  const vec2 corner[6] = vec2[6](vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(0, 1), vec2(1, 0), vec2(1, 1));
  gl_Position = vec4(mix(uBox.xy, uBox.zw, corner[gl_VertexID]) * 2.0 - 1.0, 0.0, 1.0);
}

#version 100
// The last pass when the scene is the screen's own size (RendererGles2.cpp): the scene copied
// as it is, a texel to a pixel. ogles2 draws a fullscreen frame straight into the screen being
// shown, so this pass must outrun the display's scan; the HD pass behind it need not.
precision mediump float;
varying vec2 vUv;
uniform sampler2D uScene;
void main() { gl_FragColor = texture2D(uScene, vUv); }

// The renderer on OpenGL ES 2.0 (ogles2.library on the Amiga): the passes of Renderer.cpp with
// the shaders of shaders/gles2. What GLES2 lacks is made up for here:
// - the indexed frame is an RGBA texture of four indices a texel (palette.frag), the palette
//   RGB, and their sizes are uniforms;
// - the replacement-picture map (HdPixel: four 16-bit words a pixel) is sent as it is in memory,
//   as an RGBA texture twice as wide, and the shaders put the words together from its bytes, in
//   the machine's byte order (uBigEndian);
// - the fullscreen triangle and the sprites' boxes come from vertex buffers;
// - there are no unsigned uniforms: numbers and flags go as floats.
#include "gfx/Renderer.h"

#ifdef ENCORE_GLES2

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/Log.h"

namespace encore {
namespace {

/// 1 where the high byte of a word comes first in memory (the Amiga), 0 where it comes last.
float bigEndian() {
  const u16 probe = 1;
  u8 first = 0;
  std::memcpy(&first, &probe, 1);
  return first == 0 ? 1.0f : 0.0f;
}

void linearClamp(GLenum wrapT = GL_CLAMP_TO_EDGE) {
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
}

}  // namespace

Renderer::~Renderer() {
  log::debug("leaving: the renderer object");
  release();
}

void Renderer::release() {
  if (quadVbo_) glDeleteBuffers(1, &quadVbo_);
  if (boxVbo_) glDeleteBuffers(1, &boxVbo_);
  for (GLuint& t : indexRing_)
    if (t) glDeleteTextures(1, &t);
  for (GLuint& t : mapRing_)
    if (t) glDeleteTextures(1, &t);
  indexRing_ = {};
  mapRing_ = {};
  indexRingSize_ = {};
  mapRingSize_ = {};
  if (paletteTex_) glDeleteTextures(1, &paletteTex_);
  if (litTex_) glDeleteTextures(1, &litTex_);
  litTex_ = 0;
  litW_ = litH_ = 0;
  deleteTarget(scene_);
  deleteTarget(hdScene_);
  for (GLuint& t : hdTex_)
    if (t) glDeleteTextures(1, &t);
  for (GLuint& t : spriteTex_)
    if (t) glDeleteTextures(1, &t);
  for (GLuint& t : coverTex_)
    if (t) glDeleteTextures(1, &t);
  quadVbo_ = boxVbo_ = 0;
  vao_ = indexTex_ = paletteTex_ = hdMapTex_ = 0;
  hdTex_ = {};
  spriteTex_ = {};
  coverTex_ = {};
  hdLoaded_ = 0;
  coverLoaded_ = {};
  spriteSize_ = {};
  hdMapW_ = hdMapH_ = 0;
  paletteRows_ = 0;
  palettePass_.release();
  postPass_.release();
  copyPass_.release();
  copyLoaded_ = false;
  crtPass_.release();
  hdPass_.release();
  spritePass_.release();
}

void Renderer::deleteTarget(Target& t) {
  if (t.tex) glDeleteTextures(1, &t.tex);
  if (t.fbo) glDeleteFramebuffers(1, &t.fbo);
  t = {};
}

bool Renderer::init(const std::filesystem::path& shaderDir, int frameWidth, int frameHeight, double pixelAspect) {
  shaderDir_ = shaderDir / "gles2";
  pixelAspect_ = pixelAspect;
  // The triangle that covers the screen, and the unit square the sprites' boxes are made of.
  static const float kTriangle[] = {-1, -1, 3, -1, -1, 3};
  static const float kSquare[] = {0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1};
  glGenBuffers(1, &quadVbo_);
  glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kTriangle), kTriangle, GL_STATIC_DRAW);
  glGenBuffers(1, &boxVbo_);
  glBindBuffer(GL_ARRAY_BUFFER, boxVbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kSquare), kSquare, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glGenTextures(kRing, indexRing_.data());
  glGenTextures(kRing, mapRing_.data());
  glGenTextures(1, &paletteTex_);
  glBindTexture(GL_TEXTURE_2D, paletteTex_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  resizeSource(frameWidth, frameHeight);
  if (!palettePass_.load(shaderDir_ / "fullscreen.vert", shaderDir_ / "palette.frag")) return false;
  if (!postPass_.load(shaderDir_ / "fullscreen.vert", shaderDir_ / "post.frag")) return false;
  return true;
}

void Renderer::drawFullscreen() {
  glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::resizeSource(int frameWidth, int frameHeight) {
  frameW_ = frameWidth;
  frameH_ = frameHeight;
  // (the index textures are sized as each comes round: draw)
  ensureTarget(scene_, frameWidth, frameHeight);
}

void Renderer::ensureTarget(Target& t, int w, int h) {
  if (t.tex && t.w == w && t.h == h) return;
  if (!t.tex) glGenTextures(1, &t.tex);
  if (!t.fbo) glGenFramebuffers(1, &t.fbo);
  t.w = w;
  t.h = h;
  glBindTexture(GL_TEXTURE_2D, t.tex);
  linearClamp();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) log::error("offscreen framebuffer incomplete");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::pollShaderReload() {
#ifndef __amigaos__  // (a look at the files every frame costs the Amiga's disk more than it is worth)
  palettePass_.reloadIfChanged();
  postPass_.reloadIfChanged();
  if (crtLoaded_) crtPass_.reloadIfChanged();
  if (hdPassLoaded_) hdPass_.reloadIfChanged();
  if (spritePassLoaded_) spritePass_.reloadIfChanged();
#endif
}

void Renderer::setSpritePicture(std::size_t slot, int width, int height, const u8* rgba) {
  if (slot >= kSprites) return;
  if (!spriteTex_[slot]) glGenTextures(1, &spriteTex_[slot]);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, spriteTex_[slot]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  linearClamp();
  spriteSize_[slot] = {width, height};
}

void Renderer::clearSpritePictures() { spriteSize_ = {}; }

void Renderer::setCoverPicture(int layer, int width, int height, const u8* rgba, int sourceWidth, int sourceHeight) {
  if (layer < 0 || layer > 1) return;
  const auto l = static_cast<std::size_t>(layer);
  if (!coverTex_[l]) glGenTextures(1, &coverTex_[l]);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, coverTex_[l]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  linearClamp();
  coverLoaded_[l] = true;
  coverSource_[l] = {sourceWidth, sourceHeight};
}

void Renderer::clearCoverPictures() { coverLoaded_ = {}; }

void Renderer::drawSprites(const HdFrame& hd) {
  if (!spritePassLoaded_) {
    spritePassLoaded_ = true;
    if (!spritePass_.load(shaderDir_ / "sprite.vert", shaderDir_ / "sprite.frag"))
      log::error("the flipper shader failed to load; the drawn positions are used instead");
  }
  if (!spritePass_.id()) return;
  spritePass_.use();
  glUniform1f(spritePass_.uniform("uRotate"), rotateUniform());
  glUniform1i(spritePass_.uniform("uMap"), 2);   // still bound from the picture pass
  glUniform1i(spritePass_.uniform("uSprite"), 3);
  glUniform1f(spritePass_.uniform("uBigEndian"), bigEndian());
  glUniform2f(spritePass_.uniform("uFrameSize"), static_cast<float>(hd.width), static_cast<float>(hd.height));
  glUniform2f(spritePass_.uniform("uTargetSize"), static_cast<float>(hdW_), static_cast<float>(hdH_));
  glUniform2f(spritePass_.uniform("uTargetOrigin"), static_cast<float>(hdX_), static_cast<float>(hdY_));
  glUniform1f(spritePass_.uniform("uTint"), hd.spriteTint);
  const auto layer = static_cast<std::size_t>(hd.ballLayer != 0 ? 1 : 0);
  glUniform1f(spritePass_.uniform("uHasCover"), coverLoaded_[layer] ? 1.0f : 0.0f);
  if (coverLoaded_[layer]) {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, coverTex_[layer]);
    glUniform1i(spritePass_.uniform("uCover"), 4);
    glUniform2f(spritePass_.uniform("uCoverSource"), static_cast<float>(coverSource_[layer][0]), static_cast<float>(coverSource_[layer][1]));
    glUniform1f(spritePass_.uniform("uFirstPlayfield"), static_cast<float>(HdPicture::Playfield1On));
    glUniform1f(spritePass_.uniform("uLastPlayfield"), static_cast<float>(HdPicture::Playfield4Off));
  }
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, boxVbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glActiveTexture(GL_TEXTURE3);
  const float frameW = static_cast<float>(hd.width), frameH = static_cast<float>(hd.height);
  for (const HdSprite& s : hd.sprites) {
    if (s.picture >= kSprites || !spriteTex_[s.picture] || !spriteSize_[s.picture][0]) continue;
    // Where on the screen the picture can be: its four corners, turned about the hinge as the
    // fragment shader turns the screen into the picture, and the rows it is clipped to. Only
    // that box is drawn (shaders/gles2/sprite.vert): everywhere else the shader would draw nothing.
    if (s.scaleX == 0 || s.scaleY == 0) continue;
    const float c = std::cos(s.angle), n = std::sin(s.angle);
    float x0 = frameW, y0 = frameH, x1 = 0, y1 = 0;
    for (const float cx : {0.0f, 1.0f})
      for (const float cy : {0.0f, 1.0f}) {
        const float rx = (cx - s.pivotSpriteX) / s.scaleX, ry = (cy - s.pivotSpriteY) / s.scaleY;
        const float fx = s.pivotFrameX + c * rx - n * ry, fy = s.pivotFrameY + n * rx + c * ry;
        x0 = std::min(x0, fx), x1 = std::max(x1, fx), y0 = std::min(y0, fy), y1 = std::max(y1, fy);
      }
    // (a frame dot more all round, for the screen's pixels that straddle the edge)
    x0 = std::max(x0 - 1, 0.0f), x1 = std::min(x1 + 1, frameW);
    y0 = std::max({y0 - 1, s.clipTop, 0.0f}), y1 = std::min({y1 + 1, s.clipBottom, frameH});
    if (x0 >= x1 || y0 >= y1) continue;
    // (the screen's rows are top-down, vUv's bottom-up)
    glUniform4f(spritePass_.uniform("uBox"), x0 / frameW, 1 - y1 / frameH, x1 / frameW, 1 - y0 / frameH);
    glBindTexture(GL_TEXTURE_2D, spriteTex_[s.picture]);
    glUniform2f(spritePass_.uniform("uPivotFrame"), s.pivotFrameX, s.pivotFrameY);
    glUniform2f(spritePass_.uniform("uPivotSprite"), s.pivotSpriteX, s.pivotSpriteY);
    glUniform2f(spritePass_.uniform("uScale"), s.scaleX, s.scaleY);
    glUniform1f(spritePass_.uniform("uAngle"), s.angle);
    glUniform2f(spritePass_.uniform("uClip"), s.clipTop, s.clipBottom);
    glUniform1f(spritePass_.uniform("uHiddenBy"), s.hiddenBy != 0 ? 1.0f : 0.0f);
    glUniform1f(spritePass_.uniform("uOpacity"), s.opacity);
    glUniform3f(spritePass_.uniform("uRecolour"), s.hue, s.saturation, s.brightness);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }
  glDisable(GL_BLEND);
  glActiveTexture(GL_TEXTURE0);
}

void Renderer::setHdPicture(HdPicture p, int width, int height, const u8* rgba) {
  const auto i = static_cast<std::size_t>(p);
  if (!hdTex_[i]) glGenTextures(1, &hdTex_[i]);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, hdTex_[i]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  // (mipmaps of pictures that are not a power of two in size: OES_texture_npot)
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, p == HdPicture::LeftRepeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  hdTexSize_[i] = {width, height};
  hdFlat_[i] = false;
  hdLoaded_ |= 1u << i;
}

// A picture drawn larger than it is (the screen's pixels at least its own over the original):
// only its first level is read, so it is filtered LINEAR and hd.frag skips the level sums
// (uFlat), which cost the PiStorm's GPU some 8% of a 1080-row frame. The bound texture.
void Renderer::setFlat(std::size_t i, bool flat) {
  if (hdFlat_[i] == flat) return;
  hdFlat_[i] = flat;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flat ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
}

bool Renderer::baseDrawn(const HdFrame& hd) const {
  return hd.basePicture != 0 && (hdLoaded_ & (1u << hd.basePicture)) && (hd.used & (1u << hd.basePicture)) && hdPass_.id() != 0;
}

void Renderer::drawHd(const HdFrame& hd) {
  if (!hdPassLoaded_) {
    hdPassLoaded_ = true;
    if (!hdPass_.load(shaderDir_ / "fullscreen.vert", shaderDir_ / "hd.frag")) {
      log::error("the replacement-picture shader failed to load; showing the original pictures");
      hdEnabled_ = false;
      return;
    }
  }
  if (!hdPass_.id()) return;
  // The map as it is in memory: each HdPixel's eight bytes are two RGBA texels side by side,
  // into this frame's texture of the ring.
  glActiveTexture(GL_TEXTURE2);
  hdMapTex_ = mapRing_[static_cast<std::size_t>(ring_)];
  glBindTexture(GL_TEXTURE_2D, hdMapTex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  static_assert(sizeof(HdPixel) == 8, "the map is sent as two RGBA texels a pixel");
  auto& mapSize = mapRingSize_[static_cast<std::size_t>(ring_)];
  hdMapW_ = mapSize[0];
  hdMapH_ = mapSize[1];
  if (hd.width != hdMapW_ || hd.height != hdMapH_) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, hd.width * 2, hd.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, hd.map.data());
    mapSize = {hd.width, hd.height};
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, hd.width * 2, hd.height, GL_RGBA, GL_UNSIGNED_BYTE, hd.map.data());
  }
  // The table's lit levels, read smoothly by the GPU: sent when a lamp has changed.
  const bool litPlane = hd.litPlane && hd.litWidth > 0 && hd.litHeight > 0;
  if (litPlane) {
    glActiveTexture(GL_TEXTURE5);
    if (!litTex_) glGenTextures(1, &litTex_);
    glBindTexture(GL_TEXTURE_2D, litTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (hd.litWidth != litW_ || hd.litHeight != litH_) {
      linearClamp();
      glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, hd.litWidth, hd.litHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, hd.litPlane);
      litW_ = hd.litWidth;
      litH_ = hd.litHeight;
      litVersionSent_ = hd.litVersion;
    } else if (hd.litVersion != litVersionSent_) {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, litW_, litH_, GL_LUMINANCE, GL_UNSIGNED_BYTE, hd.litPlane);
      litVersionSent_ = hd.litVersion;
    }
  }
  hdPass_.use();
  glUniform1f(hdPass_.uniform("uRotate"), rotateUniform());
  glUniform1i(hdPass_.uniform("uMap"), 2);
  glUniform1i(hdPass_.uniform("uLit"), 5);
  glUniform1f(hdPass_.uniform("uLitPlane"), litPlane ? 1.0f : 0.0f);
  if (litPlane) {
    glUniform2f(hdPass_.uniform("uLitSize"), static_cast<float>(litW_), static_cast<float>(litH_));
    glUniform1f(hdPass_.uniform("uLitRowOffset"), hd.litRowOffset);
  }
  glUniform1i(hdPass_.uniform("uPicture"), 3);
  glUniform1i(hdPass_.uniform("uPictureLit"), 4);
  glUniform2f(hdPass_.uniform("uMapSize"), static_cast<float>(hd.width), static_cast<float>(hd.height));
  glUniform1f(hdPass_.uniform("uBigEndian"), bigEndian());
  glUniform3f(hdPass_.uniform("uFadeColor"), hd.fadeColor.r / 255.0f, hd.fadeColor.g / 255.0f, hd.fadeColor.b / 255.0f);
  // The base picture's pass first, over the whole frame, with the frame's colours where the map
  // names something else (hd.frag's uFallback): the palette pass it stands in for was skipped.
  const std::size_t base = baseDrawn(hd) ? hd.basePicture : 0;
  glUniform1i(hdPass_.uniform("uIndices"), 0);
  glUniform1i(hdPass_.uniform("uPalette"), 1);
  glUniform1f(hdPass_.uniform("uPaletteRows"), static_cast<float>(std::max(paletteRows_, 1)));
  const int displayTop = displayRows_ <= 0 ? -1 : displayAtTop_ ? 0 : frameH_ - displayRows_;
  glUniform1f(hdPass_.uniform("uDisplayTop"), static_cast<float>(displayTop));
  glUniform1f(hdPass_.uniform("uDisplayRows"), static_cast<float>(displayRows_));
  // (the pictures with a pass of their own after the base picture's: hd.frag's uCovered, the
  // base pass leaves their pixels to them)
  static_assert(HdFrame::kCount <= 32, "hd.frag's uCovered has 32 entries");
  {
    std::array<float, 32> covered{};
    for (std::size_t i = 1; i < HdFrame::kCount; ++i) {
      const auto picture = static_cast<HdPicture>(i);
      if (base == 0 || i == base || !(hd.used & hdLoaded_ & (1u << i))) continue;
      if (picture >= HdPicture::Playfield1On && picture <= HdPicture::Playfield4On) continue;
      covered[i] = 1.0f;
    }
    glUniform1fv(hdPass_.uniform("uCovered"), 32, covered.data());
  }
  glActiveTexture(GL_TEXTURE3);
  for (std::size_t n = 0; n < HdFrame::kCount; ++n) {
    const std::size_t i = n == 0 ? base : n;
    if (i == 0 || (n != 0 && i == base)) continue;
    glUniform1f(hdPass_.uniform("uFallback"), i == base ? 1.0f : 0.0f);
    if (!(hd.used & hdLoaded_ & (1u << i))) continue;
    // A playfield comes as a pair: the unlit picture is drawn and the lit one blended into it
    // lamp by lamp, so the lit picture is never a pass of its own.
    const auto picture = static_cast<HdPicture>(i);
    if (picture >= HdPicture::Playfield1On && picture <= HdPicture::Playfield4On) continue;
    const bool pair = picture >= HdPicture::Playfield1Off && picture <= HdPicture::Playfield4Off;
    const std::size_t lit = i - (static_cast<std::size_t>(HdPicture::Playfield1Off) -
                                 static_cast<std::size_t>(HdPicture::Playfield1On));
    const bool hasLit = pair && (hdLoaded_ & (1u << lit));
    // (magnified: as many screen pixels to an original one as the picture has, or more, both ways)
    const bool flat = hd.size[i][0] > 0 && hd.size[i][1] > 0 &&
                      static_cast<double>(hdW_) / hd.width >= static_cast<double>(hdTexSize_[i][0]) / hd.size[i][0] &&
                      static_cast<double>(hdH_) / hd.height >= static_cast<double>(hdTexSize_[i][1]) / hd.size[i][1];
    if (hasLit) {
      glActiveTexture(GL_TEXTURE4);
      glBindTexture(GL_TEXTURE_2D, hdTex_[lit]);
      setFlat(lit, flat);
      glActiveTexture(GL_TEXTURE3);
    }
    glBindTexture(GL_TEXTURE_2D, hdTex_[i]);
    setFlat(i, flat);
    glUniform1f(hdPass_.uniform("uFlat"), flat ? 1.0f : 0.0f);
    glUniform1f(hdPass_.uniform("uHasLit"), hasLit ? 1.0f : 0.0f);
    glUniform1f(hdPass_.uniform("uId"), static_cast<float>(i));
    glUniform2f(hdPass_.uniform("uSourceSize"), hd.size[i][0], hd.size[i][1]);
    glUniform2f(hdPass_.uniform("uPictureSize"), static_cast<float>(hdTexSize_[i][0]), static_cast<float>(hdTexSize_[i][1]));
    glUniform1f(hdPass_.uniform("uFade"), hd.fade[i]);
    glUniform1f(hdPass_.uniform("uRowStep"), hd.rowStep[i]);
    // (a picture known to be in a box of the frame: its pass drawn there alone)
    const auto& box = hd.box[i];
    const bool boxed = box[2] > box[0];
    if (boxed) {
      const double sx = static_cast<double>(hdW_) / hd.width, sy = static_cast<double>(hdH_) / hd.height;
      const int x0 = static_cast<int>(std::floor(box[0] * sx)) - 1, x1 = static_cast<int>(std::ceil(box[2] * sx)) + 1;
      const int y0 = static_cast<int>(std::floor(box[1] * sy)) - 1, y1 = static_cast<int>(std::ceil(box[3] * sy)) + 1;
      glEnable(GL_SCISSOR_TEST);
      const Rect box = toScreen({x0, hdH_ - y1, x1 - x0, y1 - y0}, hdW_, hdH_);  // (GL's rows are bottom-up)
      glScissor(hdX_ + box.x, hdY_ + box.y, box.w, box.h);
    }
    drawFullscreen();
    if (boxed) glDisable(GL_SCISSOR_TEST);
  }
  glActiveTexture(GL_TEXTURE0);
}

void Renderer::setPalette(const Palette& palette) { setRowPalettes(palette.colors().data(), 1); }

void Renderer::setRowPalettes(const Rgb* colors, int rows) {
  // (a frame's palette is mostly the last one: sent only when it has changed)
  const std::size_t n = static_cast<std::size_t>(rows) * 256;
  if (rows == paletteRows_ && sentPalette_.size() == n && std::memcmp(sentPalette_.data(), colors, n * sizeof(Rgb)) == 0) return;
  sentPalette_.assign(colors, colors + n);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, paletteTex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  if (rows != paletteRows_) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, rows, 0, GL_RGB, GL_UNSIGNED_BYTE, colors);
    paletteRows_ = rows;
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, rows, GL_RGB, GL_UNSIGNED_BYTE, colors);
  }
  glActiveTexture(GL_TEXTURE0);
}

void Renderer::draw(const Framebuffer& frame, int windowWidth, int windowHeight, const HdFrame* hd) {
  if (frame.width() != frameW_ || frame.height() != frameH_) resizeSource(frame.width(), frame.height());
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  // Upload the indexed frame (to this frame's texture of the ring) and the palette.
  ring_ = (ring_ + 1) % kRing;
  indexTex_ = indexRing_[static_cast<std::size_t>(ring_)];
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, indexTex_);
  if (auto& size = indexRingSize_[static_cast<std::size_t>(ring_)]; size[0] != frameW_ || size[1] != frameH_) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameW_ / 4, frameH_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    size = {frameW_, frameH_};
  }
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  // (four indices to an RGBA texel: ogles2 sends RGBA as it is, and turns LUMINANCE into it a
  // byte at a time, which cost the PiStorm some 4 ms a frame; palette.frag picks the channel)
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameW_ / 4, frameH_, GL_RGBA, GL_UNSIGNED_BYTE, frame.data());
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, paletteTex_);
  glActiveTexture(GL_TEXTURE0);
  // (turned, the picture is laid out on the screen as it stands, its sides swapped)
  const int lw = rotation_ ? windowHeight : windowWidth, lh = rotation_ ? windowWidth : windowHeight;
  const double targetAspect = (frameW_ * pixelAspect_) / frameH_;
  int vw = lw, vh = static_cast<int>(std::lround(lw / targetAspect));
  if (vh > lh) { vh = lh; vw = static_cast<int>(std::lround(lh * targetAspect)); }
  viewport_ = {(lw - vw) / 2, (lh - vh) / 2, vw, vh};
  passRotation_ = rotation_;
  const Rect screen = toScreen(viewport_, lw, lh);
  passRotation_ = 0;

  // Pass 1: palette lookup into the scene texture: native resolution, or the window's when
  // replacement pictures are drawn into it (pass 1b).
  const bool withHd = hdEnabled_ && hd && (hd->used & hdLoaded_) && hd->width == frameW_ && hd->height == frameH_;
  Target& scene = withHd ? hdScene_ : scene_;
  // (the pictures at the screen's own size, without the CRT look: drawn straight to the screen,
  // with no scene of their own for a last pass to copy -- the GPU writes the frame once)
  // (or, as ogles2 shows the screen while drawing it, to a scene of their own all the same,
  // which a pass that only copies puts on the screen faster than the display scans it)
  bool direct = false, copy = false;
  if (withHd) {
    // (no more rows than hdMaxHeight_: the GPU fills fewer, and the last pass stretches them)
    const double scale = hdMaxHeight_ > 0 && vh > hdMaxHeight_ ? static_cast<double>(hdMaxHeight_) / vh : 1.0;
    const bool full = scale == 1.0 && !crt_;
    direct = full && directToScreen_;
    copy = full && !direct;
    if (direct) {
      hdX_ = screen.x;
      hdY_ = screen.y;
      hdW_ = vw;
      hdH_ = vh;
    } else {
      ensureTarget(hdScene_, std::max(static_cast<int>(std::lround(vw * scale)), 1), std::max(static_cast<int>(std::lround(vh * scale)), 1));
      hdX_ = hdY_ = 0;
      hdW_ = hdScene_.w;
      hdH_ = hdScene_.h;
    }
  }
  if (direct) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(screen.x, screen.y, screen.w, screen.h);
    passRotation_ = rotation_;
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, scene.fbo);
    glViewport(0, 0, scene.w, scene.h);
  }
  palettePass_.use();
  glUniform1f(palettePass_.uniform("uRotate"), rotateUniform());
  glUniform1i(palettePass_.uniform("uIndices"), 0);
  glUniform1i(palettePass_.uniform("uPalette"), 1);
  glUniform2f(palettePass_.uniform("uIndexSize"), static_cast<float>(frameW_), static_cast<float>(frameH_));
  glUniform1f(palettePass_.uniform("uPaletteRows"), static_cast<float>(std::max(paletteRows_, 1)));
  // (the dots are drawn by the first pass that is at the screen's resolution: this one with the
  // replacement pictures, the last one without them)
  const int displayTop = displayRows_ <= 0 ? -1 : displayAtTop_ ? 0 : frameH_ - displayRows_;
  glUniform1f(palettePass_.uniform("uDisplayTop"), static_cast<float>(displayTop));
  glUniform1f(palettePass_.uniform("uDisplayRows"), static_cast<float>(displayRows_));
  glUniform1f(palettePass_.uniform("uDots"), withHd ? 1.0f : 0.0f);
  // (with a base picture, its pass draws the frame's colours too: drawHd)
  if (!(withHd && baseDrawn(*hd))) drawFullscreen();
  if (withHd) {
    drawHd(*hd);
    if (!hd->sprites.empty()) drawSprites(*hd);
  }

  if (direct) {
    passRotation_ = 0;
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return;
  }

  // Pass 2: present with aspect-correct letterboxing.
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, windowWidth, windowHeight);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(screen.x, screen.y, screen.w, screen.h);
  passRotation_ = rotation_;
  if (copy && !copyLoaded_) {
    copyLoaded_ = true;
    if (!copyPass_.load(shaderDir_ / "fullscreen.vert", shaderDir_ / "copy.frag"))
      log::error("the copy shader failed to load; the last pass is the usual one");
  }
  if (copy && copyPass_.id()) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene.tex);
    copyPass_.use();
    glUniform1i(copyPass_.uniform("uScene"), 0);
    glUniform1f(copyPass_.uniform("uRotate"), rotateUniform());
    passRotation_ = 0;
    drawFullscreen();
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return;
  }
  if (crt_ && !crtLoaded_) {
    crtLoaded_ = true;
    if (!crtPass_.load(shaderDir_ / "fullscreen.vert", shaderDir_ / "crt-lottes.frag")) {
      log::error("the CRT shader failed to load; showing the picture without it");
      crt_ = false;
    }
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene.tex);
  if (crt_) {
    crtPass_.use();
    glUniform1i(crtPass_.uniform("uScene"), 0);
    glUniform1f(crtPass_.uniform("uRotate"), rotateUniform());
    glUniform2f(crtPass_.uniform("uSceneSize"), static_cast<float>(scene.w), static_cast<float>(scene.h));
    glUniform2f(crtPass_.uniform("uOutputSize"), static_cast<float>(vw), static_cast<float>(vh));
    drawFullscreen();
  } else {
    postPass_.use();
    glUniform1i(postPass_.uniform("uScene"), 0);
    glUniform1f(postPass_.uniform("uRotate"), rotateUniform());
    glUniform2f(postPass_.uniform("uSceneSize"), static_cast<float>(scene.w), static_cast<float>(scene.h));
    glUniform2f(postPass_.uniform("uOutputSize"), static_cast<float>(vw), static_cast<float>(vh));
    glUniform1f(postPass_.uniform("uFilter"), smoothEdges_ ? 1.0f : 0.0f);
    glUniform1f(postPass_.uniform("uDisplayTop"), static_cast<float>(displayTop));
    glUniform1f(postPass_.uniform("uDisplayRows"), static_cast<float>(displayRows_));
    glUniform1f(postPass_.uniform("uDots"), withHd ? 0.0f : 1.0f);
    drawFullscreen();
  }
  passRotation_ = 0;
  glDisableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// (fullscreen.vert's turns: a quarter clockwise sends the picture's x to the screen's -y, its y to
// the screen's x; anticlockwise the other way)
Rect Renderer::toScreen(const Rect& r, int w, int h) const {
  if (passRotation_ == 90) return {r.y, w - (r.x + r.w), r.h, r.w};
  if (passRotation_ == 270) return {h - (r.y + r.h), r.x, r.h, r.w};
  return r;
}

}  // namespace encore

#endif  // ENCORE_GLES2

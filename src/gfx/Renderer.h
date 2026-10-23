#pragma once
// OpenGL renderer: indexed framebuffer -> palette pass -> replacement pictures (at window
// resolution, when a screen has any) -> post-process or CRT pass -> window.
#include "gfx/Gl.h"

#include <array>
#include <filesystem>
#include <vector>

#include "gfx/Framebuffer.h"
#include "gfx/HdLayer.h"
#include "gfx/Palette.h"
#include "gfx/ShaderProgram.h"

namespace encore {

class Renderer {
 public:
  Renderer() = default;
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  /// Deletes every texture, buffer and program while the context is still there (before the
  /// window goes); the destructor then does nothing. (Gone with the context, on the Amiga these
  /// calls would reach into ogles2.library's freed memory.)
  void release();

  bool init(const std::filesystem::path& shaderDir, int frameWidth, int frameHeight, double pixelAspect);
  void resizeSource(int frameWidth, int frameHeight);
  void setPixelAspect(double aspect) { pixelAspect_ = aspect; }
  /// false draws with hard pixel edges, true softens only the edge that straddles two pixels.
  void setSmoothEdges(bool on) { smoothEdges_ = on; }
  /// The replacement pictures drawn at most this many rows high, and the scene stretched to the
  /// window after (0: at the window's own size).
  void setHdMaxHeight(int rows) { hdMaxHeight_ = rows; }
  void setHdDirect(bool on) { directToScreen_ = on; }
  /// The picture turned on the screen, for a screen turned on its side: 0, 90 (clockwise) or 270.
  void setRotation(int degrees) { rotation_ = degrees == 90 || degrees == 270 ? degrees : 0; }
  int rotation() const { return rotation_; }
  /// How many of the frame's rows are a table's dot display, its last or its first, drawn as
  /// round dots at the screen's resolution (palette.frag, post.frag); 0 for a screen without one.
  void setDisplay(int rows, bool atTop) {
    displayRows_ = rows;
    displayAtTop_ = atTop;
  }
  /// Presents the picture through the CRT-Lottes shader (shaders/crt-lottes.frag).
  void setCrt(bool on) { crt_ = on; }
  bool crt() const { return crt_; }
  /// A high-resolution replacement for one of the original pictures, RGBA.
  void setHdPicture(HdPicture p, int width, int height, const u8* rgba);
  bool hasHdPictures() const { return hdLoaded_ != 0; }
  void setHdEnabled(bool on) { hdEnabled_ = on; }
  /// A picture drawn over the scene at an angle (a flipper), RGBA; `slot` is HdSprite::picture.
  void setSpritePicture(std::size_t slot, int width, int height, const u8* rgba);
  /// What hides the ball, drawn to the table's replacement pictures (black hides it, white
  /// does not, pure red is clear plastic it is seen through greyed), for the ball on the
  /// playfield (layer 0) or on the ramps (1); `sourceWidth` and `sourceHeight` are the
  /// original playfield's size, which it covers edge to edge. Without one, the original's own
  /// dots say it.
  void setCoverPicture(int layer, int width, int height, const u8* rgba, int sourceWidth, int sourceHeight);
  void clearCoverPictures();
  void clearSpritePictures();
  bool hdEnabled() const { return hdEnabled_; }


  /// Uploads a single palette shared by every scanline.
  void setPalette(const Palette& palette);
  /// Uploads one palette per scanline: `rows` blocks of 256 colours.
  void setRowPalettes(const Rgb* colors, int rows);

  /// Draws one frame into a window of `windowWidth` x `windowHeight` drawable pixels.
  /// `hd`: where the frame drew original pictures that have replacements.
  void draw(const Framebuffer& frame, int windowWidth, int windowHeight, const HdFrame* hd = nullptr);
  void pollShaderReload();

 private:
  struct Target {
    GLuint fbo = 0, tex = 0;
    int w = 0, h = 0;
  };
  static void ensureTarget(Target& t, int w, int h);
  static void deleteTarget(Target& t);
  void drawHd(const HdFrame& hd);
  void drawSprites(const HdFrame& hd);
  GLuint vao_ = 0;
  GLuint indexTex_ = 0;
  GLuint paletteTex_ = 0;
  Target scene_;
  Target hdScene_;   ///< window-sized scene for frames with replacement pictures
  int hdX_ = 0, hdY_ = 0, hdW_ = 0, hdH_ = 0;  ///< where those pictures are drawn: hdScene_, or the screen itself
  int paletteRows_ = 0;
  int hdMaxHeight_ = 0;
  std::vector<Rgb> sentPalette_;  ///< the palette last sent, so an unchanged one is not sent again
  int frameW_ = 0, frameH_ = 0;
  double pixelAspect_ = 1.0;
  bool smoothEdges_ = true;
  int displayRows_ = 0;
  bool displayAtTop_ = false;
  Rect viewport_;    ///< the picture's place, in the screen's turned (--rotate) pixels
  int rotation_ = 0;
  int passRotation_ = 0;  ///< the rotation of the pass being drawn: rotation_ on the screen, 0 into a scene
  float rotateUniform() const { return passRotation_ == 90 ? 1.0f : passRotation_ == 270 ? -1.0f : 0.0f; }
  /// A rectangle of the turned picture (w x h of it) as the framebuffer has it.
  Rect toScreen(const Rect& r, int w, int h) const;
  ShaderProgram palettePass_;
  ShaderProgram postPass_;
  ShaderProgram copyPass_;  ///< the last pass at the scene's own size (copy.frag)
  bool copyLoaded_ = false;
  bool directToScreen_ = false;  ///< --hd-direct: the pictures drawn straight to the screen, no copy (tears on ogles2)
  ShaderProgram crtPass_;
  bool crt_ = false;
  bool crtLoaded_ = false;
  ShaderProgram hdPass_;
  bool hdPassLoaded_ = false;
  bool hdEnabled_ = true;
  u32 hdLoaded_ = 0;  ///< bit per picture with a replacement
  std::array<GLuint, HdFrame::kCount> hdTex_{};
  GLuint hdMapTex_ = 0;
  int hdMapW_ = 0, hdMapH_ = 0;
  static constexpr std::size_t kSprites = 4;
  ShaderProgram spritePass_;
  bool spritePassLoaded_ = false;
  std::array<GLuint, kSprites> spriteTex_{};
  std::array<std::array<int, 2>, kSprites> spriteSize_{};
  std::array<GLuint, 2> coverTex_{};
  std::array<bool, 2> coverLoaded_{};
  std::array<std::array<int, 2>, 2> coverSource_{};
  std::filesystem::path shaderDir_;
#ifdef ENCORE_GLES2
  // (RendererGles2.cpp: vertex buffers instead of gl_VertexID, and the pictures' own sizes for
  // choosing their mipmaps, which GLSL ES 1.00 cannot ask a texture)
  void drawFullscreen();
  /// The frame's base picture is there to be drawn first, with the frame's colours around it.
  bool baseDrawn(const HdFrame& hd) const;
  GLuint quadVbo_ = 0, boxVbo_ = 0;
  // The frame's indices and the replacement-picture map go to a texture of three in turn: the
  // one written this frame is not one the GPU may still be reading for the last, so sending it
  // does not wait for the GPU (ogles2 on the PiStorm did, 5 ms a frame).
  static constexpr int kRing = 3;
  std::array<GLuint, kRing> indexRing_{}, mapRing_{};
  std::array<std::array<int, 2>, kRing> indexRingSize_{}, mapRingSize_{};
  int ring_ = 0;
  GLuint litTex_ = 0;            ///< HdFrame::litPlane, as last sent
  u32 litVersionSent_ = 0;
  int litW_ = 0, litH_ = 0;
  std::array<std::array<int, 2>, HdFrame::kCount> hdTexSize_{};
  std::array<bool, HdFrame::kCount> hdFlat_{};  ///< filtered LINEAR, without its mipmaps (setFlat)
  void setFlat(std::size_t i, bool flat);
#endif
};

}  // namespace encore

# Pinball Fantasies: Encore! for AmigaOS 3 (PiStorm / Emu68)

An Amiga port of [Pinball Fantasies: Encore!](https://github.com/pedrocatalao/pinball-fantasies-encore)
by Pedro Catalao (GPL-3; HD artwork CC BY-SA 4.0). The engine runs as it is; the picture is drawn
by the Raspberry Pi's GPU through [ogles2.library](https://github.com/ladislawv/ogles2) (OpenGL ES 2.0
on the PiStorm's V3D), with SDL3_68k for the window, input and AHI sound.

Version 0.9 beta. The game's own files are not part of this: see the release's README.

## What the port adds

- `src/gfx/RendererGles2.cpp`, `shaders/gles2/`: the renderer on OpenGL ES 2.0 (GLSL ES 1.00):
  indices four to a texel, the HD map as two texels a pixel, a ring of upload textures, the base
  picture drawn with the frame's colours, boxes scissored, the HD scene capped in height
  (`--hd-height`) or drawn straight to the screen (`--hd-direct`), the picture turned for a screen
  on its side (`--rotate`, Help).
- `src/core/Mutex.h`, `src/core/Thread.h`: exec semaphores; threads run at once on the Amiga.
- `src/platform/amiga/AmigaHttp.c`: HTTP(S) on bsdsocket and AmiSSL 5.
- `amiga/compat/`: errno, std::filesystem and std::chrono for GCC 16 rc18's libstdc++ on libnix.
- `amiga/sdl3/`: the SDL3_68k this is built with, as patches on Steffen Haeuser's SDL3_68k.

## Building

GCC 16 rc18 (`/opt/amiga-gcc16-rc18`, `-mcrt=nix20`, C++20), the ogles2 SDK 1.3 and the MiniGL client
(PiStorm3D's MiniGL_Classic_v29.1) for SDL3's GL path:

    amiga/sdl3/fetch-sdl3.sh          # SDL3_68k at c82dea0 + amiga/sdl3/patches
    make -C amiga sdl3                # third_party/SDL3_68k/SDL3/build/libnix/libSDL3.a
    make -C amiga                     # amiga/PinballFantasies

`OGLES2_SDK`, `MINIGL_SDK` and `TOOLCHAIN` in `amiga/Makefile` point at the SDKs. The release is
built with `-DENCORE_SOUND_RATE=48000` (48 kHz mixing, as on the PC).

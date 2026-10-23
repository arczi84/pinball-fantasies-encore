#!/bin/sh
# Fetches SDL3_68k (Steffen Haeuser, github.com/SteffenHaeuser/SDL3_68k, zlib licence) at the
# commit the Amiga port is built on, into third_party/SDL3_68k, and applies the port's patches:
#   0001  nfs3recompiled's local changes (GCC 16 build, counter scale, dummy audio, thread priorities, events)
#   0002  the ogles2 project's GLES2 backend (ogles2 ports/sdl3, ladislawv/ogles2)
#   0003  fullscreen GLES2 on a screen of ogles2's own (OGLES2_CCT_MODEID)
#   0004  leaving that fullscreen: ogles2's window, its IDCMP and port left to ogles2
#   0005  atomics under Forbid()/Permit(), spinlock waits sleep a tick (as ogles2's SDL2 fix)
# Then: make -C amiga sdl3
set -e
here=$(cd "$(dirname "$0")" && pwd)
dest="$here/../../third_party/SDL3_68k"
if [ -e "$dest" ]; then
  echo "$dest exists already" >&2
  exit 1
fi
git clone https://github.com/SteffenHaeuser/SDL3_68k.git "$dest"
cd "$dest"
git checkout -q c82dea0
git am --keep-cr "$here"/patches/000*.patch  # (Makefile.amigaos3 has CRLF lines)

# Pinball Fantasies: Encore! for AmigaOS 3 on a PiStorm (Emu68, Raspberry Pi 4): the engine as
# it is, SDL3_68k with the ogles2 backend (third_party/SDL3_68k: nfs3recompiled's SDL3 plus
# ogles2's ports/sdl3), and the renderer on OpenGL ES 2.0 (src/gfx/RendererGles2.cpp,
# shaders/gles2) through ogles2.library.
#
#   make sdl3       -> third_party/SDL3_68k/SDL3/build/libnix/libSDL3.a
#   make            -> PinballFantasies
#   make dist       -> dist/PinballFantasies/ (program, shaders, font, HD pictures)
#
# GCC 16 (C++20), libnix (-mcrt=nix20): with GCC 16 never -noixemul.

TOOLCHAIN  ?= /opt/amiga-gcc16-rc18
OGLES2_SDK ?= /mnt/d/dev/wipeout/ext/ogles2-sdk/ogles2-sdk-1.3
MINIGL_SDK ?= /mnt/d/dev/Pistorm3D/MiniGL_Classic_v29.1/libraryroot

ROOT   := ..
SDL3   := $(ROOT)/third_party/SDL3_68k/SDL3
BUILD  := build
TARGET := PinballFantasies
.DEFAULT_GOAL := all

CXX := $(TOOLCHAIN)/bin/m68k-amigaos-g++
CC  := $(TOOLCHAIN)/bin/m68k-amigaos-gcc
AR  := $(TOOLCHAIN)/bin/m68k-amigaos-ar

CPU      := -m68060 -mhard-float
CPPFLAGS := -DENCORE_GLES2 -DENCORE_VERSION=\"\" -DENCORE_SOUND_RATE=24000 -D__AMIGAOS3__ \
	-I$(ROOT)/src -I$(SDL3)/include -I$(OGLES2_SDK)/include -I$(MINIGL_SDK)/include
CXXFLAGS := -std=c++20 -O2 $(CPU) -mcrt=nix20 -fbbb=- -fomit-frame-pointer -fno-strict-aliasing \
	-Wall -Wno-unused-parameter -MMD -MP
LDFLAGS  := $(CPU) -mcrt=nix20 -Wl,-u,___StackSwap -Wl,-Map,$(BUILD)/$(TARGET).map
LDLIBS   := $(BUILD)/errno_shim.o $(BUILD)/fs_amiga.o $(BUILD)/chrono_amiga.o -L$(BUILD)/libfix -L$(SDL3)/build/libnix -lSDL3 -L$(OGLES2_SDK)/lib -logles2 $(BUILD)/libminigl_client.a \
	-latomic -lm -ldebug -lamiga

SOURCES := \
	$(wildcard $(ROOT)/src/core/*.cpp) \
	$(wildcard $(ROOT)/src/data/*.cpp) \
	$(wildcard $(ROOT)/src/engine/*/*.cpp) \
	$(ROOT)/src/gfx/Framebuffer.cpp $(ROOT)/src/gfx/Palette.cpp \
	$(ROOT)/src/gfx/Gl.cpp $(ROOT)/src/gfx/RendererGles2.cpp $(ROOT)/src/gfx/ShaderProgram.cpp \
	$(ROOT)/src/game/App.cpp $(ROOT)/src/game/Art.cpp $(ROOT)/src/game/Config.cpp $(ROOT)/src/game/Fantasy.cpp \
	$(ROOT)/src/game/Online.cpp $(ROOT)/src/game/Release.cpp \
	$(ROOT)/src/platform/AudioDevice.cpp $(ROOT)/src/platform/DataLocator.cpp $(ROOT)/src/platform/Http.cpp \
	$(ROOT)/src/platform/ImageFile.cpp $(ROOT)/src/platform/SystemKeys.cpp $(ROOT)/src/platform/Window.cpp \
	$(ROOT)/src/main.cpp
C_SOURCES := $(ROOT)/src/platform/amiga/AmigaHttp.c
OBJECTS := $(patsubst $(ROOT)/src/%.cpp,$(BUILD)/%.o,$(SOURCES)) $(patsubst $(ROOT)/src/%.c,$(BUILD)/%.o,$(C_SOURCES))

# GCC 16 rc18's libstdc++ calls errno as newlib has it, a function (__errno()); libnix's is a
# variable, so those calls land in libnix's data (an illegal instruction at the first
# std::filesystem::exists). A copy of libstdc++ calls compat/errno_shim.c's function instead,
# and is found first through -L.
LIBSTDCXX := $(TOOLCHAIN)/lib/gcc/m68k-amigaos/16/libm020/libm881/libstdc++.a
$(BUILD)/libfix/libstdc++.a: $(LIBSTDCXX)
	@mkdir -p $(dir $@)
	$(TOOLCHAIN)/bin/m68k-amigaos-objcopy --redefine-sym ___errno=___encore_errno $< $@
# std::chrono's clocks on timer.device: libstdc++'s count whole seconds (time()).
$(BUILD)/chrono_amiga.o: compat/chrono_amiga.cpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++20 -O2 $(CPU) -mcrt=nix20 -fbbb=- -Wall -c $< -o $@

# std::filesystem's operations on AmigaDOS: libstdc++'s own are newlib's and mostly missing.
$(BUILD)/fs_amiga.o: compat/fs_amiga.cpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++20 -O2 $(CPU) -mcrt=nix20 -fbbb=- -Wall -c $< -o $@
$(BUILD)/errno_shim.o: compat/errno_shim.c
	@mkdir -p $(dir $@)
	$(CC) -O2 $(CPU) -mcrt=nix20 -c $< -o $@

# SDL3's MiniGL path (the GL library for anything but ES 2) needs minigl.library's client.
MINIGL_OBJECTS := $(BUILD)/minigl/minigl_base.o $(BUILD)/minigl/minigl_open.o $(BUILD)/minigl/minigl_dispatch_stub_gcc.o

.PHONY: all sdl3 dist clean
all: $(TARGET)

$(TARGET): $(OBJECTS) $(BUILD)/libminigl_client.a $(SDL3)/build/libnix/libSDL3.a $(BUILD)/libfix/libstdc++.a $(BUILD)/errno_shim.o $(BUILD)/fs_amiga.o $(BUILD)/chrono_amiga.o
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
	@! grep -qE "libstdc\+\+\.a\((fs_ops|fs_dir|cow-fs_ops|cow-fs_dir|chrono)\.o\)" $(BUILD)/$(TARGET).map || \
		{ echo "libstdc++'s fs_ops.o/fs_dir.o/chrono.o were linked: compat/fs_amiga.cpp or chrono_amiga.cpp lacks a function"; rm -f $@; false; }

$(BUILD)/%.o: $(ROOT)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: $(ROOT)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=gnu99 -O2 $(CPU) -mcrt=nix20 -fbbb=- -I$(ROOT)/src -Wall -MMD -MP -c $< -o $@

$(BUILD)/minigl/%.o: $(MINIGL_SDK)/client/%.c
	@mkdir -p $(dir $@)
	$(CC) -O2 $(CPU) -mcrt=nix20 -I$(MINIGL_SDK)/include -c $< -o $@
$(BUILD)/minigl/%.o: $(MINIGL_SDK)/client/%.s
	@mkdir -p $(dir $@)
	$(CC) -mcpu=68040 -c $< -o $@
$(BUILD)/libminigl_client.a: $(MINIGL_OBJECTS)
	$(AR) rcs $@ $^

# The same flags nfs3recompiled builds its SDL3 with (tools/build-sdl3-amiga.sh), on GCC 16
# rc18's libnix (-mcrt=nix20), with ogles2's and MiniGL's headers for the GL backends.
sdl3:
	$(MAKE) -C $(SDL3) -f Makefile.amigaos3 -j8 build/libnix/libSDL3.a \
		CC=$(TOOLCHAIN)/bin/m68k-amigaos-gcc AR=$(TOOLCHAIN)/bin/m68k-amigaos-ar RANLIB=$(TOOLCHAIN)/bin/m68k-amigaos-ranlib \
		"COMMON=-std=gnu99 -O2 -mcpu=68040 -mhard-float -fbbb=- -Wall -Wextra -Wno-unused-parameter -I./amiga-port-include -I./include -I./include/build_config -I./include/SDL3 -I./src -D__AMIGAOS3__" \
		"LIBNIX=\$$(COMMON) -mcrt=nix20 -DNOIXEMUL -I$(OGLES2_SDK)/include -I$(MINIGL_SDK)/include"

dist: $(TARGET)
	rm -rf dist/$(TARGET)
	mkdir -p dist/$(TARGET)/shaders/gles2
	cp $(TARGET) dist/$(TARGET)/
	cp $(ROOT)/shaders/gles2/* dist/$(TARGET)/shaders/gles2/
	cp $(ROOT)/assets/hd/font.png dist/$(TARGET)/

clean:
	rm -rf $(BUILD) $(TARGET) dist

-include $(OBJECTS:.o=.d)

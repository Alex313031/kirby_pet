# Shared make settings for kirby_pet's three targets. Each of src/lib, src/app
# and src/scr has its own Makefile that includes this, so any of them can be
# built on its own (`make -C src/scr`) and the root Makefile just drives all
# three in dependency order - the same shape as the BUILD.gn files.
#
# Toolchain, build type, defines and the pattern rules live here; each
# subdirectory adds its own include dirs, defines, libs and link line.

# Build type: choose Debug or Release
# Default is Debug
BUILDTYPE ?= Debug

# Directories. Everything is anchored on this file's own location, so the
# paths are right whether make was started from the project root or inside a
# subdirectory.
SRC_ROOT     := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
PROJECT_ROOT := $(abspath $(SRC_ROOT)/..)
LIB_DIR      := $(SRC_ROOT)/lib
LOGGING_DIR  := $(SRC_ROOT)/logging

# Where the .exe and .scr land. Default is the project root, like every other
# project here; override with e.g. `make OUT_DIR=release` to feed the .iss.
# A relative path is taken from the PROJECT ROOT, never from the subdirectory
# make happens to be running in: the sub-makes the root spawns inherit the
# variable on their command line and run inside src/app and src/scr, where a
# bare `release` would otherwise point somewhere that doesn't exist. Absolute
# paths (leading /, or a drive letter on Windows) are left alone. `override`
# because a command-line variable would ignore a plain assignment.
OUT_DIR ?= $(PROJECT_ROOT)
ifeq ($(filter /%,$(OUT_DIR))$(findstring :,$(OUT_DIR)),)
override OUT_DIR := $(PROJECT_ROOT)/$(OUT_DIR)
endif

# Oldest Windows the binaries will load on, like the GN winver argument.
# `make WINVER_TARGET=nt4` builds for NT 4: subsystem 4.00 and _WIN32_WINNT
# 0x0400, which is the only way the NT4 fallbacks in the code can ever run -
# NT4's loader refuses an image whose subsystem version is 5.0. It also
# switches to the i586 MinGW toolchain, as the GN build does: its runtime only
# calls what NT4's system DLLs have, and the code is held to a Pentium with
# x87 floating point - NT4 never saves SSE state across a context switch, so
# SSE code is not merely slow there but wrong. Anything else (the default) is
# Windows 2000 on the i686 toolchain, as before. Switching needs a
# `make clean`: the defines and the arch are baked into every object.
WINVER_TARGET ?= 2000
ifneq ($(filter nt4 NT4 4.0,$(WINVER_TARGET)),)
MINGW_ARCH  := i586
WIN32_WINNT := 0x0400
WIN32_IE    := 0x0400
SUBSYSTEM   := 4.00
SIMDFLAGS   := -march=i586 -mfpmath=387 -mno-mmx -mno-fxsr -mno-sse -mno-sse2
else
MINGW_ARCH  := i686
WIN32_WINNT := 0x0500
WIN32_IE    := 0x0501
SUBSYSTEM   := 5.00
SIMDFLAGS   := -mfpmath=sse -mfxsr -msse
endif

# Compiler and tools. Taken from gn-legacy's own tools/ tree when this checkout
# lives inside it (../../tools/mingw/<host>/<arch>/bin, the same binaries GN
# runs), otherwise from PATH, so a standalone checkout builds with whichever
# <arch>-w64-mingw32 toolchain is installed.
MINGW_HOST  := $(if $(filter Windows_NT,$(OS)),win,linux)
MINGW_BIN   := $(wildcard $(PROJECT_ROOT)/../../tools/mingw/$(MINGW_HOST)/$(MINGW_ARCH)/bin)
TOOL_DIR    := $(if $(MINGW_BIN),$(MINGW_BIN)/,)
TOOL_PREFIX := $(TOOL_DIR)$(MINGW_ARCH)-w64-mingw32-
CC       := $(TOOL_PREFIX)gcc
CXX      := $(TOOL_PREFIX)g++
LD       := $(TOOL_PREFIX)g++
AR       := $(TOOL_PREFIX)ar
DLLTOOL  := $(TOOL_PREFIX)dlltool
GENDEF   := $(TOOL_DIR)gendef
RC       := $(TOOL_PREFIX)windres

# Compiler flags #
DEFINES  := -DUNICODE -D_UNICODE -D_WINDOWS -DWINVER=$(WIN32_WINNT) -D_WIN32_WINNT=$(WIN32_WINNT) -D_WIN64_WINNT=0x0502 -D_WIN32_IE=$(WIN32_IE)
# Show all errors, compile everything static, ensure relocatable data.
# -municode is NOT here: the app wants it (wWinMain), the screensaver must not
# have it (scrnsave.lib supplies a narrow WinMain). Each adds its own.
CFLAGS   := $(DEFINES) -Wall -Wunused-function -static-libgcc -finput-charset=UTF-8 -fexec-charset=UTF-8 -MMD -MP

# Compiler optimization and architecture flags (SIMDFLAGS follows
# WINVER_TARGET above)
ifeq ($(BUILDTYPE), Release)
CFLAGS   += -O3 -g0 -s $(SIMDFLAGS) -DNDEBUG -D_NDEBUG
endif
ifeq ($(BUILDTYPE), Debug)
CFLAGS   += -Og -g $(SIMDFLAGS) -DDEBUG -D_DEBUG
endif

# C++ only flags
CXXFLAGS := $(CFLAGS) -std=c++17 -static-libstdc++

# windres gets the same defines (the .rc files pick FILEFLAGS off _DEBUG) and
# writes a .d next to the .res so a header edit rebuilds the resources.
RCFLAGS  := $(DEFINES) --preprocessor-arg=-MMD --preprocessor-arg=-MP

# System libraries every target links
LIBS     := -lkernel32 -luser32 -lgdi32 -lshell32 -lcomctl32 -lcomdlg32 -ladvapi32

# Pin the default goal: the dependency -includes in each Makefile would
# otherwise make the first .d rule the default target.
.DEFAULT_GOAL := all

# The output directory must exist before anything links into it. Targets list
# it as an order-only prerequisite (after a |), so its timestamp changing as
# files land in it never triggers a relink.
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# Compilation Rules #
# Compile C sources
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -static -c $< -o $@

# Compile C++ sources
%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -static -c $< -o $@

# Compile .rc -> .res (COFF object). Run from the .rc's own directory so the
# icon / manifest / ../bmp paths inside it resolve.
%.res: %.rc
	cd $(dir $<) && $(RC) $(RCFLAGS) $(INCLUDE_DIRS) --preprocessor-arg=-MT --preprocessor-arg=$@ --preprocessor-arg=-MF --preprocessor-arg=$(abspath $(@:.res=.d)) $(notdir $<) -O coff $(abspath $@)

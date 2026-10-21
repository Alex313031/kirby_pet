# Shared make settings for kirby_pet's three targets. Each of src/lib, src/app
# and src/scr has its own Makefile that includes this, so any of them can be
# built on its own (`make -C src/scr`) and the root Makefile just drives all
# three in dependency order - the same shape as the BUILD.gn files.
#
# Toolchain, build type, defines and the pattern rules live here; each
# subdirectory adds its own include dirs, defines, libs and link line.

# Compiler and tools
CC       := i686-w64-mingw32-gcc
CXX      := i686-w64-mingw32-g++
LD       := i686-w64-mingw32-g++
AR       := i686-w64-mingw32-ar
DLLTOOL  := i686-w64-mingw32-dlltool
GENDEF   := gendef
RC       := i686-w64-mingw32-windres

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
OUT_DIR ?= $(PROJECT_ROOT)

# Compiler flags #
DEFINES  := -DUNICODE -D_UNICODE -D_WINDOWS -DWINVER=0x0500 -D_WIN32_WINNT=0x0500 -D_WIN64_WINNT=0x0502 -D_WIN32_IE=0x501
# Show all errors, compile everything static, ensure relocatable data.
# -municode is NOT here: the app wants it (wWinMain), the screensaver must not
# have it (scrnsave.lib supplies a narrow WinMain). Each adds its own.
CFLAGS   := $(DEFINES) -Wall -Wunused-function -static-libgcc -finput-charset=UTF-8 -fexec-charset=UTF-8 -MMD -MP

# March flags
SIMDFLAGS = -mfpmath=sse -mfxsr -msse

# Compiler optimization and architecture flags
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

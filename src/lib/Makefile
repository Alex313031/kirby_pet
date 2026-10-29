# libpet_lib.a / pet_lib.dll MAKEFILE for compiling under MinGW32
# The library the app and the screensaver link: sprites, alpha regions,
# vacuum movement, erasing, registry. Buildable on its own. Built twice from
# the same sources, as in BUILD.gn: the static libpet_lib.a (what the app and
# the screensaver link) and pet_lib.dll with its libpet_lib.dll.a import
# library, for anything that wants the lib shared.

include ../common.mk

# Targets, named as the BUILD.gn ones: pet_lib_static and pet_lib_shared
NAME     := pet_lib_static
TARGET   := libpet_lib.a           # static lib, stays here next to the sources
DLL      := $(OUT_DIR)/pet_lib.dll # shared lib, lands with the .exe / .scr
IMPLIB   := libpet_lib.dll.a       # import library for linking the .dll
DEF      := pet_lib.def

# Automatically discover source files
HEADERS  := $(wildcard *.h include/*.h)
SRC_CPP  := $(wildcard *.cc)
SRC_RC   := $(wildcard *.rc)

# Objects. The .dll gets its own set (.dll.o / .dll.res): same sources,
# compiled with PET_LIB_SHARED so LIB_API exports and the .rc stamps VFT_DLL.
OBJ_CPP     := $(SRC_CPP:.cc=.o)
OBJ_RC      := $(SRC_RC:.rc=.res)
OBJ_DLL_CPP := $(SRC_CPP:.cc=.dll.o)
OBJ_DLL_RC  := $(SRC_RC:.rc=.dll.res)

# Project-specific include dirs
INCLUDE_DIRS := -I. -I./include
# Same private define the BUILD.gn config sets
CFLAGS   += -DPET_LIB_
CXXFLAGS += -DPET_LIB_
# ... and, for the .dll objects only, the define that selects the shared build
DLLDEFS  := -DPET_LIB_SHARED

# Include generated dependency files
-include $(OBJ_CPP:.o=.d)
-include $(OBJ_RC:.res=.d)
-include $(OBJ_DLL_CPP:.o=.d)
-include $(OBJ_DLL_RC:.res=.d)

# Build Commands #
all: $(NAME) pet_lib_shared

$(NAME): $(TARGET)

pet_lib_shared: $(DLL)

# The .a - the VERSIONINFO .res rides along for MSVC/DLL parity, as in GN.
$(TARGET): $(OBJ_CPP) $(OBJ_RC)
	$(AR) rcs $(TARGET) $(OBJ_CPP) $(OBJ_RC)

# The .dll objects: the common.mk rules plus DLLDEFS. windres gets PET_LIB_
# here too (the plain .res rule leaves it out) so lib_export.h can derive
# PET_LIB_DLL for the VFT_DLL file type.
%.dll.o: %.cc
	$(CXX) $(CXXFLAGS) $(DLLDEFS) $(INCLUDE_DIRS) -static -c $< -o $@

%.dll.res: %.rc
	cd $(dir $<) && $(RC) $(RCFLAGS) -DPET_LIB_ $(DLLDEFS) $(INCLUDE_DIRS) --preprocessor-arg=-MT --preprocessor-arg=$@ --preprocessor-arg=-MF --preprocessor-arg=$(abspath $(@:.res=.d)) $(notdir $<) -O coff $(abspath $@)

# The .dll: linked -static like the executables, so the CRT is inside it and
# it carries no MinGW runtime dependency. The .def names the exports (with
# DllGetVersion undecorated, as the shell expects); --enable-stdcall-fixup
# lets ld match that to the decorated symbol quietly.
$(DLL): $(OBJ_DLL_CPP) $(OBJ_DLL_RC) $(DEF) | $(OUT_DIR)
	$(LD) -shared -static $(OBJ_DLL_CPP) $(OBJ_DLL_RC) $(DEF) -Wl,--enable-stdcall-fixup -Wl,--out-implib,$(IMPLIB) $(LIBS) -o $(DLL)

# Cleaning Rules #
clean:
	rm -f -v $(OBJ_CPP) $(OBJ_RC) $(OBJ_CPP:.o=.d) $(OBJ_RC:.res=.d) $(TARGET)
	rm -f -v $(OBJ_DLL_CPP) $(OBJ_DLL_RC) $(OBJ_DLL_CPP:.o=.d) $(OBJ_DLL_RC:.res=.d) $(IMPLIB) $(DLL)

# Testing rules #
test:
	$(AR) t $(TARGET)
	ls -la $(DLL) $(IMPLIB)

# PHONY targets for build deps tracking
.PHONY: all $(NAME) pet_lib_shared clean test

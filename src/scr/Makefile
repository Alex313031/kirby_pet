# kirby_scr.scr MAKEFILE for compiling under MinGW32
# The screensaver. Depends on ../lib only (libpet_lib.a, or pet_lib.dll with
# USE_SHARED_PET_LIB=1 - see ../common.mk), as the BUILD.gn does. Buildable
# on its own: a missing or stale library is built here via its own Makefile.

include ../common.mk

# Targets
NAME     := kirby_scr_win32
TARGET   := $(OUT_DIR)/kirby_scr.scr

# Automatically discover source files
HEADERS  := $(wildcard *.h)
SRC_C    := $(wildcard *.c)
SRC_CPP  := $(wildcard *.cc)
SRC_RC   := $(wildcard *.rc)

# Objects
OBJ_C    := $(SRC_C:.c=.o)
OBJ_CPP  := $(SRC_CPP:.cc=.o)
OBJ_RC   := $(SRC_RC:.rc=.res)

# The library and everything it is built from (see ../app/Makefile for why
# the sources, not a phony target, are the prerequisites). PET_LIB_FILE is
# the .a, or the .dll's import library (../common.mk picks).
LIB_FILE := $(PET_LIB_FILE)
LIB_SRCS := $(wildcard $(LIB_DIR)/*.cc $(LIB_DIR)/*.h $(LIB_DIR)/include/*.h $(LIB_DIR)/*.rc $(LIB_DIR)/*.def $(LIB_DIR)/Makefile)

# Project-specific include dirs
INCLUDE_DIRS := -I. -I$(LIB_DIR)/include
# Same define the BUILD.gn config sets, plus the lib's static/shared choice
CFLAGS   += -DPET_SCR $(PET_LIB_DEFS)
CXXFLAGS += -DPET_SCR $(PET_LIB_DEFS)
# No -municode here: scrnsave.lib supplies a narrow WinMain, and -municode
# would make the CRT's startup look for a wWinMain instead. UNICODE/_UNICODE
# still come from DEFINES, so ScreenSaverProc resolves to ScreenSaverProcW.
# Linker flags. SUBSYSTEM follows WINVER_TARGET (see ../common.mk).
LDFLAGS  := -L$(LIB_DIR) -Wl,--subsystem,windows:$(SUBSYSTEM)
# PlaySound (the vacuum loop)
LIBS     += -lwinmm

# Include generated dependency files
-include $(OBJ_C:.o=.d)
-include $(OBJ_CPP:.o=.d)
-include $(OBJ_RC:.res=.d)

# Build Commands #
all: $(NAME)

$(NAME): $(TARGET)

# The .scr - scrnsavw (the UNICODE scrnsave) owns WinMain and the /s /c /p
# protocol; it must come after our objects, which define the procs it calls.
# The library goes in by path rather than -lpet_lib: under -static, ld would
# skip the .dll's import library and silently take the .a instead.
$(TARGET): $(OBJ_C) $(OBJ_CPP) $(OBJ_RC) $(LIB_FILE) | $(OUT_DIR)
	$(LD) $(LDFLAGS) -static $(OBJ_C) $(OBJ_CPP) $(OBJ_RC) -lscrnsavw $(LIB_FILE) $(LIBS) -o $(TARGET)

# The library, via its own Makefile (which builds both the .a and the .dll)
$(LIB_FILE): $(LIB_SRCS)
	$(MAKE) -C $(LIB_DIR)

# Cleaning Rules #
clean:
	rm -f -v $(OBJ_C) $(OBJ_CPP) $(OBJ_RC) $(OBJ_C:.o=.d) $(OBJ_CPP:.o=.d) $(OBJ_RC:.res=.d) $(TARGET)

# Testing rules #
test:
	ls -la $(TARGET)

# PHONY targets for build deps tracking
.PHONY: all $(NAME) clean test

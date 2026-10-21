# kirby_scr.scr MAKEFILE for compiling under MinGW32
# The screensaver. Depends on ../lib (libpet_lib.a) only, as the BUILD.gn
# does. Buildable on its own: a missing or stale library is built here via
# its own Makefile.

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
# the sources, not a phony target, are the prerequisites).
LIB_A    := $(LIB_DIR)/libpet_lib.a
LIB_SRCS := $(wildcard $(LIB_DIR)/*.cc $(LIB_DIR)/*.h $(LIB_DIR)/include/*.h $(LIB_DIR)/*.rc $(LIB_DIR)/Makefile)

# Project-specific include dirs
INCLUDE_DIRS := -I. -I$(LIB_DIR)/include
# Same define the BUILD.gn config sets
CFLAGS   += -DPET_SCR
CXXFLAGS += -DPET_SCR
# No -municode here: scrnsave.lib supplies a narrow WinMain, and -municode
# would make the CRT's startup look for a wWinMain instead. UNICODE/_UNICODE
# still come from DEFINES, so ScreenSaverProc resolves to ScreenSaverProcW.
# Linker flags
LDFLAGS  := -L$(LIB_DIR) -Wl,--subsystem,windows:5.00

# Include generated dependency files
-include $(OBJ_C:.o=.d)
-include $(OBJ_CPP:.o=.d)
-include $(OBJ_RC:.res=.d)

# Build Commands #
all: $(NAME)

$(NAME): $(TARGET)

# The .scr - scrnsavw (the UNICODE scrnsave) owns WinMain and the /s /c /p
# protocol; it must come after our objects, which define the procs it calls.
$(TARGET): $(OBJ_C) $(OBJ_CPP) $(OBJ_RC) $(LIB_A) | $(OUT_DIR)
	$(LD) $(LDFLAGS) -static $(OBJ_C) $(OBJ_CPP) $(OBJ_RC) -lscrnsavw -lpet_lib $(LIBS) -o $(TARGET)

# The library, via its own Makefile
$(LIB_A): $(LIB_SRCS)
	$(MAKE) -C $(LIB_DIR)

# Cleaning Rules #
clean:
	rm -f -v $(OBJ_C) $(OBJ_CPP) $(OBJ_RC) $(OBJ_C:.o=.d) $(OBJ_CPP:.o=.d) $(OBJ_RC:.res=.d) $(TARGET)

# Testing rules #
test:
	ls -la $(TARGET)

# PHONY targets for build deps tracking
.PHONY: all $(NAME) clean test

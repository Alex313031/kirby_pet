# kirby_pet.exe MAKEFILE for compiling under MinGW32
# The desktop pet. Depends on ../lib (libpet_lib.a, or pet_lib.dll with
# USE_SHARED_PET_LIB=1 - see ../common.mk) and compiles ../logging in
# directly, as the BUILD.gn does. Buildable on its own: a missing or stale
# library is built here via its own Makefile.

include ../common.mk

# Targets
NAME     := kirby_pet_win32
TARGET   := $(OUT_DIR)/kirby_pet.exe

# Automatically discover source files
HEADERS  := $(wildcard *.h $(LOGGING_DIR)/*.h)
SRC_C    := $(wildcard *.c)
SRC_CPP  := $(wildcard *.cc $(LOGGING_DIR)/*.cc)
SRC_RC   := $(wildcard *.rc)

# Objects
OBJ_C    := $(SRC_C:.c=.o)
OBJ_CPP  := $(SRC_CPP:.cc=.o)
OBJ_RC   := $(SRC_RC:.rc=.res)

# The library and everything it is built from. Listing its sources as the
# prerequisites (rather than a phony "always run the sub-make") means this
# Makefile only recurses when something in ../lib is actually newer than the
# library, so the .exe isn't relinked on every invocation. PET_LIB_FILE is
# the .a, or the .dll's import library (../common.mk picks).
LIB_FILE := $(PET_LIB_FILE)
LIB_SRCS := $(wildcard $(LIB_DIR)/*.cc $(LIB_DIR)/*.h $(LIB_DIR)/include/*.h $(LIB_DIR)/*.rc $(LIB_DIR)/*.def $(LIB_DIR)/Makefile)

# Project-specific include dirs
INCLUDE_DIRS := -I. -I$(LOGGING_DIR) -I$(LIB_DIR)/include
# -municode: this is a GUI windows app with a wWinMain
CFLAGS   += -municode $(PET_LIB_DEFS)
CXXFLAGS += -municode $(PET_LIB_DEFS)
# Linker flags. SUBSYSTEM follows WINVER_TARGET (see ../common.mk).
LDFLAGS  := -L$(LIB_DIR) -municode -Wl,--subsystem,windows:$(SUBSYSTEM)

# Include generated dependency files
-include $(OBJ_C:.o=.d)
-include $(OBJ_CPP:.o=.d)
-include $(OBJ_RC:.res=.d)

# Build Commands #
all: $(NAME)

$(NAME): $(TARGET)

# The .exe. The library goes in by path rather than -lpet_lib: under -static,
# ld would skip the .dll's import library and silently take the .a instead.
$(TARGET): $(OBJ_C) $(OBJ_CPP) $(OBJ_RC) $(LIB_FILE) | $(OUT_DIR)
	$(LD) $(LDFLAGS) -static $(OBJ_C) $(OBJ_CPP) $(OBJ_RC) $(LIB_FILE) $(LIBS) -o $(TARGET)

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

# kirby_pet MAKEFILE for compiling under MinGW32
#
# Drives the three sub-builds in dependency order, mirroring src/BUILD.gn:
#   src/lib  -> libpet_lib.a      (the static library the two below link)
#               pet_lib.dll       (the same lib shared, + libpet_lib.dll.a)
#   src/app  -> kirby_pet.exe     (depends on lib + logging)
#   src/scr  -> kirby_scr.scr     (depends on lib)
# Each subdirectory's Makefile also works on its own (`make -C src/scr`) and
# builds the library itself if it is missing or stale. Shared settings live in
# src/common.mk. `make BUILDTYPE=Release` and `make OUT_DIR=release` pass
# through to all three.

# Pulls in BUILDTYPE / OUT_DIR so `clean` knows where the outputs went.
include src/common.mk

# Targets
NAME     := kirby_pet_win32

# Subdirectories, in dependency order
LIB_SUBDIR := src/lib
APP_SUBDIR := src/app
SCR_SUBDIR := src/scr

# Build Commands #
all: $(NAME)

$(NAME): lib app scr

# The library first; the app and the screensaver each link it.
lib:
	$(MAKE) -C $(LIB_SUBDIR)

app: lib
	$(MAKE) -C $(APP_SUBDIR)

scr: lib
	$(MAKE) -C $(SCR_SUBDIR)

# Cleaning Rules #
clean:
	$(MAKE) -C $(APP_SUBDIR) clean
	$(MAKE) -C $(SCR_SUBDIR) clean
	$(MAKE) -C $(LIB_SUBDIR) clean

# Testing rules #
test:
	ls -la $(OUT_DIR)/kirby_pet.exe $(OUT_DIR)/kirby_scr.scr $(OUT_DIR)/pet_lib.dll $(LIB_DIR)/libpet_lib.a $(LIB_DIR)/libpet_lib.dll.a

# PHONY targets for build deps tracking
.PHONY: all $(NAME) lib app scr clean test

# libpet_lib.a MAKEFILE for compiling under MinGW32
# The static library the app and the screensaver link: sprites, alpha
# regions, vacuum movement, erasing, registry. Buildable on its own.

include ../common.mk

# Targets
NAME     := pet_lib
TARGET   := libpet_lib.a

# Automatically discover source files
HEADERS  := $(wildcard *.h include/*.h)
SRC_CPP  := $(wildcard *.cc)
SRC_RC   := $(wildcard *.rc)

# Objects
OBJ_CPP  := $(SRC_CPP:.cc=.o)
OBJ_RC   := $(SRC_RC:.rc=.res)

# Project-specific include dirs
INCLUDE_DIRS := -I. -I./include
# Same private define the BUILD.gn config sets
CFLAGS   += -DPET_LIB_
CXXFLAGS += -DPET_LIB_

# Include generated dependency files
-include $(OBJ_CPP:.o=.d)
-include $(OBJ_RC:.res=.d)

# Build Commands #
all: $(NAME)

$(NAME): $(TARGET)

# The .a - the VERSIONINFO .res rides along for MSVC/DLL parity, as in GN.
$(TARGET): $(OBJ_CPP) $(OBJ_RC)
	$(AR) rcs $(TARGET) $(OBJ_CPP) $(OBJ_RC)

# Cleaning Rules #
clean:
	rm -f -v $(OBJ_CPP) $(OBJ_RC) $(OBJ_CPP:.o=.d) $(OBJ_RC:.res=.d) $(TARGET)

# Testing rules #
test:
	$(AR) t $(TARGET)

# PHONY targets for build deps tracking
.PHONY: all $(NAME) clean test

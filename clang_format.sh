#!/bin/bash

# Copyright (c) 2026 Alex313031.

YEL='\033[1;33m' # Yellow
CYA='\033[1;96m' # Cyan
RED='\033[1;31m' # Red
GRE='\033[1;32m' # Green
c0='\033[0m' # Reset Text
bold='\033[1m' # Bold Text
underline='\033[4m' # Underline Text

# Current dir (where this script lives, not where it was invoked)
export HERE=$(cd "$(dirname "$0")" && pwd) &&

# Format file with rules
export CLANG_FORMAT_FILE=${HERE}/.clang-format &&

printf "${GRE} Clang formatting using ${CLANG_FORMAT_FILE} ${c0}\n" &&

printf "${CYA} clang-format src/logging ${c0}\n" &&
# Logging dir first
clang-format --verbose -i --style=file:${CLANG_FORMAT_FILE} ${HERE}/src/logging/{*.h,*.cc} &&
printf "${CYA} clang-format src/ ${c0}\n" &&
# Main sources
clang-format --verbose -i --style=file:${CLANG_FORMAT_FILE} ${HERE}/src/app/{*.h,*.cc} &&
clang-format --verbose -i --style=file:${CLANG_FORMAT_FILE} ${HERE}/src/scr/{*.h,*.cc} &&
# No .cc files under src/lib/include - brace expansion is not globbing, so
# {*.h,*.cc} would hand clang-format a literal "*.cc" it can't open, fail, and
# the && chain would skip src/lib entirely. Match only what's actually there.
clang-format --verbose -i --style=file:${CLANG_FORMAT_FILE} ${HERE}/src/lib/include/*.h &&
clang-format --verbose -i --style=file:${CLANG_FORMAT_FILE} ${HERE}/src/lib/{*.h,*.cc}

exit 0

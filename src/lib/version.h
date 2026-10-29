#ifndef PET_LIB_VERSION_H_
#define PET_LIB_VERSION_H_

#include "include/pet_version.h"

// clang-format off: Version DEFINES left alone

// This file is for the library's own version number and identity. The Windows
// target version, the STRINGIZE/WIDEN helpers, and the vendor strings shared
// by the lib, app, and screensaver live in include/pet_version.h.

// Adhere to semver -> semver.org
#define MAJOR_VERSION 0
#define MINOR_VERSION 0
#define BUILD_VERSION 5

#define VERSION_STRING  _VERSION(MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION) // L"0.0.2"

// Base name as a BARE token (not a "..." literal) so STRINGIZE can fold it
// into the composite literals below; see app/version.h for why.
#define INTERNAL_TOKENS pet_lib

#define FILE_DESCRIPT L"Desktop Pet Library" // File description
#define INTERNAL_NAME WIDEN(STRINGIZE(INTERNAL_TOKENS)) // L"pet_lib"
#define ORIG_FILENAME WIDEN(STRINGIZE(INTERNAL_TOKENS.lib)) // L"pet_lib.lib"
#define PRODUCT_NAME  INTERNAL_NAME // Product name

#ifndef VS_FF_RELEASE
 #define VS_FF_RELEASE 0x00000000L
#endif

// clang-format on: Done with version DEFINES

#endif // PET_LIB_VERSION_H_

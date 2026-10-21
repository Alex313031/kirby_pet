#ifndef KIRBY_SCR_VERSION_H_
#define KIRBY_SCR_VERSION_H_

#include <pet_version.h>

// clang-format off: Version DEFINES left alone

// This file is for the screensaver's own version number and its constants.
// The Windows target version, the STRINGIZE/WIDEN helpers, and the vendor
// strings shared by the lib, app, and screensaver live in pet_version.h.

// Adhere to semver -> semver.org
#define MAJOR_VERSION 0
#define MINOR_VERSION 0
#define BUILD_VERSION 2

#define VERSION_STRING  _VERSION(MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION) // L"0.0.2"

// Base name/identifier as BARE tokens (not "..." literals) so STRINGIZE can fold
// them into the composite single literals below. Never pass an existing literal
// to STRINGIZE: it escapes the inner quotes (\"), which is wrong content and
// which llvm-rc rejects outright (RC escapes a quote as "", not \").
#define APP_NAME_TOKENS Kirby Screensaver
#define INTERNAL_TOKENS kirby_scr

#define APP_NAME        WIDEN(STRINGIZE(APP_NAME_TOKENS)) // L"Kirby Screensaver"
#define MAIN_WNDCLASS   WindowsScreenSaverClass // Screensavers use a built in window class

#define FILE_DESCRIPT   L"Vacuum Cleaner Screensaver" // File description
#define INTERNAL_NAME   WIDEN(STRINGIZE(INTERNAL_TOKENS)) // L"kirby_scr"
#define ORIG_FILENAME   WIDEN(STRINGIZE(INTERNAL_TOKENS.scr)) // L"kirby_scr.scr"
#define PRODUCT_NAME    APP_NAME // Product name

#define ABOUT_TITLE     WIDEN(STRINGIZE(About APP_NAME_TOKENS)) // L"About Kirby Screensaver"
#define ABOUT_CONTENT   L"A vacuum cleaner screensaver."
#define ABOUT_VERSION   WIDEN(STRINGIZE(Version MAJOR_VERSION.MINOR_VERSION.BUILD_VERSION)) // L"Version 0.0.1"
#define ABOUT_COPYRIGHT LEGAL_COPYRIGHT

#ifndef _PACKVERSION
 #define _PACKVERSION(major,minor) MAKELONG((minor), ((major) << 8))
#endif

#ifndef VS_FF_RELEASE
 #define VS_FF_RELEASE 0x00000000L
#endif

// clang-format on: Done with version DEFINES

#endif // KIRBY_SCR_VERSION_H_

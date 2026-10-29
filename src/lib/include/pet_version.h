#ifndef PET_VERSION_H_
#define PET_VERSION_H_

// Consumers: don't import this directly, import pet_lib.h instead!

// This file is for specifying the lib's target Windows version, the string helpers, and the vendor
// strings shared by the lib, app, and screensaver. Each of those carries its own version
// number in its own version.h (lib/version.h, app/version.h, scr/version.h), which includes
// this file first.

// We need to define _UNICODE and UNICODE for TCHAR
#ifndef UNICODE
 #define UNICODE
#endif

#ifndef _UNICODE
 #define _UNICODE
#endif

#if defined(__clang__) && defined(_UNICODE)
 #pragma code_page(65001) // UTF-8
#endif

/* Including SDKDDKVer.h defines the highest available Windows platform.
   If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
   set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h. */

#include "winsdkver.h"

#ifndef _WIN32_WINNT
 #define _WIN32_WINNT 0x0500 // Windows 2000
#endif

#ifndef WINVER
 #define WINVER 0x0500 // Same as _WIN32_WINNT above
#endif

#ifndef _WIN64_WINNT
 #define _WIN64_WINNT 0x0502 // Minimum version for 64 bit, Windows Server 2003
#endif

#ifndef _WIN32_IE
 #define _WIN32_IE 0x0501 // Minimum Internet Explorer version for common controls
#endif

#if _WIN32_WINNT < 0x0601 // If we are less than Windows 7, use old ATL for safety
 #ifndef _ATL_XP_TARGETING
  #define _ATL_XP_TARGETING // For using XP-compatible ATL/MFC functions
 #endif
#endif

#include "sdkddkver.h"

// For handling DLL/static lib build defines
#include "lib_export.h"

// clang-format off: Version DEFINES left alone

// Macro to convert to string
#if !defined(_STRINGIZER_)
 #define _STRINGIZER_
 #define _STRINGIZER(in) #in
 #define STRINGIZE(in) _STRINGIZER(in)
  // Wide-string variant: L ## "x" -> L"x". Two levels so the argument expands
 // before the L## paste widens the resulting narrow literal.
 #define _WIDEN(in) L ## in
 #define WIDEN(in) _WIDEN(in)
#endif // !defined(_STRINGIZER_)

// Builds L"major.minor.build" from a binary's own numbers; each version.h
// defines its VERSION_STRING with this.
#ifndef _VERSION
 // Run stringizer above
 #define _VERSION(major,minor,build) WIDEN(STRINGIZE(major.minor.build))
#endif // _VERSION

// Vendor strings shared by every binary's VERSIONINFO
#define COMMENTS        L"https://github.com/Alex313031/kirby_pet" // Project GitHub URL
#define COMPANYNAME     L"Alex313031" // My developer name
#define TRADEMARKS      L"BSD-3" // License
#define LEGAL_COPYRIGHT L"\251 2026 Alex313031" // \251 is the © symbol

// clang-format on: Done with version DEFINES

#endif // PET_VERSION_H_

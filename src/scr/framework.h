#ifndef KIRBY_SCR_FRAMEWORK_H_
#define KIRBY_SCR_FRAMEWORK_H_

// clang-format off
#include "version.h" // Keep this at the top!

// Use std::min / std::max, instead of the Win32-specific MIN/MAX.
#ifndef NOMINMAX
 #define NOMINMAX
#endif

// Only what the screensaver's own sources reach for through this header
// (IWYU, per project). scr_main.cc includes <scrnsave.h> and <cstdlib>
// itself; anything else a single file needs, it includes itself.

// On MSVC, <windows.h> defines a small subset of NTSTATUS codes, which then
// conflict with the full set in <ntstatus.h>. WIN32_NO_STATUS suppresses those
// definitions so <ntstatus.h> can own them without redefinition errors.
// MinGW handles this automatically, so the guard is MSVC-only.
#ifdef _MSC_VER
 #define WIN32_NO_STATUS
#endif
#include <windows.h> // Main Windows header
#ifdef _MSC_VER
 #undef WIN32_NO_STATUS
#endif
#include <commctrl.h> // Common Controls: the speed trackbar in the configure dialog
#include <ntstatus.h> // Full NTSTATUS codes (e.g. STATUS_SUCCESS)

// C++ STL Headers
#include <algorithm> // std::max

#ifndef __cplusplus
 #error APP_NAME requires a C++ compiler
#endif // !__cplusplus

#if __cplusplus < 201103L
 // For old compilers without constexpr or inline
 #if !defined(constexpr) || !defined(__cpp_constexpr)
  #define constexpr const
 #endif // constexpr
 #define inline
#endif // __cplusplus < 201103L

// clang-format on

// Alias
#ifndef __FUNC__
 #define __FUNC__ __func__
#endif // !__FUNC__

// Defines for missing windowsx.h definitions, don't want to inlcude
// the heavy .h file just for this one thing.
#if !defined(GET_X_LPARAM) || !defined(GET_Y_LPARAM)
 #define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
 #define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// Convert compiler defines to usable bools
inline constexpr bool is_dcheck =
#ifdef DCHECK_ON
    true;
#else
    false;
#endif // DCHECK_ON

inline constexpr bool is_debug =
#if defined(DEBUG) || defined(_DEBUG)
    true;
#else
    false;
#endif // DEBUG || _DEBUG

#endif // KIRBY_SCR_FRAMEWORK_H_

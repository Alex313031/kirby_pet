#ifndef KIRBY_SCR_MAIN_H_
#define KIRBY_SCR_MAIN_H_

#include "framework.h"

// Screensaver tuning. One tick every kTickMs; speed = pixels moved per tick.
// The default speed is not a constant: it comes from the display table in
// pet_lib (pet::ProfileForHeight) and depends on the screen height. The speed
// range itself (pet::kSpeedMin / kSpeedMax) is the library's too.
inline constexpr UINT kTickTimerId = 1;
inline constexpr UINT kTickMs      = 25;

// Persisted settings live under HKCU\Software\<kRegSubkey>; the erase values
// are the library's (pet::EraseSettings), the sound switch is ours.
inline constexpr wchar_t kRegSubkey[]    = L"Alex313031\\Kirby Screensaver";
inline constexpr wchar_t kRegPlaySound[] = L"PlaySound";

#endif // KIRBY_SCR_MAIN_H_

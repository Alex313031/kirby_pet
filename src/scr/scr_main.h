#ifndef KIRBY_SCR_MAIN_H_
#define KIRBY_SCR_MAIN_H_

#include "framework.h"

// Screensaver tuning. One tick every kTickMs; speed = pixels moved per tick.
// The default speed is not a constant: it comes from the display table in
// pet_lib (pet::ProfileForHeight) and depends on the screen height.
inline constexpr UINT kTickTimerId = 1;
inline constexpr UINT kTickMs      = 25;
inline constexpr int kSpeedMin     = 1;
inline constexpr int kSpeedMax     = 32;

// Persisted settings: HKCU\Software\<kRegSubkey>\<value>
inline constexpr wchar_t kRegSubkey[]     = L"Alex313031\\Kirby Screensaver";
inline constexpr wchar_t kRegSpeed[]      = L"Speed";
inline constexpr wchar_t kRegEraseColor[] = L"EraseColor";
inline constexpr wchar_t kRegUseDesktop[] = L"UseDesktopColor";

#endif // KIRBY_SCR_MAIN_H_

#ifndef KIRBY_PET_CONSTANTS_H_
#define KIRBY_PET_CONSTANTS_H_

#include "framework.h"

// Color constants
#define RGB_BLACK   RGB(0, 0, 0)
#define RGB_WHITE   RGB(255, 255, 255)
#define RGB_GREY    RGB(128, 128, 128)
#define RGB_DKGREY  RGB(64, 64, 64)
#define RGB_RED     RGB(255, 0, 0)
#define RGB_GREEN   RGB(0, 255, 0)
#define RGB_BLUE    RGB(0, 0, 255)
#define RGB_YELLOW  RGB(255, 255, 0)
#define RGB_CYAN    RGB(0, 255, 255)
#define RGB_MAGENTA RGB(255, 0, 255)

// Names of the system DLLs we dynamically resolve symbols from. Centralized
// so a typo only has to be fixed once and so call sites read by intent
// (kNtDll) rather than literal ("ntdll.dll").
inline constexpr wchar_t kNtDll[]       = L"ntdll.dll";
inline constexpr wchar_t kKernel32Dll[] = L"kernel32.dll";
inline constexpr wchar_t kUser32Dll[]   = L"user32.dll";
inline constexpr wchar_t kComCtl32Dll[] = L"comctl32.dll";

// Control layout (in pixels, relative to the client area).
inline constexpr INT kPadLeft       = 14;
inline constexpr INT kPadTop        = 14;
inline constexpr INT kHGap          = 8;
inline constexpr INT kVGap          = 8;
inline constexpr INT kLabelWidth    = 60;
inline constexpr INT kControlHeight = 21;
inline constexpr INT kSliderHeight  = 28;
inline constexpr INT kButtonWidth   = 110;
inline constexpr INT kButtonHeight  = 24;
inline constexpr INT kSwatchSize    = 24;

// Settings window: a small fixed-size control panel. These are CLIENT
// dimensions - InitWindow runs them through AdjustWindowRectEx to get the
// outer size, so the caption, borders and menu bar can't eat the last row.
inline constexpr INT CW_WIDTH = 300;
// Derived from the very constants LayoutChildren steps through, so the window
// can't drift out of sync with the rows it has to hold: top pad, slider row,
// the two mode checkboxes, the erase label, the desktop-color checkbox, the
// button/swatch row, and the matching bottom pad.
inline constexpr INT CW_HEIGHT = kPadTop +                    // top pad
                                 kSliderHeight + kVGap +      // speed row
                                 kControlHeight + kVGap +     // follow
                                 kControlHeight + kVGap * 2 + // run away
                                 kControlHeight + kVGap / 2 + // "Erase to:"
                                 kControlHeight + kVGap +     // desktop color
                                 kButtonHeight +              // pick + swatch
                                 kPadTop;                     // bottom pad

// Pet animation: one tick every kTickMs; speed = pixels moved per tick. The
// default speed is not a constant: it comes from the display table in pet_lib
// (pet::ProfileForHeight) and depends on the screen height.
inline constexpr UINT kTickTimerId = 1;
inline constexpr UINT kTickMs      = 50;
inline constexpr int kSpeedMin     = 1;
inline constexpr int kSpeedMax     = 32;
// Run-away mode: flee when the cursor comes within this many pixels of the
// vacuum's centre, running this far away from it.
inline constexpr int kFleeRadius = 120;
// How long to wait for Explorer to finish repainting the desktop before we
// paint the trail over it (see desktop::EraseOnDesktop). Explorer normally
// answers in well under a millisecond; this only matters if it is hung.
inline constexpr UINT kDesktopPaintTimeoutMs = 50;

// Registry: HKCU\Software\<kRegSubkey>\<value>
inline constexpr wchar_t kRegSubkey[]     = L"Alex313031\\Kirby Pet";
inline constexpr wchar_t kRegSpeed[]      = L"Speed";
inline constexpr wchar_t kRegMode[]       = L"Mode";
inline constexpr wchar_t kRegEraseColor[] = L"EraseColor";
inline constexpr wchar_t kRegUseDesktop[] = L"UseDesktopColor";
inline constexpr wchar_t kRegTray[]       = L"MinimizeToTray";

// Window class of the on-screen vacuum sprite.
inline constexpr wchar_t kPetWndClass[] = L"KirbyPetSprite";
// What the sprite window paints around the frame and tells Windows to treat
// as see-through (LWA_COLORKEY). Must never appear in the sprite art itself.
inline constexpr COLORREF kSpriteColorKey = RGB_MAGENTA;

// Child window style
inline constexpr DWORD dwCHILD = WS_CHILD | WS_VISIBLE;

// Minimum common controls version for certain functions, used for fallback codepaths
// See https://learn.microsoft.com/en-us/windows/win32/controls/common-control-versions
inline constexpr DWORD dwComCtl32TargetVer =
    _PACKVERSION(static_cast<DWORD>(5u), static_cast<DWORD>(82u));

#endif // KIRBY_PET_CONSTANTS_H_

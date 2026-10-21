// The desktop hack: locate the window that paints the wallpaper and draw on it.
// Ported from neko-ng's CDesktopHack (DesktopHunter) in plain-function form.

#include "desktop.h"

#include <pet_lib.h>

namespace desktop {

  /* --------------------------------- Types ---------------------------------- */

  // Which pass of the enumeration the callback is running.
  enum Pass { PASS_TOP = 1, PASS_DEFVIEW = 2, PASS_CHILD = 3 };

  /* ------------------------------ Static state ------------------------------ */

  static HWND s_desktop = nullptr; // the cached drawing window
  static HWND s_defView = nullptr; // SHELLDLL_DefView, parent of the list view

  /* -------------------------- Forward declarations -------------------------- */

  static BOOL CALLBACK Hunter(HWND hWnd, LPARAM lParam);

  /* ---------------------------- Static Functions ---------------------------- */

  // EnumWindows / EnumChildWindows callback; `lParam` carries which Pass we are
  // running. Recurses into the next pass as soon as its anchor window is found.
  static BOOL CALLBACK Hunter(HWND hWnd, LPARAM lParam) {
    wchar_t cls[32] = {};
    switch (lParam) {
      case PASS_TOP:
        // Top level: find Program Manager, then look inside it.
        GetClassNameW(hWnd, cls, 31);
        if (lstrcmpiW(cls, L"Progman") == 0) {
          EnumChildWindows(hWnd, Hunter, PASS_DEFVIEW);
          return s_desktop == nullptr; // keep enumerating until one is found
        }
        break;

      case PASS_DEFVIEW:
        GetClassNameW(hWnd, cls, 31);
        if (lstrcmpiW(cls, L"SHELLDLL_DefView") == 0) {
          s_defView = hWnd;
          EnumChildWindows(hWnd, Hunter, PASS_CHILD);
          return FALSE;
        }
        break;

      case PASS_CHILD:
        // Only direct children of the shell default view count.
        if (GetParent(hWnd) != s_defView) {
          break;
        }
        GetClassNameW(hWnd, cls, 31);
        if (lstrcmpiW(cls, L"SysListView32") == 0) {
          s_desktop = hWnd; // the normal desktop; keep looking in case an
          return TRUE;      // Active Desktop server follows and should win
        }
        if (lstrcmpiW(cls, L"Internet Explorer_Server") == 0) {
          s_desktop = hWnd; // IE4+ Active Desktop
          return FALSE;
        }
        break;

      default:
        break;
    }
    return TRUE;
  }

  /* ------------------------------- Functions -------------------------------- */

  HWND FindHandle() {
    s_desktop = nullptr;
    s_defView = nullptr;
    EnumWindows(Hunter, PASS_TOP);
    if (s_desktop == nullptr) {
      // No Explorer-style shell (win32s / NT 3.x / a replacement shell): the
      // raw desktop window is the best we can do.
      s_desktop = GetDesktopWindow();
    }
    return s_desktop;
  }

  void EraseOnDesktop(const RECT& screenRect, COLORREF color, bool rescan) {
    if (rescan || s_desktop == nullptr || !IsWindow(s_desktop)) {
      FindHandle();
    }
    if (s_desktop == nullptr) {
      return;
    }
    // The list view's client origin isn't (0,0) on multi-monitor layouts with a
    // display left of / above the primary; map the screen rect into its space.
    POINT topLeft     = {screenRect.left, screenRect.top};
    POINT bottomRight = {screenRect.right, screenRect.bottom};
    ScreenToClient(s_desktop, &topLeft);
    ScreenToClient(s_desktop, &bottomRight);
    RECT clientRect;
    SetRect(&clientRect, topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);

    HDC hdc = GetDC(s_desktop);
    if (hdc == nullptr) {
      return;
    }
    pet::EraseRect(hdc, clientRect, color);
    ReleaseDC(s_desktop, hdc);
  }

} // namespace desktop

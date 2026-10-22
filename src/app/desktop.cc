// The desktop hack: locate the window that paints the wallpaper and draw on it.
// Ported from neko-ng's CDesktopHack (DesktopHunter) in plain-function form.

#include "desktop.h"

#include <pet_lib.h>

#include "constants.h"

namespace desktop {

  /* --------------------------------- Types ---------------------------------- */

  // Which pass of the enumeration the callback is running.
  enum Pass { PASS_TOP = 1, PASS_DEFVIEW = 2, PASS_CHILD = 3 };

  /* ------------------------------ Static state ------------------------------ */

  static HWND s_desktop = nullptr; // the cached drawing window
  static HWND s_defView = nullptr; // SHELLDLL_DefView, parent of the list view

  /* -------------------------- Forward declarations -------------------------- */

  static BOOL CALLBACK Hunter(HWND hWnd, LPARAM lParam);
  static RECT ToClient(const RECT& screenRect);

  /* ---------------------------- Static Functions ---------------------------- */

  // Maps a screen rect into the desktop window's client space. Its origin isn't
  // (0,0) on multi-monitor layouts with a display left of / above the primary.
  static RECT ToClient(const RECT& screenRect) {
    POINT topLeft     = {screenRect.left, screenRect.top};
    POINT bottomRight = {screenRect.right, screenRect.bottom};
    ScreenToClient(s_desktop, &topLeft);
    ScreenToClient(s_desktop, &bottomRight);
    RECT clientRect;
    SetRect(&clientRect, topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
    return clientRect;
  }

  // EnumWindows / EnumChildWindows callback; `lParam` carries which Pass we are
  // running. Recurses into the next pass as soon as its anchor window is found.
  static BOOL CALLBACK Hunter(HWND hWnd, LPARAM lParam) {
    wchar_t cls[32] = {};
    switch (lParam) {
      case PASS_TOP:
        // Top level: find Program Manager and look inside it. Windows 8 and
        // later can re-parent the shell view into one of the WorkerW windows
        // (once any wallpaper tool has sent Progman the 0x052C message), so
        // those are searched too; the ones without a view just come up empty.
        GetClassNameW(hWnd, cls, 31);
        if (lstrcmpiW(cls, L"Progman") == 0 || lstrcmpiW(cls, L"WorkerW") == 0) {
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

  bool EraseOnDesktop(const RECT& screenRect, COLORREF color, bool rescan) {
    if (rescan || s_desktop == nullptr || !IsWindow(s_desktop)) {
      FindHandle();
    }
    if (s_desktop == nullptr) {
      return false;
    }

    // Moving the sprite just uncovered a strip of desktop, and Windows
    // invalidated it: Explorer has a WM_PAINT queued that will redraw the
    // background and icons there - from its own process, a few milliseconds
    // from now, on top of whatever we paint next. Make it paint NOW, before
    // our fill, so ours lands last. Sent with a timeout so a hung Explorer
    // costs this tick its flush rather than hanging us with it.
    if (GetUpdateRect(s_desktop, nullptr, FALSE)) {
      DWORD_PTR unused = 0;
      SendMessageTimeoutW(s_desktop, WM_PAINT, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK,
                          kDesktopPaintTimeoutMs, &unused);
    }

    const RECT clientRect = ToClient(screenRect);
    HDC hdc               = GetDC(s_desktop);
    if (hdc == nullptr) {
      return false;
    }
    // The DC is clipped to what is visible, so the sprite's own outline (it is
    // topmost) is skipped automatically; everything else in the rect is ours.
    const bool filled = pet::EraseRect(hdc, clientRect, color);
    ReleaseDC(s_desktop, hdc);
    return filled;
  }

  void Restore() {
    if (s_desktop == nullptr || !IsWindow(s_desktop)) {
      FindHandle();
    }
    if (s_desktop == nullptr) {
      return;
    }
    // Invalidate from the top of the desktop's window tree, children included:
    // on newer shells the wallpaper is painted by an ancestor of the list view
    // (the view is transparent), so repainting the view alone would not bring
    // it back. If we only ever had the raw desktop window to draw on, repaint
    // just that - its children are every top-level window on the screen.
    HWND top = s_desktop;
    for (HWND parent = GetParent(top); parent != nullptr; parent = GetParent(top)) {
      top = parent;
    }
    const UINT flags = RDW_INVALIDATE | RDW_ERASE |
                       (top == GetDesktopWindow() ? 0u : static_cast<UINT>(RDW_ALLCHILDREN));
    RedrawWindow(top, nullptr, nullptr, flags);
  }

} // namespace desktop

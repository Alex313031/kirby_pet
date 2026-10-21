#ifndef KIRBY_PET_DESKTOP_H_
#define KIRBY_PET_DESKTOP_H_

#include "framework.h"

// The "desktop hack" (Neko98 lineage): find the window that actually paints
// the wallpaper so the vacuumed trail can be drawn straight onto it.
namespace desktop {

  // Locates the desktop's drawing window - the SysListView32 under Progman's
  // SHELLDLL_DefView, or the Active Desktop's Internet Explorer_Server - and
  // falls back to GetDesktopWindow() on shells without one. Cheap enough to
  // re-run now and then in case Explorer restarted.
  HWND FindHandle();

  // Fills `screenRect` (screen coordinates) on the desktop window with `color`.
  // Uses the cached handle; `rescan` forces a FindHandle first.
  void EraseOnDesktop(const RECT& screenRect, COLORREF color, bool rescan = false);

} // namespace desktop

#endif // KIRBY_PET_DESKTOP_H_

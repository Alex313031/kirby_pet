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
  // First makes Explorer finish any repaint it has pending there, so what we
  // paint lands last and its icons can't come back over the trail. Uses the
  // cached handle; `rescan` forces a FindHandle first.
  void EraseOnDesktop(const RECT& screenRect, COLORREF color, bool rescan = false);

  // Puts the desktop back the way Explorer draws it, wiping every trail we
  // painted: the whole desktop window tree is invalidated with erase and left
  // for Explorer to repaint. Asynchronous, so a hung shell can't hold up an
  // exit. Call it on the way out.
  void Restore();

} // namespace desktop

#endif // KIRBY_PET_DESKTOP_H_

#ifndef KIRBY_PET_PET_WINDOW_H_
#define KIRBY_PET_PET_WINDOW_H_

// clang-format off
#include "framework.h"

#include <pet_lib.h>
// clang-format on

// The on-screen vacuum: a popup window driven by a pet::Vacuum, owning the
// sprites, that never takes the mouse (layered + colour-keyed and
// click-through on 2000+; region-shaped on NT4). Topmost by default, or
// pinned just above the desktop so it passes under every other window. The
// settings window ticks it on a timer.
namespace petwin {

  enum Mode { MODE_IDLE = 0, MODE_FOLLOW = 1, MODE_RUNAWAY = 2 };

  // The display-table row for the primary screen's height: which frame set,
  // what size to scale it to, and the default speed. Cheap; ask any time.
  const pet::DisplayProfile& ScreenProfile();

  // Loads the frames the screen's profile calls for out of `frameIds` (both
  // baked sets; the IDs are copied, so the array need not outlive the call),
  // registers the window class, and shows the sprite parked at the top-left
  // of the screen. False on failure; LastLoadResult says why.
  [[nodiscard]] bool Create(HINSTANCE hInst, const pet::FrameIds& frameIds);
  void Destroy();

  // What the most recent sprite load reported, for the error message.
  pet::LoadResult LastLoadResult();

  void SetSpeed(int pixelsPerTick); // clamped to pet::kSpeedMin..kSpeedMax
  void SetMode(Mode mode);
  void SetEraseColor(COLORREF color);
  // true: the vacuum travels under other windows (bottom of the Z order, just
  // above the desktop); false: it rides on top of everything (topmost). False
  // if the window exists and Windows refused to move it in the Z order; the
  // setting is remembered either way and applied at the next Create.
  [[nodiscard]] bool SetUnderWindows(bool under);
  // Call on WM_DISPLAYCHANGE: re-fits the roaming area to the new screen and,
  // if the display table wants another frame set or size for it, reloads the
  // sprites. False if that reload or the window's new outline failed - the old
  // frames stay in use, and LastLoadResult says what went wrong.
  [[nodiscard]] bool UpdateBounds();

  // One animation tick: aim per the mode, step, move the window, and vacuum
  // (erase to the color) the spot just vacated on the desktop.
  void Tick();

  HWND Handle();

} // namespace petwin

#endif // KIRBY_PET_PET_WINDOW_H_

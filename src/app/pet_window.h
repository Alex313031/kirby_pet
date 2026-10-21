#ifndef KIRBY_PET_PET_WINDOW_H_
#define KIRBY_PET_PET_WINDOW_H_

#include <pet_lib.h>

#include "framework.h"

// The on-screen vacuum: a topmost, region-shaped popup window driven by a
// pet::Vacuum, owning the sprites. The settings window ticks it on a timer.
namespace petwin {

  enum Mode { MODE_IDLE = 0, MODE_FOLLOW = 1, MODE_RUNAWAY = 2 };

  // Loads the frames from `resIds` (pet::Dir order), registers the window class,
  // and shows the sprite parked at the top-left of the screen. False on failure.
  bool Create(HINSTANCE hInst, const int resIds[pet::DIR_COUNT]);
  void Destroy();

  void SetSpeed(int pixelsPerTick); // clamped to kSpeedMin..kSpeedMax
  void SetMode(Mode mode);
  void SetEraseColor(COLORREF color);
  void UpdateBounds(); // call on WM_DISPLAYCHANGE

  // One animation tick: aim per the mode, step, move / reshape the window, and
  // vacuum (erase to the color) the spot just vacated on the desktop.
  void Tick();

  HWND Handle();

} // namespace petwin

#endif // KIRBY_PET_PET_WINDOW_H_

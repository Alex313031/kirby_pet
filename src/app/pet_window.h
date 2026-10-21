#ifndef KIRBY_PET_PET_WINDOW_H_
#define KIRBY_PET_PET_WINDOW_H_

// clang-format off
#include "framework.h"

#include <pet_lib.h>
// clang-format on

// The on-screen vacuum: a topmost, region-shaped popup window driven by a
// pet::Vacuum, owning the sprites. The settings window ticks it on a timer.
namespace petwin {

  enum Mode { MODE_IDLE = 0, MODE_FOLLOW = 1, MODE_RUNAWAY = 2 };

  // The display-table row for the primary screen's height: which frame set,
  // what size to scale it to, and the default speed. Cheap; ask any time.
  const pet::DisplayProfile& ScreenProfile();

  // Loads the frames the screen's profile calls for out of `frameIds` (both
  // baked sets, pet::SpriteSet by pet::Dir), registers the window class, and
  // shows the sprite parked at the top-left of the screen. False on failure.
  bool Create(HINSTANCE hInst, const int frameIds[pet::SPRITE_SET_COUNT][pet::DIR_COUNT]);
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

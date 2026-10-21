#ifndef PET_LIB_H_
#define PET_LIB_H_

// Public API of pet_lib: the vacuum sprite + movement logic shared by the
// Kirby Pet app and the Kirby screensaver. The lib owns NO window and NO
// resources - consumers embed the four frame bitmaps in their own .rc and
// hand the IDs in; where the vacuum draws (a region-shaped popup, a
// screensaver back buffer, the desktop) is entirely the consumer's business.

// clang-format off
// Keep this above <windows.h>: it pins _WIN32_WINNT/WINVER and the UNICODE
// defines, which windows.h reads on its way through.
#include "pet_version.h"

// Use std::min / std::max, instead of the Win32-specific MIN/MAX.
#ifndef NOMINMAX
 #define NOMINMAX
#endif

#include <windows.h>

// clang-format on

namespace pet {

  /* --------------------------------- Types ---------------------------------- */

  // Cardinal facing. Doubles as the index into Sprites::bmp / Sprites::rgn, so
  // the resource-ID array handed to LoadSprites must be in this order.
  enum Dir { DIR_UP = 0, DIR_DOWN = 1, DIR_LEFT = 2, DIR_RIGHT = 3, DIR_COUNT = 4 };

  // The four 32-bpp frames plus, for each, the region of its opaque pixels
  // (alpha != 0) with origin (0,0). Frames are DIB sections so the alpha
  // channel can be read back; all four must be the same size.
  struct Sprites {
    HBITMAP bmp[DIR_COUNT];
    HRGN rgn[DIR_COUNT];
    SIZE size;
  };

  // The moving vacuum. Pure state, no handles, so it's trivially shared.
  struct Vacuum {
    POINT pos;    // top-left of the sprite, in the consumer's coordinate space
    POINT target; // where it is heading
    Dir facing;
    int speed;   // pixels per VacuumStep
    RECT bounds; // allowed range for `pos` (already shrunk by the sprite size)
    SIZE size;   // sprite size, copied from Sprites
  };

  // Which baked frame set a consumer hands to LoadSprites. Indexes the
  // consumer's resource-ID table, in this order: 1x is 32 px, 2x is 64 px.
  enum SpriteSet { SPRITE_1X = 0, SPRITE_2X = 1, SPRITE_SET_COUNT = 2 };

  // One row of the display table: on a screen up to `maxHeight` pixels tall,
  // load `set`, scale its frames to `spriteSize` pixels square, and default
  // the speed to `speed` pixels per tick.
  struct DisplayProfile {
    int maxHeight;
    SpriteSet set;
    int spriteSize;
    int speed;
  };

  /* ------------------------------- Functions -------------------------------- */

  /* ------------------------------- Sprites ---------------------------------- */

  // The display-table row for a screen `screenHeight` pixels tall: the first
  // row whose maxHeight it does not exceed. The last row catches everything.
  const DisplayProfile& ProfileForHeight(int screenHeight);

  // Loads the frames from `hInst`'s RT_BITMAP resources `resIds` (Dir order),
  // resamples each to `spriteSize` pixels tall (0 keeps the native size;
  // nearest-neighbour, so the pixel art stays crisp and the alpha survives),
  // and builds their alpha regions. Returns false - and frees whatever it had
  // loaded - if any frame is missing, not 32-bpp, or a different size.
  bool LoadSprites(HINSTANCE hInst, const int resIds[DIR_COUNT], int spriteSize, Sprites* out);
  void FreeSprites(Sprites* sprites);

  // A caller-owned copy of a frame's region (origin 0,0). SetWindowRgn takes
  // ownership of what it's given, so hand it a fresh clone each time.
  HRGN CloneRegion(const Sprites& sprites, Dir dir);

  /* ------------------------------- Movement --------------------------------- */

  // Sets `vacuum` up to roam inside `area` (exclusive right/bottom, e.g. the virtual
  // screen or a client rect) with a sprite of `spriteSize`, parked at area's
  // top-left and facing down.
  void VacuumInit(Vacuum* vacuum, const RECT& area, const SIZE& spriteSize, int speed);
  void VacuumSetTarget(Vacuum* vacuum, int targetX, int targetY);

  // Re-bounds `vacuum` to a new `area` (e.g. after WM_DISPLAYCHANGE), keeping its
  // position but clamping it back inside if the area shrank.
  void VacuumSetArea(Vacuum* vacuum, const RECT& area);

  // Advances one tick toward the target: runs at `speed`, jumps the last gap
  // when closer than that, clamps to bounds, and turns to face the dominant
  // axis of the move it actually made. Returns true if the position changed.
  bool VacuumStep(Vacuum* vacuum);
  bool VacuumAtTarget(const Vacuum* vacuum);

  // The sprite's footprint at its current position.
  RECT VacuumRect(const Vacuum* vacuum);

  /* ------------------------------- Drawing ---------------------------------- */

  // Blits the facing frame at `pos` onto `hdc`, clipped to the frame's alpha
  // region so transparent pixels never paint. Correct on any DC, so the
  // screensaver can composite onto a back buffer with it.
  void DrawVacuum(HDC hdc, const Sprites& sprites, const Vacuum& vacuum);

  // Blits frame `dir` with its top-left at (posX, posY), alpha-clipped. DrawVacuum is
  // this at the vacuum's position; a region-shaped window paints with (0, 0).
  void DrawFrame(HDC hdc, const Sprites& sprites, Dir dir, int posX, int posY);

  // Fills `rect` with a solid color: the "vacuumed clean" eraser.
  void EraseRect(HDC hdc, const RECT& rect, COLORREF color);

  // The color both consumers erase to unless the user picked one: the desktop
  // background (COLOR_DESKTOP), read live so a theme change tracks.
  COLORREF DefaultEraseColor();

  /* ------------------------------- Registry --------------------------------- */

  // Minimal HKCU\Software\<subkey> DWORD persistence shared by app and saver.
  bool RegReadDword(const wchar_t* subkey, const wchar_t* name, DWORD* out);
  bool RegWriteDword(const wchar_t* subkey, const wchar_t* name, DWORD value);

} // namespace pet

#endif // PET_LIB_H_

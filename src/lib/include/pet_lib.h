#ifndef PET_LIB_H_
#define PET_LIB_H_

// Public API of pet_lib: the vacuum sprite + movement logic shared by the
// Kirby Pet app and the Kirby screensaver. The lib owns NO window and NO
// resources - consumers embed the frame bitmaps in their own .rc and hand the
// IDs in; where the vacuum draws (a region-shaped popup, a screensaver back
// buffer, the desktop) is entirely the consumer's business.
//
// UNICODE everywhere: every string here is wchar_t and every Win32 call is
// the W variant. Building with UNICODE / _UNICODE defined is the consumer's
// responsibility; pet_version.h below only sets them if nothing has yet.

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

  // Cardinal facing. Doubles as the index into the per-frame arrays, so the
  // resource-ID array handed to LoadSprites must be in this order.
  enum Dir { DIR_UP = 0, DIR_DOWN = 1, DIR_LEFT = 2, DIR_RIGHT = 3, DIR_COUNT = 4 };

  // Which baked frame set a consumer hands to LoadSprites. Indexes the
  // consumer's resource-ID table, in this order: 1x is 32 px, 2x is 64 px.
  enum SpriteSet { SPRITE_1X = 0, SPRITE_2X = 1, SPRITE_SET_COUNT = 2 };

  // A consumer's resource-ID table: both baked sets, each in Dir order.
  typedef int FrameIds[SPRITE_SET_COUNT][DIR_COUNT];

  // Why LoadSprites said no. LoadResultText turns one into words for a log
  // line or a message box.
  enum LoadResult {
    LOAD_OK = 0,
    LOAD_BAD_ARGS,      // null instance / IDs / output
    LOAD_MISSING_FRAME, // a resource ID didn't load (wrong ID, or not in this module)
    LOAD_NOT_32BPP,     // a frame isn't 32-bpp, so there is no alpha to read
    LOAD_SIZE_MISMATCH, // the four frames aren't all the same size
    LOAD_SCALE_FAILED,  // couldn't resample a frame to the requested size
    LOAD_REGION_FAILED, // GDI refused to build a frame's alpha region
  };

  // The four frames and everything drawn or derived from them, all the same
  // size. Per facing: the 32-bpp frame itself (a DIB section, so the alpha
  // channel can be read); its opaque-pixel region, origin (0,0), for shaping a
  // window; and the two halves of the classic two-blit transparent draw - a
  // mask (white where transparent, black where opaque) and the frame with its
  // transparent pixels blacked out. `memDC` is kept for DrawFrame so no DC is
  // created per draw. Zero-initialise before the first LoadSprites.
  struct Sprites {
    HBITMAP bmp[DIR_COUNT];
    HRGN rgn[DIR_COUNT];
    HBITMAP mask[DIR_COUNT];
    HBITMAP opaque[DIR_COUNT];
    HDC memDC;
    SIZE size;
  };

  // The moving vacuum. Pure state, no handles, so it's trivially shared.
  struct Vacuum {
    POINT pos;    // top-left of the sprite, in the consumer's coordinate space
    POINT target; // where it is heading; always within `bounds`
    Dir facing;
    int speed;   // pixels per VacuumStep
    RECT bounds; // allowed range for `pos` (already shrunk by the sprite size)
    SIZE size;   // sprite size, copied from Sprites
  };

  // The range a speed may take, in pixels per tick. VacuumInit and the
  // settings loader clamp to it; consumers size their sliders by it.
  const int kSpeedMin = 1;
  const int kSpeedMax = 32;

  // One row of the display table: on a screen up to `maxHeight` pixels tall,
  // load `set`, scale its frames to `spriteSize` pixels square, and default
  // the speed to `speed` pixels per tick.
  struct DisplayProfile {
    int maxHeight;
    SpriteSet set;
    int spriteSize;
    int speed;
  };

  // What both consumers let the user set about the erasing, persisted as
  // DWORDs under HKCU\Software\<subkey>. `customPalette` is the custom-colour
  // row of the ChooseColor dialog, kept so it survives between picks.
  struct EraseSettings {
    int speed;            // pixels per tick, within kSpeedMin..kSpeedMax
    COLORREF customColor; // the colour the user picked
    bool useDesktopColor; // erase to the live desktop colour instead of customColor
    COLORREF customPalette[16];
  };

  /* ------------------------------- Functions -------------------------------- */

  /* ------------------------------- Sprites ---------------------------------- */

  // The display-table row for a screen `screenHeight` pixels tall: the first
  // row whose maxHeight it does not exceed. The last row catches everything.
  const DisplayProfile& ProfileForHeight(int screenHeight);

  // Loads the frames from `hInst`'s RT_BITMAP resources `resIds` (Dir order),
  // resamples each to `spriteSize` pixels tall (0 keeps the native size;
  // nearest-neighbour, so the pixel art stays crisp and the alpha survives),
  // and builds their regions and blit masks. Anything `out` already holds is
  // freed first. On any failure `out` is left empty and the reason returned.
  LoadResult LoadSprites(HINSTANCE hInst,
                         const int (&resIds)[DIR_COUNT],
                         int spriteSize,
                         Sprites* out);

  // LoadSprites with the set and size a display-table row calls for.
  LoadResult LoadSpritesForProfile(HINSTANCE hInst,
                                   const FrameIds& frameIds,
                                   const DisplayProfile& profile,
                                   Sprites* out);

  // A short description of a LoadResult, for logs and message boxes.
  const wchar_t* LoadResultText(LoadResult result);

  void FreeSprites(Sprites* sprites);

  // A caller-owned copy of a frame's region (origin 0,0). SetWindowRgn takes
  // ownership of what it's given, so hand it a fresh clone each time.
  HRGN CloneRegion(const Sprites& sprites, Dir dir);

  /* ------------------------------- Movement --------------------------------- */

  // Sets `vacuum` up to roam inside `area` (exclusive right/bottom, e.g. the
  // virtual screen or a client rect) with a sprite of `spriteSize`, parked at
  // area's top-left and facing down. `speed` is clamped to kSpeedMin..Max.
  void VacuumInit(Vacuum* vacuum, const RECT& area, const SIZE& spriteSize, int speed);

  // Aims the vacuum. The target is clamped into `bounds`, so a point past the
  // edge means "as far that way as it can go" and VacuumStep's false always
  // means "arrived".
  void VacuumSetTarget(Vacuum* vacuum, int targetX, int targetY);

  // Re-bounds `vacuum` to a new `area` (e.g. after WM_DISPLAYCHANGE), keeping
  // its position and target but clamping both back inside if the area shrank.
  void VacuumSetArea(Vacuum* vacuum, const RECT& area);

  // Advances one tick toward the target: runs at `speed`, jumps the last gap
  // when closer than that, and turns to face the dominant axis of the move.
  // Returns true if the position changed, false once it is at the target.
  bool VacuumStep(Vacuum* vacuum);
  bool VacuumAtTarget(const Vacuum* vacuum);

  // The sprite's footprint at its current position.
  RECT VacuumRect(const Vacuum* vacuum);

  /* ------------------------------- Drawing ---------------------------------- */

  // Blits the facing frame at `pos` onto `hdc`, transparent pixels left alone.
  // Two BitBlts (mask, then frame), no regions, no per-call GDI objects.
  // Correct on any DC, so the screensaver can composite onto a back buffer.
  void DrawVacuum(HDC hdc, const Sprites& sprites, const Vacuum& vacuum);

  // Blits frame `dir` with its top-left at (posX, posY), transparent pixels
  // left alone. DrawVacuum is this at the vacuum's position; a window that is
  // the sprite paints with (0, 0).
  void DrawFrame(HDC hdc, const Sprites& sprites, Dir dir, int posX, int posY);

  // Fills `rect` with a solid color: the "vacuumed clean" eraser.
  void EraseRect(HDC hdc, const RECT& rect, COLORREF color);

  // The color both consumers erase to unless the user picked one: the desktop
  // background (COLOR_DESKTOP), read live so a theme change tracks.
  COLORREF DefaultEraseColor();

  /* ------------------------------- Settings --------------------------------- */

  // Fills `settings` from HKCU\Software\<subkey>, falling back to
  // `defaultSpeed` (the display table's, usually), black, and the desktop
  // colour for anything not saved. The speed is clamped to kSpeedMin..Max.
  void LoadEraseSettings(const wchar_t* subkey, int defaultSpeed, EraseSettings* settings);

  // Writes the three persisted fields back. False if any write failed.
  bool SaveEraseSettings(const wchar_t* subkey, const EraseSettings& settings);

  // The colour the vacuum erases to right now: the live desktop colour or the
  // custom one, per the setting.
  COLORREF EffectiveEraseColor(const EraseSettings& settings);

  // Runs the ChooseColor dialog over `owner`, seeded with the custom colour and
  // palette. On OK stores the pick, switches `useDesktopColor` off (picking a
  // colour means "not the desktop's"), and returns true; on Cancel changes
  // nothing and returns false. The caller refreshes its own controls.
  bool PickEraseColor(HWND owner, EraseSettings* settings);

  /* ------------------------------- Registry --------------------------------- */

  // Minimal HKCU\Software\<subkey> DWORD persistence shared by app and saver.
  bool RegReadDword(const wchar_t* subkey, const wchar_t* name, DWORD* out);
  bool RegWriteDword(const wchar_t* subkey, const wchar_t* name, DWORD value);

  // Removes HKCU\Software\<subkey> and everything saved under it, so the next
  // load finds nothing and falls back to the defaults. True if it is gone,
  // including when it never existed.
  bool RegDeleteSubkey(const wchar_t* subkey);

} // namespace pet

#endif // PET_LIB_H_

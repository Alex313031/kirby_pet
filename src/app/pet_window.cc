// The sprite window: a popup that pet_lib drives, riding on top of everything
// or pinned to the desktop under every other window. It never takes the mouse
// - a pet that parks under the pointer must not eat the click.

#include "pet_window.h"

#include <cstring> // memcpy

#include "constants.h"
#include "desktop.h"
#include "globals.h"

namespace petwin {

  /* --------------------------------- Types ---------------------------------- */

  // Windows 2000+. Resolved at run time rather than imported, so the binary
  // still loads on NT4, which gets the region-shaped fallback instead.
  typedef BOOL(WINAPI* FnSetLayeredWindowAttributes)(HWND, COLORREF, BYTE, DWORD);

// winuser.h hides the layered-window constants when building with an NT4
// WINVER. The function is resolved at run time, so the constants are all we
// need, and their values have never changed.
#ifndef WS_EX_LAYERED
 #define WS_EX_LAYERED 0x00080000
#endif
#ifndef LWA_COLORKEY
 #define LWA_COLORKEY 0x00000001
#endif

  /* ------------------------------ Static state ------------------------------ */

  static HWND s_hwnd            = nullptr;
  static pet::Sprites s_sprites = {};
  static pet::Vacuum s_vac      = {};
  static Mode s_mode            = MODE_FOLLOW;
  static COLORREF s_eraseColor  = 0;
  static bool s_classRegistered = false;
  static int s_ticksSinceRescan = 0;
  static const int kRescanEvery = 50; // re-find the desktop window this often (ticks)

  // Which kind of window the sprite is. Layered (2000+): colour-keyed, so the
  // outline is per pixel, and WS_EX_TRANSPARENT makes the whole thing
  // click-through at the system level; the system composites it, so moving it
  // never disturbs the desktop beneath. Fallback (NT4): region-shaped, and the
  // mouse still lands on it.
  static bool s_layered = false;

  // Where the sprite sits in the Z order: on top of every window (topmost), or
  // at the bottom, where it travels under them all and only ever covers the
  // desktop. Set before Create too; Create applies it.
  static bool s_underWindows = false;

  // What Create was given (the IDs copied, so the caller's array need not
  // outlive the call) and which display-table row the loaded frames are for,
  // kept so a resolution change can reload at the new size.
  static HINSTANCE s_hInst                    = nullptr;
  static pet::FrameIds s_frameIds             = {};
  static const pet::DisplayProfile* s_profile = nullptr;
  static pet::LoadResult s_lastLoad           = pet::LOAD_OK;

  /* -------------------------- Forward declarations -------------------------- */

  static FnSetLayeredWindowAttributes ResolveSetLayeredWindowAttributes();
  static RECT VirtualScreen();
  static void MoveWindowTo(const POINT& pos);
  static void ApplyZOrder();
  static bool LoadForProfile(const pet::DisplayProfile& profile);
  static void ApplyShape();
  static LRESULT CALLBACK PetWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

  /* ---------------------------- Static Functions ---------------------------- */

  static FnSetLayeredWindowAttributes ResolveSetLayeredWindowAttributes() {
    static FnSetLayeredWindowAttributes pfn = nullptr;
    static bool s_resolved                  = false;
    if (!s_resolved) {
      HMODULE hUser32 = GetModuleHandleW(kUser32Dll);
      if (hUser32 != nullptr) {
        pfn = reinterpret_cast<FnSetLayeredWindowAttributes>(
            GetProcAddress(hUser32, "SetLayeredWindowAttributes"));
      }
      s_resolved = true;
    }
    return pfn;
  }

  // Loads (or, later, reloads) the frame set a display-table row calls for,
  // scaled to its size. The old frames are only let go once the new ones are in.
  static bool LoadForProfile(const pet::DisplayProfile& profile) {
    pet::Sprites loaded = {};
    s_lastLoad          = pet::LoadSpritesForProfile(s_hInst, s_frameIds, profile, &loaded);
    if (s_lastLoad != pet::LOAD_OK) {
      return false;
    }
    pet::FreeSprites(&s_sprites);
    s_sprites = loaded;
    s_profile = &profile;
    return true;
  }

  // The area the vacuum may roam: the whole virtual screen. NT4 has no multimon
  // metrics (they read 0), so fall back to the primary display there.
  static RECT VirtualScreen() {
    RECT area;
    const int width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width == 0 || height == 0) {
      SetRect(&area, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    } else {
      const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
      const int top  = GetSystemMetrics(SM_YVIRTUALSCREEN);
      SetRect(&area, left, top, left + width, top + height);
    }
    return area;
  }

  static void MoveWindowTo(const POINT& pos) {
    SetWindowPos(s_hwnd, nullptr, pos.x, pos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  }

  // Puts the sprite where s_underWindows says. HWND_BOTTOM also drops the
  // topmost status, and lands above the shell's desktop window (Progman),
  // which Windows keeps bottommost of all - so "under windows" still shows
  // over the wallpaper and icons. HWND_TOPMOST puts the status back. Every
  // move after this is SWP_NOZORDER, so the position sticks; being shown
  // with SW_SHOWNA (the NT4 turn path) keeps it too.
  static void ApplyZOrder() {
    SetWindowPos(s_hwnd, s_underWindows ? HWND_BOTTOM : HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  // Gives the window the current facing's outline. Layered: nothing to do, the
  // colour key shapes it per pixel at paint time. Fallback: the window region,
  // which SetWindowRgn takes ownership of, hence the fresh clone.
  static void ApplyShape() {
    if (s_layered) {
      InvalidateRect(s_hwnd, nullptr, FALSE);
    } else {
      SetWindowRgn(s_hwnd, pet::CloneRegion(s_sprites, s_vac.facing), TRUE);
    }
  }

  /* ------------------------------- Functions -------------------------------- */

  const pet::DisplayProfile& ScreenProfile() {
    return pet::ProfileForHeight(GetSystemMetrics(SM_CYSCREEN));
  }

  pet::LoadResult LastLoadResult() {
    return s_lastLoad;
  }

  bool Create(HINSTANCE hInst, const pet::FrameIds& frameIds) {
    if (s_hwnd != nullptr) {
      return true;
    }
    s_hInst = hInst;
    memcpy(s_frameIds, frameIds, sizeof(s_frameIds));
    s_layered                          = (ResolveSetLayeredWindowAttributes() != nullptr);
    const pet::DisplayProfile& profile = ScreenProfile();
    if (!LoadForProfile(profile)) {
      return false;
    }
    if (!s_classRegistered) {
      WNDCLASSEXW wndClass = {};
      wndClass.cbSize      = sizeof(wndClass);
      // CS_SAVEBITS only matters for the region-shaped fallback: it lets
      // Windows put back what was under the sprite where it can instead of
      // asking the desktop to repaint. A layered window is composited by the
      // system and never uncovers anything.
      wndClass.style         = CS_SAVEBITS;
      wndClass.lpfnWndProc   = PetWndProc;
      wndClass.hInstance     = hInst;
      wndClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
      wndClass.hbrBackground = nullptr; // WM_PAINT covers every pixel itself
      wndClass.lpszClassName = kPetWndClass;
      if (RegisterClassExW(&wndClass) == 0) {
        pet::FreeSprites(&s_sprites);
        return false;
      }
      s_classRegistered = true;
    }
    pet::VacuumInit(&s_vac, VirtualScreen(), s_sprites.size, profile.speed);
    s_eraseColor = pet::DefaultEraseColor();

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    if (s_layered) {
      // WS_EX_TRANSPARENT on a layered window: every click, on every pixel,
      // goes to whatever is underneath. The key colour makes the rest of the
      // rectangle around the frame invisible.
      exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
    }
    s_hwnd =
        CreateWindowExW(exStyle, kPetWndClass, nullptr, WS_POPUP, s_vac.pos.x, s_vac.pos.y,
                        s_sprites.size.cx, s_sprites.size.cy, nullptr, nullptr, hInst, nullptr);
    if (s_hwnd == nullptr) {
      pet::FreeSprites(&s_sprites);
      return false;
    }
    if (s_layered) {
      ResolveSetLayeredWindowAttributes()(s_hwnd, kSpriteColorKey, 0, LWA_COLORKEY);
    }
    ApplyShape();
    ApplyZOrder();                 // settle under the windows, if asked, before first showing
    ShowWindow(s_hwnd, SW_SHOWNA); // never steal focus from whatever's active
    UpdateWindow(s_hwnd);
    desktop::FindHandle();
    return true;
  }

  void Destroy() {
    if (s_hwnd != nullptr) {
      DestroyWindow(s_hwnd);
      s_hwnd = nullptr;
      // Leave the desktop as we found it: the trail was only ever paint on
      // Explorer's window, so have Explorer draw its desktop again.
      desktop::Restore();
    }
    pet::FreeSprites(&s_sprites);
    s_profile = nullptr;
  }

  void SetSpeed(int pixelsPerTick) {
    s_vac.speed = std::clamp(pixelsPerTick, pet::kSpeedMin, pet::kSpeedMax);
  }

  void SetMode(Mode mode) {
    s_mode = mode;
  }

  void SetEraseColor(COLORREF color) {
    s_eraseColor = color;
  }

  void SetUnderWindows(bool under) {
    s_underWindows = under;
    if (s_hwnd != nullptr) {
      ApplyZOrder();
    }
  }

  void UpdateBounds() {
    // The new resolution may sit on a different display-table row. If that
    // row wants another frame set or size, reload and re-fit the window to it;
    // the user's speed is theirs and stays.
    const pet::DisplayProfile& profile = ScreenProfile();
    if (s_hwnd != nullptr && s_profile != nullptr &&
        (profile.set != s_profile->set || profile.spriteSize != s_profile->spriteSize) &&
        LoadForProfile(profile)) {
      s_vac.size = s_sprites.size;
      SetWindowPos(s_hwnd, nullptr, 0, 0, s_sprites.size.cx, s_sprites.size.cy,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      ApplyShape();
    }
    pet::VacuumSetArea(&s_vac, VirtualScreen());
    s_vac.target = s_vac.pos;
    if (s_hwnd != nullptr) {
      MoveWindowTo(s_vac.pos);
    }
  }

  HWND Handle() {
    return s_hwnd;
  }

  void Tick() {
    if (s_hwnd == nullptr) {
      return;
    }
    POINT cur;
    GetCursorPos(&cur);
    switch (s_mode) {
      case MODE_FOLLOW:
        if (s_layered && !kForceFollowGap) {
          // Park centred under the cursor with the nozzle just above it. Only
          // safe because WS_EX_TRANSPARENT passes every click straight through.
          pet::VacuumSetTarget(&s_vac, cur.x - s_vac.size.cx / 2, cur.y - s_vac.size.cy + 1);
        } else {
          // Region-shaped fallback (or kForceFollowGap): HTTRANSPARENT only
          // hands a click on to windows of our own thread, so a sprite under
          // the pointer would eat every click meant for other apps. Heel just
          // below and right of the pointer instead, where it sits clear once
          // parked - it can still pass under the pointer on the way there, and
          // the bounds clamp can pin it under it at the screen's far edge.
          pet::VacuumSetTarget(&s_vac, cur.x + kFollowGap, cur.y + kFollowGap);
        }
        break;

      case MODE_RUNAWAY: {
        const int centerX = s_vac.pos.x + s_vac.size.cx / 2;
        const int centerY = s_vac.pos.y + s_vac.size.cy / 2;
        const int deltaX  = centerX - cur.x;
        const int deltaY  = centerY - cur.y;
        const double dist =
            std::sqrt(static_cast<double>(deltaX) * deltaX + static_cast<double>(deltaY) * deltaY);
        if (dist < kFleeRadius) {
          // Flee along the away-from-cursor vector to a point kFleeRadius out;
          // if the cursor is dead centre, just pick right.
          const double unitX = dist > 0.0 ? deltaX / dist : 1.0;
          const double unitY = dist > 0.0 ? deltaY / dist : 0.0;
          pet::VacuumSetTarget(&s_vac, s_vac.pos.x + static_cast<int>(unitX * kFleeRadius),
                               s_vac.pos.y + static_cast<int>(unitY * kFleeRadius));
        }
        break; // otherwise keep heading wherever it was already going
      }

      default:
        pet::VacuumSetTarget(&s_vac, s_vac.pos.x, s_vac.pos.y); // idle: stay put
        break;
    }

    const RECT vacated = pet::VacuumRect(&s_vac);
    const pet::Dir was = s_vac.facing;
    if (!pet::VacuumStep(&s_vac)) {
      return;
    }
    if (s_vac.facing != was && !s_layered) {
      // Fallback only. A turn changes the region, and reshaping in place
      // throws away the pixels Windows saved under us (SetWindowRgn is a frame
      // change, not a move), so the move that follows would invalidate the
      // desktop instead of restoring them. Hide first: that restores the saved
      // pixels synchronously; the reshape and move then happen off-screen;
      // showing again saves fresh bits under the new outline.
      ShowWindow(s_hwnd, SW_HIDE);
      SetWindowRgn(s_hwnd, pet::CloneRegion(s_sprites, s_vac.facing), FALSE);
      MoveWindowTo(s_vac.pos);
      ShowWindow(s_hwnd, SW_SHOWNA);
      UpdateWindow(s_hwnd); // paint the new facing now, not a message-loop later
    } else {
      MoveWindowTo(s_vac.pos);
      if (s_vac.facing != was) {
        InvalidateRect(s_hwnd, nullptr, FALSE); // layered: just paint the new facing
      }
    }

    // Now vacuum the whole spot we just left. Layered: the desktop was never
    // disturbed by the move, and its DC isn't clipped by us, so this simply
    // paints the trail (under the sprite too, which composites over it).
    // Fallback: the DC is clipped to what is visible, so the part under the
    // new outline is skipped and gets erased when we move on; painting the
    // full rect is what keeps the old frame's pixels from lingering.
    const bool rescan = (++s_ticksSinceRescan >= kRescanEvery);
    if (rescan) {
      s_ticksSinceRescan = 0;
    }
    desktop::EraseOnDesktop(vacated, s_eraseColor, rescan);
  }

  /* ---------------------------- Window procedure ---------------------------- */

  static LRESULT CALLBACK PetWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
      case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC hdc = BeginPaint(hWnd, &paint);
        if (s_layered) {
          // Everything the frame doesn't cover is the key colour: see-through.
          RECT client;
          GetClientRect(hWnd, &client);
          pet::EraseRect(hdc, client, kSpriteColorKey);
        }
        pet::DrawFrame(hdc, s_sprites, s_vac.facing, 0, 0);
        EndPaint(hWnd, &paint);
        return 0;
      }
      case WM_ERASEBKGND:
        return 1; // WM_PAINT covers every pixel; there is nothing to erase
      case WM_NCHITTEST:
        // Never ours to click. WS_EX_TRANSPARENT already hands the mouse to
        // whatever is underneath on 2000+; this covers our own windows (the
        // settings panel) in the fallback, and costs nothing elsewhere.
        return HTTRANSPARENT;
      case WM_SYSCOMMAND:
        // Swallow close / minimize / maximize so Alt+F4 and friends can't take
        // the sprite out from under the app.
        switch (wParam & 0xFFF0) {
          case SC_CLOSE:
          case SC_MINIMIZE:
          case SC_MAXIMIZE:
            return 0;
          default:
            break;
        }
        break;
      default:
        break;
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
  }

} // namespace petwin

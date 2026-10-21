// The sprite window: a topmost, region-shaped popup that pet_lib drives.

#include "pet_window.h"

#include "constants.h"
#include "desktop.h"
#include "globals.h"

namespace petwin {

  /* ------------------------------ Static state ------------------------------ */

  static HWND s_hwnd            = nullptr;
  static pet::Sprites s_sprites = {};
  static pet::Vacuum s_vac      = {};
  static Mode s_mode            = MODE_FOLLOW;
  static COLORREF s_eraseColor  = 0;
  static bool s_dragging        = false;
  static bool s_classRegistered = false;
  static int s_ticksSinceRescan = 0;
  static const int kRescanEvery = 50; // re-find the desktop window this often (ticks)

  // What Create was given and which display-table row the loaded frames are
  // for, kept so a resolution change can reload at the new size.
  static HINSTANCE s_hInst                       = nullptr;
  static const int (*s_frameIds)[pet::DIR_COUNT] = nullptr; // [pet::SpriteSet][pet::Dir]
  static const pet::DisplayProfile* s_profile    = nullptr;

  /* -------------------------- Forward declarations -------------------------- */

  static RECT VirtualScreen();
  static void MoveWindowTo(const POINT& pos);
  static bool LoadForProfile(const pet::DisplayProfile& profile);
  static LRESULT CALLBACK PetWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

  /* ---------------------------- Static Functions ---------------------------- */

  // Loads (or, later, reloads) the frame set a display-table row calls for,
  // scaled to its size. The old frames are only let go once the new ones are in.
  static bool LoadForProfile(const pet::DisplayProfile& profile) {
    pet::Sprites loaded = {};
    if (!pet::LoadSprites(s_hInst, s_frameIds[profile.set], profile.spriteSize, &loaded)) {
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

  /* ------------------------------- Functions -------------------------------- */

  const pet::DisplayProfile& ScreenProfile() {
    return pet::ProfileForHeight(GetSystemMetrics(SM_CYSCREEN));
  }

  bool Create(HINSTANCE hInst, const int frameIds[pet::SPRITE_SET_COUNT][pet::DIR_COUNT]) {
    if (s_hwnd != nullptr) {
      return true;
    }
    s_hInst                            = hInst;
    s_frameIds                         = frameIds;
    const pet::DisplayProfile& profile = ScreenProfile();
    if (!LoadForProfile(profile)) {
      return false;
    }
    if (!s_classRegistered) {
      WNDCLASSEXW wndClass = {};
      wndClass.cbSize      = sizeof(wndClass);
      // CS_SAVEBITS lets Windows put back what was under the sprite itself
      // where it can (show/hide, and moves on some versions) instead of asking
      // the desktop to repaint. Where it can't, EraseOnDesktop flushes that
      // repaint before painting the trail, so either way the trail wins.
      wndClass.style         = CS_SAVEBITS | CS_DBLCLKS;
      wndClass.lpfnWndProc   = PetWndProc;
      wndClass.hInstance     = hInst;
      wndClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
      wndClass.hbrBackground = nullptr; // never erased: the region clips to the sprite
      wndClass.lpszClassName = kPetWndClass;
      if (RegisterClassExW(&wndClass) == 0) {
        pet::FreeSprites(&s_sprites);
        return false;
      }
      s_classRegistered = true;
    }
    pet::VacuumInit(&s_vac, VirtualScreen(), s_sprites.size, profile.speed);
    s_eraseColor = pet::DefaultEraseColor();

    s_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kPetWndClass, nullptr, WS_POPUP,
                             s_vac.pos.x, s_vac.pos.y, s_sprites.size.cx, s_sprites.size.cy,
                             nullptr, nullptr, hInst, nullptr);
    if (s_hwnd == nullptr) {
      pet::FreeSprites(&s_sprites);
      return false;
    }
    SetWindowRgn(s_hwnd, pet::CloneRegion(s_sprites, s_vac.facing), FALSE);
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
    s_vac.speed = std::clamp(pixelsPerTick, kSpeedMin, kSpeedMax);
  }

  void SetMode(Mode mode) {
    s_mode = mode;
  }

  void SetEraseColor(COLORREF color) {
    s_eraseColor = color;
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
      SetWindowRgn(s_hwnd, pet::CloneRegion(s_sprites, s_vac.facing), TRUE);
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
    if (s_hwnd == nullptr || s_dragging) {
      return;
    }
    POINT cur;
    GetCursorPos(&cur);
    switch (s_mode) {
      case MODE_FOLLOW:
        // Park centred under the cursor with the nozzle just above it.
        pet::VacuumSetTarget(&s_vac, cur.x - s_vac.size.cx / 2, cur.y - s_vac.size.cy + 1);
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
    if (s_vac.facing != was) {
      // A turn changes the outline. Reshaping in place throws away the pixels
      // Windows saved under us (SetWindowRgn is a frame change, not a move), so
      // the move that follows would invalidate the desktop instead of restoring
      // them, and explorer would repaint its wallpaper over the spot we are about
      // to erase - a sliver of wallpaper left at every turn. Hide first: that
      // restores the saved pixels synchronously; the reshape and move then happen
      // off-screen; showing again saves fresh bits under the new outline.
      ShowWindow(s_hwnd, SW_HIDE);
      SetWindowRgn(s_hwnd, pet::CloneRegion(s_sprites, s_vac.facing), FALSE);
      MoveWindowTo(s_vac.pos);
      ShowWindow(s_hwnd, SW_SHOWNA);
      UpdateWindow(s_hwnd); // paint the new facing now, not a message-loop later
    } else {
      MoveWindowTo(s_vac.pos);
    }

    // Now vacuum the whole spot we just left. The desktop DC is clipped to
    // what is visible, so the part under the sprite's new outline is skipped
    // for free and gets erased when we move on; painting the full rect is what
    // keeps the old frame's pixels from lingering around the new one.
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
        pet::DrawFrame(hdc, s_sprites, s_vac.facing, 0, 0);
        EndPaint(hWnd, &paint);
        return 0;
      }
      case WM_ERASEBKGND:
        return 1; // the region clips to the sprite; there is nothing to erase
      case WM_LBUTTONDOWN:
        // Drag the vacuum around: hand the system's own move loop the job.
        SendMessageW(hWnd, WM_SYSCOMMAND, SC_MOVE | 0x0002, 0);
        return 0;
      case WM_ENTERSIZEMOVE:
        s_dragging = true;
        return 0;
      case WM_EXITSIZEMOVE: {
        s_dragging = false;
        RECT windowRect;
        GetWindowRect(hWnd, &windowRect);
        s_vac.pos.x  = std::clamp(windowRect.left, s_vac.bounds.left, s_vac.bounds.right);
        s_vac.pos.y  = std::clamp(windowRect.top, s_vac.bounds.top, s_vac.bounds.bottom);
        s_vac.target = s_vac.pos; // stay where dropped until the mode says otherwise
        MoveWindowTo(s_vac.pos);
        return 0;
      }
      case WM_RBUTTONUP:
      case WM_LBUTTONDBLCLK:
        // Bring the settings window back.
        if (mainHwnd != nullptr) {
          ShowWindow(mainHwnd, SW_SHOWNORMAL);
          SetForegroundWindow(mainHwnd);
        }
        return 0;
      case WM_SYSCOMMAND:
        // Swallow close / minimize / maximize so Alt+F4 and friends can't take
        // the sprite out from under the app; everything else (incl. our SC_MOVE)
        // goes to the default handler.
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

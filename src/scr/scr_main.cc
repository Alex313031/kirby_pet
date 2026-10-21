// Kirby screensaver: vacuums a snapshot of the desktop top to bottom, erasing
// it to a solid color, then starts over. Built on scrnsave.lib, which owns
// WinMain, the /s /c /p protocol, the fullscreen window, and input-to-exit;
// all we supply are ScreenSaverProc, the settings dialog, and the sweep.

#include "scr_main.h"

#include <pet_lib.h>
#include <scrnsave.h>

#include "resource.h"

/* --------------------------------- Types ---------------------------------- */

// The sweep: boustrophedon rows, each one sprite tall, alternating direction.
// PHASE_SWEEP runs a row left/right; PHASE_DOWN drops to the next one.
enum Phase { PHASE_SWEEP = 0, PHASE_DOWN = 1 };

/* ------------------------------ Static state ------------------------------ */

static pet::Sprites s_sprites = {};
static pet::Vacuum s_vac      = {};
static bool s_haveSprites     = false;

// The screen we're cleaning. s_canvas is what's shown and gets erased as we
// go; s_pristine is the untouched snapshot we restore to start a new pass.
static HDC s_canvasDC      = nullptr;
static HBITMAP s_canvas    = nullptr;
static HBITMAP s_canvasOld = nullptr;
static HBITMAP s_pristine  = nullptr;
static int s_canvasWidth   = 0;
static int s_canvasHeight  = 0;

// Where the sweep currently is.
static Phase s_phase    = PHASE_SWEEP;
static int s_row        = 0;
static bool s_rightward = true;

// Settings, loaded from the registry on WM_CREATE and on the dialog's init.
static int s_speed               = 0; // LoadSettings: the display table's default, or the saved one
static COLORREF s_customColor    = RGB(0, 0, 0);
static bool s_useDesktopColor    = true;
static COLORREF s_custColors[16] = {}; // ChooseColor's custom-palette slots

/* -------------------------- Forward declarations -------------------------- */

static COLORREF EraseColor();
static void LoadSettings(int defaultSpeed);
static void SaveSettings();
static int NumRows();
static int RowY(int row);
static int RightX();
static void StartSweep();
static bool NextLeg();
static void RestartPass(HWND hWnd);
static void Tick(HWND hWnd);
static void CreateCanvas(HWND hWnd);
static void DestroyCanvas();
static void UpdateSpeedLabel(HWND hDlg);
static void RefreshSwatch(HWND hDlg);
static void PickColor(HWND hDlg);
static INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

/* ---------------------------- Static Functions ---------------------------- */

/* ------------------------------- Settings --------------------------------- */

static COLORREF EraseColor() {
  return s_useDesktopColor ? pet::DefaultEraseColor() : s_customColor;
}

// `defaultSpeed` is what the display table says for this screen; a saved
// value, if there is one, wins over it.
static void LoadSettings(int defaultSpeed) {
  DWORD value = 0;
  s_speed     = std::clamp(defaultSpeed, kSpeedMin, kSpeedMax);
  if (pet::RegReadDword(kRegSubkey, kRegSpeed, &value)) {
    s_speed = std::clamp(static_cast<int>(value), kSpeedMin, kSpeedMax);
  }
  if (pet::RegReadDword(kRegSubkey, kRegEraseColor, &value)) {
    s_customColor = static_cast<COLORREF>(value);
  }
  s_useDesktopColor = true;
  if (pet::RegReadDword(kRegSubkey, kRegUseDesktop, &value)) {
    s_useDesktopColor = (value != 0);
  }
}

static void SaveSettings() {
  pet::RegWriteDword(kRegSubkey, kRegSpeed, static_cast<DWORD>(s_speed));
  pet::RegWriteDword(kRegSubkey, kRegEraseColor, static_cast<DWORD>(s_customColor));
  pet::RegWriteDword(kRegSubkey, kRegUseDesktop, s_useDesktopColor ? 1u : 0u);
}

/* --------------------------------- Sweep ---------------------------------- */

static int NumRows() {
  const int spriteHeight = s_vac.size.cy;
  return spriteHeight > 0 ? (s_canvasHeight + spriteHeight - 1) / spriteHeight : 1;
}

// Top of row `r`; the last row is pulled up so the sprite's bottom edge meets
// the screen's (the bounds clamp handles a window shorter than the sprite).
static int RowY(int row) {
  const int spriteHeight = s_vac.size.cy;
  return (row >= NumRows() - 1) ? (s_canvasHeight - spriteHeight) : row * spriteHeight;
}

static int RightX() {
  return s_canvasWidth - s_vac.size.cx;
}

static void StartSweep() {
  s_row           = 0;
  s_rightward     = true;
  s_phase         = PHASE_SWEEP;
  s_vac.pos.x     = 0;
  s_vac.pos.y     = 0;
  const RECT area = {0, 0, s_canvasWidth, s_canvasHeight};
  pet::VacuumSetArea(&s_vac, area); // re-clamps pos into bounds
  pet::VacuumSetTarget(&s_vac, RightX(), RowY(0));
}

// Advances to the next leg of the sweep. False once the last row is done.
static bool NextLeg() {
  if (s_phase == PHASE_SWEEP) {
    if (s_row >= NumRows() - 1) {
      return false; // that was the last row
    }
    ++s_row;
    s_phase = PHASE_DOWN;
    pet::VacuumSetTarget(&s_vac, s_vac.pos.x, RowY(s_row));
  } else {
    s_rightward = !s_rightward;
    s_phase     = PHASE_SWEEP;
    pet::VacuumSetTarget(&s_vac, s_rightward ? RightX() : 0, s_vac.pos.y);
  }
  return true;
}

// Puts the original desktop back on the canvas and starts cleaning again.
static void RestartPass(HWND hWnd) {
  HDC mem = CreateCompatibleDC(s_canvasDC);
  if (mem != nullptr) {
    HGDIOBJ old = SelectObject(mem, s_pristine);
    BitBlt(s_canvasDC, 0, 0, s_canvasWidth, s_canvasHeight, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
  }
  StartSweep();
  InvalidateRect(hWnd, nullptr, FALSE);
}

static void Tick(HWND hWnd) {
  // Vacuum what's under us now, then move on; the spot we leave stays clean.
  const RECT before = pet::VacuumRect(&s_vac);
  pet::EraseRect(s_canvasDC, before, EraseColor());
  if (!pet::VacuumStep(&s_vac)) {
    // Arrived (or pinned): line up the next leg; the next tick moves along it.
    if (!NextLeg()) {
      RestartPass(hWnd);
      return;
    }
  }
  // Repaint just what changed: the old footprint (now erased) and the new one.
  const RECT after = pet::VacuumRect(&s_vac);
  RECT dirty;
  UnionRect(&dirty, &before, &after);
  InvalidateRect(hWnd, &dirty, FALSE);
}

/* --------------------------------- Canvas --------------------------------- */

static void CreateCanvas(HWND hWnd) {
  RECT clientRect;
  GetClientRect(hWnd, &clientRect);
  s_canvasWidth  = clientRect.right - clientRect.left;
  s_canvasHeight = clientRect.bottom - clientRect.top;

  // Snapshot the live screen before our window paints over it (WM_CREATE
  // runs before the window is shown). The /p preview window is tiny, so there
  // the whole primary screen is stretched into it.
  HDC screen = GetDC(nullptr);
  if (screen == nullptr) {
    return;
  }
  s_canvasDC = CreateCompatibleDC(screen);
  s_canvas   = CreateCompatibleBitmap(screen, s_canvasWidth, s_canvasHeight);
  s_pristine = CreateCompatibleBitmap(screen, s_canvasWidth, s_canvasHeight);
  if (s_canvasDC != nullptr && s_canvas != nullptr && s_pristine != nullptr) {
    s_canvasOld = static_cast<HBITMAP>(SelectObject(s_canvasDC, s_canvas));
    if (fChildPreview) {
      StretchBlt(s_canvasDC, 0, 0, s_canvasWidth, s_canvasHeight, screen, 0, 0,
                 GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SRCCOPY);
    } else {
      // Copy from where the window actually sits: the screen DC's origin is the
      // primary monitor's top-left, so a saver window spanning a virtual screen
      // with a monitor left of/above the primary starts at negative coordinates.
      RECT win;
      GetWindowRect(hWnd, &win);
      BitBlt(s_canvasDC, 0, 0, s_canvasWidth, s_canvasHeight, screen, win.left, win.top, SRCCOPY);
    }
    // Keep an untouched copy to restore from when a pass completes.
    HDC mem = CreateCompatibleDC(screen);
    if (mem != nullptr) {
      HGDIOBJ old = SelectObject(mem, s_pristine);
      BitBlt(mem, 0, 0, s_canvasWidth, s_canvasHeight, s_canvasDC, 0, 0, SRCCOPY);
      SelectObject(mem, old);
      DeleteDC(mem);
    }
  }
  ReleaseDC(nullptr, screen);
}

static void DestroyCanvas() {
  if (s_canvasDC != nullptr) {
    SelectObject(s_canvasDC, s_canvasOld);
    DeleteDC(s_canvasDC);
    s_canvasDC = nullptr;
  }
  if (s_canvas != nullptr) {
    DeleteObject(s_canvas);
    s_canvas = nullptr;
  }
  if (s_pristine != nullptr) {
    DeleteObject(s_pristine);
    s_pristine = nullptr;
  }
}

/* --------------------------- Settings dialog UI --------------------------- */

static void UpdateSpeedLabel(HWND hDlg) {
  wchar_t buf[32];
  wsprintfW(buf, L"Speed: %d", s_speed);
  SetDlgItemTextW(hDlg, IDC_SPEED_LABEL, buf);
}

static void RefreshSwatch(HWND hDlg) {
  InvalidateRect(GetDlgItem(hDlg, IDC_SWATCH), nullptr, TRUE);
}

static void PickColor(HWND hDlg) {
  CHOOSECOLORW chooser = {};
  chooser.lStructSize  = sizeof(chooser);
  chooser.hwndOwner    = hDlg;
  chooser.rgbResult    = s_customColor;
  chooser.lpCustColors = s_custColors;
  chooser.Flags        = CC_RGBINIT | CC_FULLOPEN;
  if (!ChooseColorW(&chooser)) {
    return; // cancelled
  }
  s_customColor     = chooser.rgbResult;
  s_useDesktopColor = false; // picking a color implies "not the desktop's"
  CheckDlgButton(hDlg, IDC_DESKTOPCOLOR, BST_UNCHECKED);
  EnableWindow(GetDlgItem(hDlg, IDC_COLOR), TRUE);
  RefreshSwatch(hDlg);
}

/* ------------------------------ About dialog ------------------------------ */

static INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
    case WM_INITDIALOG: {
      static const HICON kAboutIcon = LoadIconW(hMainInstance, MAKEINTRESOURCEW(IDI_MAIN));
      SendMessageW(hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(kAboutIcon));
      SendMessageW(hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(kAboutIcon));
      return TRUE;
    }
    case WM_CLOSE:
      EndDialog(hDlg, TRUE);
      return TRUE;
    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
        EndDialog(hDlg, LOWORD(wParam));
        return TRUE;
      }
      break;
    default:
      break;
  }
  return FALSE;
}

/* ------------------------------- Functions -------------------------------- */

// The three entry points scrnsave.lib calls into. Everything above is ours.

LRESULT WINAPI ScreenSaverProcW(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE: {
      CreateCanvas(hWnd);
      // The display table picks the frame set, the size to scale it to and the
      // default speed from the height we are drawing into: the real screen for
      // /s, the tiny Display Properties monitor for /p (its smallest row).
      const pet::DisplayProfile& profile = pet::ProfileForHeight(s_canvasHeight);
      LoadSettings(profile.speed);
      static const int kFrameIds[pet::SPRITE_SET_COUNT][pet::DIR_COUNT] = {
          {IDB_PET_UP_1X, IDB_PET_DOWN_1X, IDB_PET_LEFT_1X, IDB_PET_RIGHT_1X},
          {IDB_PET_UP_2X, IDB_PET_DOWN_2X, IDB_PET_LEFT_2X, IDB_PET_RIGHT_2X}};
      s_haveSprites =
          pet::LoadSprites(hMainInstance, kFrameIds[profile.set], profile.spriteSize, &s_sprites);
      // Without sprites we still sweep (an invisible vacuum still cleans).
      const SIZE size =
          s_haveSprites ? s_sprites.size : SIZE{profile.spriteSize, profile.spriteSize};
      const RECT area = {0, 0, s_canvasWidth, s_canvasHeight};
      pet::VacuumInit(&s_vac, area, size, s_speed);
      StartSweep();
      SetTimer(hWnd, kTickTimerId, kTickMs, nullptr);
      break; // DefScreenSaverProc wants to see WM_CREATE as well
    }
    case WM_TIMER:
      if (wParam == kTickTimerId && s_canvasDC != nullptr) {
        Tick(hWnd);
      }
      return 0;
    case WM_ERASEBKGND:
      return 1; // the canvas covers everything
    case WM_PAINT: {
      PAINTSTRUCT paint;
      HDC hdc = BeginPaint(hWnd, &paint);
      if (s_canvasDC != nullptr) {
        const RECT& paintRect = paint.rcPaint;
        BitBlt(hdc, paintRect.left, paintRect.top, paintRect.right - paintRect.left,
               paintRect.bottom - paintRect.top, s_canvasDC, paintRect.left, paintRect.top,
               SRCCOPY);
        if (s_haveSprites) {
          pet::DrawVacuum(hdc, s_sprites, s_vac); // honours the paint clip
        }
      }
      EndPaint(hWnd, &paint);
      return 0;
    }
    case WM_DESTROY:
      KillTimer(hWnd, kTickTimerId);
      if (s_haveSprites) {
        pet::FreeSprites(&s_sprites);
        s_haveSprites = false;
      }
      DestroyCanvas();
      break; // DefScreenSaverProc posts the quit message
    default:
      break;
  }
  return DefScreenSaverProc(hWnd, message, wParam, lParam);
}

// scrnsave.lib runs this for /c. Note the WINBOOL return: that's why the
// swatch is an owner-drawn button answered via WM_DRAWITEM rather than a
// STATIC answered with an HBRUSH from WM_CTLCOLORSTATIC (an HBRUSH wouldn't
// survive the trip through a 32-bit return on x64).
WINBOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_INITDIALOG: {
      LoadSettings(pet::ProfileForHeight(GetSystemMetrics(SM_CYSCREEN)).speed);
      HWND slider = GetDlgItem(hDlg, IDC_SPEED);
      SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(kSpeedMin, kSpeedMax));
      SendMessageW(slider, TBM_SETTICFREQ, 4, 0);
      SendMessageW(slider, TBM_SETPOS, TRUE, s_speed);
      UpdateSpeedLabel(hDlg);
      CheckDlgButton(hDlg, IDC_DESKTOPCOLOR, s_useDesktopColor ? BST_CHECKED : BST_UNCHECKED);
      EnableWindow(GetDlgItem(hDlg, IDC_COLOR), s_useDesktopColor ? FALSE : TRUE);
      return TRUE;
    }
    case WM_HSCROLL:
      if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hDlg, IDC_SPEED)) {
        s_speed = static_cast<int>(SendMessageW(reinterpret_cast<HWND>(lParam), TBM_GETPOS, 0, 0));
        UpdateSpeedLabel(hDlg);
      }
      return TRUE;
    case WM_DRAWITEM: {
      const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
      if (dis != nullptr && dis->CtlID == IDC_SWATCH) {
        pet::EraseRect(dis->hDC, dis->rcItem, EraseColor());
        FrameRect(dis->hDC, &dis->rcItem, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        return TRUE;
      }
      break;
    }
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDC_DESKTOPCOLOR:
          s_useDesktopColor = (IsDlgButtonChecked(hDlg, IDC_DESKTOPCOLOR) == BST_CHECKED);
          EnableWindow(GetDlgItem(hDlg, IDC_COLOR), s_useDesktopColor ? FALSE : TRUE);
          RefreshSwatch(hDlg);
          return TRUE;
        case IDC_COLOR:
        case IDC_SWATCH: // clicking the swatch is a shortcut to the picker
          PickColor(hDlg);
          return TRUE;
        case IDC_ABOUT:
          DialogBoxW(hMainInstance, MAKEINTRESOURCEW(IDD_ABOUTDLG), hDlg, AboutDlgProc);
          return TRUE;
        case IDOK:
          SaveSettings();
          EndDialog(hDlg, TRUE);
          return TRUE;
        case IDCANCEL:
          EndDialog(hDlg, FALSE);
          return TRUE;
        default:
          break;
      }
      break;
    default:
      break;
  }
  return FALSE;
}

// scrnsave.lib calls this before the settings dialog; the trackbar needs the
// common-controls bar classes registered.
WINBOOL WINAPI RegisterDialogClasses(HANDLE hInst) {
  UNREFERENCED_PARAMETER(hInst);
  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(icex);
  icex.dwICC  = ICC_BAR_CLASSES;
  if (!InitCommonControlsEx(&icex)) {
    return FALSE;
  }
  return TRUE;
}

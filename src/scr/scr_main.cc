// Kirby screensaver: vacuums a snapshot of the desktop away to a solid colour,
// then paints it back, then vacuums it again - each pass sweeping the whole
// screen in rows from a random direction. Built on scrnsave.lib, which owns
// WinMain, the /s /c /p protocol, the fullscreen window, and input-to-exit;
// all we supply are ScreenSaverProc, the settings dialog, and the sweep.

#include "scr_main.h"

#include <pet_lib.h>
#include <scrnsave.h>

#include <cstdlib> // rand / srand

#include "resource.h"

/* --------------------------------- Types ---------------------------------- */

// Within a pass: PHASE_SWEEP runs along the current row, PHASE_STEP crosses
// over to the next one. PHASE_TRANSIT is the run along an edge to the corner
// the next pass starts from.
enum Phase { PHASE_SWEEP = 0, PHASE_STEP = 1, PHASE_TRANSIT = 2 };

// One way the next pass can begin: from the corner at (startX, startY) -
// where the vacuum already is, or the far end of an edge it runs along first -
// with rows advancing in `progress`.
struct PassChoice {
  int startX;
  int startY;
  pet::Dir progress;
};

/* ------------------------------ Static state ------------------------------ */

static pet::Sprites s_sprites = {};
static pet::Vacuum s_vac      = {};
static bool s_haveSprites     = false;

// The screen we're working on. s_canvas is what's shown and what the vacuum
// erases from and paints onto; s_pristine is the untouched snapshot, kept
// selected into its own DC so painting the desktop back is one BitBlt.
static HDC s_canvasDC        = nullptr;
static HBITMAP s_canvas      = nullptr;
static HBITMAP s_canvasOld   = nullptr;
static HDC s_pristineDC      = nullptr;
static HBITMAP s_pristine    = nullptr;
static HBITMAP s_pristineOld = nullptr;
static int s_canvasWidth     = 0;
static int s_canvasHeight    = 0;

// The sweep. A pass covers the whole screen in boustrophedon rows that
// advance in one of the four directions: DOWN / UP means horizontal rows
// stepping down / up the screen, LEFT / RIGHT means vertical columns stepping
// left / right across it. Within a pass it is the usual row, turn, row. A pass
// ends when the rows reach the far edge, which leaves the vacuum in a corner.
// The next pass is chosen at random from everything possible there: a pass
// straight from that corner, or a run along either edge to the neighbouring
// corner and a pass from there - six choices, of which the plain reversal of
// the last pass is only one. Passes flip between vacuuming the desktop away
// and painting it back; a transit already works in the new pass's mode.
static pet::Dir s_progress        = pet::DIR_DOWN; // which way the rows advance
static pet::Dir s_pendingProgress = pet::DIR_DOWN; // the pass to run once a transit arrives
static bool s_sweepForward        = true;          // current row runs toward +x (+y for columns)
static int s_row                  = 0;             // row index along the progression
static Phase s_phase              = PHASE_SWEEP;
static bool s_painting            = false; // false: erase to the colour; true: restore the desktop

// Speed, erase colour and the desktop-colour switch, as the library keeps
// them, plus our own sound switch; loaded on WM_CREATE and on the dialog's
// init.
static pet::EraseSettings s_erase = {};
static bool s_playSound           = true;

/* -------------------------- Forward declarations -------------------------- */

static int MaxX();
static int MaxY();
static bool RowsAreHorizontal();
static int NumRows();
static int RowCoord(int row);
static int SweepEnd();
static void AimAlongRow();
static void AimAtNextRow();
static void AddCornerChoices(int cornerX, int cornerY, PassChoice* choices, int* count);
static void BeginRows(pet::Dir progress);
static void StartPass(bool firstPass);
static bool NextLeg();
static void PaintFootprint(const RECT& rect);
static void Tick(HWND hWnd);
static void CreateCanvas(HWND hWnd);
static void DestroyCanvas();
static void LoadSettings(int defaultSpeed);
static void StartSound();
static void StopSound();
static void UpdateSpeedLabel(HWND hDlg);
static void RefreshSwatch(HWND hDlg);
static void PickColor(HWND hDlg);
static INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

/* ---------------------------- Static Functions ---------------------------- */

/* --------------------------------- Sweep ---------------------------------- */

// The far corner's coordinates: where the sprite's right / bottom edge meets
// the screen's. Never negative, so a screen smaller than the sprite still
// has a (single) corner.
static int MaxX() {
  return std::max(0, s_canvasWidth - static_cast<int>(s_vac.size.cx));
}

static int MaxY() {
  return std::max(0, s_canvasHeight - static_cast<int>(s_vac.size.cy));
}

// Rows run across the direction of progress: horizontal rows when the pass
// advances down or up, vertical columns when it advances left or right.
static bool RowsAreHorizontal() {
  return s_progress == pet::DIR_DOWN || s_progress == pet::DIR_UP;
}

// How many rows it takes a pass to cover the screen.
static int NumRows() {
  const int span = RowsAreHorizontal() ? s_canvasHeight : s_canvasWidth;
  const int step = RowsAreHorizontal() ? s_vac.size.cy : s_vac.size.cx;
  return step > 0 ? (span + step - 1) / step : 1;
}

// Where row `row` sits along the progression: its top for horizontal rows,
// its left for columns. The last row is pulled flush with the far edge so a
// pass always ends exactly in a corner (the bounds clamp copes with a screen
// smaller than the sprite).
static int RowCoord(int row) {
  const int span     = RowsAreHorizontal() ? s_canvasHeight : s_canvasWidth;
  const int step     = RowsAreHorizontal() ? s_vac.size.cy : s_vac.size.cx;
  const int farEdge  = span - step;
  const int offset   = (row >= NumRows() - 1) ? farEdge : row * step;
  const bool forward = (s_progress == pet::DIR_DOWN || s_progress == pet::DIR_RIGHT);
  return forward ? offset : farEdge - offset;
}

// The end of the current row in the direction it is being swept.
static int SweepEnd() {
  const int span = RowsAreHorizontal() ? s_canvasWidth : s_canvasHeight;
  const int step = RowsAreHorizontal() ? s_vac.size.cx : s_vac.size.cy;
  return s_sweepForward ? (span - step) : 0;
}

// Point the vacuum at the far end of the current row.
static void AimAlongRow() {
  if (RowsAreHorizontal()) {
    pet::VacuumSetTarget(&s_vac, SweepEnd(), RowCoord(s_row));
  } else {
    pet::VacuumSetTarget(&s_vac, RowCoord(s_row), SweepEnd());
  }
}

// Point the vacuum across to the current row from where it is: the step
// between rows, perpendicular to the sweep.
static void AimAtNextRow() {
  if (RowsAreHorizontal()) {
    pet::VacuumSetTarget(&s_vac, s_vac.pos.x, RowCoord(s_row));
  } else {
    pet::VacuumSetTarget(&s_vac, RowCoord(s_row), s_vac.pos.y);
  }
}

// The pass directions that lead into the screen from the corner at
// (cornerX, cornerY): down from the top edge, up from the bottom, right from
// the left, left from the right. Appends one choice per direction.
static void AddCornerChoices(int cornerX, int cornerY, PassChoice* choices, int* count) {
  if (cornerY <= 0) {
    choices[(*count)++] = {cornerX, cornerY, pet::DIR_DOWN};
  }
  if (cornerY >= MaxY()) {
    choices[(*count)++] = {cornerX, cornerY, pet::DIR_UP};
  }
  if (cornerX <= 0) {
    choices[(*count)++] = {cornerX, cornerY, pet::DIR_RIGHT};
  }
  if (cornerX >= MaxX()) {
    choices[(*count)++] = {cornerX, cornerY, pet::DIR_LEFT};
  }
}

// Lays out a pass advancing in `progress` from where the vacuum is and aims
// it along the first row, which runs away from whichever side it is on.
static void BeginRows(pet::Dir progress) {
  s_progress     = progress;
  s_sweepForward = RowsAreHorizontal() ? (s_vac.pos.x <= 0) : (s_vac.pos.y <= 0);
  s_row          = 0;
  s_phase        = PHASE_SWEEP;
  AimAlongRow();
}

// Chooses the next pass at random from everything possible where the vacuum
// is, flipping between vacuuming and painting, and sets off - either straight
// into the rows or along an edge to the corner they start from. The first
// pass starts in a random corner and vacuums.
static void StartPass(bool firstPass) {
  if (firstPass) {
    s_vac.pos.x = (std::rand() & 1) ? MaxX() : 0;
    s_vac.pos.y = (std::rand() & 1) ? MaxY() : 0;
    s_painting  = false;
  } else {
    s_painting = !s_painting;
  }
  const RECT area = {0, 0, s_canvasWidth, s_canvasHeight};
  pet::VacuumSetArea(&s_vac, area); // re-clamps pos into bounds
  const int hereX = s_vac.pos.x;
  const int hereY = s_vac.pos.y;

  // Every way a pass can begin from here: straight from this corner, or a run
  // along either edge to the neighbouring corner and a pass from there. Each
  // corner offers two directions, so that is six choices rather than two,
  // and the plain reversal of the last pass is only one of them.
  PassChoice choices[12];
  int count = 0;
  AddCornerChoices(hereX, hereY, choices, &count);
  const int otherX = (hereX <= 0) ? MaxX() : 0;
  const int otherY = (hereY <= 0) ? MaxY() : 0;
  if (otherX != hereX) {
    AddCornerChoices(otherX, hereY, choices, &count);
  }
  if (otherY != hereY) {
    AddCornerChoices(hereX, otherY, choices, &count);
  }
  if (count == 0) {
    // Not on any edge - can't happen after a pass, but never leave it stuck.
    choices[count++] = {hereX, hereY, pet::DIR_DOWN};
    choices[count++] = {hereX, hereY, pet::DIR_UP};
    choices[count++] = {hereX, hereY, pet::DIR_LEFT};
    choices[count++] = {hereX, hereY, pet::DIR_RIGHT};
  }
  const PassChoice& pick = choices[std::rand() % count];
  if (pick.startX == hereX && pick.startY == hereY) {
    BeginRows(pick.progress);
  } else {
    // Run along the edge to that corner first, working as we go; NextLeg
    // begins the rows on arrival.
    s_pendingProgress = pick.progress;
    s_phase           = PHASE_TRANSIT;
    pet::VacuumSetTarget(&s_vac, pick.startX, pick.startY);
  }
}

// Advances to the next leg of the pass. False once the last row is done.
static bool NextLeg() {
  if (s_phase == PHASE_TRANSIT) {
    BeginRows(s_pendingProgress); // at the corner the pass starts from
    return true;
  }
  if (s_phase == PHASE_SWEEP) {
    if (s_row >= NumRows() - 1) {
      return false; // that was the last row: we're in a corner
    }
    ++s_row;
    s_phase = PHASE_STEP;
    AimAtNextRow();
  } else {
    s_sweepForward = !s_sweepForward;
    s_phase        = PHASE_SWEEP;
    AimAlongRow();
  }
  return true;
}

// What the vacuum does to the spot under it: vacuums it away to the erase
// colour, or on a painting pass puts that piece of the desktop back.
static void PaintFootprint(const RECT& rect) {
  if (s_painting && s_pristineDC != nullptr) {
    BitBlt(s_canvasDC, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
           s_pristineDC, rect.left, rect.top, SRCCOPY);
  } else {
    pet::EraseRect(s_canvasDC, rect, pet::EffectiveEraseColor(s_erase));
  }
}

static void Tick(HWND hWnd) {
  // Work on what's under us now, then move on; the spot we leave stays done.
  const RECT before = pet::VacuumRect(&s_vac);
  PaintFootprint(before);
  if (!pet::VacuumStep(&s_vac)) {
    // Arrived: line up the next leg; the next tick moves along it. Out of
    // legs means the pass is over: turn around and start the next.
    if (!NextLeg()) {
      StartPass(false);
    }
  }
  // Repaint just what changed: the old footprint (now done) and the new one.
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
  s_canvasDC   = CreateCompatibleDC(screen);
  s_pristineDC = CreateCompatibleDC(screen);
  s_canvas     = CreateCompatibleBitmap(screen, s_canvasWidth, s_canvasHeight);
  s_pristine   = CreateCompatibleBitmap(screen, s_canvasWidth, s_canvasHeight);
  if (s_canvasDC != nullptr && s_pristineDC != nullptr && s_canvas != nullptr &&
      s_pristine != nullptr) {
    s_canvasOld   = static_cast<HBITMAP>(SelectObject(s_canvasDC, s_canvas));
    s_pristineOld = static_cast<HBITMAP>(SelectObject(s_pristineDC, s_pristine));
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
    // The untouched copy the painting passes draw from.
    BitBlt(s_pristineDC, 0, 0, s_canvasWidth, s_canvasHeight, s_canvasDC, 0, 0, SRCCOPY);
  }
  ReleaseDC(nullptr, screen);
}

static void DestroyCanvas() {
  if (s_canvasDC != nullptr) {
    SelectObject(s_canvasDC, s_canvasOld);
    DeleteDC(s_canvasDC);
    s_canvasDC = nullptr;
  }
  if (s_pristineDC != nullptr) {
    SelectObject(s_pristineDC, s_pristineOld);
    DeleteDC(s_pristineDC);
    s_pristineDC = nullptr;
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

/* ------------------------------- Settings --------------------------------- */

// Everything persisted: the library's erase settings, with `defaultSpeed`
// (the display table's) as the fallback, and our sound switch, on by default.
static void LoadSettings(int defaultSpeed) {
  pet::LoadEraseSettings(kRegSubkey, defaultSpeed, &s_erase);
  DWORD value = 0;
  s_playSound = true;
  if (pet::RegReadDword(kRegSubkey, kRegPlaySound, &value)) {
    s_playSound = (value != 0);
  }
}

/* --------------------------------- Sound ---------------------------------- */

// The Tradition, running, for as long as the saver is up - if the user wants
// it. The system loops the WAVE resource itself (Microsoft ADPCM, which every
// Windows plays), so there is nothing to feed or time. Fails silently on a
// machine with no sound device, and never substitutes the default system
// sound.
static void StartSound() {
  if (!s_playSound) {
    return;
  }
  PlaySoundW(MAKEINTRESOURCEW(IDR_3CB_WAV), hMainInstance,
             SND_RESOURCE | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
}

static void StopSound() {
  PlaySoundW(nullptr, nullptr, 0);
}

/* --------------------------- Settings dialog UI --------------------------- */

static void UpdateSpeedLabel(HWND hDlg) {
  wchar_t buf[32];
  wsprintfW(buf, L"Speed: %d", s_erase.speed);
  SetDlgItemTextW(hDlg, IDC_SPEED_LABEL, buf);
}

static void RefreshSwatch(HWND hDlg) {
  InvalidateRect(GetDlgItem(hDlg, IDC_SWATCH), nullptr, TRUE);
}

static void PickColor(HWND hDlg) {
  if (!pet::PickEraseColor(hDlg, &s_erase)) {
    return; // cancelled
  }
  // The pick switched the desktop-colour setting off; show that.
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
      if (fChildPreview) {
        // The preview is a ~150x110 box: the slider's speed (saved, or the
        // table's default) is a blur in it, so run at half, never below the min.
        s_erase.speed = std::max(pet::kSpeedMin, s_erase.speed / 2);
      }
      static const pet::FrameIds kFrameIds = {
          {IDB_PET_UP_1X, IDB_PET_DOWN_1X, IDB_PET_LEFT_1X, IDB_PET_RIGHT_1X},
          {IDB_PET_UP_2X, IDB_PET_DOWN_2X, IDB_PET_LEFT_2X, IDB_PET_RIGHT_2X}};
      s_haveSprites =
          pet::LoadSpritesForProfile(hMainInstance, kFrameIds, profile, &s_sprites) == pet::LOAD_OK;
      // Without sprites we still sweep (an invisible vacuum still cleans).
      const SIZE size =
          s_haveSprites ? s_sprites.size : SIZE{profile.spriteSize, profile.spriteSize};
      const RECT area = {0, 0, s_canvasWidth, s_canvasHeight};
      pet::VacuumInit(&s_vac, area, size, s_erase.speed);
      // Seed the corner and direction choices; two savers started a second
      // apart (the preview and the real thing) shouldn't look identical.
      std::srand(static_cast<unsigned int>(GetTickCount() ^ GetCurrentProcessId()));
      StartPass(/*firstPass=*/true);
      SetTimer(hWnd, kTickTimerId, kTickMs, nullptr);
      if (!fChildPreview) {
        StartSound(); // the real thing only; the Display Properties preview stays quiet
      }
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
      StopSound(); // harmless if nothing is playing (the preview)
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
      SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(pet::kSpeedMin, pet::kSpeedMax));
      SendMessageW(slider, TBM_SETTICFREQ, 4, 0);
      SendMessageW(slider, TBM_SETPOS, TRUE, s_erase.speed);
      UpdateSpeedLabel(hDlg);
      CheckDlgButton(hDlg, IDC_DESKTOPCOLOR, s_erase.useDesktopColor ? BST_CHECKED : BST_UNCHECKED);
      EnableWindow(GetDlgItem(hDlg, IDC_COLOR), s_erase.useDesktopColor ? FALSE : TRUE);
      CheckDlgButton(hDlg, IDC_PLAYSOUND, s_playSound ? BST_CHECKED : BST_UNCHECKED);
      return TRUE;
    }
    case WM_HSCROLL:
      if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hDlg, IDC_SPEED)) {
        s_erase.speed =
            static_cast<int>(SendMessageW(reinterpret_cast<HWND>(lParam), TBM_GETPOS, 0, 0));
        UpdateSpeedLabel(hDlg);
      }
      return TRUE;
    case WM_DRAWITEM: {
      const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
      if (dis != nullptr && dis->CtlID == IDC_SWATCH) {
        pet::EraseRect(dis->hDC, dis->rcItem, pet::EffectiveEraseColor(s_erase));
        FrameRect(dis->hDC, &dis->rcItem, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        return TRUE;
      }
      break;
    }
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDC_DESKTOPCOLOR:
          s_erase.useDesktopColor = (IsDlgButtonChecked(hDlg, IDC_DESKTOPCOLOR) == BST_CHECKED);
          EnableWindow(GetDlgItem(hDlg, IDC_COLOR), s_erase.useDesktopColor ? FALSE : TRUE);
          RefreshSwatch(hDlg);
          return TRUE;
        case IDC_COLOR:
        case IDC_SWATCH: // clicking the swatch is a shortcut to the picker
          PickColor(hDlg);
          return TRUE;
        case IDC_PLAYSOUND:
          s_playSound = (IsDlgButtonChecked(hDlg, IDC_PLAYSOUND) == BST_CHECKED);
          return TRUE;
        case IDC_ABOUT:
          DialogBoxW(hMainInstance, MAKEINTRESOURCEW(IDD_ABOUTDLG), hDlg, AboutDlgProc);
          return TRUE;
        case IDOK:
          pet::SaveEraseSettings(kRegSubkey, s_erase);
          pet::RegWriteDword(kRegSubkey, kRegPlaySound, s_playSound ? 1u : 0u);
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

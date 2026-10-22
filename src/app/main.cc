/*------------------------------------------
   Kirby Pet Win32
   Copyright (c) 2026 Alex313031
  ------------------------------------------*/

#include "main.h"

#include <pet_lib.h>

#include "constants.h"
#include "pet_window.h"
#include "resource.h"
#include "utils.h"

/* -------------------------------- Statics --------------------------------- */

static std::string winever = ""; // For storing Wine version if applicable

// Shared font for every child control; created in CreateChildControls and
// freed in WM_DESTROY.
static HFONT s_uiFont = nullptr;

// Settings-window child controls.
static HWND s_speedLabel   = nullptr;
static HWND s_speedSlider  = nullptr;
static HWND s_followCheck  = nullptr;
static HWND s_runawayCheck = nullptr;
static HWND s_underCheck   = nullptr;
static HWND s_eraseLabel   = nullptr;
static HWND s_desktopCheck = nullptr;
static HWND s_colorButton  = nullptr;
static HWND s_swatch       = nullptr;

// Speed, erase colour and the desktop-colour switch, as the library keeps
// them. The swatch is painted with s_swatchBrush, which is rebuilt whenever
// the effective colour changes.
static pet::EraseSettings s_erase = {};
static HBRUSH s_swatchBrush       = nullptr;

static petwin::Mode s_mode = petwin::MODE_FOLLOW;
static bool s_underWindows = false; // the vacuum travels under other windows
static bool s_inTray       = false;

// Set while WM_CLOSE minimizes the window to the taskbar because the tray
// refused our icon: the WM_SIZE that minimize sends would otherwise try the
// tray a second time and warn about it twice.
static bool s_minimizeIsFallback = false;

// True once LoadSettings has filled the controls and the statics from the
// registry. SaveSettings writes nothing before that, so a window torn down
// part-way through WM_CREATE can't persist a blank slider as speed 0.
static bool s_settingsLoaded = false;

// Tray icon plumbing. TaskbarCreated is the shell's "Explorer (re)started"
// broadcast: every tray icon is gone then and has to be added again.
static constexpr UINT WM_TRAYICON = WM_APP + 1;
static constexpr UINT kTrayIconId = 1;
static UINT s_taskbarCreatedMsg   = 0;

/* -------------------------------- Globals --------------------------------- */

// When true, minimize to tray
bool tray_mode = false;

HWND mainHwnd = nullptr;

HINSTANCE g_hInstance = nullptr;

bool g_debug_mode = is_debug;
// CLI flags. Set by ParseCommandLine before InitLogging runs so the log
// sink picks up --debug, and so --version / --help can short-circuit
// wWinMain before the window is created.
bool g_show_version = false;
bool g_show_help    = false;

// Store handles to main icon since commonly used
HICON kMainIcon  = nullptr;
HICON kSmallIcon = nullptr;

bool is_on_wine = false; // Whether we are on wine

/* -------------------------- Forward declarations -------------------------- */

static bool ParseCommandLine(int argc, LPWSTR argv[]);
static int ShowVersionAndExit();
static int ShowHelpAndExit();
static HWND MakeControl(HWND hWnd,
                        LPCWSTR cls,
                        LPCWSTR text,
                        DWORD style,
                        DWORD exStyle,
                        int ctrlId);
static bool CreateChildControls(HWND hWnd);
static bool LayoutChildren(HWND hWnd);
static void LoadSettings(HWND hWnd);
static bool SaveSettings();
static bool ResetSettings(HWND hWnd);
static COLORREF CurrentEraseColor();
static bool RefreshSwatch();
static void ApplySettingsToPet();
static void ApplyMode(petwin::Mode mode);
static void UpdateSpeedLabel();
static void PickColor(HWND hWnd);
static bool AddTrayIcon(HWND hWnd);
static bool RemoveTrayIcon(HWND hWnd);
static void ShowTrayMenu(HWND hWnd);
static bool MinimizeToTray(HWND hWnd);
static void RestoreFromTray(HWND hWnd);

/* ---------------------------- Static Functions ---------------------------- */

/* ------------------------------ Command line ------------------------------ */

// Walks the wchar_t argv produced by CommandLineToArgvW and flips any of
// g_debug_mode / g_show_version / g_show_help that the user passed. Each
// flag accepts the common Win32 / Unix variants (--foo, -foo, -f, /f) so
// we work the same way from PowerShell, cmd.exe, and a Unix shell under
// Wine. Returns false only when argv itself is null (i.e. the system
// failed to split the command line), so wWinMain can give up cleanly
// before we depend on log output.
static bool ParseCommandLine(int argc, LPWSTR argv[]) {
  if (argv == nullptr) {
    return false;
  }
  bool is_debug_mode   = false;
  bool is_version_mode = false;
  bool is_help_mode    = false;
  // argv[0] is the .exe path (CommandLineToArgvW convention); skip it so
  // a path containing characters that happen to match a flag literal
  // can't false-trigger one of the wcscmp checks below.
  for (int arg_idx = 1; arg_idx < argc; ++arg_idx) {
    wchar_t* arg = argv[arg_idx];
    is_debug_mode |= (wcscmp(arg, L"--debug") == 0) || (wcscmp(arg, L"-d") == 0) ||
                     (wcscmp(arg, L"-debug") == 0) || (wcscmp(arg, L"/d") == 0) ||
                     (wcscmp(arg, L"/D") == 0);
    is_version_mode |= (wcscmp(arg, L"--version") == 0) || (wcscmp(arg, L"-v") == 0) ||
                       (wcscmp(arg, L"-ver") == 0) || (wcscmp(arg, L"/v") == 0) ||
                       (wcscmp(arg, L"/V") == 0);
    is_help_mode |= (wcscmp(arg, L"--help") == 0) || (wcscmp(arg, L"-h") == 0) ||
                    (wcscmp(arg, L"-?") == 0) || (wcscmp(arg, L"/h") == 0) ||
                    (wcscmp(arg, L"/H") == 0) || (wcscmp(arg, L"/?") == 0);
  }
  if (is_version_mode && !is_help_mode) {
    g_show_version = true;
  }
  if (is_help_mode) {
    g_show_help = true;
  }
  if (is_debug_mode) {
    g_debug_mode = true;
  }
  return true;
}

// Prints the app name + semver to the attached console (InitLogging has
// already done the AttachConsole/AllocConsole dance) and returns wWinMain's
// exit code. `system("pause")` keeps the window open when launched from
// Explorer so the user can actually read the line.
static int ShowVersionAndExit() {
  std::wcout << L"\n " << GetAppName() << L" Version " << GetVersionString() << L"\n " << std::endl;
  system("pause");
  return 0;
}

// Same as above, but for --help. Lists the recognised flags exactly as
// ParseCommandLine spells them out so the two stay in sync.
static int ShowHelpAndExit() {
  std::wcout << L"\n " << ORIG_FILENAME << L" Usage: \n" << std::flush;
  std::wostringstream wostr;
  wostr << L"   /d | -d | --debug   : Enable debug logging\n"
        << L"   /v | -v | --version : Show version info \n"
        << L"   /? | -h | --help    : Show this Help \n"
        << std::flush;
  static const std::wstring kHelpMsg = wostr.str();
  std::wcout << kHelpMsg.c_str() << std::endl;
  system("pause");
  return 0;
}

/* --------------------------- Child controls / UI -------------------------- */

static HWND MakeControl(HWND hWnd,
                        LPCWSTR cls,
                        LPCWSTR text,
                        DWORD style,
                        DWORD exStyle,
                        int ctrlId) {
  return CreateWindowExW(exStyle, cls, text, dwCHILD | style, 0, 0, 0, 0, hWnd,
                         reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ctrlId)), g_hInstance,
                         nullptr);
}

// Builds the speed slider, the two mode checkboxes, the under-windows
// checkbox, and the erase-color row. Returns false if any control or the
// shared font fails to create.
static bool CreateChildControls(HWND hWnd) {
  s_uiFont = GetFont(0); // 8pt Tahoma, DPI-scaled (size <= 0 path)

  s_speedLabel = MakeControl(hWnd, L"STATIC", L"Speed:", SS_LEFT, 0, IDC_STATIC);
  s_speedSlider =
      MakeControl(hWnd, TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS, 0, IDC_SPEED);
  s_followCheck  = MakeControl(hWnd, L"BUTTON", L"Follow the cursor", WS_TABSTOP | BS_AUTOCHECKBOX,
                               0, IDC_FOLLOW);
  s_runawayCheck = MakeControl(hWnd, L"BUTTON", L"Run away from the cursor",
                               WS_TABSTOP | BS_AUTOCHECKBOX, 0, IDC_RUNAWAY);
  s_underCheck   = MakeControl(hWnd, L"BUTTON", L"Travel under other windows",
                               WS_TABSTOP | BS_AUTOCHECKBOX, 0, IDC_UNDERWINDOWS);
  s_eraseLabel   = MakeControl(hWnd, L"STATIC", L"Erase to:", SS_LEFT, 0, IDC_STATIC);
  s_desktopCheck = MakeControl(hWnd, L"BUTTON", L"Use desktop background color",
                               WS_TABSTOP | BS_AUTOCHECKBOX, 0, IDC_DESKTOPCOLOR);
  s_colorButton =
      MakeControl(hWnd, L"BUTTON", L"Pick Color...", WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_COLOR);
  s_swatch = MakeControl(hWnd, L"STATIC", L"", SS_LEFT, WS_EX_CLIENTEDGE, IDC_SWATCH);

  if (s_speedLabel == nullptr || s_speedSlider == nullptr || s_followCheck == nullptr ||
      s_runawayCheck == nullptr || s_underCheck == nullptr || s_eraseLabel == nullptr ||
      s_desktopCheck == nullptr || s_colorButton == nullptr || s_swatch == nullptr) {
    return false;
  }

  SendMessageW(s_speedSlider, TBM_SETRANGE, TRUE, MAKELPARAM(pet::kSpeedMin, pet::kSpeedMax));
  SendMessageW(s_speedSlider, TBM_SETTICFREQ, 4, 0);

  if (s_uiFont != nullptr) {
    const WPARAM font = reinterpret_cast<WPARAM>(s_uiFont);
    const HWND all[]  = {s_speedLabel, s_speedSlider,  s_followCheck, s_runawayCheck, s_underCheck,
                         s_eraseLabel, s_desktopCheck, s_colorButton, s_swatch};
    for (HWND h : all) {
      SendMessageW(h, WM_SETFONT, font, TRUE);
    }
  }
  return true;
}

// Stacks the rows down the client area. Safe to call before the controls
// exist (WM_SIZE can fire early) - it no-ops, successfully, until then. False
// if the deferred positioning failed: nothing was moved, so every control is
// still at zero size and the window is unusable.
static bool LayoutChildren(HWND hWnd) {
  if (s_swatch == nullptr) {
    return true; // controls not created yet
  }
  RECT clientRect;
  GetClientRect(hWnd, &clientRect);
  const int clientW = clientRect.right - clientRect.left;
  const int innerW  = clientW - kPadLeft * 2;

  HDWP hdwp = BeginDeferWindowPos(9);
  int rowY  = kPadTop;

  // Row: "Speed: N" label, then the slider beside it.
  if (hdwp != nullptr) {
    hdwp = DeferWindowPos(hdwp, s_speedLabel, nullptr, kPadLeft, rowY + 4, kLabelWidth,
                          kControlHeight, SWP_NOZORDER);
  }
  if (hdwp != nullptr) {
    hdwp = DeferWindowPos(hdwp, s_speedSlider, nullptr, kPadLeft + kLabelWidth + kHGap, rowY,
                          innerW - kLabelWidth - kHGap, kSliderHeight, SWP_NOZORDER);
  }
  rowY += kSliderHeight + kVGap;

  // Rows: the two mode checkboxes, then the under-windows switch.
  if (hdwp != nullptr) {
    hdwp = DeferWindowPos(hdwp, s_followCheck, nullptr, kPadLeft, rowY, innerW, kControlHeight,
                          SWP_NOZORDER);
  }
  rowY += kControlHeight + kVGap;
  if (hdwp != nullptr) {
    hdwp = DeferWindowPos(hdwp, s_runawayCheck, nullptr, kPadLeft, rowY, innerW, kControlHeight,
                          SWP_NOZORDER);
  }
  rowY += kControlHeight + kVGap;
  if (hdwp != nullptr) {
    hdwp = DeferWindowPos(hdwp, s_underCheck, nullptr, kPadLeft, rowY, innerW, kControlHeight,
                          SWP_NOZORDER);
  }
  rowY += kControlHeight + kVGap * 2;

  // Rows: erase color - label, the desktop-color checkbox, then button + swatch.
  if (hdwp != nullptr) {
    hdwp = DeferWindowPos(hdwp, s_eraseLabel, nullptr, kPadLeft, rowY, innerW, kControlHeight,
                          SWP_NOZORDER);
  }
  rowY += kControlHeight + kVGap / 2;
  if (hdwp != nullptr) {
    hdwp = DeferWindowPos(hdwp, s_desktopCheck, nullptr, kPadLeft, rowY, innerW, kControlHeight,
                          SWP_NOZORDER);
  }
  rowY += kControlHeight + kVGap;
  if (hdwp != nullptr) {
    hdwp = DeferWindowPos(hdwp, s_colorButton, nullptr, kPadLeft, rowY, kButtonWidth, kButtonHeight,
                          SWP_NOZORDER);
  }
  if (hdwp != nullptr) {
    hdwp = DeferWindowPos(hdwp, s_swatch, nullptr, kPadLeft + kButtonWidth + kHGap, rowY,
                          kSwatchSize * 2, kButtonHeight, SWP_NOZORDER);
  }
  if (hdwp == nullptr) {
    return false; // BeginDeferWindowPos or a DeferWindowPos failed
  }
  return EndDeferWindowPos(hdwp) != FALSE;
}

/* ------------------------------- Settings --------------------------------- */

static COLORREF CurrentEraseColor() {
  return pet::EffectiveEraseColor(s_erase);
}

// Rebuilds the swatch brush for the effective erase colour and repaints the
// swatch. False, with a warning logged, if GDI refused the brush: the old one
// stays, so the swatch keeps showing the previous colour rather than nothing.
static bool RefreshSwatch() {
  HBRUSH brush = CreateSolidBrush(CurrentEraseColor());
  if (brush == nullptr) {
    LOG(WARN) << L"Could not create the erase colour swatch brush";
    return false;
  }
  if (s_swatchBrush != nullptr) {
    DeleteObject(s_swatchBrush);
  }
  s_swatchBrush = brush;
  if (s_swatch != nullptr) {
    InvalidateRect(s_swatch, nullptr, TRUE);
  }
  return true;
}

static void ApplySettingsToPet() {
  petwin::SetSpeed(static_cast<int>(SendMessageW(s_speedSlider, TBM_GETPOS, 0, 0)));
  petwin::SetMode(s_mode);
  petwin::SetEraseColor(CurrentEraseColor());
  if (!petwin::SetUnderWindows(s_underWindows)) {
    LOG(WARN) << L"Could not move the vacuum " << (s_underWindows ? L"under" : L"over")
              << L" the other windows";
  }
}

// Keeps the two checkboxes mutually exclusive and pushes the mode to the pet.
// (Named ApplyMode, not SetMode: with a petwin::Mode argument, ADL would make
// a plain SetMode(...) call ambiguous against petwin::SetMode.)
static void ApplyMode(petwin::Mode mode) {
  s_mode = mode;
  CheckDlgButton(mainHwnd, IDC_FOLLOW, mode == petwin::MODE_FOLLOW ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(mainHwnd, IDC_RUNAWAY, mode == petwin::MODE_RUNAWAY ? BST_CHECKED : BST_UNCHECKED);
  petwin::SetMode(mode);
}

static void UpdateSpeedLabel() {
  const int pos = static_cast<int>(SendMessageW(s_speedSlider, TBM_GETPOS, 0, 0));
  std::wostringstream wostr;
  wostr << L"Speed: " << pos;
  SetWindowTextW(s_speedLabel, wostr.str().c_str());
}

static void PickColor(HWND hWnd) {
  if (!pet::PickEraseColor(hWnd, &s_erase)) {
    return; // cancelled
  }
  // The pick switched the desktop-colour setting off; show that.
  CheckDlgButton(hWnd, IDC_DESKTOPCOLOR, BST_UNCHECKED);
  EnableWindow(s_colorButton, TRUE);
  RefreshSwatch();
  ApplySettingsToPet();
  LOG(INFO) << L"Erase color set";
}

static void LoadSettings(HWND hWnd) {
  DWORD value = 0;

  // Speed, colour and the desktop-colour switch are the library's, with the
  // display table's speed for this screen as the default.
  pet::LoadEraseSettings(kRegSubkey, petwin::ScreenProfile().speed, &s_erase);
  SendMessageW(s_speedSlider, TBM_SETPOS, TRUE, s_erase.speed);
  UpdateSpeedLabel();

  s_mode = petwin::MODE_FOLLOW;
  if (pet::RegReadDword(kRegSubkey, kRegMode, &value) && value <= petwin::MODE_RUNAWAY) {
    s_mode = static_cast<petwin::Mode>(value);
  }
  CheckDlgButton(hWnd, IDC_FOLLOW, s_mode == petwin::MODE_FOLLOW ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hWnd, IDC_RUNAWAY, s_mode == petwin::MODE_RUNAWAY ? BST_CHECKED : BST_UNCHECKED);

  s_underWindows = false;
  if (pet::RegReadDword(kRegSubkey, kRegUnderWindows, &value)) {
    s_underWindows = (value != 0);
  }
  CheckDlgButton(hWnd, IDC_UNDERWINDOWS, s_underWindows ? BST_CHECKED : BST_UNCHECKED);

  CheckDlgButton(hWnd, IDC_DESKTOPCOLOR, s_erase.useDesktopColor ? BST_CHECKED : BST_UNCHECKED);
  EnableWindow(s_colorButton, s_erase.useDesktopColor ? FALSE : TRUE);
  RefreshSwatch();

  tray_mode = false;
  if (pet::RegReadDword(kRegSubkey, kRegTray, &value)) {
    tray_mode = (value != 0);
  }
  CheckMenuItem(GetMenu(hWnd), IDM_TRAY, MF_BYCOMMAND | (tray_mode ? MF_CHECKED : MF_UNCHECKED));
  s_settingsLoaded = true;
}

// Writes every setting back to the registry. False if any value failed to
// write; the rest are still attempted, so one bad value costs only itself.
static bool SaveSettings() {
  if (!s_settingsLoaded) {
    return true; // nothing was ever loaded into the controls, so nothing to write back
  }
  s_erase.speed = static_cast<int>(SendMessageW(s_speedSlider, TBM_GETPOS, 0, 0));
  bool saved    = pet::SaveEraseSettings(kRegSubkey, s_erase);
  saved         = pet::RegWriteDword(kRegSubkey, kRegMode, static_cast<DWORD>(s_mode)) && saved;
  saved         = pet::RegWriteDword(kRegSubkey, kRegUnderWindows, s_underWindows ? 1u : 0u) && saved;
  saved         = pet::RegWriteDword(kRegSubkey, kRegTray, tray_mode ? 1u : 0u) && saved;
  return saved;
}

// Settings -> Reset Settings. Wipes what is saved, then reloads so every
// default lands at once - the display table's speed for this screen, follow
// mode, on top of other windows, erase to the desktop colour, no tray - on
// the controls, in the menu, and in the pet. The defaults get saved again on
// the way out as usual. True if the settings were reset; false if the user
// declined, or the wipe failed (told to the user here) and nothing changed.
static bool ResetSettings(HWND hWnd) {
  if (MessageBoxW(hWnd, L"Reset all settings to their defaults?", L"Reset Settings",
                  MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
    return false;
  }
  if (!pet::RegDeleteSubkey(kRegSubkey)) {
    LOG(WARN) << L"Could not delete the saved settings";
    WarnBox(hWnd, L"Reset Settings",
            L"The saved settings could not be removed, so nothing was reset.");
    return false;
  }
  LoadSettings(hWnd);
  ApplySettingsToPet();
  return true;
}

/* --------------------------------- Tray ----------------------------------- */

// False if the shell refused the icon (no Explorer running, or an NT4 shell
// rejecting the struct): the caller must not hide the window then.
static bool AddTrayIcon(HWND hWnd) {
  NOTIFYICONDATAW nid = {};
  // The original (NT4 / IE3) layout, not sizeof: we only use the fields it
  // has, every later shell still accepts it, and NT4's shell32 4.0 rejects
  // anything bigger - silently, which with tray mode on would strand the
  // window hidden with no icon to bring it back.
  nid.cbSize           = NOTIFYICONDATAW_V1_SIZE;
  nid.hWnd             = hWnd;
  nid.uID              = kTrayIconId;
  nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  nid.uCallbackMessage = WM_TRAYICON;
  nid.hIcon            = kSmallIcon;
  lstrcpynW(nid.szTip, APP_NAME, static_cast<int>(sizeof(nid.szTip) / sizeof(nid.szTip[0])));
  return Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
}

// False if the shell refused to take the icon back. s_inTray is cleared either
// way: the next minimize must add the icon afresh rather than hide the window
// trusting one that may no longer be there.
static bool RemoveTrayIcon(HWND hWnd) {
  if (!s_inTray) {
    return true; // nothing to remove
  }
  NOTIFYICONDATAW nid = {};
  nid.cbSize          = NOTIFYICONDATAW_V1_SIZE; // see AddTrayIcon
  nid.hWnd            = hWnd;
  nid.uID             = kTrayIconId;
  const bool removed  = Shell_NotifyIconW(NIM_DELETE, &nid) != FALSE;
  s_inTray            = false;
  return removed;
}

static void ShowTrayMenu(HWND hWnd) {
  POINT cursor;
  GetCursorPos(&cursor);
  HMENU menu = CreatePopupMenu();
  if (menu == nullptr) {
    return;
  }
  AppendMenuW(menu, MF_STRING, IDM_SHOW, L"&Show Settings");
  AppendMenuW(menu, MF_STRING, IDM_ABOUT, L"&About");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, IDM_EXIT, L"E&xit");
  SetMenuDefaultItem(menu, IDM_SHOW, FALSE);
  // The classic tray-menu dance: without SetForegroundWindow the menu won't
  // dismiss when the user clicks away, and the WM_NULL afterwards works
  // around the shell leaving it stuck open on the second showing.
  SetForegroundWindow(hWnd);
  TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hWnd, nullptr);
  PostMessageW(hWnd, WM_NULL, 0, 0);
  DestroyMenu(menu);
}

// False if there is no tray icon to come back from, in which case the window
// is left alone: hiding it then would strand it with no way to restore.
static bool MinimizeToTray(HWND hWnd) {
  if (!s_inTray) {
    if (!AddTrayIcon(hWnd)) {
      LOG(WARN) << L"Could not add the tray icon; leaving the window on the taskbar";
      return false;
    }
    s_inTray = true;
  }
  ShowWindow(hWnd, SW_HIDE);
  return true;
}

static void RestoreFromTray(HWND hWnd) {
  ShowWindow(hWnd, SW_SHOWNORMAL);
  SetForegroundWindow(hWnd);
  if (!RemoveTrayIcon(hWnd)) {
    LOG(WARN) << L"Could not remove the tray icon";
  }
}

/* ------------------------------- Functions -------------------------------- */

bool RegisterWndClass(HINSTANCE hInstance, LPCWSTR className) {
  if (kMainIcon == nullptr || kSmallIcon == nullptr) {
    return false;
  }
  WNDCLASSEXW wndclass;
  wndclass.cbSize      = sizeof(WNDCLASSEX);
  wndclass.style       = CS_HREDRAW | CS_VREDRAW;
  wndclass.lpfnWndProc = WindowProc;
  wndclass.cbClsExtra  = 0;
  wndclass.cbWndExtra  = 0;
  wndclass.hInstance   = hInstance;
  wndclass.hIcon       = kMainIcon;
  wndclass.hCursor     = LoadCursorW(nullptr, IDC_ARROW);
  // Classic grey client area; the OS repaints it on WM_SYSCOLORCHANGE because
  // we hand it the system pseudo-brush rather than a fixed HBRUSH.
  wndclass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
  wndclass.lpszMenuName  = MAKEINTRESOURCEW(IDR_MAIN);
  wndclass.lpszClassName = className;
  wndclass.hIconSm       = kSmallIcon;

  // RegisterClassEx returns an ATOM (typedef unsigned short - really a short
  // pointer left over from Win16 days), 0 on failure. The double cast spells
  // out "this is an ATOM-shaped zero" rather than relying on the implicit
  // promotion from int 0.
  if (RegisterClassExW(&wndclass) == static_cast<ATOM>(static_cast<unsigned short>(0))) {
    return false;
  }
  return true;
}

bool InitWindow(HINSTANCE hInstance, LPCWSTR className, LPCWSTR title, int iCmdShow) {
  static constexpr DWORD exStyle = WS_EX_OVERLAPPEDWINDOW;
  // A small fixed-size control panel: no maximize box, no size box.
  static constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

  // CW_WIDTH/CW_HEIGHT are the client area the controls need. Grow them into
  // an outer size: the caption, the borders and - the one that bites, since
  // the class carries IDR_MAIN - the menu bar all live outside the client
  // rect, and the metrics differ per theme and per Windows version.
  RECT windowRect = {0, 0, CW_WIDTH, CW_HEIGHT};
  AdjustWindowRectEx(&windowRect, style, /*bMenu=*/TRUE, exStyle);

  // Create main window
  mainHwnd = CreateWindowExW(exStyle, className, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                             windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
                             nullptr, nullptr, hInstance, nullptr);

  if (mainHwnd == nullptr) {
    return false;
  }
  ShowWindow(mainHwnd, iCmdShow);
  if (!UpdateWindow(mainHwnd)) {
    return false;
  }
  return true;
}

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int iCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  g_hInstance = hInstance;

  // Initialize common controls (ICC_BAR_CLASSES covers the speed trackbar).
  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC  = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
  InitCommonControlsEx(&icex);

  static const std::wstring name   = GetAppName();
  static const LPCWSTR appTitle    = name.c_str();
  static const LPCWSTR szClassName = MAIN_WNDCLASS;

  // Parse the command line into a real argv via CommandLineToArgvW
  // (lpCmdLine is the post-exe-path tail only; we want the full thing
  // so argv[0] is the exe path that ParseCommandLine's loop skips).
  // Failure path is "no flags set" - we can't LOG(ERROR) here because
  // logging isn't initialized yet.
  int argc     = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!ParseCommandLine(argc, argv)) {
    std::wcerr << L"Failed to parse command line, aborting!" << std::endl;
    return 2;
  }
  if (argv != nullptr) {
    LocalFree(argv);
  }

  // Where the log goes: a console, or nowhere - never a file. A console only
  // when there is something text-y to show right now, --version / --help
  // (one-shot output) or --debug (live logging): one we allocate flashes up
  // on every launch and can't be hidden under Windows Terminal, and one
  // inherited from the command prompt that launched us is the user's own
  // window, not ours to hide. With no sink, LOG() lines go nowhere (the
  // logger only pings OutputDebugString).
  logging::LogDest logSink = logging::LOG_NONE;
  if (g_show_version || g_show_help || g_debug_mode) {
    logSink = logging::LOG_TO_STDERR;
  }
  logging::LogInitSettings LoggingSettings;
  LoggingSettings.log_sink          = logSink;
  LoggingSettings.app_name          = appTitle;
  LoggingSettings.show_func_sigs    = false;
  LoggingSettings.show_line_numbers = false;
  LoggingSettings.show_time         = false;
  LoggingSettings.full_prefix_level = LOG_ERROR;
  if (!logging::InitLogging(g_hInstance, LoggingSettings)) {
    ErrorBox(nullptr, L"Logging Initialization Failure", L"InitLogging failed!");
    return 3;
  }
  logging::SetIsDCheck(is_dcheck);
  is_on_wine = IsRunningOnWine(&winever);
  if (g_show_version) {
    return ShowVersionAndExit();
  }
  if (g_show_help) {
    return ShowHelpAndExit();
  }
  LOG(INFO) << GetWelcomeMessage();

  kMainIcon  = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
  kSmallIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SMALL));

  // Register our window class.
  if (!RegisterWndClass(g_hInstance, szClassName)) {
    ErrorBox(nullptr, L"RegisterClassEx Error", L"This program requires Windows NT!");
    return 1;
  }

  // Open our window now
  if (!InitWindow(g_hInstance, szClassName, appTitle, iCmdShow)) {
    return 4;
  }

  HACCEL hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDR_MAIN));
  if (hAccel == nullptr) {
    return 5;
  }

  MSG msg;
  int got = 0;
  while ((got = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
    if (got == -1) {
      // GetMessage reports an error as -1, which a plain while() takes as
      // true; without this check an invalid-handle error spins forever.
      LOG(ERROR) << L"GetMessage failed: " << logging::Hex(GetLastError());
      break;
    }
    // Accelerator first so menu shortcuts win over dialog navigation.
    if (TranslateAcceleratorW(mainHwnd, hAccel, &msg)) {
      continue;
    }
    // Dialog-style navigation: TAB / Shift+TAB between the WS_TABSTOP
    // controls, Space toggles a focused checkbox, Enter clicks a button.
    if (mainHwnd != nullptr && IsDialogMessageW(mainHwnd, &msg)) {
      continue;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  if (hAccel != nullptr) {
    DestroyAcceleratorTable(hAccel);
  }
  return got == -1 ? 6 : static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE: {
      if (mainHwnd == nullptr) {
        mainHwnd = hWnd; // Prevent race condition in InitApp
      }
      s_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
      CenterWindowOnScreen(hWnd, /*multimon=*/true);
      if (!CreateChildControls(hWnd)) {
        LOG(ERROR) << L"Failed to create child controls!";
        return -1;
      }
      if (!LayoutChildren(hWnd)) {
        LOG(ERROR) << L"Failed to lay out the child controls!";
        return -1;
      }
      LoadSettings(hWnd);
      if (!InitApp(hWnd)) {
        return -1;
      }
      SetFocus(s_speedSlider);
    } break;
    case WM_TIMER:
      if (wParam == kTickTimerId) {
        petwin::Tick();
      }
      break;
    case WM_HSCROLL:
      // The speed trackbar reports every nudge here.
      if (reinterpret_cast<HWND>(lParam) == s_speedSlider) {
        petwin::SetSpeed(static_cast<int>(SendMessageW(s_speedSlider, TBM_GETPOS, 0, 0)));
        UpdateSpeedLabel();
      }
      break;
    case WM_CTLCOLORSTATIC:
      // Paint the swatch in the current erase color.
      if (reinterpret_cast<HWND>(lParam) == s_swatch && s_swatchBrush != nullptr) {
        return reinterpret_cast<LRESULT>(s_swatchBrush);
      }
      return DefWindowProcW(hWnd, message, wParam, lParam);
    case WM_SYSCOLORCHANGE:
      // The desktop background may have changed; track it if we're using it.
      if (s_erase.useDesktopColor) {
        RefreshSwatch();
        ApplySettingsToPet();
      }
      return DefWindowProcW(hWnd, message, wParam, lParam);
    case WM_DISPLAYCHANGE:
      if (!petwin::UpdateBounds()) {
        LOG(WARN) << L"Could not refit the vacuum to the new display: "
                  << pet::LoadResultText(petwin::LastLoadResult());
      }
      break;
    case WM_SIZE:
      if (wParam == SIZE_MINIMIZED && tray_mode && !s_minimizeIsFallback) {
        MinimizeToTray(hWnd);
      }
      break;
    case WM_TRAYICON:
      switch (LOWORD(lParam)) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
          RestoreFromTray(hWnd);
          break;
        case WM_RBUTTONUP:
          ShowTrayMenu(hWnd);
          break;
        default:
          break;
      }
      break;
    case WM_COMMAND: {
      const int cmdId = LOWORD(wParam);
      switch (cmdId) {
        case IDC_FOLLOW:
          // The two mode boxes are mutually exclusive; both off means idle.
          ApplyMode(IsDlgButtonChecked(hWnd, IDC_FOLLOW) == BST_CHECKED ? petwin::MODE_FOLLOW
                                                                        : petwin::MODE_IDLE);
          break;
        case IDC_RUNAWAY:
          ApplyMode(IsDlgButtonChecked(hWnd, IDC_RUNAWAY) == BST_CHECKED ? petwin::MODE_RUNAWAY
                                                                         : petwin::MODE_IDLE);
          break;
        case IDC_UNDERWINDOWS:
          s_underWindows = (IsDlgButtonChecked(hWnd, IDC_UNDERWINDOWS) == BST_CHECKED);
          if (petwin::SetUnderWindows(s_underWindows)) {
            LOG(INFO) << L"Vacuum travels " << (s_underWindows ? L"under" : L"over")
                      << L" other windows";
          } else {
            LOG(WARN) << L"Could not move the vacuum " << (s_underWindows ? L"under" : L"over")
                      << L" the other windows";
          }
          break;
        case IDC_DESKTOPCOLOR:
          s_erase.useDesktopColor = (IsDlgButtonChecked(hWnd, IDC_DESKTOPCOLOR) == BST_CHECKED);
          EnableWindow(s_colorButton, s_erase.useDesktopColor ? FALSE : TRUE);
          RefreshSwatch();
          ApplySettingsToPet();
          break;
        case IDC_COLOR:
          PickColor(hWnd);
          break;
        case IDM_TRAY:
          tray_mode = ToggleMenuCheck(hWnd, IDM_TRAY);
          LOG(INFO) << L"Minimize to tray " << (tray_mode ? L"on" : L"off");
          break;
        case IDM_RESET:
          if (ResetSettings(hWnd)) {
            LOG(INFO) << L"Settings reset to defaults";
          }
          break;
        case IDM_SHOW:
          RestoreFromTray(hWnd);
          break;
        case IDM_EXIT:
          ShutDownApp();
          break;
        case IDM_ABOUT:
          DialogBoxW(g_hInstance, MAKEINTRESOURCEW(IDD_ABOUTDLG), hWnd, AboutDlgProc);
          break;
        case IDM_HELP:
          LaunchHelp(hWnd);
          break;
        default:
          return DefWindowProcW(hWnd, message, wParam, lParam);
      }
    } break;
    case WM_HELP:
      LaunchHelp(hWnd);
      break;
    case WM_CLOSE:
      // With tray mode on, the X tucks the panel away and the pet keeps
      // running; otherwise it's a normal exit.
      if (tray_mode) {
        if (!MinimizeToTray(hWnd)) {
          // No tray icon: keep it reachable from the taskbar. ShowWindow sends
          // WM_SIZE before it returns, so the flag only needs to span the call.
          s_minimizeIsFallback = true;
          ShowWindow(hWnd, SW_MINIMIZE);
          s_minimizeIsFallback = false;
        }
      } else {
        ShutDownApp();
      }
      break;
    case WM_QUERYENDSESSION:
      LOG(DEBUG) << L"Window station is going down now!";
      return TRUE;
    case WM_ENDSESSION:
      // The session really is ending. The process is killed after this and
      // WM_DESTROY never comes, so persist now or lose this session's changes.
      if (wParam) {
        if (SaveSettings()) {
          LOG(INFO) << L"Settings saved for session end";
        } else {
          LOG(WARN) << L"Could not save the settings for session end";
        }
      }
      return 0;
    case WM_DESTROY:
      KillTimer(hWnd, kTickTimerId);
      if (!SaveSettings()) {
        LOG(WARN) << L"Could not save the settings";
      }
      if (!RemoveTrayIcon(hWnd)) {
        LOG(WARN) << L"Could not remove the tray icon";
      }
      petwin::Destroy();
      if (s_swatchBrush != nullptr) {
        DeleteObject(s_swatchBrush);
        s_swatchBrush = nullptr;
      }
      if (s_uiFont != nullptr) {
        DeleteObject(s_uiFont);
        s_uiFont = nullptr;
      }
      PostQuitMessage(0); // WM_QUIT
      break;
    case WM_NCDESTROY:
      mainHwnd = nullptr;
      LOG(DEBUG) << L"Bye bye!";
      // Last message this window will receive. Close log + console cleanly
      // here, before the loop sees the WM_QUIT that WM_DESTROY queued.
      logging::DeInitLogging(g_hInstance);
      break;
    default:
      if (message != 0 && message == s_taskbarCreatedMsg && s_inTray) {
        // Explorer restarted and took our icon with it; put it back so the
        // hidden window stays reachable.
        AddTrayIcon(hWnd);
        break;
      }
      return DefWindowProcW(hWnd, message, wParam, lParam);
  }
  return 0;
}

bool InitApp(HWND hWnd) {
  if (hWnd == nullptr) {
    return false;
  }
  // Both baked sets, pet::SpriteSet by pet::Dir. The pet window picks the set
  // and the size to scale it to for this screen from the display table.
  static const pet::FrameIds kFrameIds = {
      {IDB_PET_UP_1X, IDB_PET_DOWN_1X, IDB_PET_LEFT_1X, IDB_PET_RIGHT_1X},
      {IDB_PET_UP_2X, IDB_PET_DOWN_2X, IDB_PET_LEFT_2X, IDB_PET_RIGHT_2X}};
  // Settled before Create so the sprite first appears where it belongs,
  // rather than on top for a tick and then dropping under the windows. The
  // result is dropped: with no window yet it is always true, and
  // ApplySettingsToPet below checks the real application.
  static_cast<void>(petwin::SetUnderWindows(s_underWindows));
  if (!petwin::Create(g_hInstance, kFrameIds)) {
    const std::wstring reason = pet::LoadResultText(petwin::LastLoadResult());
    LOG(ERROR) << L"Failed to create the pet window: " << reason;
    ErrorBox(hWnd, L"Kirby Pet", L"Could not load the vacuum sprites: " + reason);
    return false;
  }
  ApplySettingsToPet();
  if (SetTimer(hWnd, kTickTimerId, kTickMs, nullptr) == 0) {
    LOG(ERROR) << L"SetTimer failed!";
    return false;
  }
  LOG(INFO) << L"Pet running";
  return true;
}

bool ShutDownApp() {
  // mainHwnd is cleared in WM_NCDESTROY; guard so a duplicate exit path
  // (e.g. WM_CLOSE arriving after WM_DESTROY's tear-down began) doesn't pass
  // NULL to DestroyWindow, which is undefined per MSDN.
  DCHECK(mainHwnd != nullptr);
  if (mainHwnd == nullptr) {
    return false;
  }
  return DestroyWindow(mainHwnd); // Send WM_DESTROY
}

bool LaunchHelp(HWND hWnd) {
  return InfoBox(hWnd, L"Kirby Pet Help",
                 L"The vacuum roams your desktop and erases where it has been.\n\n"
                 L"Follow the cursor - it chases the mouse pointer.\n"
                 L"Run away from the cursor - it flees when the pointer gets close.\n"
                 L"Neither - it stays where it is.\n\n"
                 L"Travel under other windows - it keeps to the desktop and passes\n"
                 L"beneath your windows instead of riding on top of them.\n\n"
                 L"The vacuum never gets in the way: clicks go straight through it\n"
                 L"to whatever is underneath. Bring this window back from the\n"
                 L"taskbar, or from the tray icon when minimize-to-tray is on.");
}

INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
    case WM_INITDIALOG: {
      // Set icon in titlebar of about dialog
      static const HICON kAboutIcon = LoadIconW(g_hInstance, MAKEINTRESOURCEW(IDI_ABOUT));
      SendMessageW(hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(kAboutIcon));
      SendMessageW(hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(kAboutIcon));
      // The template places the dialog relative to its owner. From the tray
      // menu the owner is the hidden settings window, so that offset would land
      // it wherever the window last was; centre it on the screen instead.
      const HWND owner = GetWindow(hDlg, GW_OWNER);
      if (owner == nullptr || !IsWindowVisible(owner)) {
        CenterWindowOnScreen(hDlg, /*multimon=*/true);
      }
      LOG(INFO) << L"Showed About dialog";
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

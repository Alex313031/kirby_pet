// {{NO_DEPENDENCIES}}
// For #define-ing static resources for resource script file(s).
// Used by kirby_pet.rc
//
// ID range map (keep new entries inside their range):
//   100-119  IDI_*  Icons
//   120-129  IDR_*  Main program resource (menu / accelerators)
//   130-149  IDD_*  Dialogs
//   150-169  IDB_*  Bitmaps (vacuum sprite frames)
//   200-299  IDM_*  Menu commands
//   300-399  IDC_*  Child control IDs, grouped by logical role

// clang-format off

/* Icons (100-119) */
#define IDI_MAIN                101 /* 32x32 & 48x48 icon */
#define IDI_SMALL               102 /* Small 16x16 icon */
#define IDI_ABOUT               103 /* About Dialog icon */

/* Main application resource - also used to attach menu */
#define IDR_MAIN                120

/* Dialogs (130-149) */
#define IDD_ABOUTDLG            130 // About Dialog

/* Bitmaps (150-169): the vacuum's four facings in pet::Dir order, per size.
   1x = 32x32, 2x = 64x64 (the app uses 2x for now). */
#define IDB_PET_UP_1X           150
#define IDB_PET_DOWN_1X         151
#define IDB_PET_LEFT_1X         152
#define IDB_PET_RIGHT_1X        153
#define IDB_PET_UP_2X           154
#define IDB_PET_DOWN_2X         155
#define IDB_PET_LEFT_2X         156
#define IDB_PET_RIGHT_2X        157

/* Menu items (200-299) */
#define IDM_ABOUT               200 // About dialog
#define IDM_EXIT                201 // Exit immediately
#define IDM_HELP                203 // Opens help
#define IDM_TRAY                204 // Toggle minimizing to tray or taskbar
#define IDM_SHOW                205 // Restore the settings window (tray menu)
#define IDM_RESET               206 // Reset all settings to defaults

/* Child control IDs (300-399) */
#define IDC_SPEED               300 // Slider to change Kirby's speed
#define IDC_FOLLOW              301 // Checkbox for Kirby to follow cursor
#define IDC_RUNAWAY             302 // Checkbox for Kirby to run away from cursor
#define IDC_COLOR               303 // "Erase Color..." button (ChooseColor)
#define IDC_SWATCH              304 // Static showing the current erase color
#define IDC_DESKTOPCOLOR        305 // Checkbox: erase to the desktop background color
#define IDC_UNDERWINDOWS        306 // Checkbox: the vacuum travels under other windows

// For resources to be loaded without an ID from the system.
#ifndef IDC_STATIC
 #define IDC_STATIC             -1
#endif // IDC_STATIC

// clang-format on

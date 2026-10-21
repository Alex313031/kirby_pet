// {{NO_DEPENDENCIES}}
// For #define-ing static resources for resource script file(s).
// Used by kirby_pet_scr.rc
//
// ID range map (keep new entries inside their range):
//   100-119  IDI_*  Icons (ID_APP = 100 is the one scrnsave.lib loads)
//   130-149  IDD_*  Dialogs
//   150-169  IDB_*  Bitmaps (vacuum sprite frames)
//   300-399  IDC_*  Config-dialog control IDs
//   scrnsave.lib fixed IDs: IDS_DESCRIPTION = 1, DLG_SCRNSAVECONFIGURE = 2003

// clang-format off

/* scrnsave.lib's fixed IDs. Same values as <scrnsave.h>, which isn't RC-safe,
   so they're repeated here (guarded) for the resource compiler. */
#ifndef IDS_DESCRIPTION
 #define IDS_DESCRIPTION        1    // string: the saver's display name
#endif
#ifndef ID_APP
 #define ID_APP                 100  // icon scrnsave uses for the saver
#endif
#ifndef DLG_SCRNSAVECONFIGURE
 #define DLG_SCRNSAVECONFIGURE  2003 // the /c settings dialog
#endif

/* Icons (100-119) */
#define IDI_MAIN                101 /* 32x32 & 48x48 icon (About dialog) */

/* Dialogs (130-149) */
#define IDD_ABOUTDLG            130 // About Dialog

/* Bitmaps (150-169): the vacuum's four facings in pet::Dir order, per size.
   1x = 32x32 (used by the /p preview), 2x = 64x64 (the real screen). */
#define IDB_PET_UP_1X           150
#define IDB_PET_DOWN_1X         151
#define IDB_PET_LEFT_1X         152
#define IDB_PET_RIGHT_1X        153
#define IDB_PET_UP_2X           154
#define IDB_PET_DOWN_2X         155
#define IDB_PET_LEFT_2X         156
#define IDB_PET_RIGHT_2X        157

/* Config-dialog controls (300-399) */
#define IDC_SPEED               300 // Speed trackbar
#define IDC_SPEED_LABEL         301 // "Speed: N"
#define IDC_DESKTOPCOLOR        302 // Checkbox: erase to the desktop background color
#define IDC_COLOR               303 // "Pick Color..." (ChooseColor)
#define IDC_SWATCH              304 // Owner-drawn swatch of the current erase color
#define IDC_ABOUT               305 // "About..." button

// For resources to be loaded without an ID from the system.
#ifndef IDC_STATIC
 #define IDC_STATIC             -1
#endif // IDC_STATIC

// clang-format on

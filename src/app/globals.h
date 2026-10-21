#ifndef KIRBY_PET_GLOBALS_H_
#define KIRBY_PET_GLOBALS_H_

#include "framework.h"

extern HINSTANCE g_hInstance; // This program instance, everything descends from this

extern HWND mainHwnd; // Our main window handle

extern bool tray_mode; // Whether tray mode is enabled

extern bool is_on_wine;

#endif // KIRBY_PET_GLOBALS_H_

## Layout

[app/](./app/): sources for kirby_pet.exe, a Neko98 lookalike.

[lib/](./lib/): sources for static/shared library "pet_lib" for pet movement routines.

[logging/](./logging/): logging library used by many of my projects, right now used for debugging the pet .exe.

[res/](./res/): contains shared sprite bitmaps and sound .wavs.

[scr/](./scr/): sources for kirby_scr.scr, a screensaver that "vacuums" your screen.

## Building

Use the [BUILD.gn](../BUILD.gn) or [Makefile](../Makefile) in the project root.

## Details

.bmp and .wav files are embedded from res/ into the .exe and .scr, for shared sprite logic between the two, a library in lib/ 
is used under the `pet::` namespace. With some modification, it could be made more general purpose for making Windows desktop pet toys.

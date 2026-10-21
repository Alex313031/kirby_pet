#ifndef PET_PET_H_
#define PET_PET_H_

// Private declarations for pet_lib. Nothing here is visible to consumers;
// the public surface is include/pet_lib.h.

#include "framework.h"

namespace pet {

  // Builds the region of every pixel whose alpha byte is non-zero in a 32-bpp
  // DIB section, origin (0,0). Handles both bottom-up and top-down sections.
  // Returns nullptr on failure (not a 32-bpp DIB section, or GDI refused).
  HRGN BuildAlphaRegion(HBITMAP hDib);

} // namespace pet

#endif // PET_PET_H_

// pet_lib: vacuum sprites, alpha regions, and movement shared by the Kirby Pet
// app and the Kirby screensaver.

#include "pet.h"

#include <cstdlib> // std::abs(int)
#include <cstring> // memcpy

#include "include/pet_lib.h"

namespace pet {

  /* -------------------------------- Statics --------------------------------- */

  // The display table. Rows are tried top to bottom and the first whose
  // maxHeight the screen does not exceed wins, so taller screens get bigger
  // sprites and a faster default; the last row catches anything above 2160.
  //   max height  set        size  speed
  static const DisplayProfile kDisplayProfiles[] = {
      {600, SPRITE_1X, 24, 12},   {800, SPRITE_1X, 32, 16},
      {1024, SPRITE_2X, 48, 16},  {1080, SPRITE_2X, 64, 16},
      {1440, SPRITE_2X, 64, 18},  {2048, SPRITE_2X, 96, 18},
      {2160, SPRITE_2X, 128, 18}, {std::numeric_limits<int>::max(), SPRITE_2X, 128, 20},
  };

  // Registry value names for EraseSettings. Both consumers used these very
  // names before the settings moved in here, so nothing saved is orphaned.
  static const wchar_t kRegSpeed[]      = L"Speed";
  static const wchar_t kRegEraseColor[] = L"EraseColor";
  static const wchar_t kRegUseDesktop[] = L"UseDesktopColor";

  /* -------------------------- Forward declarations -------------------------- */

  static HBITMAP LoadBitmap(HINSTANCE hInst, const int resId);
  static HBITMAP CreateDib32(int width, int height, void** bits, int* stride);
  static HBITMAP ScaleDib(HBITMAP source, int targetHeight);
  static bool BuildBlitPair(HBITMAP frame, HBITMAP* mask, HBITMAP* opaque);
  static std::wstring RegPath(const wchar_t* subkey);

  /* ---------------------------- Static Functions ---------------------------- */

  static HBITMAP LoadBitmap(HINSTANCE hInst, const int resId) {
    if (hInst == nullptr || resId == -1) {
      return nullptr;
    }
    return static_cast<HBITMAP>(
        LoadImageW(hInst, MAKEINTRESOURCEW(resId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
  }

  // A fresh bottom-up 32-bpp BI_RGB DIB section - the exact layout LoadImage
  // gives the native frames, so nothing downstream (the region builder, the
  // blits, GetObject's idea of orientation) sees anything it hasn't already
  // handled. Created against a real screen DC rather than NULL: allowed
  // either way for DIB_RGB_COLORS, but the DC form is the one every GDI since
  // 95 has seen. Returns the bits and GDI's own stride through the pointers.
  static HBITMAP CreateDib32(int width, int height, void** bits, int* stride) {
    BITMAPINFO info              = {};
    info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth       = width;
    info.bmiHeader.biHeight      = height; // positive: bottom-up
    info.bmiHeader.biPlanes      = 1;
    info.bmiHeader.biBitCount    = 32;
    info.bmiHeader.biCompression = BI_RGB;
    *bits                        = nullptr;
    HDC screen                   = GetDC(nullptr);
    HBITMAP target = CreateDIBSection(screen, &info, DIB_RGB_COLORS, bits, nullptr, 0);
    if (screen != nullptr) {
      ReleaseDC(nullptr, screen);
    }
    DIBSECTION section = {};
    if (target == nullptr || *bits == nullptr ||
        GetObjectW(target, sizeof(section), &section) != static_cast<int>(sizeof(section))) {
      if (target != nullptr) {
        DeleteObject(target);
      }
      return nullptr;
    }
    *stride = section.dsBm.bmWidthBytes;
    return target;
  }

  // Nearest-neighbour resample of a 32-bpp DIB section to `targetHeight`
  // pixels tall, width in proportion. Done by hand rather than with StretchBlt
  // so every pixel is copied whole - alpha byte included - and nothing is
  // filtered: the sprites are hard-edged pixel art and must stay that way at
  // any size. Both buffers are addressed by visual row (0 = top) and mapped to
  // their storage row explicitly, so neither orientation is assumed.
  static HBITMAP ScaleDib(HBITMAP source, int targetHeight) {
    DIBSECTION src = {};
    if (GetObjectW(source, sizeof(src), &src) != static_cast<int>(sizeof(src)) ||
        src.dsBm.bmBits == nullptr || src.dsBm.bmBitsPixel != 32 || targetHeight <= 0) {
      return nullptr;
    }
    const int srcWidth  = src.dsBm.bmWidth;
    const int srcHeight = src.dsBm.bmHeight;
    const int dstHeight = targetHeight;
    const int dstWidth  = std::max(1, srcWidth * targetHeight / srcHeight);

    void* dstBits  = nullptr;
    int dstStride  = 0;
    HBITMAP target = CreateDib32(dstWidth, dstHeight, &dstBits, &dstStride);
    if (target == nullptr) {
      return nullptr;
    }
    const BYTE* srcBase    = static_cast<const BYTE*>(src.dsBm.bmBits);
    const int srcStride    = src.dsBm.bmWidthBytes;
    const bool srcBottomUp = src.dsBmih.biHeight > 0;
    BYTE* dstBase          = static_cast<BYTE*>(dstBits);
    for (int dstY = 0; dstY < dstHeight; ++dstY) {
      const int srcY      = dstY * srcHeight / dstHeight;
      const int srcRow    = srcBottomUp ? (srcHeight - 1 - srcY) : srcY;
      const int dstRow    = dstHeight - 1 - dstY; // bottom-up storage
      const BYTE* srcLine = srcBase + srcRow * srcStride;
      DWORD* dstLine      = reinterpret_cast<DWORD*>(dstBase + dstRow * dstStride);
      for (int dstX = 0; dstX < dstWidth; ++dstX) {
        const int srcX = dstX * srcWidth / dstWidth;
        dstLine[dstX]  = *reinterpret_cast<const DWORD*>(srcLine + srcX * 4);
      }
    }
    return target;
  }

  // The two halves of the classic two-blit transparent draw, built once from
  // a frame's alpha: `mask` is white where the frame is transparent and black
  // where it is opaque (AND it onto the destination to punch the hole), and
  // `opaque` is the frame with its transparent pixels blacked out (OR it on to
  // fill the hole). 32-bpp rather than 1-bpp for the mask so the blit never
  // depends on the destination DC's text/background colours.
  static bool BuildBlitPair(HBITMAP frame, HBITMAP* mask, HBITMAP* opaque) {
    DIBSECTION src = {};
    *mask          = nullptr;
    *opaque        = nullptr;
    if (GetObjectW(frame, sizeof(src), &src) != static_cast<int>(sizeof(src)) ||
        src.dsBm.bmBits == nullptr || src.dsBm.bmBitsPixel != 32) {
      return false;
    }
    const int width  = src.dsBm.bmWidth;
    const int height = src.dsBm.bmHeight;
    void* maskBits   = nullptr;
    void* opaqueBits = nullptr;
    int maskStride   = 0;
    int opaqueStride = 0;
    *mask            = CreateDib32(width, height, &maskBits, &maskStride);
    *opaque          = CreateDib32(width, height, &opaqueBits, &opaqueStride);
    if (*mask == nullptr || *opaque == nullptr) {
      if (*mask != nullptr) {
        DeleteObject(*mask);
        *mask = nullptr;
      }
      if (*opaque != nullptr) {
        DeleteObject(*opaque);
        *opaque = nullptr;
      }
      return false;
    }
    const BYTE* srcBase    = static_cast<const BYTE*>(src.dsBm.bmBits);
    const int srcStride    = src.dsBm.bmWidthBytes;
    const bool srcBottomUp = src.dsBmih.biHeight > 0;
    for (int row = 0; row < height; ++row) {
      const int srcRow     = srcBottomUp ? (height - 1 - row) : row;
      const int dstRow     = height - 1 - row; // bottom-up storage
      const DWORD* srcLine = reinterpret_cast<const DWORD*>(srcBase + srcRow * srcStride);
      DWORD* maskLine =
          reinterpret_cast<DWORD*>(static_cast<BYTE*>(maskBits) + dstRow * maskStride);
      DWORD* opaqueLine =
          reinterpret_cast<DWORD*>(static_cast<BYTE*>(opaqueBits) + dstRow * opaqueStride);
      for (int column = 0; column < width; ++column) {
        const DWORD pixel   = srcLine[column];
        const bool isOpaque = (pixel & 0xFF000000u) != 0;
        maskLine[column]    = isOpaque ? 0x00000000u : 0x00FFFFFFu;
        opaqueLine[column]  = isOpaque ? (pixel & 0x00FFFFFFu) : 0x00000000u;
      }
    }
    return true;
  }

  // HKCU is implied; every value the app and the saver keep lives under Software.
  static std::wstring RegPath(const wchar_t* subkey) {
    std::wstring path = L"Software\\";
    if (subkey != nullptr) {
      path += subkey;
    }
    return path;
  }

  /* ------------------------------- Functions -------------------------------- */

  /* ------------------------------- Sprites ---------------------------------- */

  HRGN BuildAlphaRegion(HBITMAP hDib) {
    DIBSECTION dibSection = {};
    // GetObject fills a full DIBSECTION only for DIB sections; a plain bitmap
    // returns sizeof(BITMAP) and leaves bmBits null - either way, bail.
    if (GetObjectW(hDib, sizeof(dibSection), &dibSection) != static_cast<int>(sizeof(dibSection)) ||
        dibSection.dsBm.bmBits == nullptr || dibSection.dsBm.bmBitsPixel != 32) {
      return nullptr;
    }
    const int width      = dibSection.dsBm.bmWidth;
    const int height     = dibSection.dsBm.bmHeight;
    const int stride     = dibSection.dsBm.bmWidthBytes; // DIB sections are already DWORD-aligned
    const bool bottom_up = dibSection.dsBmih.biHeight > 0;
    const BYTE* bits     = static_cast<const BYTE*>(dibSection.dsBm.bmBits);

    // Collect one rect per horizontal run of opaque pixels, row by row.
    std::vector<RECT> rects;
    rects.reserve(256);
    for (int pixelY = 0; pixelY < height; ++pixelY) {
      const int row        = bottom_up ? (height - 1 - pixelY) : pixelY;
      const BYTE* rowBytes = bits + row * stride;
      int pixelX           = 0;
      while (pixelX < width) {
        while (pixelX < width && rowBytes[pixelX * 4 + 3] == 0) {
          ++pixelX; // skip transparent
        }
        if (pixelX >= width) {
          break;
        }
        const int runStart = pixelX;
        while (pixelX < width && rowBytes[pixelX * 4 + 3] != 0) {
          ++pixelX; // opaque run
        }
        const RECT runRect = {runStart, pixelY, pixelX, pixelY + 1};
        rects.push_back(runRect);
      }
    }

    // Hand the rects to ExtCreateRegion in chunks OR'd together. Win9x choked on
    // more than ~4000 rects per call; 2000 keeps a margin (same as neko-ng).
    HRGN hRgn                = nullptr;
    static const size_t kMax = 2000;
    for (size_t start = 0; start < rects.size(); start += kMax) {
      const size_t count = std::min(kMax, rects.size() - start);
      const size_t bytes = sizeof(RGNDATAHEADER) + sizeof(RECT) * count;
      std::vector<BYTE> buf(bytes);
      RGNDATA* data      = reinterpret_cast<RGNDATA*>(buf.data());
      data->rdh.dwSize   = sizeof(RGNDATAHEADER);
      data->rdh.iType    = RDH_RECTANGLES;
      data->rdh.nCount   = static_cast<DWORD>(count);
      data->rdh.nRgnSize = 0;
      SetRect(&data->rdh.rcBound, 0, 0, width, height); // loose bound is fine; it's a hint
      // The rects follow the header; write them through the vector's storage
      // rather than RGNDATA::Buffer[1], which newer GCC flags as out of bounds.
      memcpy(buf.data() + sizeof(RGNDATAHEADER), &rects[start], sizeof(RECT) * count);
      HRGN part = ExtCreateRegion(nullptr, static_cast<DWORD>(bytes), data);
      if (part == nullptr) {
        if (hRgn != nullptr) {
          DeleteObject(hRgn);
        }
        return nullptr;
      }
      if (hRgn == nullptr) {
        hRgn = part;
      } else {
        CombineRgn(hRgn, hRgn, part, RGN_OR);
        DeleteObject(part);
      }
    }
    if (hRgn == nullptr) {
      hRgn = CreateRectRgn(0, 0, 0, 0); // fully transparent frame: an empty region
    }
    return hRgn;
  }

  const DisplayProfile& ProfileForHeight(int screenHeight) {
    const size_t rows = sizeof(kDisplayProfiles) / sizeof(kDisplayProfiles[0]);
    for (size_t row = 0; row < rows; ++row) {
      if (screenHeight <= kDisplayProfiles[row].maxHeight) {
        return kDisplayProfiles[row];
      }
    }
    return kDisplayProfiles[rows - 1]; // can't happen: the last row's maxHeight is INT_MAX
  }

  LoadResult LoadSprites(HINSTANCE hInst,
                         const int (&resIds)[DIR_COUNT],
                         int spriteSize,
                         Sprites* out) {
    if (hInst == nullptr || out == nullptr) {
      return LOAD_BAD_ARGS;
    }
    FreeSprites(out); // whatever it held, so a reload never leaks (also zeroes it)
    out->memDC = CreateCompatibleDC(nullptr);
    if (out->memDC == nullptr) {
      return LOAD_REGION_FAILED;
    }
    for (int frame = 0; frame < DIR_COUNT; ++frame) {
      // LR_CREATEDIBSECTION keeps the resource's 32-bpp layout (and its alpha
      // byte) instead of converting to the screen format, so the region builder
      // can read it back.
      HBITMAP bitmap = LoadBitmap(hInst, resIds[frame]);
      if (bitmap == nullptr) {
        FreeSprites(out);
        return LOAD_MISSING_FRAME;
      }
      BITMAP bitmapInfo = {};
      if (GetObjectW(bitmap, sizeof(bitmapInfo), &bitmapInfo) == 0 ||
          bitmapInfo.bmBitsPixel != 32) {
        DeleteObject(bitmap);
        FreeSprites(out);
        return LOAD_NOT_32BPP;
      }
      // Resample to the size the display table asked for, unless it already is.
      if (spriteSize > 0 && bitmapInfo.bmHeight != spriteSize) {
        HBITMAP scaled = ScaleDib(bitmap, spriteSize);
        DeleteObject(bitmap);
        if (scaled == nullptr) {
          FreeSprites(out);
          return LOAD_SCALE_FAILED;
        }
        bitmap = scaled;
        GetObjectW(bitmap, sizeof(bitmapInfo), &bitmapInfo);
      }
      out->bmp[frame] = bitmap;
      if (frame == 0) {
        out->size.cx = bitmapInfo.bmWidth;
        out->size.cy = bitmapInfo.bmHeight;
      } else if (bitmapInfo.bmWidth != out->size.cx || bitmapInfo.bmHeight != out->size.cy) {
        FreeSprites(out);
        return LOAD_SIZE_MISMATCH;
      }
      out->rgn[frame] = BuildAlphaRegion(bitmap);
      if (out->rgn[frame] == nullptr) {
        FreeSprites(out);
        return LOAD_REGION_FAILED;
      }
      if (!BuildBlitPair(bitmap, &out->mask[frame], &out->opaque[frame])) {
        FreeSprites(out);
        return LOAD_REGION_FAILED;
      }
    }
    return LOAD_OK;
  }

  LoadResult LoadSpritesForProfile(HINSTANCE hInst,
                                   const FrameIds& frameIds,
                                   const DisplayProfile& profile,
                                   Sprites* out) {
    return LoadSprites(hInst, frameIds[profile.set], profile.spriteSize, out);
  }

  const wchar_t* LoadResultText(LoadResult result) {
    switch (result) {
      case LOAD_OK:
        return L"OK";
      case LOAD_BAD_ARGS:
        return L"bad arguments";
      case LOAD_MISSING_FRAME:
        return L"a frame bitmap resource is missing";
      case LOAD_NOT_32BPP:
        return L"a frame bitmap is not 32-bpp";
      case LOAD_SIZE_MISMATCH:
        return L"the frame bitmaps are not all the same size";
      case LOAD_SCALE_FAILED:
        return L"a frame could not be scaled";
      case LOAD_REGION_FAILED:
        return L"GDI could not build a frame's region or mask";
      default:
        return L"unknown error";
    }
  }

  void FreeSprites(Sprites* sprites) {
    if (sprites == nullptr) {
      return;
    }
    if (sprites->memDC != nullptr) {
      DeleteDC(sprites->memDC);
    }
    for (int frame = 0; frame < DIR_COUNT; ++frame) {
      if (sprites->bmp[frame] != nullptr) {
        DeleteObject(sprites->bmp[frame]);
      }
      if (sprites->rgn[frame] != nullptr) {
        DeleteObject(sprites->rgn[frame]);
      }
      if (sprites->mask[frame] != nullptr) {
        DeleteObject(sprites->mask[frame]);
      }
      if (sprites->opaque[frame] != nullptr) {
        DeleteObject(sprites->opaque[frame]);
      }
    }
    ZeroMemory(sprites, sizeof(*sprites));
  }

  HRGN CloneRegion(const Sprites& sprites, Dir dir) {
    if (dir < 0 || dir >= DIR_COUNT || sprites.rgn[dir] == nullptr) {
      return nullptr;
    }
    HRGN copy = CreateRectRgn(0, 0, 0, 0);
    if (copy == nullptr) {
      return nullptr;
    }
    CombineRgn(copy, sprites.rgn[dir], nullptr, RGN_COPY);
    return copy;
  }

  /* ------------------------------- Movement --------------------------------- */

  void VacuumSetArea(Vacuum* vacuum, const RECT& area) {
    if (vacuum == nullptr) {
      return;
    }
    vacuum->bounds.left   = area.left;
    vacuum->bounds.top    = area.top;
    vacuum->bounds.right  = area.right - vacuum->size.cx;  // pos may reach this, so the
    vacuum->bounds.bottom = area.bottom - vacuum->size.cy; // sprite's far edge meets area's
    if (vacuum->bounds.right < vacuum->bounds.left) {
      vacuum->bounds.right = vacuum->bounds.left; // area narrower than the sprite: pin
    }
    if (vacuum->bounds.bottom < vacuum->bounds.top) {
      vacuum->bounds.bottom = vacuum->bounds.top;
    }
    // Keep the sprite, and where it is going, inside the (possibly smaller) area.
    vacuum->pos.x    = std::clamp(vacuum->pos.x, vacuum->bounds.left, vacuum->bounds.right);
    vacuum->pos.y    = std::clamp(vacuum->pos.y, vacuum->bounds.top, vacuum->bounds.bottom);
    vacuum->target.x = std::clamp(vacuum->target.x, vacuum->bounds.left, vacuum->bounds.right);
    vacuum->target.y = std::clamp(vacuum->target.y, vacuum->bounds.top, vacuum->bounds.bottom);
  }

  void VacuumInit(Vacuum* vacuum, const RECT& area, const SIZE& spriteSize, int speed) {
    if (vacuum == nullptr) {
      return;
    }
    vacuum->size     = spriteSize;
    vacuum->pos.x    = area.left; // SetArea clamps this into the derived bounds
    vacuum->pos.y    = area.top;
    vacuum->target.x = area.left;
    vacuum->target.y = area.top;
    VacuumSetArea(vacuum, area);
    vacuum->facing = DIR_DOWN;
    vacuum->speed  = std::clamp(speed, kSpeedMin, kSpeedMax);
  }

  void VacuumSetTarget(Vacuum* vacuum, int targetX, int targetY) {
    if (vacuum == nullptr) {
      return;
    }
    // Clamped, so "past the edge" means "as far that way as it can go" and a
    // target is never somewhere the vacuum can't actually reach.
    vacuum->target.x =
        std::clamp(static_cast<LONG>(targetX), vacuum->bounds.left, vacuum->bounds.right);
    vacuum->target.y =
        std::clamp(static_cast<LONG>(targetY), vacuum->bounds.top, vacuum->bounds.bottom);
  }

  bool VacuumAtTarget(const Vacuum* vacuum) {
    return vacuum != nullptr && vacuum->pos.x == vacuum->target.x &&
           vacuum->pos.y == vacuum->target.y;
  }

  bool VacuumStep(Vacuum* vacuum) {
    if (vacuum == nullptr) {
      return false;
    }
    const int deltaX = vacuum->target.x - vacuum->pos.x;
    const int deltaY = vacuum->target.y - vacuum->pos.y;
    if (deltaX == 0 && deltaY == 0) {
      return false;
    }
    int moveX, moveY;
    const double len =
        std::sqrt(static_cast<double>(deltaX) * deltaX + static_cast<double>(deltaY) * deltaY);
    if (len <= vacuum->speed) {
      moveX = deltaX; // closer than one step: jump the gap and land exactly on target
      moveY = deltaY;
    } else {
      moveX = static_cast<int>(std::lround(vacuum->speed * deltaX / len));
      moveY = static_cast<int>(std::lround(vacuum->speed * deltaY / len));
      // Rounding can zero both for a tiny speed on a shallow angle; always creep
      // at least one pixel along the dominant axis so it can never stall.
      if (moveX == 0 && moveY == 0) {
        if (std::abs(deltaX) >= std::abs(deltaY)) {
          moveX = deltaX > 0 ? 1 : -1;
        } else {
          moveY = deltaY > 0 ? 1 : -1;
        }
      }
    }
    const POINT old = vacuum->pos;
    vacuum->pos.x   = std::clamp(vacuum->pos.x + moveX, vacuum->bounds.left, vacuum->bounds.right);
    vacuum->pos.y   = std::clamp(vacuum->pos.y + moveY, vacuum->bounds.top, vacuum->bounds.bottom);
    const int actualX = vacuum->pos.x - old.x;
    const int actualY = vacuum->pos.y - old.y;
    if (actualX == 0 && actualY == 0) {
      return false; // can't happen with a clamped target; belt and braces
    }
    // Face the dominant axis of the move actually made; ties go horizontal.
    if (std::abs(actualX) >= std::abs(actualY)) {
      vacuum->facing = actualX > 0 ? DIR_RIGHT : DIR_LEFT;
    } else {
      vacuum->facing = actualY > 0 ? DIR_DOWN : DIR_UP;
    }
    return true;
  }

  RECT VacuumRect(const Vacuum* vacuum) {
    RECT rect = {0, 0, 0, 0};
    if (vacuum != nullptr) {
      SetRect(&rect, vacuum->pos.x, vacuum->pos.y, vacuum->pos.x + vacuum->size.cx,
              vacuum->pos.y + vacuum->size.cy);
    }
    return rect;
  }

  /* ------------------------------- Drawing ---------------------------------- */

  void DrawFrame(HDC hdc, const Sprites& sprites, Dir dir, int posX, int posY) {
    if (hdc == nullptr || sprites.memDC == nullptr || dir < 0 || dir >= DIR_COUNT ||
        sprites.mask[dir] == nullptr || sprites.opaque[dir] == nullptr) {
      return;
    }
    // AND the mask on (clears the frame's opaque pixels to black, leaves the
    // rest of the destination alone), then OR the frame on (fills exactly that
    // black with the sprite). BitBlt honours whatever clip the DC already has.
    HGDIOBJ previous = SelectObject(sprites.memDC, sprites.mask[dir]);
    BitBlt(hdc, posX, posY, sprites.size.cx, sprites.size.cy, sprites.memDC, 0, 0, SRCAND);
    SelectObject(sprites.memDC, sprites.opaque[dir]);
    BitBlt(hdc, posX, posY, sprites.size.cx, sprites.size.cy, sprites.memDC, 0, 0, SRCPAINT);
    SelectObject(sprites.memDC, previous);
  }

  void DrawVacuum(HDC hdc, const Sprites& sprites, const Vacuum& vacuum) {
    DrawFrame(hdc, sprites, vacuum.facing, vacuum.pos.x, vacuum.pos.y);
  }

  void EraseRect(HDC hdc, const RECT& rect, COLORREF color) {
    if (hdc == nullptr) {
      return;
    }
    HBRUSH brush = CreateSolidBrush(color);
    if (brush == nullptr) {
      return;
    }
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
  }

  COLORREF DefaultEraseColor() {
    return GetSysColor(COLOR_DESKTOP);
  }

  /* ------------------------------- Settings --------------------------------- */

  void LoadEraseSettings(const wchar_t* subkey, int defaultSpeed, EraseSettings* settings) {
    if (settings == nullptr) {
      return;
    }
    DWORD value               = 0;
    settings->speed           = std::clamp(defaultSpeed, kSpeedMin, kSpeedMax);
    settings->customColor     = RGB(0, 0, 0);
    settings->useDesktopColor = true;
    ZeroMemory(settings->customPalette, sizeof(settings->customPalette));
    if (RegReadDword(subkey, kRegSpeed, &value)) {
      settings->speed = std::clamp(static_cast<int>(value), kSpeedMin, kSpeedMax);
    }
    if (RegReadDword(subkey, kRegEraseColor, &value)) {
      settings->customColor = static_cast<COLORREF>(value);
    }
    if (RegReadDword(subkey, kRegUseDesktop, &value)) {
      settings->useDesktopColor = (value != 0);
    }
  }

  bool SaveEraseSettings(const wchar_t* subkey, const EraseSettings& settings) {
    bool success = RegWriteDword(subkey, kRegSpeed, static_cast<DWORD>(settings.speed));
    success =
        RegWriteDword(subkey, kRegEraseColor, static_cast<DWORD>(settings.customColor)) && success;
    success = RegWriteDword(subkey, kRegUseDesktop, settings.useDesktopColor ? 1u : 0u) && success;
    return success;
  }

  COLORREF EffectiveEraseColor(const EraseSettings& settings) {
    return settings.useDesktopColor ? DefaultEraseColor() : settings.customColor;
  }

  bool PickEraseColor(HWND owner, EraseSettings* settings) {
    if (settings == nullptr) {
      return false;
    }
    CHOOSECOLORW chooser = {};
    chooser.lStructSize  = sizeof(chooser);
    chooser.hwndOwner    = owner;
    chooser.rgbResult    = settings->customColor;
    chooser.lpCustColors = settings->customPalette;
    chooser.Flags        = CC_RGBINIT | CC_FULLOPEN;
    if (!ChooseColorW(&chooser)) {
      return false; // cancelled
    }
    settings->customColor     = chooser.rgbResult;
    settings->useDesktopColor = false; // picking a colour means "not the desktop's"
    return true;
  }

  /* ------------------------------- Registry --------------------------------- */

  bool RegReadDword(const wchar_t* subkey, const wchar_t* name, DWORD* out) {
    if (subkey == nullptr || name == nullptr || out == nullptr) {
      return false;
    }
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RegPath(subkey).c_str(), 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
      return false;
    }
    DWORD type  = 0;
    DWORD value = 0;
    DWORD size  = sizeof(value);
    const LONG result =
        RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || type != REG_DWORD || size != sizeof(value)) {
      return false;
    }
    *out = value;
    return true;
  }

  bool RegWriteDword(const wchar_t* subkey, const wchar_t* name, DWORD value) {
    if (subkey == nullptr || name == nullptr) {
      return false;
    }
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RegPath(subkey).c_str(), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) {
      return false;
    }
    const LONG result = RegSetValueExW(key, name, 0, REG_DWORD,
                                       reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
  }

  bool RegDeleteSubkey(const wchar_t* subkey) {
    if (subkey == nullptr) {
      return false;
    }
    // Our keys hold values only, never subkeys, so the plain NT4-era delete
    // (which refuses a key with children) is all that is needed.
    const LONG result = RegDeleteKeyW(HKEY_CURRENT_USER, RegPath(subkey).c_str());
    return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
  }

} // namespace pet

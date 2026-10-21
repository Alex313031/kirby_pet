// pet_lib: vacuum sprites, alpha regions, and movement shared by the Kirby Pet
// app and the Kirby screensaver.

#include "pet.h"

#include <cstdlib> // std::abs(int)
#include <cstring> // memcpy

#include "include/pet_lib.h"

namespace pet {

  /* -------------------------- Forward declarations -------------------------- */

  static HBITMAP LoadBitmap(HINSTANCE hInst, const int resId);
  static std::wstring RegPath(const wchar_t* subkey);

  /* ---------------------------- Static Functions ---------------------------- */

  static HBITMAP LoadBitmap(HINSTANCE hInst, const int resId) {
    if (hInst == nullptr || resId == -1) {
      return nullptr;
    }
    return static_cast<HBITMAP>(
        LoadImageW(hInst, MAKEINTRESOURCEW(resId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
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

  bool LoadSprites(HINSTANCE hInst, const int resIds[DIR_COUNT], Sprites* out) {
    if (hInst == nullptr || resIds == nullptr || out == nullptr) {
      return false;
    }
    ZeroMemory(out, sizeof(*out));
    for (int frame = 0; frame < DIR_COUNT; ++frame) {
      // LR_CREATEDIBSECTION keeps the resource's 32-bpp layout (and its alpha
      // byte) instead of converting to the screen format, so the region builder
      // can read it back.
      HBITMAP bitmap = LoadBitmap(hInst, resIds[frame]);
      if (bitmap == nullptr) {
        FreeSprites(out);
        return false;
      }
      out->bmp[frame]   = bitmap;
      BITMAP bitmapInfo = {};
      if (GetObjectW(bitmap, sizeof(bitmapInfo), &bitmapInfo) == 0 ||
          bitmapInfo.bmBitsPixel != 32) {
        FreeSprites(out);
        return false;
      }
      if (frame == 0) {
        out->size.cx = bitmapInfo.bmWidth;
        out->size.cy = bitmapInfo.bmHeight;
      } else if (bitmapInfo.bmWidth != out->size.cx || bitmapInfo.bmHeight != out->size.cy) {
        FreeSprites(out);
        return false;
      }
      out->rgn[frame] = BuildAlphaRegion(bitmap);
      if (out->rgn[frame] == nullptr) {
        FreeSprites(out);
        return false;
      }
    }
    return true;
  }

  void FreeSprites(Sprites* sprites) {
    if (sprites == nullptr) {
      return;
    }
    for (int frame = 0; frame < DIR_COUNT; ++frame) {
      if (sprites->bmp[frame] != nullptr) {
        DeleteObject(sprites->bmp[frame]);
      }
      if (sprites->rgn[frame] != nullptr) {
        DeleteObject(sprites->rgn[frame]);
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

  /* ------------------------------- Movement -------------------------------- */

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
    // Keep the sprite inside the (possibly smaller) new area.
    vacuum->pos.x = std::clamp(vacuum->pos.x, vacuum->bounds.left, vacuum->bounds.right);
    vacuum->pos.y = std::clamp(vacuum->pos.y, vacuum->bounds.top, vacuum->bounds.bottom);
  }

  void VacuumInit(Vacuum* vacuum, const RECT& area, const SIZE& spriteSize, int speed) {
    if (vacuum == nullptr) {
      return;
    }
    vacuum->size  = spriteSize;
    vacuum->pos.x = area.left; // SetArea clamps this into the derived bounds
    vacuum->pos.y = area.top;
    VacuumSetArea(vacuum, area);
    vacuum->target = vacuum->pos;
    vacuum->facing = DIR_DOWN;
    vacuum->speed  = speed > 0 ? speed : 1;
  }

  void VacuumSetTarget(Vacuum* vacuum, int targetX, int targetY) {
    if (vacuum == nullptr) {
      return;
    }
    vacuum->target.x = targetX;
    vacuum->target.y = targetY;
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
      return false; // pinned against the bounds
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

  /* ------------------------------- Drawing --------------------------------- */

  void DrawFrame(HDC hdc, const Sprites& sprites, Dir dir, int posX, int posY) {
    if (hdc == nullptr || dir < 0 || dir >= DIR_COUNT) {
      return;
    }
    const HBITMAP frame = sprites.bmp[dir];
    if (frame == nullptr) {
      return;
    }
    HDC mem = CreateCompatibleDC(hdc);
    if (mem == nullptr) {
      return;
    }
    HGDIOBJ old = SelectObject(mem, frame);

    // Clip the blit to the frame's opaque pixels, offset to where it lands, and
    // intersected with any clip the caller already had so we never widen it.
    HRGN saved    = CreateRectRgn(0, 0, 0, 0);
    const int had = GetClipRgn(hdc, saved); // 1 = a clip existed and is now in `saved`
    HRGN shape    = CreateRectRgn(0, 0, 0, 0);
    if (shape != nullptr && saved != nullptr) {
      CombineRgn(shape, sprites.rgn[dir], nullptr, RGN_COPY);
      OffsetRgn(shape, posX, posY);
      if (had == 1) {
        CombineRgn(shape, shape, saved, RGN_AND);
      }
      SelectClipRgn(hdc, shape); // SelectClipRgn copies, so `shape` is ours to delete
      BitBlt(hdc, posX, posY, sprites.size.cx, sprites.size.cy, mem, 0, 0, SRCCOPY);
      SelectClipRgn(hdc, had == 1 ? saved : nullptr); // put the caller's clip back
    }
    if (shape != nullptr) {
      DeleteObject(shape);
    }
    if (saved != nullptr) {
      DeleteObject(saved);
    }
    SelectObject(mem, old);
    DeleteDC(mem);
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

  /* ------------------------------- Registry -------------------------------- */

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

} // namespace pet

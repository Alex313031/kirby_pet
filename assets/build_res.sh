#!/bin/bash

# Copyright (c) 2026 Alex313031.
#
# Builds the binary resources in src/res from the originals kept here:
#
#   sprites/{1x,2x}/{up,down,left,right}.bmp  ->  ../src/res/bmp/<name>_<set>.bmp
#     Photoshop / GIMP export (56-byte BITMAPV3 header with channel masks) to
#     the plain 40-byte BITMAPINFOHEADER, 32-bpp BI_RGB, bottom-up BGRA layout
#     that LoadImage accepts, alpha kept.
#
#   vacuum_on.mp3  ->  ../src/res/sounds/3CB.wav
#     Stereo Microsoft ADPCM: the one WAVE format that plays reliably on every
#     Windows version, at a quarter the size of PCM. The rate is the size knob
#     (ADPCM is 4 bits per sample whatever the rate): see WAV_RATE below.
#
#   ./build_res.sh                 build everything
#   ./build_res.sh sprites         just the sprites
#   ./build_res.sh wav             just the wav, at WAV_RATE
#   ./build_res.sh preview [rate ...]
#                                  encode at each rate (default WAV_RATE) into
#                                  /tmp and play it, so rates can be compared by
#                                  ear before changing WAV_RATE
#   ./build_res.sh sizes           encode every rate in WAV_RATES_TO_TRY into /tmp
#                                  and list the resulting sizes
#
# Needs ffmpeg and python3 (standard library only). preview uses ffplay, mpv,
# or ffmpeg piped into aplay, whichever is installed.

YEL='\033[1;33m' # Yellow
CYA='\033[1;96m' # Cyan
RED='\033[1;31m' # Red
GRE='\033[1;32m' # Green
c0='\033[0m' # Reset Text
bold='\033[1m' # Bold Text
underline='\033[4m' # Underline Text

# --- The knobs ---------------------------------------------------------------

# Sample rate of the wav that goes into src/res. 44100 is the original; 22050
# halves the file and drops nothing a vacuum has (its content above 11 kHz is
# ~28 dB below the rest); 16000 is a third; 11025 starts to sound dull.
WAV_RATE=22050
WAV_CHANNELS=2
WAV_VOLUME=1.0

# What `sizes` and a bare `preview` run through.
WAV_RATES_TO_TRY="44100 32000 22050 16000 11025"

# The ffmpeg line. Everything above feeds into it; tweak it here if the
# encoding itself needs changing (codec, filters, trimming with -ss / -t).
encode_wav() {
  local rate="$1"
  local out="$2"
  ffmpeg -loglevel error -y -i "${MP3}" -filter:a "volume=${WAV_VOLUME}" -c:a adpcm_ms \
    -ar "${rate}" -ac "${WAV_CHANNELS}" "${out}"
}

# --- Paths -------------------------------------------------------------------

# Current dir (where this script lives, not where it was invoked)
export HERE=$(cd "$(dirname "$0")" && pwd)
export ROOT=$(cd "${HERE}/.." && pwd)

MP3="${HERE}/vacuum_on.mp3"
SPRITES_IN="${HERE}/sprites"
BMP_OUT="${ROOT}/src/res/bmp"
WAV_OUT="${ROOT}/src/res/sounds/3CB.wav"
PREVIEW_DIR="/tmp/kirby_pet_wav_preview"

# --- Helpers -----------------------------------------------------------------

need() {
  command -v "$1" >/dev/null 2>&1 || { printf "${RED} $1 is required but not installed ${c0}\n"; exit 1; }
}

# One Photoshop/GIMP BMP -> one plain BMP. Parses the 56-byte header by hand:
# PIL refuses it, and ImageMagick would pick its own output header. Pure
# standard-library python3.
convert_sprite() {
  local in="$1"
  local out="$2"
  python3 - "${in}" "${out}" <<'PYEOF'
import struct, sys
src, dst = sys.argv[1], sys.argv[2]
data = open(src, 'rb').read()
magic, _, _, _, offset = struct.unpack_from('<2sIHHI', data, 0)
hsize, width, height, planes, bpp, comp, _, _, _, _, _ = struct.unpack_from('<IiiHHIIiiII', data, 14)
if magic != b'BM' or bpp != 32:
    sys.exit('%s: need a 32-bpp BMP (got magic %r, %d bpp)' % (src, magic, bpp))
if hsize >= 56:
    rmask, gmask, bmask, amask = struct.unpack_from('<IIII', data, 14 + 40)
else:
    rmask, gmask, bmask, amask = 0, 0, 0, 0
if comp == 0 or rmask == 0:                 # no masks: Windows default order
    rmask, gmask, bmask, amask = 0xFF0000, 0xFF00, 0xFF, 0xFF000000

def shift(mask):
    n = 0
    while mask and not (mask >> n) & 1:
        n += 1
    return n

rs, gs, bs, as_ = (shift(m) for m in (rmask, gmask, bmask, amask))
topdown = height < 0
height = abs(height)
stride = ((width * 4 + 3) // 4) * 4
rows = []                                    # bottom-up order, as stored
for row in range(height):
    line = data[offset + row * stride: offset + row * stride + width * 4]
    out = bytearray()
    for x in range(width):
        px = struct.unpack_from('<I', line, x * 4)[0]
        a = ((px & amask) >> as_) if amask else 255
        out += bytes(((px & bmask) >> bs, (px & gmask) >> gs, (px & rmask) >> rs, a))
    rows.append(bytes(out))
if topdown:
    rows.reverse()
pixels = b''.join(rows)
file_header = struct.pack('<2sIHHI', b'BM', 14 + 40 + len(pixels), 0, 0, 54)
info_header = struct.pack('<IiiHHIIiiII', 40, width, height, 1, 32, 0, len(pixels), 11811, 11811, 0, 0)
open(dst, 'wb').write(file_header + info_header + pixels)
alphas = pixels[3::4]
print('    %dx%d, %d of %d px transparent' % (width, height, alphas.count(0), width * height))
PYEOF
}

play_wav() {
  local file="$1"
  if command -v ffplay >/dev/null 2>&1; then
    ffplay -nodisp -autoexit -loglevel error "${file}"
  elif command -v mpv >/dev/null 2>&1; then
    mpv --no-video --really-quiet "${file}"
  elif command -v aplay >/dev/null 2>&1; then
    # aplay only speaks PCM; decode on the way through.
    ffmpeg -loglevel error -i "${file}" -f wav - | aplay -q
  else
    printf "${RED} No player found (ffplay, mpv or aplay) ${c0}\n"
    return 1
  fi
}

wav_size() {
  stat -c %s "$1"
}

# --- Targets -----------------------------------------------------------------

build_sprites() {
  need python3
  printf "${CYA} Sprites: ${SPRITES_IN} -> ${BMP_OUT} ${c0}\n"
  mkdir -p "${BMP_OUT}"
  for set in 1x 2x; do
    for name in up down left right; do
      local in="${SPRITES_IN}/${set}/${name}.bmp"
      local out="${BMP_OUT}/${name}_${set}.bmp"
      [ -f "${in}" ] || { printf "${RED} missing ${in} ${c0}\n"; exit 1; }
      printf "  ${name}_${set}.bmp\n"
      convert_sprite "${in}" "${out}" || exit 1
    done
  done
}

build_wav() {
  need ffmpeg
  printf "${CYA} Wav: ${MP3} -> ${WAV_OUT} at ${WAV_RATE} Hz ${c0}\n"
  mkdir -p "$(dirname "${WAV_OUT}")"
  encode_wav "${WAV_RATE}" "${WAV_OUT}" || exit 1
  printf "  $(wav_size "${WAV_OUT}") bytes\n"
}

preview_wav() {
  need ffmpeg
  mkdir -p "${PREVIEW_DIR}"
  local rates="$*"
  [ -n "${rates}" ] || rates="${WAV_RATE}"
  for rate in ${rates}; do
    local file="${PREVIEW_DIR}/3CB_${rate}.wav"
    encode_wav "${rate}" "${file}" || exit 1
    printf "${CYA} ${rate} Hz: $(wav_size "${file}") bytes - playing ${file} ${c0}\n"
    play_wav "${file}"
  done
}

list_sizes() {
  need ffmpeg
  mkdir -p "${PREVIEW_DIR}"
  printf "${CYA} Stereo MS-ADPCM size by sample rate (files left in ${PREVIEW_DIR}) ${c0}\n"
  local base=""
  for rate in ${WAV_RATES_TO_TRY}; do
    local file="${PREVIEW_DIR}/3CB_${rate}.wav"
    encode_wav "${rate}" "${file}" || exit 1
    local size=$(wav_size "${file}")
    [ -n "${base}" ] || base=${size}
    printf "  %6d Hz  %8d bytes  (%3d%%)%s\n" "${rate}" "${size}" $((100 * size / base)) \
      "$([ "${rate}" = "${WAV_RATE}" ] && echo '  <- WAV_RATE')"
  done
}

# --- Main --------------------------------------------------------------------

case "${1:-all}" in
  all)
    build_sprites
    build_wav
    ;;
  sprites)
    build_sprites
    ;;
  wav)
    build_wav
    ;;
  preview)
    shift
    preview_wav "$@"
    ;;
  sizes)
    list_sizes
    ;;
  *)
    printf "${YEL} usage: $0 [all|sprites|wav|preview [rate ...]|sizes] ${c0}\n"
    exit 1
    ;;
esac

printf "${GRE} Done. ${c0}\n"
exit 0

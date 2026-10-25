#!/bin/sh
# Builds the screen preview on a desktop and renders every screen to PNG.
#   tools/screen-preview/build.sh [recorder|bench213] [out-dir]
# Needs a C++ compiler, Python 3 with Pillow, and one `pio run` in firmware/
# first (for the Adafruit GFX sources and the Arduino core's String).
set -e
here=$(cd "$(dirname "$0")" && pwd)
root="$here/../.."
env=${1:-recorder}
out=${2:-$here/out-$env}
core="$HOME/.platformio/packages/framework-arduinoespressif32/cores/esp32"
gfx="$root/firmware/.pio/libdeps/$env/Adafruit GFX Library"
case "$env" in
  recorder) display=-DPEN_DISPLAY_097 ;;
  bench213) display=-DPEN_DISPLAY_213 ;;
  *) echo "unknown env $env" >&2; exit 1 ;;
esac
mkdir -p "$out/core"
# Compile copies of the core sources so their #include "Arduino.h" finds the
# shim rather than the real core header next to them.
cp "$core/WString.cpp" "$core/Print.cpp" "$core/stdlib_noniso.c" "$out/core/"
c++ -std=gnu++17 -O1 -w -DARDUINO=10000 $display \
  -I"$here/shim" -I"$root/firmware/src" -I"$gfx" -I"$core" \
  "$here/preview.cpp" "$root/firmware/src/screen.cpp" "$gfx/Adafruit_GFX.cpp" \
  "$out/core/WString.cpp" "$out/core/Print.cpp" -x c "$out/core/stdlib_noniso.c" "$here/shim/itoa.c" \
  -o "$out/preview"
"$out/preview" "$out"
python3 "$here/sheet.py" "$out"

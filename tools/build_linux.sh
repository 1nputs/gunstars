#!/usr/bin/env bash
# gunstar for x86-64 Linux as the release has it: build/linux-x64/gunstar, built with Zig's Clang
# against glibc 2.28 (so it runs on distributions from 2018 on) with the C++ runtime linked in, so
# it needs only the system's SDL2. tools/package_release.sh packs it; make golden-cross plays the
# golden runs with it (a Clang build against the GCC one's: netplay's peers may be either).
# Needs Zig (`zig`, or `pip install ziglang`; ZIG= names another) and SDL2's development files
# (the headers and libSDL2-2.0.so.0 to link against: libsdl2-dev).
set -euo pipefail
cd "$(dirname "$0")/.."
if [ -z "${ZIG:-}" ]; then
  if command -v zig > /dev/null; then ZIG=zig
  elif python3 -c "import ziglang" 2> /dev/null; then ZIG="python3 -m ziglang"
  else echo "no Zig: install it (or pip install ziglang), or give ZIG=..."; exit 1; fi
fi
# SDL2's headers alone (not the system's C headers, which would override Zig's glibc 2.28 ones):
# its directory, and on Debian-style systems the multiarch one with its configuration
B=build/linux-x64
rm -rf "$B"
mkdir -p "$B/obj" "$B/sdlinc/a" "$B/sdlinc/b"
INC=$(pkg-config --variable=includedir sdl2)
LIBDIR=$(pkg-config --variable=libdir sdl2)
ln -s "$INC/SDL2" "$B/sdlinc/a/SDL2"
MA=$(gcc -print-multiarch 2> /dev/null || true)
[ -n "$MA" ] && [ -d "$INC/$MA/SDL2" ] && ln -s "$INC/$MA/SDL2" "$B/sdlinc/b/SDL2"
SDLLIB=$(ls "$LIBDIR"/libSDL2-2.0.so.0 2> /dev/null || ls "$LIBDIR"/libSDL2.so)
T="-target x86_64-linux-gnu.2.28"
SDLC="-I$PWD/$B/sdlinc/b -I$PWD/$B/sdlinc/a -I$INC/SDL2 -D_REENTRANT"
CFLAGS="-std=c11 -O2 -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Werror"
make -s -C ext/pulseman MODE=release B=build/linux-x64 CC="$ZIG cc $T" CXX="$ZIG c++ $T" AR="$ZIG ar" \
  SDLCFLAGS="$SDLC" build/linux-x64/libmdrt.a build/linux-x64/libmdrt_sdl2.a
make -s build/ports_gen.inc build/build_id.h
ls src/*.c native/*.c | xargs -P "$(nproc)" -I{} sh -c \
  "$ZIG cc $T $CFLAGS -Isrc -Ibuild -Inative -Iext/pulseman/include $SDLC -c {} -o $B/obj/\$(basename {} .c).o"
$ZIG c++ $T -s -o "$B/gunstar" "$B"/obj/*.o ext/pulseman/build/linux-x64/libmdrt_sdl2.a \
  ext/pulseman/build/linux-x64/libmdrt.a "$SDLLIB" -lm
echo "$B/gunstar"

#!/usr/bin/env bash
# gunstars for Windows (x86-64), cross-built on Linux with Zig's Clang and its MinGW-w64 headers
# and libraries, against SDL2's official MinGW development package:
#   build/win/gunstar.exe and SDL2.dll beside it (the C++ runtime is linked in; it needs nothing
#   else on Windows 10 or later), and build/gunstars-windows-x64.zip with the licence and README.
# Zig: `zig` on the PATH, or `pip install ziglang` (then `python3 -m ziglang`); ZIG= names another.
# SDL2: ref/sdl2-mingw (fetched from SDL's releases when missing; SDL2_MINGW= names another).
set -euo pipefail
cd "$(dirname "$0")/.."
SDL_VERSION=2.32.10
VERSION=$(sed -n 's/^#define GUNSTAR_VERSION "\(.*\)"/\1/p' native/main.c)
if [ -z "${ZIG:-}" ]; then
  if command -v zig > /dev/null; then ZIG=zig
  elif python3 -c "import ziglang" 2> /dev/null; then ZIG="python3 -m ziglang"
  else echo "no Zig: install it (or pip install ziglang), or give ZIG=..."; exit 1; fi
fi
SDL=${SDL2_MINGW:-ref/sdl2-mingw/SDL2-$SDL_VERSION/x86_64-w64-mingw32}
if [ ! -d "$SDL" ]; then
  mkdir -p ref/sdl2-mingw
  curl -sSfL -o ref/sdl2-mingw/sdl.tar.gz \
    "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-devel-$SDL_VERSION-mingw.tar.gz"
  tar xzf ref/sdl2-mingw/sdl.tar.gz -C ref/sdl2-mingw
fi
SDL=$(cd "$SDL" && pwd)                                  # (pulseman's make runs in its own directory)
TARGET="-target x86_64-windows-gnu"
CC="$ZIG cc $TARGET"
CXX="$ZIG c++ $TARGET"
SDLCFLAGS="-I$SDL/include -I$SDL/include/SDL2 -Dmain=SDL_main"
CFLAGS="-std=c11 -O2 -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes ${WERROR--Werror}"

# pulseman's libraries (its release build, in its own directory)
make -s -C ext/pulseman MODE=release B=build/win CC="$CC" CXX="$CXX" AR="$ZIG ar" SDLCFLAGS="$SDLCFLAGS" \
  build/win/libmdrt.a build/win/libmdrt_sdl2.a
# the generated adapters and dispatch (the same on every host)
make -s build/ports_gen.inc build/build_id.h
mkdir -p build/win/obj
ls src/*.c native/*.c | xargs -P "$(nproc)" -I{} sh -c \
  "$CC $CFLAGS -Isrc -Ibuild -Inative -Iext/pulseman/include $SDLCFLAGS -c {} -o build/win/obj/\$(basename {} .c).o"
$CXX -o build/win/gunstar.exe build/win/obj/*.o ext/pulseman/build/win/libmdrt_sdl2.a ext/pulseman/build/win/libmdrt.a \
  "$SDL/lib/libSDL2main.a" "$SDL/lib/libSDL2.dll.a" -lm -lws2_32
cp "$SDL/bin/SDL2.dll" build/win/
cp LICENSE build/win/LICENSE.txt
cat > build/win/README.txt <<EOF
gunstars $VERSION: Gunstar Heroes, rebuilt in C (https://github.com/1nputs/gunstars).

Run gunstar.exe. The first time it asks for your ROM: Gunstar Heroes (U), the 1 MB retail
cartridge; pick it in the file browser, or run gunstar.exe --rom "C:\\path\\to\\the.gen".
Keys: pad 1 arrows, Z / X / C = A / B / C, Enter = Start (A weapon, B shoot, C jump);
pad 2 I J K L, Q / W / E, Tab. Esc opens the menu (save slots, video, sound, controls).
F4 held rewinds, F5 pauses, F6 held fast-forwards, F10 saves the last 20 seconds as a GIF.

Settings, saves and screenshots go beside gunstar.exe. SDL2.dll is SDL's (zlib licence).
The game's code is MIT (LICENSE.txt); Gunstar Heroes itself belongs to Sega and Treasure.
EOF
(cd build/win && rm -f ../gunstars-windows-x64.zip && python3 -c "
import zipfile
with zipfile.ZipFile('../gunstars-windows-x64.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for f in ('gunstar.exe', 'SDL2.dll', 'README.txt', 'LICENSE.txt'): z.write(f, 'gunstars/' + f)
")
echo "build/win/gunstar.exe, build/gunstars-windows-x64.zip"

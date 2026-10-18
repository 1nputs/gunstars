#!/usr/bin/env bash
# The release files, in build/release/ (the version is GUNSTAR_VERSION in native/main.c):
#   gunstars-VERSION-linux-x64.tar.gz    gunstar for x86-64 Linux, built with Zig's Clang against
#                                        glibc 2.28 (so it runs on distributions from 2018 on) with
#                                        the C++ runtime linked in: it needs only the system's SDL2
#   gunstars-VERSION-windows-x64.zip     gunstar.exe and SDL2.dll (tools/build_windows.sh)
#   SHA256SUMS
#   release-notes.md                     this version's section of CHANGELOG.md, unwrapped
# Needs Zig (`zig`, or `pip install ziglang`; ZIG= names another) and SDL2's development files
# (the headers and libSDL2-2.0.so.0 to link against: libsdl2-dev).
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION=$(sed -n 's/^#define GUNSTAR_VERSION "\(.*\)"/\1/p' native/main.c)
[ -n "$VERSION" ] || { echo "no GUNSTAR_VERSION in native/main.c"; exit 1; }
if [ -z "${ZIG:-}" ]; then
  if command -v zig > /dev/null; then ZIG=zig
  elif python3 -c "import ziglang" 2> /dev/null; then ZIG="python3 -m ziglang"
  else echo "no Zig: install it (or pip install ziglang), or give ZIG=..."; exit 1; fi
fi
export ZIG
OUT=build/release
rm -rf "$OUT"
mkdir -p "$OUT"

# ---- Linux ---------------------------------------------------------------------------------
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
make -s build/ports_gen.inc
ls src/*.c native/*.c | xargs -P "$(nproc)" -I{} sh -c \
  "$ZIG cc $T $CFLAGS -Isrc -Ibuild -Inative -Iext/pulseman/include $SDLC -c {} -o $B/obj/\$(basename {} .c).o"
$ZIG c++ $T -s -o "$B/gunstar" "$B"/obj/*.o ext/pulseman/build/linux-x64/libmdrt_sdl2.a \
  ext/pulseman/build/linux-x64/libmdrt.a "$SDLLIB" -lm
P=$B/pkg/gunstars
mkdir -p "$P"
cp "$B/gunstar" "$P/"
cp LICENSE "$P/LICENSE.txt"
cat > "$P/README.txt" <<EOF
gunstars $VERSION: Gunstar Heroes, rebuilt in C (https://github.com/1nputs/gunstars).

Run ./gunstar. The first time it asks for your ROM: Gunstar Heroes (U), the 1 MB retail
cartridge; pick it in the file browser, or run ./gunstar --rom "/path/to/the.gen".
It needs SDL2 (most desktops have it):
  sudo apt install libsdl2-2.0-0     (Debian, Ubuntu)
  sudo dnf install SDL2              (Fedora)
  sudo pacman -S sdl2                (Arch)
and a 64-bit x86 Linux from about 2018 on (glibc 2.28).

Keys: pad 1 arrows, Z / X / C = A / B / C, Enter = Start (A weapon, B shoot, C jump);
pad 2 I J K L, Q / W / E, Tab. Esc opens the menu (save slots, video, sound, controls).
F4 held rewinds, F5 pauses, F6 held fast-forwards, F10 saves the last 20 seconds as a GIF.

Settings, saves and screenshots go in the folder you run it from.
The game's code is MIT (LICENSE.txt); Gunstar Heroes itself belongs to Sega and Treasure.
EOF
tar -C "$B/pkg" --owner=0 --group=0 -czf "$OUT/gunstars-$VERSION-linux-x64.tar.gz" gunstars

# ---- Windows -------------------------------------------------------------------------------
tools/build_windows.sh > /dev/null
cp build/gunstars-windows-x64.zip "$OUT/gunstars-$VERSION-windows-x64.zip"

(cd "$OUT" && sha256sum gunstars-* > SHA256SUMS)

# the release's notes: this version's section of CHANGELOG.md, for the GitHub release
python3 - "$VERSION" "$OUT/release-notes.md" <<'PYEOF'
import re, sys
version, out = sys.argv[1:3]
text = open('CHANGELOG.md').read()
m = re.search(r'^## ' + re.escape(version) + r'\b.*?(?=^## |\Z)', text, re.M | re.S)
if not m:
    sys.exit(f'no section for {version} in CHANGELOG.md')
# unwrapped: a paragraph or a list item on one line (GitHub shows every line break in a release's
# notes); headings, tables, code and blank lines as they are
lines, code = [], False
for line in m.group(0).rstrip().split('\n'):
    starts = line.startswith(('#', '|', '- ', '* ', '> ')) or re.match(r'\d+\. ', line)
    if line.startswith('```'):
        code = not code
        lines.append(line)
    elif (not code and line.strip() and not starts and lines and lines[-1].strip()
          and not lines[-1].startswith(('#', '|', '```'))):
        lines[-1] += ' ' + line.strip()
    else:
        lines.append(line)
open(out, 'w').write('\n'.join(lines) + '\n')
PYEOF
ls -l "$OUT"

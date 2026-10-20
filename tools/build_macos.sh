#!/usr/bin/env bash
# gunstars for macOS 11 or later, Apple Silicon and Intel in one program, cross-built on Linux with
# Zig's Clang against SDL2's official macOS framework:
#   build/mac/pkg/gunstars/gunstars.app (Contents/MacOS/gunstar, SDL2.framework in
#   Contents/Frameworks) with the licence and README, and build/gunstars-macos-universal.zip.
# Zig strips each program as it links it and signs the Apple Silicon one ad hoc (stripped after,
# that signature would no longer hold); llvm-lipo joins the two and leaves each as it is. No
# registered developer signs it: the README says how to open it the first time.
# Zig: `zig` on the PATH, or `pip install ziglang` (then `python3 -m ziglang`); ZIG= names another.
# SDL2: ref/sdl2-macos (SDL's .dmg, fetched from SDL's releases when missing and unpacked with 7z;
# SDL2_MACOS= names another folder holding SDL2.framework). llvm-lipo: on the PATH or in
# /usr/lib/llvm-*/bin (LIPO= names another). zip: Info-ZIP's, which keeps the framework's links.
set -euo pipefail
cd "$(dirname "$0")/.."
SDL_VERSION=2.32.10
VERSION=$(sed -n 's/^#define GUNSTAR_VERSION "\(.*\)"/\1/p' native/main.c)
if [ -z "${ZIG:-}" ]; then
  if command -v zig > /dev/null; then ZIG=zig
  elif python3 -c "import ziglang" 2> /dev/null; then ZIG="python3 -m ziglang"
  else echo "no Zig: install it (or pip install ziglang), or give ZIG=..."; exit 1; fi
fi
LIPO=${LIPO:-$(command -v llvm-lipo || ls /usr/lib/llvm-*/bin/llvm-lipo 2> /dev/null | tail -n 1 || true)}
[ -n "$LIPO" ] || { echo "no llvm-lipo: install LLVM, or give LIPO=..."; exit 1; }
SDL=${SDL2_MACOS:-ref/sdl2-macos/SDL2-$SDL_VERSION/SDL2}
if [ ! -d "$SDL/SDL2.framework" ]; then
  mkdir -p ref/sdl2-macos
  curl -sSfL -o "ref/sdl2-macos/SDL2-$SDL_VERSION.dmg" \
    "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-$SDL_VERSION.dmg"
  7z x -y -o"ref/sdl2-macos/SDL2-$SDL_VERSION" "ref/sdl2-macos/SDL2-$SDL_VERSION.dmg" > /dev/null
fi
SDL=$(cd "$SDL" && pwd)                                  # (pulseman's make runs in its own directory)
SDLCFLAGS="-I$SDL/SDL2.framework/Headers -F$SDL"
CFLAGS="-std=c11 -O2 -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes ${WERROR--Werror}"
B=build/mac
rm -rf "$B" ext/pulseman/build/mac-arm64 ext/pulseman/build/mac-x64
make -s build/ports_gen.inc build/build_id.h

# the game for each processor; without a version Zig's least macOS would be its newest
for a in arm64:aarch64 x64:x86_64; do
  N=mac-${a%%:*}
  T="-target ${a#*:}-macos.11.0"
  make -s -C ext/pulseman MODE=release B="build/$N" CC="$ZIG cc $T" CXX="$ZIG c++ $T" AR="$ZIG ar" \
    SDLCFLAGS="$SDLCFLAGS" "build/$N/libmdrt.a" "build/$N/libmdrt_sdl2.a"
  mkdir -p "$B/$N/obj"
  ls src/*.c native/*.c | xargs -P "$(nproc)" -I{} sh -c \
    "$ZIG cc $T $CFLAGS -Isrc -Ibuild -Inative -Iext/pulseman/include $SDLCFLAGS -c {} -o $B/$N/obj/\$(basename {} .c).o"
  $ZIG c++ $T -s -o "$B/$N/gunstar" "$B/$N"/obj/*.o "ext/pulseman/build/$N/libmdrt_sdl2.a" \
    "ext/pulseman/build/$N/libmdrt.a" -F"$SDL" -framework SDL2 -Wl,-rpath,@executable_path/../Frameworks -lm
done

# the app: one program for both, SDL2's framework inside (its links kept)
P=$B/pkg/gunstars
APP=$P/gunstars.app/Contents
mkdir -p "$APP/MacOS" "$APP/Frameworks"
"$LIPO" -create "$B/mac-arm64/gunstar" "$B/mac-x64/gunstar" -output "$APP/MacOS/gunstar"
cp -a "$SDL/SDL2.framework" "$APP/Frameworks/"
cat > "$APP/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleDisplayName</key>
	<string>gunstars</string>
	<key>CFBundleExecutable</key>
	<string>gunstar</string>
	<key>CFBundleIdentifier</key>
	<string>io.github.1nputs.gunstars</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>gunstars</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>$VERSION</string>
	<key>CFBundleVersion</key>
	<string>$VERSION</string>
	<key>LSApplicationCategoryType</key>
	<string>public.app-category.action-games</string>
	<key>LSMinimumSystemVersion</key>
	<string>11.0</string>
	<key>NSHighResolutionCapable</key>
	<true/>
</dict>
</plist>
EOF
cp LICENSE "$P/LICENSE.txt"
cat > "$P/README.txt" <<EOF
gunstars $VERSION: Gunstar Heroes, rebuilt in C (https://github.com/1nputs/gunstars).

Open gunstars.app. The first time it asks for your ROM: Gunstar Heroes (U), the 1 MB retail
cartridge; pick it in the file browser.

The first time, macOS will not open it with a double-click: no registered Apple developer signed
it. Control-click (or right-click) gunstars.app, choose Open, then Open again. On macOS 15 and
newer, try to open it once, then in System Settings, Privacy & Security, choose Open Anyway.
(Or in Terminal: xattr -dr com.apple.quarantine gunstars.app)

Keys: pad 1 arrows, Z / X / C = A / B / C, Enter = Start (A weapon, B shoot, C jump);
pad 2 I J K L, Q / W / E, Tab. Esc opens the menu (save slots, video, sound, controls).
F4 held rewinds, F5 pauses, F6 held fast-forwards, F10 saves the last 20 seconds as a GIF.
Hold Fn for the function keys (or have them act as standard function keys in System Settings,
Keyboard).

Online: Esc, Online play. Type your name, then host (the other joins your address; your
router must let UDP port 7000 in, unless you share a LAN or a VPN), join a host, or both
connect (each types the other's address). Both need this same version. Allow incoming
connections when macOS asks.

Opened from the Finder, it keeps its settings, saves and screenshots in
~/Library/Application Support/gunstars (in the Finder: Go, Go to Folder, and paste that).
Run from Terminal (gunstars.app/Contents/MacOS/gunstar, --rom "/path/to/the.gen" for the ROM),
in the folder you run it from.

macOS 11 (Big Sur) or newer, on Apple Silicon or Intel: one app, with SDL2 inside it (SDL's,
zlib licence). Built on Linux with Zig's Clang and not yet run on a Mac: tell us how it runs.
The game's code is MIT (LICENSE.txt); Gunstar Heroes itself belongs to Sega and Treasure.
EOF
(cd "$B/pkg" && rm -f ../../gunstars-macos-universal.zip && zip -qrXy ../../gunstars-macos-universal.zip gunstars)
echo "$P/gunstars.app, build/gunstars-macos-universal.zip"

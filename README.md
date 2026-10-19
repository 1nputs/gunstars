# gunstars 💥

*Gunstar Heroes, rebuilt in C.*

**Treasure's 1993 run-and-gun masterpiece, running natively on your machine, with no emulator in
sight.** Every one of the cartridge's roughly 4,400 routines, the 68000's and the Z80's, has been
rewritten in C and checked against the original call by call, wherever play reaches them, until
the two can't be told apart. The same four weapons and all ten of their combinations, the same Seven Force, the same
dice palace, the same drum samples crackling through the YM2612. Two players on one couch, at a
smooth 60 frames a second, without the cartridge's slowdown.

And things the cartridge never had: **save anywhere in a stage and load it back (eight slots in
the menu), and hold F4 to rewind** the hit you just took. Hold F6 to fast-forward, slow a boss
fight down, step through it a frame at a time. Closed the window mid-stage? **The next start puts
you back where you were.** Pulled off something beautiful? **F10 saves the last 20 seconds** as a
GIF to share. Next up: Red and Blue, online, together.

Bring your own cartridge dump. **Download** the release for Linux or Windows
([CHANGELOG.md](CHANGELOG.md) says what's in it), unpack it and run `gunstar` (`gunstar.exe`), or
build it and you're shooting in a minute (what to install first is under [Build](#build)):

```
git clone --recursive https://github.com/1nputs/gunstars.git && cd gunstars
make gunstar
./build/gunstar       # the first time, it asks for your ROM: Gunstar Heroes (U), the 1 MB cartridge
```

The first time, pick your ROM in the game's file browser (or give it with `--rom FILE`); it is
remembered after that.

| | Player 1 | Player 2 |
|---|---|---|
| D-pad | arrows | I J K L |
| A / B / C | Z / X / C | Q / W / E |
| Start | Enter | Tab |

As the cartridge sets them, A changes weapon, B shoots and C jumps (the game's own Options screen
moves them). Game controllers work too (the first two are players 1 and 2).

| Key | |
|---|---|
| F1 | mute |
| F2 / F3 | volume down / up |
| F4 (held) | rewind |
| F5 | pause |
| F6 (held) | fast-forward |
| F7 | slow motion on / off |
| F8 | a frame at a time, while paused |
| F10 | save the replay: the last 20 seconds, as a GIF (in `videos/`) |
| F11 | start / stop recording a GIF |
| F12 | screenshot (a PNG in `screenshots/`) |
| Esc | the menu: save and load states there |
| Alt+Enter | full screen |

**The menu** (Esc, or a controller's Back button; arrows and Enter, or the d-pad, A and B) has the
ROM, a reset, states, the video (a pixel-art upscaler: Scale2x, Scale3x, MMPX or xBR; sharp,
integer or fit scaling; square pixels or 4:3; the colours' levels; dithering blended as a
television blended it; a picture tube's scanlines and mask; full screen), captures, the sound
(volume, and the FM sound core and chip: hear the difference as you play), the players'
controllers, buttons and turbo, and every key: pick one and press the new key.

**Save slots and quick resume:** the menu's Save and Load pages have eight slots, each with a
picture of the moment, in `states/` beside the game. A state is taken between frames once the
game's work for the frame is done; while a stage's graphics are being unpacked (a second or so
at a stage's start) a save waits for that. Quit in the middle of a stage and the next start puts
you back where you were (an option in the menu).

All of it is kept in `settings.ini`, in the folder you run the game from, beside its saves (you
can also edit it while the game is closed). The game pauses when its window loses focus (an
option in the menu). On a Mac, hold Fn for the function keys.

## Online co-op

Two players, each at their own machine, play the whole game together from power on, with
rollback: your own player answers your keys at once, and the other's buttons are guessed until
they arrive; when a guess was wrong the game puts the last few frames right before showing the
next, so you hardly ever see it. Each of you plays with your own pad 1 controls (keys or
controller). By IP address, no server.

In the menu (Esc), **Online play**:

- **Your name**: what the other player sees when you join ("Sam has joined", on the bottom of
  the picture) and when you leave.
- **Play as**: *host* (you are player 1; the other joins you), *join a host* (you are player 2),
  or *both connect* (each of you types the other's address and starts: most home routers need
  no setup then, and the two games decide who is player 1).
- **Address** (to join, or both connect): the other's public address (or a LAN or VPN one). Type
  it in (Ctrl+V pastes).
- **Port**: 7000 unless you both change it. To host, your router must let this UDP port in to
  you (a forwarded port), unless you share a network or a VPN such as Tailscale or ZeroTier.
  The page shows your LAN address (for a friend on the same network, or the address to forward
  the port to).
- **Input delay**: how many frames late a button press reaches the game (the host's choice, 2
  by default): more is fewer corrections, less is snappier.
- **Start**: the game starts again, waits for the other player (Esc gives up), and you play.

What you chose is kept for next time. The window's title shows the round trip, the delay and the
last correction. While you play together, pause, slow motion, fast-forward, rewind, the save slots
and quick resume are off (the game's own pause, Start, pauses both), and the menu does not stop
the game; **Leave the session** is on the Online play page. If either leaves or the connection
drops, the other's game goes on alone. Both of you need the same build (the game says so if not).

From a terminal it is the same: `./gunstar --host [PORT]`, `./gunstar --join ADDRESS[:PORT]`, or
`./gunstar --peer ADDRESS` on both sides, with `--delay N` and `--name NAME`.

## Build

The hardware side is [pulseman](https://github.com/1nputs/pulseman), our Mega Drive runtime,
included as a git submodule in `ext/pulseman`: clone with `--recursive`, or after a plain clone
run `git submodule update --init`. `make gunstar` builds it too. Besides that you need a C and
C++ compiler, `make`, `pkg-config` and SDL2.

**Linux** (built and tested here, Debian 13, GCC 14):

```
sudo apt install build-essential pkg-config libsdl2-dev          # Debian, Ubuntu
sudo dnf install gcc gcc-c++ make pkgconf-pkg-config SDL2-devel  # Fedora
sudo pacman -S --needed base-devel sdl2                          # Arch
make gunstar
./build/gunstar --rom "/path/to/Gunstar Heroes (U) [!].gen"
```

**macOS** (not tried yet):

```
brew install sdl2 pkg-config
make gunstar
./build/gunstar --rom "/path/to/Gunstar Heroes (U) [!].gen"
```

**Windows**: cross-built here from Linux, warning-free, with [Zig](https://ziglang.org)'s Clang
(`pip install ziglang` will do) and SDL2's own MinGW package, which the script fetches:

```
make windows        # build/win/gunstar.exe with SDL2.dll, and build/gunstars-windows-x64.zip
```

Unzip it anywhere on Windows 10 or later and run `gunstar.exe` (it needs only `SDL2.dll` beside
it). Not yet run on Windows itself: tell us how it goes. Or build on Windows with
[MSYS2](https://www.msys2.org), in its *UCRT64* shell (not tried yet):

```
pacman -S --needed git make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2
make CC=gcc CXX=g++ gunstar
./build/gunstar.exe --rom "C:/path/to/Gunstar Heroes (U) [!].gen"
```

If a compiler newer than these warns about something, `make WERROR= gunstar` builds anyway;
please tell us what it said.

**Headless** (no window: for tests, tools and agents): `./build/gunstar --headless --frames N`
with `--input FILE` (both pads from a script: `FRAME:BUTTONS|BUTTONS` lines, as
`tools/oracle/script.py` writes them), `--shot FILE.ppm`, `--shots DIR --every N`, `--sound
FILE.sndtrace` (everything the sound chips were told, and what they played), `--save-state
FRAME:FILE`, `--load-state FILE`, `--requests FILE --state FILE` (save, load and rewind as
the menu and F4 make them), `--golden FILE` (a line of hashes a frame: `make golden`), `--fm-core
nuked|ymfm`, RAM words held or written (`--hold ADDR=VAL`, `--hold-if WADDR=WVAL:ADDR=VAL`,
`--poke FRAME:ADDR=VAL`), and netplay (`--host`, `--join`, `--peer`, `--port`, `--delay`, and
`--net-sim MS,MS,%,%` for a simulated network: each player's buttons their column of the script).

## For agents: where this stands (19 October 2026)

- **What it is.** A C reimplementation of Gunstar Heroes (Genesis, US retail, SHA-1
  `7721e302555f16e3242ad4a47ed87a1de1882122`). No labelled disassembly existed, so this project
  made its own: `tools/disasm` rebuilds the ROM byte for byte from a listing it writes from the
  recorded runs' evidence, and `tools/disasm/names.txt` names the routines as they are ported.
  `src/` holds the port (62 files, about 80,000 lines): the engine, the players and their
  weapons, every stage, every object type, the modes, the 68k sound driver, and the Z80's sample
  player (`src/z80.c`, its 176 instructions). It runs on **pulseman**
  ([1nputs/pulseman](https://github.com/1nputs/pulseman), MIT, the git submodule `ext/pulseman`;
  its C API keeps the `mdrt_` prefix): the VDP, PSG, YM2612 and pads as C modules, with an SDL2
  platform. There is no 68k or Z80 emulation.
- **How it is verified.** Genesis Plus GX with a hook (the oracle, a development tool only)
  records real play: every call into a routine with the registers, RAM and port accesses around
  it. `make test` replays 39,405 recorded calls from 86 capture sessions (the attract mode, two
  players through the stages, held vitality runs through stages 4 to 6, the options screen, each
  player's weapons held at every pair); each must write the same RAM and touch the hardware in
  the same order with the same values. `make z80test` replays the oracle's sound traces: the
  sample player's 1,167,306 writes to the YM2612 must come at the same master clock. `make
  sweep` plays the native game through every stage headless (15 runs, 1.29 million frames, both
  players' vitality held): none may stop. `make golden-check` plays 12 golden runs (238,100
  frames: the attract loop, the options, the TAS's two-player game, every stage, a stage
  cleared, a continue) and compares every frame's hashes (work RAM, the VDP, the Z80's RAM, the
  picture, the sound) with those `make golden` recorded from an earlier build, bit for bit; a
  state saved in four of them and loaded in a new process, and the menu's save, load and rewind,
  must go on frame for frame as the run did. `make golden-cross` plays them with the Linux
  release's build (Zig's Clang) and `make golden-ubsan` with a build that stops at any undefined
  behaviour: both bit for bit as GCC's, which is what lets two players' builds agree. `make
  netplay-check` plays three netplay sessions, two headless processes each over the loopback
  through a simulated network (delay, jitter, loss, duplicates), and every frame either finally
  played must be a solo run's with the same buttons.
- **Done.** The harness and the disassembler; every routine of the cartridge; the native game on
  pulseman (sound, the H-interrupt effects, the samples), save states, quick resume and rewind;
  rollback netplay (pulseman's `mdrt/netplay.h` and `mdrt/net_udp.h`; the game's side in
  `native/net.c`: its resumable point MainLoop's wait, a rollback going on from there as a loaded
  state does).
- **Not done yet.** Lockstep (the native game against the oracle at every vertical blank, as
  ninety-four's `make lockstep`); the native game has no lag frames, so its runs drift from the
  oracle's after the first. Records for the parts no recorded run reaches: the
  bubble under water, a few object types, stage 1's maze after the mine cart, stage 5's
  corridor, most of stage 6.
- **Read first.** `ext/pulseman/README.md` and `docs/API.md` there; the comments at the top of
  `tools/hybrid/oracle_shim.c`, `tools/hybrid/capture_all.sh`, `tools/disasm/build.py` and
  `test/captest.c`. The working notes (the handoff guide, the plan, the survey, the playbook for
  the method) are kept outside the repository.
- **Releases.** `tools/package_release.sh` makes `build/release/`: the Linux tarball (Zig's
  Clang against glibc 2.28, so it runs on 2018-era distributions, with the C++ runtime linked
  in), the Windows zip, and `SHA256SUMS`; the version is `GUNSTAR_VERSION` in `native/main.c`,
  and `CHANGELOG.md` says what each one holds.
- **Rules.** The ROM and everything made from it (the disassembly, captures, traces, states,
  screenshots, GIFs) stay out of git. Commit after each finished job. Before and after a change:
  `make test`, `make z80test` for the sound, `make golden-check` and `make sweep` for the native
  game (`make golden` first, on the build before the change: `tools/golden.py record --force`
  records again), `make netplay-check` for netplay or pulseman, `make golden-cross` now and then.
  A routine is ported against its records: name it
  in `tools/disasm/names.txt`, give its prototype a `/* $ADDRESS registers */` comment in
  `src/game.h`, `make disasm && make gen && make`, capture a session that reaches it, test.
  pulseman is changed only in `ext/pulseman` on its branch `gunstar`, and merged into its `main`
  from there.

```
./tools/fetch_refs.sh                                # references: the oracle core (patched), vasm, the TAS
cp /path/to/rom "Gunstar Heroes (U) [!].gen"         # your ROM at the repo root
make oracle && make evidence && make gen             # the oracle; the evidence runs; the disassembly
tools/hybrid/capture_all.sh                          # every capture session (about two hours)
make test && make z80test                            # the checks (z80test: tools/oracle/sndtrace.py into out/snd)
make golden && make golden-check                     # the native game's golden runs, recorded, then checked
make netplay-check                                   # two players over a simulated network, against solo runs
```

## Features

From the least ambitious to the most.

- [x] The whole cartridge ported: every stage, boss, weapon, mode and screen, from power on
- [x] FM and PSG music and effects, the Z80's drum and voice samples ported to C
- [x] Keyboard and two game controllers, two players on one machine
- [x] Window scaling, full screen, 4:3 aspect, hi-DPI
- [x] Smooth 60 frames a second (no cartridge slowdown)
- [x] Save states (the menu's eight slots, with pictures) and rewind (F4)
- [x] Quick resume on quit and start
- [x] FM core and chip model choice (Nuked-OPN2 or ymfm; YM2612 or YM3438), changed as you play
- [x] Headless runs: screenshots, sound traces, scripted input, states
- [x] Mute, volume, audio device and latency options
- [x] A settings file, a menu, every key rebindable, controller remapping, turbo, dead zones
- [x] Pixel-art upscalers: Scale2x, Scale3x, MMPX, xBR
- [x] Colour options; scanlines, CRT mask, composite dither blending
- [x] Screenshots, the replay (the last 20 seconds) and recordings as GIFs
- [x] Fast-forward, slow motion, frame step, pause when the window loses focus
- [x] Golden runs: every frame of 12 runs against a recorded build, bit for bit, states too
- [ ] Lockstep against the original at every frame
- [ ] Every corner recorded and tested (stage 5's corridor, stage 6, stage 1's maze)
- [x] A Windows build (`make windows`, cross-built with Zig)
- [ ] Played on Windows; built and played on macOS
- [ ] The cartridge's slowdown as an option
- [ ] Skip the logos, boot straight into a game
- [ ] Game Genie and Action Replay codes
- [ ] Input recording and replay files
- [ ] Save states anywhere, a stage's loading included
- [ ] Debug overlay: VRAM, palettes, sprites, objects, a RAM watch
- [ ] Run-ahead for lower input latency
- [ ] Play in the browser (WebAssembly)
- [ ] Widescreen
- [ ] Lua scripting: overlays, practice tools, bots
- [x] **Rollback netplay: two players online**, by IP address (not yet played between two real machines)

## Greets

Big, big greets to everyone whose work this stands on:

- **Treasure**, who made Gunstar Heroes in 1993, and **Sega**, who published it. Still the best
  run-and-gun ever made.
- **Samsara's two-player run on [TASVideos](https://tasvideos.org)** (`3274M`): its menu inputs
  start most of the recorded sessions.
- **OrangyTang's notes** on the [SpritesMind](https://gendev.spritesmind.net) forum: the first map
  of the game's H-interrupt buffer, its object table and its compression.
- **[Genesis Plus GX](https://github.com/ekeeke/Genesis-Plus-GX)** by Eke-Eke (after Charles
  MacDonald's Genesis Plus): the oracle. Every routine here was checked against it. A development
  tool only, never shipped.
- **[Nuked-OPN2](https://github.com/nukeykt/Nuked-OPN2)** by nukeykt and
  **[ymfm](https://github.com/aaronsgiles/ymfm)** by Aaron Giles: the YM2612 cores that make the
  music.
- **[vasm](http://sun.hasenbraten.de/vasm/)** by Volker Barthelmann and Frank Wille: the assembler
  that rebuilds the ROM byte for byte from the listing.
- **[SDL](https://www.libsdl.org)**: the window, the sound and the controllers.
- **MMPX** by Morgan McGuire and Mara Gagiu, **xBR** by Hyllian, and **Scale2x / Scale3x** by
  Andrea Mazzoleni after Eric Johnston's EPX: the upscalers in the Video menu.
- **[libretro](https://www.libretro.com)**: the API the oracle is hosted through.
- The documenters of the hardware: Charles MacDonald (`genvdp.txt`),
  [Plutiedev](https://plutiedev.com), [Sega Retro](https://segaretro.org),
  [SMS Power](https://www.smspower.org) (the SN76489) and the
  [SpritesMind](https://gendev.spritesmind.net) forum (Nemesis's YM2612 and VDP research).
- **[ninety-four](https://github.com/1nputs/ninety-four)**, NHL '94 rebuilt the same way: its
  tools, its runtime work and its method came first.

## License

MIT (`LICENSE`), for the code written for this project. It grants nothing over Gunstar Heroes
itself, its name or its data, which belong to Sega and Treasure: bring your own copy of the game.
pulseman is MIT too; its FM chip cores are Nuked-OPN2 (LGPL-2.1) and ymfm (BSD-3), and its MMPX
and xBR upscalers follow their authors' MIT-licensed code.

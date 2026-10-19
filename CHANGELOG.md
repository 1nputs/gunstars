# Changelog

## 0.2.0 (19 October 2026): online co-op

Gunstar Heroes, Treasure's 1993 run-and-gun, rebuilt in C on
[pulseman](https://github.com/1nputs/pulseman). Bring your own ROM: Gunstar Heroes (U), the 1 MB
retail cartridge. This release takes the two-player game online: Red and Blue at two machines,
with rollback netplay.

**Downloads**

| | |
|---|---|
| `gunstars-0.2.0-linux-x64.tar.gz` | x86-64 Linux from about 2018 on (glibc 2.28); needs SDL2 (`libsdl2-2.0-0`, `SDL2`, `sdl2`) |
| `gunstars-0.2.0-windows-x64.zip` | Windows 10 or later, x86-64; `gunstar.exe` with `SDL2.dll` beside it. Cross-built: tell us how it runs |
| `SHA256SUMS` | the files' checksums |

Or build it: `git clone --recursive`, `make gunstar` (see the README).

**Online co-op** (new)

- Two players, each at their own machine, play the whole game together from power on. Rollback
  netplay: your own player answers your keys at once; the other's buttons are guessed until they
  arrive, and when a guess was wrong the game plays the last few frames again before showing the
  next, so you hardly ever see it. The two games compare themselves every second.
- From the menu: Esc, **Online play**. Your name; play as host (player 1), join a host (player 2),
  or both connect; the other's address and the port; the input delay; Start. Your choices are
  kept, and the page shows your LAN address. Or from a terminal: `--host`, `--join ADDRESS`,
  `--peer ADDRESS`, `--delay N`, `--name NAME`.
- By IP address, no server: the host lets UDP port 7000 in (a forwarded port), or you share a LAN
  or a VPN (Tailscale, ZeroTier); or you both connect at once, which gets through most home
  routers with no setup.
- "NAME has joined" and "NAME left" along the bottom of the picture; the round trip, the input
  delay and the last correction in the window's title.
- While you play together the game's own pause (Start) pauses both; the window's pause, slow
  motion, fast-forward, rewind, save slots and quick resume are off, and the menu does not stop
  the game. If one of you leaves or the connection drops, the other's game goes on alone. Both
  need this version: the game says so if not.

**The game**

- Every routine of the cartridge in C: the 68000's (about 4,400: the engine, both players and the
  four weapons with all ten of their combinations, every stage and boss, every object type, the
  title, the story, the options, the ending, the 68k's sound driver) and the Z80's drum and voice
  sample player. No emulator inside.
- Two players, on one machine or now on two; the keyboard or controllers.
- Smooth 60 frames a second, without the cartridge's slowdown: the logos and the intro run at
  full speed (the cartridge ran them at about 65-80%), and the stages lose the 2-3% of frames the
  cartridge lagged.

**Playing**

- Save states in eight slots, each with a picture; quick resume (quit mid-stage, start again
  there); rewind (F4 held); fast-forward (F6), slow motion (F7), pause and frame step (F5, F8).
- Vsync (on by default); pixel-art upscalers (Scale2x, Scale3x, MMPX, xBR); sharp, integer or fit
  scaling; 4:3; colour levels, dither blending, scanlines and CRT masks; full screen; a frames a
  second counter (new, in Options).
- The FM sound core (Nuked-OPN2 or ymfm) and chip (YM2612 or YM3438), changed as you play.
- The last 20 seconds saved as a GIF (F10), recordings (F11), PNG screenshots (F12).
- Every key and controller button can be changed; turbo A, B, C; the stick's dead zone.
- `gunstar --version`; headless runs for tools and tests (`--headless`, see the README).

**How it was checked**

- 39,405 calls recorded from the original running in an emulator (Genesis Plus GX) replay
  exactly: the same memory written, the same hardware touched in the same order.
- The Z80 sample player's 1,167,306 writes to the sound chip come at the emulator's exact master
  clock.
- The game plays every stage headless (15 runs, 1.29 million frames, a continue among them)
  without a stop.
- Golden runs (new): 12 runs and 238,100 frames of the native game (the attract loop, the options,
  the TAS's two-player game, every stage, a stage cleared, a continue), every frame's memory,
  video chip, Z80, picture and sound compared bit for bit from build to build; a saved state
  loaded in a new process goes on the same. The Linux download's build (Clang) and a build that
  stops at any undefined behaviour play every one of them exactly as the GCC build does, which is
  what lets two players' builds agree.
- Online play: three sessions of two players through simulated networks (15-50 ms each way with
  jitter, 2-10% of packets lost, input delay 0 to 2, host and join and both connecting) play every
  frame exactly as one machine with the same buttons; two windows on one machine played and
  parted; the Online play page played by its keys in pulseman's tests.

**Fixed since 0.1.0**

- Continuing after a game over: the source tagged 0.1.0 stopped there ("routine $00ED90 is not
  ported yet"; its downloads had the fix).
- 0.1.0's save states still load.

**Known**

- The US retail ROM only (SHA-1 `7721e302555f16e3242ad4a47ed87a1de1882122`); the game's own
  checksum stops a patched ROM.
- No cartridge slowdown, on purpose (a faithful-pacing option later).
- Parts no recorded play reached are ported but not yet checked against recordings: the bubble
  under water, a few object types, stage 1's maze after the mine cart, stage 5's corridor, much of
  stage 6. Tell us if anything there looks wrong.
- Not yet checked frame by frame against the original over whole runs.
- Online play is new: tested over simulated networks and between two windows on one machine, not
  yet between two homes. Tell us how it goes.
- macOS: not tried yet. Windows: cross-built, tell us how it runs.

**Next**

- Lockstep: the whole game frame by frame against the original.
- Online: picking a session up again after a dropped connection; starting one from a save state.

## 0.1.0 (18 October 2026): the first release

Gunstar Heroes, Treasure's 1993 run-and-gun, rebuilt in C on
[pulseman](https://github.com/1nputs/pulseman). Bring your own ROM: Gunstar Heroes (U), the 1 MB
retail cartridge.

**Downloads**

| | |
|---|---|
| `gunstars-0.1.0-linux-x64.tar.gz` | x86-64 Linux from about 2018 on (glibc 2.28); needs SDL2 (`libsdl2-2.0-0`, `SDL2`, `sdl2`) |
| `gunstars-0.1.0-windows-x64.zip` | Windows 10 or later, x86-64; `gunstar.exe` with `SDL2.dll` beside it. Cross-built: tell us how it runs |
| `SHA256SUMS` | the files' checksums |

Or build it: `git clone --recursive`, `make gunstar` (see the README).

**The game**

- Every routine of the cartridge in C: the 68000's (about 4,400: the engine, both players and the
  four weapons with all ten of their combinations, every stage and boss, every object type, the
  title, the story, the options, the ending, the 68k's sound driver) and the Z80's drum and voice
  sample player. No emulator inside.
- Two players on one machine, on the keyboard or controllers.
- Smooth 60 frames a second, without the cartridge's slowdown: the logos and the intro run at
  full speed (the cartridge ran them at about 65-80%), and the stages lose the 2-3% of frames the
  cartridge lagged.

**Playing**

- Save states in eight slots, each with a picture; quick resume (quit mid-stage, start again
  there); rewind (F4 held); fast-forward (F6), slow motion (F7), pause and frame step (F5, F8).
- Vsync (on by default); pixel-art upscalers (Scale2x, Scale3x, MMPX, xBR); sharp, integer or fit
  scaling; 4:3; colour levels, dither blending, scanlines and CRT masks; full screen.
- The FM sound core (Nuked-OPN2 or ymfm) and chip (YM2612 or YM3438), changed as you play.
- The last 20 seconds saved as a GIF (F10), recordings (F11), PNG screenshots (F12).
- Every key and controller button can be changed; turbo A, B, C; the stick's dead zone.
- `gunstar --version`; headless runs for tools and tests (`--headless`, see the README).

**How it was checked**

- 39,405 calls recorded from the original running in an emulator (Genesis Plus GX) replay
  exactly: the same memory written, the same hardware touched in the same order.
- The Z80 sample player's 1,167,306 writes to the sound chip come at the emulator's exact master
  clock.
- The game plays every stage headless (15 runs, 1.29 million frames, a continue among them)
  without a stop, and a saved state goes on byte for byte as the run it was taken from.

**Known**

- The US retail ROM only (SHA-1 `7721e302555f16e3242ad4a47ed87a1de1882122`); the game's own
  checksum stops a patched ROM.
- No cartridge slowdown, on purpose (a faithful-pacing option later).
- Parts no recorded play reached are ported but not yet checked against recordings: the bubble
  under water, a few object types, stage 1's maze after the mine cart, stage 5's corridor, much of
  stage 6. Tell us if anything there looks wrong.
- Not yet checked frame by frame against the original over whole runs (that is next).
- macOS: not tried yet. Windows: built, not yet run.

**Next**

- Lockstep: the whole game frame by frame against the original.
- Online co-op: rollback netplay for two players.

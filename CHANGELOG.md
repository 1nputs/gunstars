# Changelog

## Unreleased

**Online co-op.** Two players, each at their own machine, play the whole game together with
rollback netplay: your own player answers at once, the other's buttons are guessed until they
arrive, and a wrong guess is put right within the frame. By IP address, from the menu's Online
play page (host, join a host, or both connect; the address and port typed in; the input delay,
2 frames by default) or the command line (`--host`, `--join ADDRESS`, `--peer ADDRESS`,
`--delay N`). The host's router lets UDP port 7000 in, or you share a network or a VPN; or both
connect at once, which gets through most home routers with no forwarding. The window's title shows the
round trip and the corrections; pause, slow motion, fast-forward, rewind and the save slots are
off while you play together, and if one leaves the other plays on alone.

- Checked: three sessions over a simulated network (delay, jitter, 2-10% of packets lost) play
  every frame as a solo run with the same buttons; the release's Clang build and a build with the
  undefined behaviour sanitizer play every golden run bit for bit as the GCC build; a save state
  from 0.1.0 loads and plays on the same.
- Not yet played between two real machines.

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

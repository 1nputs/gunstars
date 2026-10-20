# Changelog

## 0.2.2 (20 October 2026): a Mac app, states online

Gunstar Heroes, Treasure's 1993 run-and-gun, rebuilt in C on
[pulseman](https://github.com/1nputs/pulseman). Bring your own ROM: Gunstar Heroes (U), the 1 MB
retail cartridge. This release brings a macOS app, save states and the host's reset to online play,
and the Autosave; everything else is as in 0.2.0, whose notes go over the whole game.

**Downloads**

| | |
|---|---|
| `gunstars-0.2.2-linux-x64.tar.gz` | x86-64 Linux from about 2018 on (glibc 2.28); needs SDL2 (`libsdl2-2.0-0`, `SDL2`, `sdl2`) |
| `gunstars-0.2.2-windows-x64.zip` | Windows 10 or later, x86-64; `gunstar.exe` with `SDL2.dll` beside it. Cross-built: tell us how it runs |
| `gunstars-0.2.2-macos-universal.zip` (new) | macOS 11 or later, Apple Silicon and Intel in one app, SDL2 inside it. Cross-built: tell us how it runs |
| `SHA256SUMS` | the files' checksums |

Or build it: `git clone --recursive`, `make gunstar` (see the README).

**New**

- **macOS**: `gunstars.app`, one app for Apple Silicon and Intel. No registered Apple developer
  signed it, so the first time, Control-click it and choose Open (on macOS 15 and later: try to open
  it once, then Open Anyway in System Settings, Privacy & Security). Opened from the Finder it
  keeps its settings, saves and screenshots in `~/Library/Application Support/gunstars`. The first
  time you join a game, allow it to find devices on your local network: without that a Mac can
  host but not join. Built on Linux, not yet run on a Mac.
- **Save states online**: either player's Save state keeps the newest moment both games have
  played alike, and closing the game keeps one as the Autosave. The host's Load state sends a
  state to the other game; both load it at the same frame and play on together from there (only
  the host loads).
- **The host's Reset online** starts the game again from power on on both machines, at the same
  frame.
- **Joining** says "reaching the host" until the host answers, with the system's reason when
  nothing can be sent and, after a few seconds, what to check; then "connected to" the host's
  name, on the picture too.
- **The Autosave**, in place of quick resume: closing the game keeps the moment in
  `states/autosave.state`, and the Load state page's first line takes you back. The next start is
  always a fresh one.
- Shorter labels in the menu: Save state, Load state, FM core, FPS counter; on the Online play page
  Name, Role, Host address, Port, Host game or Connect, LAN address, Leave session.

**Changed**

- The window no longer pauses when it loses focus (Esc still pauses), and the option is gone.

**Fixed**

- Hosting again on the same port after leaving a session said "Address already in use"; it hosts.

**From 0.2.1**

- Both players need 0.2.2 to play online (the game says "another version of netplay" if not).
- Save slots and settings carry over. 0.2.1's quick resume (`states/resume.state`) is not
  loaded any more: rename it `autosave.state` to keep it, then load it from the Load state page.

**How it was checked**

- Again on this build: the 39,405 recorded calls, the Z80 sample player, every stage headless (15
  runs, 1.29 million frames), the 12 golden runs (238,100 frames) and their saved states, all
  exactly as 0.2.1, built by GCC and by Clang and with the undefined-behaviour checker.
- The host's Reset: a reset at frame 1500 plays on exactly as a fresh power on (2,489 frames, the
  whole machine compared). Online, a session where the host resets at frame 3000 plays all 6,000
  frames on both machines exactly as one machine with the same buttons; so do resets placed just
  after the guest's buttons change, which the host has to roll back across (one host powered on
  again three times in a session).
- States online: the host's state loaded for both mid-session; both machines load it at the same
  frame and play all 5,000 frames as one machine that loads it there. A state saved in the session,
  loaded alone afterwards, plays on as the session did (1,001 frames).
- Ten harsher sessions (input delay 0 and 1, up to 60 ms each way with 50 of jitter, 15% of packets
  lost, the TAS's game, a continue, stages 3 and 6, two with resets and two with loads) play every
  frame exactly as one machine; two windows on one machine reset and loaded a state together, 24
  desync checks agreeing, and parted.
- pulseman's own tests, and its window's: the Autosave, a state saved and loaded in a session, the
  host's Reset, the menu. Their frame-rate checks fail on this machine while it is loaded, as they
  do for 0.2.1's pulseman.
- The downloads: the Linux one plays the golden runs exactly, loads a 0.2.0 state, and played a
  session (a reset in it) joined to a GCC build of the same commit; 0.2.1 and 0.2.2 refuse each
  other, either one hosting. The Mac app was checked inside (both architectures, signed for
  running, SDL2 in it) but not run on a Mac.

## 0.2.1 (19 October 2026): an online fix

Gunstar Heroes, Treasure's 1993 run-and-gun, rebuilt in C on
[pulseman](https://github.com/1nputs/pulseman). Bring your own ROM: Gunstar Heroes (U), the 1 MB
retail cartridge. This release fixes a way an online session could end early; everything else is
as in 0.2.0, whose notes go over the whole game.

**Downloads**

| | |
|---|---|
| `gunstars-0.2.1-linux-x64.tar.gz` | x86-64 Linux from about 2018 on (glibc 2.28); needs SDL2 (`libsdl2-2.0-0`, `SDL2`, `sdl2`) |
| `gunstars-0.2.1-windows-x64.zip` | Windows 10 or later, x86-64; `gunstar.exe` with `SDL2.dll` beside it. Cross-built: tell us how it runs |
| `SHA256SUMS` | the files' checksums |

Or build it: `git clone --recursive`, `make gunstar` (see the README).

**Fixed**

- Online: a session could end as "desync at frame N". When the game corrects a wrong guess of
  the other player's buttons it goes back to a state it saved and plays the frames again; it could
  keep a state saved while the guess played, and when the right buttons gave the game no moment to
  save at on that frame, a later correction could load the guessed state, and the two games no
  longer agreed. A correction now forgets every state after the frame it goes back to. Found in
  the four-player sessions of [ninety-four](https://github.com/1nputs/ninety-four) (NHL '94,
  rebuilt on pulseman too); gunstar's own sessions never hit it, but nothing in the game ruled it
  out.
- The v0.2.0 tag went on the commit before that release (`acf7468`; the downloads were built from
  `b66ed55`): a build of it calls itself 0.1.0 and cannot play online with the 0.2.0 downloads.
  The v0.2.1 tag names the commit its downloads were built from.

**Online with 0.2.0**

- Both players need 0.2.1: it does not play online with 0.2.0 (the game says so: "another version
  of netplay"). Save states, quick resume and settings carry over.
- Under it is pulseman's newest netplay, which its four-player games use for up to four machines;
  gunstar stays two players, one at each machine, as in 0.2.0.

**How it was checked**

- Again on this build: the 39,405 recorded calls, the Z80 sample player, every stage headless (15
  runs, 1.29 million frames), the 12 golden runs (238,100 frames) and their saved states, all
  exactly as 0.2.0.
- Online: pulseman's new test, a game whose moments to save at depend on how it was played, ended
  as a desync without the fix (64 and 126 of 900 frames right) and plays every frame right with
  it. gunstar's three sessions through simulated networks, and six harsher ones (input delay 0 and
  1, up to 60 ms each way with 50 of jitter, 15% of packets lost, the TAS's game, a continue, stages
  3 and 6), play every frame exactly as one machine with the same buttons; two windows on one
  machine played through a lossy network and parted; 0.2.0 and 0.2.1 refuse each other, either one
  hosting.

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

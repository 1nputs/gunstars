# gunstar

Gunstar Heroes (Sega Genesis, the US retail ROM) reimplemented in C11 on
[pulseman](https://github.com/1nputs/pulseman), verified against an emulator frame by frame, the
way [ninety-four](https://github.com/1nputs/ninety-four) did NHL '94. The method is
`../playbook/PLAYBOOK.md`; this game's plan, survey and living guide are in `../playbook/gunstar/`.

pulseman is the git submodule `ext/pulseman`, on the branch `gunstar`: runtime changes this game
needs are made there and merged into pulseman's `main` by its owner (`git -C ext/pulseman log
main..gunstar` lists them; the remote `local` is the checkout at `../pulseman`).

The ROM is your own copy, `Gunstar Heroes (U) [!].gen` at the repository root (SHA-1
`7721e302555f16e3242ad4a47ed87a1de1882122`). It is never committed, and nothing derived from it
is either: `ref/`, `build/` and `out/` are git-ignored.

Status: every routine of the game is ported (the 68k's and the Z80's sample player), each tested
against the emulator's recorded calls (`make test`, `make z80test`).

## Playing

`make gunstar` builds `build/gunstar` (SDL2). It opens a window and plays the game from your ROM;
Esc opens pulseman's menu: the ROM, a reset, eight save slots (and quick resume: closing the
window keeps where you were, the next start goes on from there), video, sound, controllers and
keys. F4 held steps back through the last ten seconds. Pad 1 is the arrows with Z / X / C and
Enter, pad 2 I J K L with Q / W / E and Tab; controllers work as plugged in.

A state is taken at the game's main loop, once a frame's work is done; while a stage's graphics
are being unpacked a save waits for that (a second at most). `build/gunstar --headless --frames
N [--input FILE] [--shot FILE.ppm] [--save-state FRAME:FILE] [--load-state FILE]` runs without a
window (the input as `tools/oracle/script.py`'s scripts, by the native frame count).

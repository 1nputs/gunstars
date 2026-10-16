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

Status: set up; nothing ported yet.

#!/usr/bin/env bash
# The capture sessions (tools/hybrid/capture.py) as recipes: each writes out/captures/NAME.cap.
# The input scripts are evidence_all.sh's (it makes them; run it first on a clean checkout).
#   capture_all.sh               every session
#   capture_all.sh NAME...       those sessions
# Sessions, by the routines they record:
#   frame_*    the vertical blank's plumbing (FRAME below): pads, pause, the H-interrupt effects,
#              the VDP register and DMA updates, the job loop and its decompressors. attract: the
#              logos, title and demos; stage0: a two-player game of stage 0 (Start every 600
#              frames: pauses, continues); inv4-inv6: the later stages, vitality held
#   sound_*    the sound driver: the title and demos' music, stage music and effects
#   vblank_*   VBlank (each record a whole frame) and the reset's set-up routines
#   engine_*, game_*, logo_*, treasure_*: the engine's routines, the modes', the logos'
# A session with --novel keeps a record only when it runs an instruction or a branch direction
# no kept record of its routine ran (tools/hybrid/capcov.py reports what is still untested);
# --spread samples calls across the session for variety in the data.
set -euo pipefail
cd "$(dirname "$0")/../.."
ROM="Gunstar Heroes (U) [!].gen"
PY="python3"
CAP="$PY tools/hybrid/capture.py"
mkdir -p out/captures
INV="FFA424=03E7,FFA624=03E7,FFC9A4=0001"
# the routines under these, leaf first (tools/disasm/draft.py --tree)
FRAME_ROOTS=HintEffect,VdpWriteReg0,VdpWriteRegs,VBlankDma,ReadPads,CheckPause,Random,FadePalette,QueueHScroll,QueueVScroll,RunJobs,RunCommands
FRAME=$($PY tools/disasm/draft.py --tree "$FRAME_ROOTS" --names)
# the reset's set-up and the vertical blank itself (its records hold the whole frame)
BOOT=VdpInitRegs,IoInit,FrameInit,ClearDmaArea,ClearRamF000,ClearRamB800,ResetPads,SoundInit
# the sound driver ($00C88 calls it each frame)
SOUND=$($PY tools/disasm/draft.py --tree sub_00C88 --names)

# the routines tools/disasm/names.txt lists under a heading "# ---- ..." that matches
names() {
  $PY - "$1" <<'PYEOF'
import sys, re
want = {'the engine': ('clearing', 'sprites'), 'the game': ('modes',), 'the logo': ('the SEGA logo',),
        'the treasure': ('the Treasure logo',)}[sys.argv[1]]
out, on = [], False
for line in open('tools/disasm/names.txt'):
    if line.startswith('# ----'):
        on = any(w in line for w in want)
        continue
    w = line.split('#', 1)[0].split()
    if on and len(w) >= 2 and int(w[0], 16) < 0xFF0000:
        out.append(w[1])
if 'the engine' == sys.argv[1]:
    out += ['LoadPalettes', 'LoadPaletteLine']
print(','.join(out))
PYEOF
}

# the engine: clearing, sprites, palettes; the modes' shared routines; the SEGA logo
ENGINE=$(names "the engine")
GAME=$(names "the game")
LOGO=$(names "the logo")
TREASURE=$(names "the treasure"),FF2058

stage_poke() {   # the pokes that pick stage $1 (4-6) after stage 0's select
  $PY -c "print(';'.join(f'{f}:FFA204={4 * $1:04X}' for f in range(1334, 1480)))"
}

session() {
  local name=$1; shift
  echo "== $name"
  $CAP "$ROM" --out "out/captures/$name.cap" "$@" | tail -3
}

run() {
  case $1 in
    frame_attract) session $1 --fn "$FRAME" --frames 8000 --max 24 --spread ;;
    frame_stage0)  session $1 --fn "$FRAME" --frames 12000 --max 24 --spread --script @out/scripts/stage0.txt ;;
    frame_inv4|frame_inv5|frame_inv6)
      n=${1#frame_inv}
      session $1 --fn "$FRAME" --frames 9000 --max 16 --spread --from 1500 --script @out/scripts/inv$n.txt \
          --hold "$INV" --poke "$(stage_poke $n)" ;;
    novel_attract) session $1 --fn "$FRAME" --frames 20000 --max 64 --novel ;;
    novel_stage0|novel_stage1|novel_stage2|novel_stage3)
      n=${1#novel_stage}
      session $1 --fn "$FRAME" --frames 30000 --max 64 --novel --script @out/scripts/stage$n.txt ;;
    novel_inv0|novel_inv1|novel_inv2|novel_inv3|novel_inv4|novel_inv5|novel_inv6)
      n=${1#novel_inv}; poke=""
      [ "$n" -gt 3 ] && poke=$(stage_poke $n)
      session $1 --fn "$FRAME" --frames 60000 --max 64 --novel --script @out/scripts/inv$n.txt \
          --hold "$INV" --poke "$poke" ;;
    vblank_attract) session $1 --fn "VBlank,$BOOT" --frames 8000 --max 24 --spread ;;
    vblank_stage0) session $1 --fn "VBlank" --frames 12000 --max 24 --spread --script @out/scripts/stage0.txt ;;
    engine_attract) session $1 --fn "$ENGINE" --frames 20000 --max 64 --novel ;;
    engine_stage0) session $1 --fn "$ENGINE" --frames 30000 --max 64 --novel --script @out/scripts/stage0.txt ;;
    engine_spread) session $1 --fn "$ENGINE" --frames 20000 --max 16 --spread --script @out/scripts/stage1.txt ;;
    engine_inv5) session $1 --fn "$ENGINE" --frames 30000 --max 64 --novel --script @out/scripts/inv5.txt \
          --hold "$INV" --poke "$(stage_poke 5)" ;;
    game_attract) session $1 --fn "$GAME" --frames 30000 --max 48 --novel ;;
    game_stage0|game_stage2)
      n=${1#game_stage}
      session $1 --fn "$GAME" --frames 30000 --max 48 --novel --script @out/scripts/stage$n.txt ;;
    logo_attract) session $1 --fn "$LOGO" --frames 1000 --max 64 --novel ;;
    logo_spread) session $1 --fn "$LOGO" --frames 1000 --max 12 --spread ;;
    treasure_attract) session $1 --fn "$TREASURE" --frames 1000 --max 64 --novel ;;
    treasure_spread) session $1 --fn "$TREASURE" --frames 1000 --max 12 --spread ;;
    sound_attract) session $1 --fn "$SOUND" --frames 20000 --max 64 --novel ;;
    sound_spread) session $1 --fn "$SOUND" --frames 20000 --max 16 --spread --script @out/scripts/stage0.txt ;;
    sound_stage0|sound_stage1|sound_stage2|sound_stage3)
      n=${1#sound_stage}
      session $1 --fn "$SOUND" --frames 30000 --max 64 --novel --script @out/scripts/stage$n.txt ;;
    sound_inv4|sound_inv5|sound_inv6)
      n=${1#sound_inv}
      session $1 --fn "$SOUND" --frames 30000 --max 64 --novel --script @out/scripts/inv$n.txt \
          --hold "$INV" --poke "$(stage_poke $n)" ;;
    *) echo "no session $1"; exit 1 ;;
  esac
}

NOVEL="novel_attract novel_stage0 novel_stage1 novel_stage2 novel_stage3 novel_inv0 novel_inv1 novel_inv2 novel_inv3 novel_inv4 novel_inv5 novel_inv6"
SOUNDS="sound_attract sound_spread sound_stage0 sound_stage1 sound_stage2 sound_stage3 sound_inv4 sound_inv5 sound_inv6"
ALL="frame_attract frame_stage0 frame_inv4 frame_inv5 frame_inv6 $NOVEL $SOUNDS vblank_attract vblank_stage0
     engine_attract engine_stage0 engine_spread engine_inv5 game_attract game_stage0 game_stage2 logo_attract logo_spread treasure_attract treasure_spread"
for s in ${@:-$ALL}; do run "$s"; done

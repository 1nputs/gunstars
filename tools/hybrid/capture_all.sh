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
#   pause      CheckPause around a Start press (frame 2700 of stage0.txt): the pause branch
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
    *) echo "no session $1"; exit 1 ;;
  esac
}

NOVEL="novel_attract novel_stage0 novel_stage1 novel_stage2 novel_stage3 novel_inv0 novel_inv1 novel_inv2 novel_inv3 novel_inv4 novel_inv5 novel_inv6"
ALL="frame_attract frame_stage0 frame_inv4 frame_inv5 frame_inv6 $NOVEL"
for s in ${@:-$ALL}; do run "$s"; done

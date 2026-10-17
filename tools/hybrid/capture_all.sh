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
set -euo pipefail
cd "$(dirname "$0")/../.."
ROM="Gunstar Heroes (U) [!].gen"
PY="python3"
CAP="$PY tools/hybrid/capture.py"
mkdir -p out/captures
INV="FFA424=03E7,FFA624=03E7,FFC9A4=0001"
FRAME=CheckPause,HintEffect,HintNone,ReadPad1,ReadPad2,ReadPads,RunJobs,sub_00D36,sub_00F4C,sub_00F9C,sub_00FB4,sub_012F4,sub_0134A,sub_01462,sub_01534,sub_015C8,sub_0176E,sub_022DE,sub_02310,sub_02326,sub_023A0,sub_023CC,sub_023E6,sub_02478,sub_0258A,sub_025A6,sub_025FC,sub_02660,sub_0267E,sub_02702,sub_02712,sub_02886,sub_028B2,sub_0290C,sub_0311E,sub_03148,sub_03156,sub_03630

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
    frame_attract) session $1 --fn "$FRAME" --frames 8000 --max 48 --spread ;;
    frame_stage0)  session $1 --fn "$FRAME" --frames 12000 --max 48 --spread --script @out/scripts/stage0.txt ;;
    frame_inv4|frame_inv5|frame_inv6)
      n=${1#frame_inv}
      session $1 --fn "$FRAME" --frames 9000 --max 32 --spread --from 1500 --script @out/scripts/inv$n.txt \
          --hold "$INV" --poke "$(stage_poke $n)" ;;
    pause) session $1 --fn CheckPause --frames 2800 --max 64 --from 2690 --script @out/scripts/stage0.txt ;;
    *) echo "no session $1"; exit 1 ;;
  esac
}

ALL="frame_attract frame_stage0 frame_inv4 frame_inv5 frame_inv6 pause"
for s in ${@:-$ALL}; do run "$s"; done

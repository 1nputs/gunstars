#!/usr/bin/env bash
# The disassembly's evidence from nothing: the input scripts of every session, then each session
# through the oracle into out/evidence (tools/hybrid/evidence.py), then the summary.
#   attract     power on, no input: the logos, the title, its demos and story screens
#   stageN      a two-player game from the TAS's menu inputs (ref/tas, Samsara's 2P run: the title,
#               two players, both weapon screens), stage N (0-3) picked at the stage select (Right N
#               times, Start after its description), then random play on both pads, Start every
#               600 frames (continues, the game over, pauses)
#   invN        stages 0-6 with both players' vitality held at 999 ($FFA424, $FFA624) and the boss
#               slot's at 1 ($FFC9A4), so random play gets through them; stages 4-6 (the Empire's
#               base, the space shooter, the final battle: there is no stage select for them) by
#               writing $FFA204, the stage times 4, after stage 0 is picked; Start every 5000
# Needs build/oracle.so (make -f tools/hybrid/Makefile) and ref/tas/samsara-gunstarheroes.bk2.
# EVIDENCE=DIR writes there instead of out/evidence (to compare a rebuild with the last).
set -euo pipefail
cd "$(dirname "$0")/../.."
ROM="Gunstar Heroes (U) [!].gen"
PY="python3"
FRAMES=${FRAMES:-30000}
EV=${EVIDENCE:-out/evidence}
mkdir -p out/scripts
[ -f out/scripts/tas2p.txt ] || $PY tools/oracle/bk2script.py ref/tas/samsara-gunstarheroes.bk2 out/scripts/tas2p.txt
for n in 0 1 2 3; do
  ins="880:,"
  for k in $(seq 1 $n); do ins="$ins$((880 + k * 12)):RIGHT,$((886 + k * 12)):,"; done
  ins="${ins}1330:START|START,1333:,1360:B|B,1363:"
  $PY tools/oracle/randscript.py out/scripts/stage$n.txt --seed $((100 + n)) --frames "$FRAMES" \
      --prefix @out/scripts/tas2p.txt --cut 841 --insert "$ins" --from 1500 --start-every 600
done
rm -rf "$EV"
$PY tools/hybrid/evidence.py "$ROM" --out "$EV" --name attract --frames 20000 > /dev/null
for n in 0 1 2 3; do
  $PY tools/hybrid/evidence.py "$ROM" --out "$EV" --name stage$n --frames "$FRAMES" --script @out/scripts/stage$n.txt > /dev/null
done
INV="FFA424=03E7,FFA624=03E7,FFC9A4=0001"
for n in 0 1 2 3 4 5 6; do
  s=$n; [ $n -gt 3 ] && s=0
  $PY tools/oracle/randscript.py out/scripts/inv$n.txt --seed $((200 + n)) --frames 60000 \
      --prefix @out/scripts/stage$s.txt --cut 1500 --from 1500 --start-every 5000 > /dev/null
  poke=""
  [ $n -gt 3 ] && poke=$($PY -c "print(';'.join(f'{f}:FFA204={4 * $n:04X}' for f in range(1334, 1480)))")
  $PY tools/hybrid/evidence.py "$ROM" --out "$EV" --name inv$n --frames 60000 --script @out/scripts/inv$n.txt --hold "$INV" --poke "$poke" > /dev/null
done
$PY tools/hybrid/evidence.py "$ROM" --out "$EV" --summary

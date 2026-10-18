#!/usr/bin/env bash
# The native game through every stage, headless: the evidence runs' inputs (evidence_all.sh makes
# them: invN, 60,000 frames of random play; rushN, 120,000 alternating random play with both pads
# running right and firing), both players' vitality and the boss's held, stage N picked by
# holding the stage number ($FFA204, 4 N) while the stage select is up (mode $28: the native
# select comes up at another frame than the oracle's, so its Right presses and pokes by frame
# miss). A run passes when it plays its frames out: a routine
# not ported, a crash or a hang (past its time limit) fails it. Each run's log (the mode and
# stage every 30 frames) goes to out/sweep/NAME.log, and a summary is printed: the stages and
# modes each run went through. "continue" is stage1.txt to frame 2000, where both players'
# vitality is emptied, then Start every 40 frames with the continue screen's choice held at "yes",
# so the game over continues (once: the idle players are not knocked out again).
#   tools/native_sweep.sh [NAME...]     (default: inv0-inv6 rush0-rush6 continue; JOBS=7 at once)
set -uo pipefail
cd "$(dirname "$0")/.."
BIN=build/gunstar
INV="FFA424=03E7,FFA624=03E7,FFC9A4=0001"
JOBS=${JOBS:-7}
mkdir -p out/sweep
[ -x "$BIN" ] || { echo "no $BIN (make gunstar)"; exit 1; }

one() {
  if [ "$1" = continue ]; then
    # stage1.txt to the knock-out, then Start every 40 frames (in play it pauses and goes on; on
    # the continue screen it takes the choice)
    { awk -F: '$1 + 0 < 2000' out/scripts/stage1.txt
      for f in $(seq 2000 40 29960); do echo "$f:START"; echo "$((f + 2)):"; done; } > out/sweep/continue.txt
    GUNSTAR_TRACE=1 timeout --signal=KILL 1200 "$BIN" --headless --frames 30000 --input out/sweep/continue.txt \
        --poke "2000:FFA424=0000,FFA624=0000" --hold-if "FFA284=003C:FFA28A=0000" \
        > out/sweep/continue.out 2> out/sweep/continue.log
    echo "$?" > out/sweep/continue.status
    return
  fi
  local name=$1 n=${1: -1} frames=60000
  [ "${name#rush}" != "$name" ] && frames=120000
  [ -f "out/scripts/$name.txt" ] || { echo "$name: no out/scripts/$name.txt (tools/hybrid/evidence_all.sh)"; return 1; }
  local pick=""
  [ "$n" -gt 0 ] && pick=$(printf 'FFA284=0028:FFA204=%04X' $((4 * n)))
  GUNSTAR_TRACE=1 timeout --signal=KILL $((frames / 60 + 600)) "$BIN" --headless --frames "$frames" \
      --input "out/scripts/$name.txt" --hold "$INV" ${pick:+--hold-if "$pick"} > "out/sweep/$name.out" 2> "out/sweep/$name.log"
  echo "$?" > "out/sweep/$name.status"
}
export -f one
export BIN INV

names=${*:-inv0 inv1 inv2 inv3 inv4 inv5 inv6 rush0 rush1 rush2 rush3 rush4 rush5 rush6 continue}
printf '%s\n' $names | xargs -P "$JOBS" -I{} bash -c 'one {}'

python3 - $names <<'EOF'
import re, sys
bad = 0
for name in sys.argv[1:]:
    try:
        status = int(open(f'out/sweep/{name}.status').read())
        log = open(f'out/sweep/{name}.log').read()
    except OSError:
        print(f'{name}: no result'); bad += 1; continue
    frames = re.findall(r'^frame (\d+): mode (\w+) step \w+ stage (\w+)', log, re.M)
    stages, modes = [], []
    for f, m, st in frames:
        if not stages or stages[-1] != st: stages.append(st)
        if m not in modes: modes.append(m)
    last = frames[-1][0] if frames else '0'
    stop = [l for l in log.splitlines() if 'not ported' in l]
    ok = status == 0 and not stop
    bad += not ok
    print(f'{name}: {"ok" if ok else "FAILED"} (exit {status}), {last} frames; stages {" ".join(stages)}; modes {" ".join(modes)}'
          + (f'\n    {stop[0]}' if stop else ''))
print('sweep:', 'all passed' if not bad else f'{bad} failed')
sys.exit(1 if bad else 0)
EOF

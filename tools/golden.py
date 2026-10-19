#!/usr/bin/env python3
"""Golden runs: the native game headless from power on with scripted input, a line of hashes a
frame (gunstar --golden: the frame, work RAM, the VDP's state and the Z80's RAM at the frame's
handshake, the picture, the frame's sound and its samples). `record` keeps a build's runs in
out/golden, with the input scripts they played (out/golden/inputs: the runs go on playing the
same input whatever out/scripts becomes); `check` runs the build again and compares them frame
by frame, so a change (the RAM as native structs, pulseman, the native client) can be shown to
change nothing it was not meant to: everything must match bit for bit.

The runs: the attract loop; the options screen; the TAS's two-player game to its game over;
every stage (invN: random play on both pads with both players' vitality and the boss's held,
stage N picked at the stage select, as tools/native_sweep.sh); stage 0 cleared into stage 1
(rush0); a game over continued (stage 1's script to frame 2000, both players knocked out, Start
every 40 frames with the continue screen's choice held at "yes").

`check` also saves a native state in four of the runs, loads it in a new process and plays on:
every frame must be the run's own (a state holds everything the game goes on from). And it plays
the window's requests (SAVE at handshake 2000, LOAD at 3000, REWIND held from 4000 to 4150, in
inv0): every frame must be inv0's frame of that number, the load must go back to the save's
frame and the rewind back three frames a frame.

The out/scripts inputs come from tools/hybrid/evidence_all.sh (the TAS's menus, then seeded random
play) and tools/hybrid/options_script.py. tools/native_sweep.sh plays the same runs for longer
(1.29 million frames) and checks only that they play out.

    tools/golden.py record [--force]     (make golden: the runs not yet recorded; --force: all of them)
    tools/golden.py check                (make golden-check)
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS = os.path.join(ROOT, 'out', 'scripts')
GUNSTAR = os.path.join(ROOT, 'build', 'gunstar')

INV = ['--hold', 'FFA424=03E7,FFA624=03E7,FFC9A4=0001']


def pick(n):
    """stage n held while the stage select is up (mode $28)"""
    return ['--hold-if', 'FFA284=0028:FFA204=%04X' % (4 * n)] if n else []


# (name, frames, options): an --input names a script of out/scripts (continue.txt is made here)
RUNS = [
    ('attract', 7500, []),
    ('options', 3600, ['--input', 'options.txt']),
    ('tas2p', 7000, ['--input', 'tas2p.txt']),
    *[('inv%d' % n, 24000, ['--input', 'inv%d.txt' % n] + INV + pick(n)) for n in range(7)],
    ('rush0', 40000, ['--input', 'rush0.txt'] + INV),
    ('continue', 12000, ['--input', 'continue.txt', '--poke', '2000:FFA424=0000,FFA624=0000',
                         '--hold-if', 'FFA284=003C:FFA28A=0000']),
]
# the runs a state is saved in (at the first MainLoop wait from this frame on) and loaded from
STATES = {'tas2p': 4000, 'inv3': 12000, 'rush0': 37000, 'continue': 8740}
# the window's requests, by handshake, in inv0 (the frames all inv0's)
KEYS = ('inv0', 4000, ['2000:SAVE', '2001:', '3000:LOAD', '3001:', '4000:REWIND', '4150:'])
COLUMNS = ['frame', 'work RAM', 'VDP state', 'Z80 RAM', 'picture', 'sound', 'samples']
SAVED = re.compile(r'state saved at frame (\d+)')


def continue_script(path):
    """stage1.txt to frame 2000, then Start every 40 frames (in play it pauses and goes on; on the
    continue screen it takes the choice)"""
    with open(os.path.join(SCRIPTS, 'stage1.txt')) as f:
        lines = [l.strip() for l in f if l.strip() and not l.startswith('#') and int(l.split(':')[0]) < 2000]
    for k in range(2000, 29961, 40):
        lines += ['%d:START' % k, '%d:' % (k + 2)]
    with open(path, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def inputs(golden_dir):
    """the runs' input scripts, from out/scripts into golden_dir/inputs (those not there yet)"""
    d = os.path.join(golden_dir, 'inputs')
    os.makedirs(d, exist_ok=True)
    for _, _, opts in RUNS:
        for k, o in enumerate(opts):
            if o != '--input' or os.path.exists(os.path.join(d, opts[k + 1])):
                continue
            name = opts[k + 1]
            if name == 'continue.txt':
                continue_script(os.path.join(d, name))
            elif os.path.exists(os.path.join(SCRIPTS, name)):
                shutil.copy(os.path.join(SCRIPTS, name), os.path.join(d, name))
            else:
                sys.exit('no out/scripts/%s (tools/hybrid/evidence_all.sh, tools/hybrid/options_script.py)' % name)


def run(name, frames, opts, out, input_dir, extra=()):
    """gunstar headless into out; returns (name, error or None, the frame a state was saved at)"""
    args = [GUNSTAR, '--frames', str(frames), '--golden', out, *extra]
    for k, o in enumerate(opts):
        args.append(os.path.join(input_dir, o) if k and opts[k - 1] == '--input' else o)
    try:
        r = subprocess.run(args, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                           timeout=frames / 30 + 300)
    except subprocess.TimeoutExpired:
        return name, 'gunstar did not finish (a hang)', None
    if r.returncode != 0 or 'not ported' in r.stderr:
        return name, 'gunstar failed (%d): %s' % (r.returncode, r.stderr.strip()[-300:]), None
    m = SAVED.search(r.stdout)
    if '--save-state' in extra and not m:
        return name, 'no state saved', None
    return name, None, int(m.group(1)) if m else None


def record(dirname, input_dir, jobs, runs, states=False):
    """the runs into dirname, the longest first (states: also save one in the runs of STATES).
    Returns the frames the states were saved at (by name), or None on a failure."""
    os.makedirs(dirname, exist_ok=True)

    def one(r):
        extra = ['--save-state', '%d:%s' % (STATES[r[0]], os.path.join(dirname, r[0] + '.mdrs'))] \
            if states and r[0] in STATES else []
        return run(r[0], r[1], r[2], os.path.join(dirname, r[0] + '.golden'), input_dir, extra)
    with ThreadPoolExecutor(jobs) as ex:
        results = list(ex.map(one, sorted(runs, key=lambda r: -r[1])))
    bad = [(n, e) for n, e, _ in results if e]
    for n, e in bad:
        print('%s: %s' % (n, e))
    return None if bad else {n: f for n, _, f in results if f is not None}


def lines(path):
    with open(path) as f:
        return [l.split() for l in f]


def differ(want, got):
    """the columns of a line that differ"""
    return ', '.join(COLUMNS[k] for k in range(len(COLUMNS)) if k >= len(want) or k >= len(got) or want[k] != got[k])


def compare(want, got):
    """the first difference of run got from want, frame for frame, or None"""
    a, b = lines(want), lines(got)
    for x, y in zip(a, b):
        if x != y:
            return 'frame %s: %s differ' % (x[0], differ(x, y))
    if len(a) != len(b):
        return '%d frames, the golden run has %d' % (len(b), len(a))
    return None


def by_frame(want, got):
    """run got against want by frame number (got may go back: a state loaded); returns the first
    difference or None, and got's lines"""
    a = {x[0]: x for x in lines(want)}
    b = lines(got)
    for y in b:
        x = a.get(y[0])
        if x is None:
            return 'frame %s: not in the golden run' % y[0], b
        if x != y:
            return 'frame %s: %s differ' % (y[0], differ(x, y)), b
    return None, b


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('action', choices=['record', 'check'])
    ap.add_argument('--dir', default=os.path.join(ROOT, 'out', 'golden'))
    ap.add_argument('--force', action='store_true', help='record over existing golden runs')
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    a = ap.parse_args()
    if not os.path.exists(GUNSTAR):
        sys.exit('no build/gunstar (make gunstar)')
    golden_inputs = os.path.join(a.dir, 'inputs')
    if a.action == 'record':                              # (without --force: only the runs it has none of)
        if a.force and os.path.isdir(a.dir):
            shutil.rmtree(a.dir)
        inputs(a.dir)
        todo = [r for r in RUNS if not os.path.exists(os.path.join(a.dir, r[0] + '.golden'))]
        ok = record(a.dir, golden_inputs, a.jobs, todo) is not None
        if ok:
            build = subprocess.run(['git', 'describe', '--always', '--dirty'], cwd=ROOT, stdout=subprocess.PIPE, text=True).stdout.strip()
            with open(os.path.join(a.dir, 'BUILD'), 'a') as f:
                f.write('%s: %s\n' % (build, ' '.join(r[0] for r in todo) or 'none'))
        frames = sum(r[1] for r in todo)
        print('golden: %d runs (%d frames) recorded in %s' % (len(todo), frames, os.path.relpath(a.dir, ROOT)) if ok else 'golden: FAILED')
        sys.exit(0 if ok else 1)

    if not os.path.isdir(golden_inputs):
        sys.exit('no golden runs in %s (make golden)' % os.path.relpath(a.dir, ROOT))
    new = a.dir.rstrip('/') + '-new'
    shutil.rmtree(new, ignore_errors=True)
    saved = record(new, golden_inputs, a.jobs, RUNS, states=True)
    if saved is None:
        sys.exit('golden: FAILED')
    bad = 0
    for name, frames, _ in RUNS:
        want = os.path.join(a.dir, name + '.golden')
        if not os.path.exists(want):
            print('%-9s no golden run (make golden)' % name)
            bad += 1
            continue
        d = compare(want, os.path.join(new, name + '.golden'))
        print('%-9s %s' % (name, d or 'identical, %d frames' % frames))
        bad += d is not None
    print('golden: %d of %d runs identical' % (len(RUNS) - bad, len(RUNS)))

    # the states, each loaded in a new process; and the window's requests
    spec = {r[0]: r for r in RUNS}

    def loaded(name):
        r = spec[name]
        return run(name, r[1], r[2], os.path.join(new, name + '.loaded'), golden_inputs,
                   ['--load-state', os.path.join(new, name + '.mdrs')])

    def keys():
        req = os.path.join(new, 'keys.requests')
        with open(req, 'w') as f:
            f.write('\n'.join(KEYS[2]) + '\n')
        r = spec[KEYS[0]]
        return run('keys', KEYS[1], r[2], os.path.join(new, 'keys.golden'), golden_inputs,
                   ['--requests', req, '--state', os.path.join(new, 'keys.mdrs')])
    with ThreadPoolExecutor(a.jobs) as ex:
        results = list(ex.map(loaded, STATES))
        keyed = ex.submit(keys).result()
    sbad = 0
    for name, err, _ in results:
        d, got = err, []
        if not d:
            d, got = by_frame(os.path.join(new, name + '.golden'), os.path.join(new, name + '.loaded'))
            back = [int(y[0]) for y in got[1:2]]
            if not d and back != [saved[name] + 1]:
                d = 'the state loaded goes on from frame %s, saved at %d' % (back, saved[name])
        print('%-9s %s' % (name, d or 'a state saved at frame %d and loaded: the %d frames after it the run\'s own'
                           % (saved[name], len(got) - 1)))
        sbad += d is not None
    print('golden: %d of %d states go on as the run' % (len(STATES) - sbad, len(STATES)))

    _, d, at = keyed
    if not d:
        d, got = by_frame(os.path.join(new, KEYS[0] + '.golden'), os.path.join(new, 'keys.golden'))
    if not d:
        f = [int(y[0]) for y in got]
        steps = [f[k] - f[k - 1] for k in range(1, len(f))]
        loads = [(f[k - 1], f[k]) for k in range(1, len(f)) if steps[k - 1] < -3]
        rewound = steps.count(-3)
        if len(loads) != 1 or at is None or loads[0][1] != at + 1 or rewound < 145:
            d = 'saved at frame %s; went back %s; the rewind stepped back %d frames' % (at, loads, rewound)
        else:
            d = None
            ok = 'SAVE at frame %d, LOAD from frame %d back to %d; REWIND: %d frames stepped back three; every frame inv0\'s' \
                % (at, loads[0][0], loads[0][1], rewound)
    print('%-9s %s' % ('keys', d or ok))
    sys.exit(1 if bad or sbad or d else 0)


if __name__ == '__main__':
    main()

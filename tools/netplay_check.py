#!/usr/bin/env python3
"""Netplay checked (make netplay-check): two headless gunstar processes over the loopback, each
playing its own pad's column of a script (the host pad 1, the guest pad 2) through a simulated
network (gunstar --net-sim: delay, jitter, so packets overtake each other, loss, duplicates),
against a solo run of the same buttons: the script moved later by the input delay (netplay's
buttons reach the game delay + 1 frames after they are pressed, a solo script's one). Every frame
each peer finally played (the last golden line it wrote for it: a rollback writes frames again)
must be the solo run's: work RAM, the VDP's state and the Z80's RAM, bit for bit. Each peer must
play out its frames, its own desync checks agreeing.

  stage2   host and join, 40 ms each way +-10, 5% lost, 1% twice, input delay 2: stage 2 with
           both players' vitality held (from frame 3851 a job waits on its buffer: no resumable
           point for a while)
  tas2p    both connect (--peer), 25 ms +-25, 10% lost, delay 1: the TAS's two-player game from
           power on (the title, the menus, both players in stage 0)
  stage5   host and join, 15 ms +-5, delay 0 (a rollback for nearly every change of buttons):
           stage 5 (from frame 4735 without a resumable point)
  reset    host and join, 30 ms +-10, 5% lost, delay 2: the TAS's game, the host resetting the
           console at frame 3000 (its script's RESET: every machine powered on again at the same
           frame), against a solo run that resets there (the frames after it a fresh power on's)
  load     host and join, 25 ms +-10, 5% lost, delay 1: stage 2, the host loading a state for
           everyone from frame 1500 (--net-load: a solo run's at frame 3000), against a solo run
           that loads it at the frame the session did (--load-at); and the guest saving one from
           frame 4000 (--net-save: a state every machine played, as the window's Save state in a
           session), which a solo run loads and plays on from as the session did

    tools/netplay_check.py [NAME...]     (the inputs: out/golden/inputs, from make golden)
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GUNSTAR = os.path.join(ROOT, 'build', 'gunstar')
INPUTS = os.path.join(ROOT, 'out', 'golden', 'inputs')
OUT = os.path.join(ROOT, 'out', 'netplay')
INV = ['--hold', 'FFA424=03E7,FFA624=03E7,FFC9A4=0001']


def pick(n):
    return ['--hold-if', 'FFA284=0028:FFA204=%04X' % (4 * n)]


# name, frames, script, the game's options, how they connect, delay, the network (ms, ms, %, %),
# and what the host does in it: ('reset', FRAME) its script's RESET there, ('load', SAVED, FRAME) a
# state a solo run saved at SAVED loaded for everyone from FRAME
SCENARIOS = [
    ('stage2', 4500, 'inv2.txt', INV + pick(2), 'join', 2, '40,10,5,1', None),
    ('tas2p', 6000, 'tas2p.txt', [], 'peer', 1, '25,25,10,1', None),
    ('stage5', 5200, 'inv5.txt', INV + pick(5), 'join', 0, '15,5,2,0', None),
    ('reset', 6000, 'tas2p.txt', [], 'join', 2, '30,10,5,1', ('reset', 3000)),
    ('load', 5000, 'inv2.txt', INV + pick(2), 'join', 1, '25,10,5,1', ('load', 3000, 1500)),
]


def entries(path):
    """a script's entries: (frame, the rest)"""
    out = []
    with open(path) as f:
        for line in f:
            for item in line.split('#')[0].replace(',', '\n').split('\n'):
                item = item.strip()
                if ':' in item:
                    frame, rest = item.split(':', 1)
                    out.append((int(frame), rest))
    return out


def with_reset(src, dst, at):
    """the script with pad 1's RESET at frame at, the buttons held there kept"""
    e = entries(src)
    held = '|'
    for frame, rest in e:
        if frame <= at:
            held = rest
    p1, p2 = (held.split('|', 1) + [''])[:2]
    e = [x for x in e if x[0] != at] + [(at, '%s|%s' % ('+'.join(b for b in (p1, 'RESET') if b), p2))]
    e.sort(key=lambda x: x[0])
    with open(dst, 'w') as f:
        f.write(''.join('%d:%s\n' % x for x in e))


def shifted(src, dst, by):
    """the script with every entry by frames later"""
    with open(dst, 'w') as f:
        f.write(''.join('%d:%s\n' % (frame + by, rest) for frame, rest in entries(src)))


def last_lines(path):
    """the last golden line of each frame: work RAM, VDP, Z80 RAM"""
    lines = {}
    with open(path) as f:
        for line in f:
            x = line.split()
            if len(x) >= 4:
                lines[int(x[0])] = x[1:4]
    return lines


def scenario(k, s):
    name, frames, script, opts, how, delay, sim, act = s
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    src = os.path.join(INPUTS, script)
    if not os.path.exists(src):
        return name, 'no %s (make golden)' % os.path.relpath(src, ROOT)
    limit = frames / 20 + 120
    host_opts, guest_opts, solo_opts = [], [], []
    if act and act[0] == 'reset':
        with_reset(src, os.path.join(d, 'session.txt'), act[1])
        src = os.path.join(d, 'session.txt')
    if act and act[0] == 'load':
        state = os.path.join(d, 'saved.state')
        if os.path.exists(state):
            os.remove(state)
        subprocess.run([GUNSTAR, '--headless', '--frames', str(act[1] + 100), '--input', src, '--save-state',
                        '%d:%s' % (act[1], state)] + opts, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=limit)
        if not os.path.exists(state):
            return name, 'the solo run saved no state at frame %d' % act[1]
        host_opts = ['--net-load', '%d:%s' % (act[2], state)]
        kept = os.path.join(d, 'session.state')
        if os.path.exists(kept):
            os.remove(kept)
        guest_opts = ['--net-save', '%d:%s' % (frames - 1000, kept)]
    shifted(src, os.path.join(d, 'solo.txt'), delay)
    base = [GUNSTAR, '--headless', '--frames', str(frames), '--input', src, '--delay', str(delay), '--net-sim', sim] + opts
    pa, pb = 7470 + 2 * k, 7471 + 2 * k
    if how == 'join':
        roles = [['--host', str(pa)], ['--join', '127.0.0.1:%d' % pa]]
    else:
        roles = [['--peer', '127.0.0.1:%d' % pb, '--port', str(pa)], ['--peer', '127.0.0.1:%d' % pa, '--port', str(pb)]]
    procs = []
    for who, role in zip(('a', 'b'), roles):
        err = open(os.path.join(d, who + '.log'), 'w')
        procs.append((subprocess.Popen(base + role + (host_opts if who == 'a' else guest_opts) + ['--golden', os.path.join(d, who + '.golden')],
                                       cwd=ROOT, stdout=subprocess.DEVNULL, stderr=err), err))
    bad = []
    for (p, err), who in zip(procs, 'ab'):
        try:
            code = p.wait(timeout=limit)
        except subprocess.TimeoutExpired:
            p.kill()
            code = 'a hang'
        err.close()
        if code != 0:
            bad.append('%s failed (%s)' % (who, code))
    if act and act[0] == 'load':                          # (the solo run loads it where the session did)
        at = set()
        for who in 'ab':
            at |= set(re.findall(r"the host's state loaded at frame (\d+)", open(os.path.join(d, who + '.log')).read()))
        if len(at) != 1:
            return name, 'the state was loaded at %s' % (sorted(at) or 'no frame'), []
        solo_opts = ['--load-at', '%s:%s' % (at.pop(), state)]
    solo = subprocess.run([GUNSTAR, '--headless', '--frames', str(frames), '--input', os.path.join(d, 'solo.txt'),
                           '--golden', os.path.join(d, 'solo.golden')] + opts + solo_opts, cwd=ROOT, stdout=subprocess.DEVNULL,
                          stderr=subprocess.PIPE, text=True, timeout=limit)
    if solo.returncode != 0:
        return name, 'the solo run failed: ' + solo.stderr[-200:]
    ref = last_lines(os.path.join(d, 'solo.golden'))
    report = []
    for who in 'ab':
        log = open(os.path.join(d, who + '.log')).read()
        got = last_lines(os.path.join(d, who + '.golden'))
        same = sum(1 for f in range(1, frames + 1) if got.get(f) == ref.get(f))
        first = next((f for f in range(1, frames + 1) if got.get(f) != ref.get(f)), None)
        m = re.search(r'frame (\d+), (\d+) rollbacks \((\d+) frames played again, at most (\d+)\), (\d+) frames given back, '
                      r'(\d+) desync checks, round trip (-?\d+) ms, (\d+) resets, (\d+) loads', log)
        role = 'host' if 'hosting' in log else 'guest'
        if same != frames:
            bad.append('%s: frame %s differs from the solo run' % (role, first))
        if not m or int(m.group(1)) < frames:
            bad.append('%s did not play out: %s' % (role, log.strip().splitlines()[-1] if log.strip() else 'no log'))
        elif int(m.group(6)) < frames // 60 - 3:
            bad.append('%s: only %s desync checks' % (role, m.group(6)))
        if 'desync at' in log or 'lost' in log:
            bad.append('%s: %s' % (role, [l for l in log.splitlines() if 'desync at' in l or 'lost' in l][0]))
        if m and act and int(m.group(8 if act[0] == 'reset' else 9)) < 1:
            bad.append('%s: no %s' % (role, act[0]))
        if m:
            report.append('%s %d/%d as the solo run, %s rollbacks (%s frames again, at most %s), %s given back, %s checks, %s ms'
                          % (role, same, frames, m.group(2), m.group(3), m.group(4), m.group(5), m.group(6), m.group(7))
                          + (', %s resets' % m.group(8) if act and act[0] == 'reset' else '')
                          + (', %s loads' % m.group(9) if act and act[0] == 'load' else ''))
    if act and act[0] == 'load':
        report.append('the state loaded at frame %s' % solo_opts[1].split(':')[0])
        saved = re.findall(r'the state of frame (\d+) saved', open(os.path.join(d, 'b.log')).read())
        if not saved or not os.path.exists(kept):
            bad.append('the guest saved no state')
        else:                                             # (the session's state, alone: on as the session went)
            again = os.path.join(d, 'again.golden')
            r = subprocess.run([GUNSTAR, '--headless', '--frames', str(frames), '--input', os.path.join(d, 'solo.txt'), '--load-state',
                                kept, '--golden', again] + opts, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                               text=True, timeout=limit)
            got, at = last_lines(again), int(saved[0])
            same = sum(1 for f in range(at + 1, frames + 1) if got.get(f) == ref.get(f))
            if r.returncode != 0 or same != frames - at:
                bad.append("the guest's state of frame %d, loaded alone: %d of %d frames as the session" % (at, same, frames - at))
            report.append("the guest's state of frame %d, loaded alone: %d/%d frames as the session" % (at, same, frames - at))
    return name, '; '.join(bad) if bad else None, report


def main():
    if not os.path.exists(GUNSTAR):
        sys.exit('no build/gunstar (make gunstar)')
    names = sys.argv[1:] or [s[0] for s in SCENARIOS]
    todo = [(k, s) for k, s in enumerate(SCENARIOS) if s[0] in names]
    with ThreadPoolExecutor(len(todo)) as ex:
        results = list(ex.map(lambda ks: scenario(*ks), todo))
    failed = 0
    for r in results:
        name, err = r[0], r[1]
        print('%-7s %s' % (name, 'ok' if not err else 'FAILED: ' + err))
        for line in (r[2] if len(r) > 2 else []):
            print('        ' + line)
        failed += err is not None
    print('netplay: %s' % ('all %d sessions as their solo runs' % len(results) if not failed else '%d failed' % failed))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()

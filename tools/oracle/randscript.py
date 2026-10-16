#!/usr/bin/env python3
"""Make an input script: a prefix (another script, cut at a frame), then random play on both pads
from a seed: directions held a few frames at a time, mostly forward, with the fire button held
most of the time and the others pressed now and then. Deterministic for a seed.

usage: randscript.py OUT --seed N --frames N [--prefix @FILE --cut F] [--insert "F:BTN|BTN,..."]
                     [--from F] [--pads 1|2]
"""
import argparse, os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from script import parse, write  # noqa: E402

DIRS = [('RIGHT',), ('RIGHT',), ('RIGHT',), ('LEFT',), ('UP',), ('DOWN',), ('UP', 'RIGHT'), ('DOWN', 'RIGHT'), ()]


def pad(rng):
    d = rng.choice(DIRS)
    b = []
    if rng.random() < 0.75:
        b.append('B')
    if rng.random() < 0.15:
        b.append('C')
    if rng.random() < 0.03:
        b.append('A')
    return tuple(d) + tuple(b)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('--seed', type=int, required=True)
    ap.add_argument('--frames', type=int, required=True, help='last frame of the script')
    ap.add_argument('--prefix', default='')
    ap.add_argument('--cut', type=int, default=0, help='prefix entries before this frame only')
    ap.add_argument('--insert', default='', help='entries added to the prefix')
    ap.add_argument('--from', dest='start', type=int, default=0, help='random play from this frame')
    ap.add_argument('--pads', type=int, default=2)
    ap.add_argument('--start-every', type=int, default=0, help='press START on both pads every N frames (continues)')
    a = ap.parse_args()
    rng = random.Random(a.seed)
    entries = [e for e in parse(a.prefix) if e[0] < a.cut] if a.prefix else []
    if a.insert:
        entries = sorted(entries + parse(a.insert), key=lambda e: e[0])
    f = max(a.start, a.cut)
    p = [(), ()]
    while f < a.frames:
        k = rng.randrange(2) if a.pads == 2 else 0
        p[k] = pad(rng)
        if a.pads == 1:
            p[1] = ()
        entries.append((f, p[0], p[1]))
        f += rng.randint(2, 24)
    if a.start_every:
        extra = []
        for g in range(max(a.start, a.cut), a.frames, a.start_every):
            extra += [(g, ('START',), ('START',)), (g + 2, (), ())]
        entries = sorted(entries + extra, key=lambda e: e[0])
    write(a.out, entries)
    print(f'{a.out}: {len(entries)} entries to frame {a.frames}')


if __name__ == '__main__':
    main()

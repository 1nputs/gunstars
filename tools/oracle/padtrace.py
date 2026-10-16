#!/usr/bin/env python3
"""Record a pad port test vector (.padtrace) for pulseman: the 68k's accesses to the I/O ports
($A10000-$A1001F) and, every frame, the buttons held on pads 1 and 2.

usage: padtrace.py ROM --out FILE.padtrace [--state STATE] [--frames N] [--script S]   (both pads: tools/oracle/script.py)
"""
import argparse, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'hybrid'))
from oracle import Oracle  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from script import Player  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--out', required=True)
    ap.add_argument('--state', default='')
    ap.add_argument('--frames', type=int, default=600)
    ap.add_argument('--script', default='')
    a = ap.parse_args()
    o = Oracle()
    o.load(a.rom)
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    script = Player(a.script)
    if not o.lib.oracle_padtrace_open(a.out.encode()):
        sys.exit(f'cannot write {a.out}')
    for f in range(a.frames):
        script.at(o, f)
        o.lib.oracle_padtrace_frame(*script.words())
        o.run()
    o.lib.oracle_padtrace_close()
    print(f'{a.out}: {a.frames} frames, {os.path.getsize(a.out) // 1024} KB')


if __name__ == '__main__':
    main()

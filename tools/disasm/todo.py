#!/usr/bin/env python3
"""What is left to port under a routine: its callees (and itself) leaf first, those not ported,
only the ones a coverage file says ran (capture.py --coverage; else every one the recorded runs
executed).

usage: todo.py ROOT [--cov FILE] [--all]
"""
import argparse, array, glob, os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from program import Program, ROOT  # noqa: E402


def ported():
    out = set()
    for fn in glob.glob(os.path.join(ROOT, 'src', '*.h')):
        for m in re.finditer(r'/\*\s*\$([0-9A-Fa-f]{6})', open(fn).read()):
            out.add(int(m.group(1), 16))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('root')
    ap.add_argument('--cov', default='')
    ap.add_argument('--all', action='store_true', help='also the ported ones')
    a = ap.parse_args()
    p = Program()
    if a.cov:
        cov = array.array('I'); cov.frombytes(open(a.cov, 'rb').read())
        ran = lambda x: cov[x] > 0
    else:
        ran = lambda x: x in p.executed
    done = ported()
    order, seen = [], set()

    def visit(e):
        if e in seen:
            return
        seen.add(e)
        r = p.routines[e]
        for s, ts in sorted(r.calls.items()):
            for t in ts:
                visit(t)
        for s, ts in sorted(r.tails.items()):
            for t in ts:
                visit(t)
        order.append(e)
    visit(p.by_name(a.root).entry)
    n = ins = 0
    for e in order:
        if not ran(e) or (e in done and not a.all):
            continue
        r = p.routines[e]
        n += 1
        ins += len(r.body)
        print('%-8s %-14s %4d insns' % ('ported' if e in done else '', r.name, len(r.body)))
    print('%d routines, %d instructions' % (n, ins))


if __name__ == '__main__':
    main()

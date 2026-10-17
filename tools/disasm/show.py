#!/usr/bin/env python3
"""Print routines' instructions as the disassembly has them (only theirs: their body in address
order, labels where something branches), with ;! on instructions no recorded run executed.

usage: show.py NAME...        (names or hex addresses)
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from program import Program  # noqa: E402
import m68k  # noqa: E402


def main():
    p = Program()
    for n in sys.argv[1:]:
        r = p.by_name(n)
        if not r:
            print('%s: no routine' % n)
            continue
        targets = set()
        for a in r.body:
            for s in r.succ.get(a, []):
                i = p.insn[a]
                if s != a + i.size:
                    targets.add(s)
        prev = None
        print('%s:   ; $%06X' % (r.name, r.entry))
        for a in r.body:
            i = p.insn[a]
            if prev is not None and prev != a:
                print('\t; ...')
            if a in targets and a != r.entry:
                print('%s:' % p.label(a))
            try:
                text = m68k.render(i, lambda t, h: p.name.get(t) or ('$%X' % t))
            except Exception:
                text = i.mnem
            print('\t%-40s ; %06X%s' % (text, a, '' if a in p.executed else ' ;!'))
            prev = a + i.size
        for s, ts in sorted(r.tails.items()):
            print('\t; %06X continues into %s' % (s, ', '.join(p.routines[t].name for t in ts)))
        print()


if __name__ == '__main__':
    main()

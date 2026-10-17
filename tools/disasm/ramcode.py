#!/usr/bin/env python3
"""Disassemble code the game copies into work RAM, at the address it runs at: the copy's ROM
bytes placed at their RAM address, followed from an entry by flow, with ;! where the recorded
runs did not execute (out/evidence/ram_cov.u32).

usage: ramcode.py RAM_START ROM_SOURCE SIZE ENTRY...      (hex)
"""
import array, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import m68k  # noqa: E402
from program import ROM, ROOT  # noqa: E402


def main():
    base, src, size = (int(x, 16) for x in sys.argv[1:4])
    entries = [int(x, 16) for x in sys.argv[4:]] or [base]
    rom = open(ROM, 'rb').read()
    mem = bytearray(0x1000000)
    mem[:len(rom)] = rom
    mem[base & 0xFFFFFF:(base & 0xFFFFFF) + size] = rom[src:src + size]
    cov = array.array('I')
    p = os.path.join(ROOT, 'out', 'evidence', 'ram_cov.u32')
    if os.path.exists(p):
        cov.frombytes(open(p, 'rb').read())
    seen, work, insns = set(), list(entries), {}
    lo, hi = base & 0xFFFFFF, (base & 0xFFFFFF) + size
    while work:
        a = work.pop() & 0xFFFFFF
        if a in seen or not (lo <= a < hi):
            continue
        seen.add(a)
        try:
            i = m68k.decode(mem, a)
        except m68k.Bad:
            continue
        insns[a] = i
        if i.flow in (m68k.SEQ, m68k.COND, m68k.CALL):
            work.append(a + i.size)
        if i.target is not None and i.flow in (m68k.BRANCH, m68k.COND, m68k.JUMP, m68k.CALL):
            work.append(i.target)
    targets = {i.target & 0xFFFFFF for i in insns.values() if i.target is not None}
    for a in sorted(insns):
        i = insns[a]
        if a in targets or a in [e & 0xFFFFFF for e in entries]:
            print('loc_%06X:' % a)
        ran = cov[a & 0xFFFF] if cov and (a & 0xFF0000) == 0xFF0000 else 1
        try:
            text = m68k.render(i, lambda t, h: ('loc_%06X' % (t & 0xFFFFFF)) if (t & 0xFFFFFF) in targets else '$%X' % t)
        except m68k.Bad:
            text = i.mnem
        print('\t%-40s ; %06X (ROM %06X)%s' % (text, a, a - lo + src, '' if ran else ' ;!'))


if __name__ == '__main__':
    main()

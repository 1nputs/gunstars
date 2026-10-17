#!/usr/bin/env python3
"""Code the game copies into work RAM, disassembled at the address it runs at: the copy's ROM
bytes placed at their RAM address, followed from an entry by flow (and from every address the
recorded runs executed there, out/evidence/ram_cov.u32, which reaches the code behind computed
jumps), with ;! where the runs did not execute. RAM_CODE lists the copies the port knows.

usage: ramcode.py ENTRY...                      (hex; in a RAM_CODE copy)
       ramcode.py RAM_START ROM_SOURCE SIZE ENTRY...
"""
import array, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import m68k  # noqa: E402
from program import ROM, ROOT  # noqa: E402

# (RAM address, ROM source, size, what)
RAM_CODE = [
    (0xFF2000, 0x062F7A, 0x3F0, "the Treasure logo's polygon fill (TreasureCopyFill)"),
]


def load_u(name, typecode, n):
    a = array.array(typecode)
    p = os.path.join(ROOT, 'out', 'evidence', name)
    if os.path.exists(p):
        a.frombytes(open(p, 'rb').read())
    return a if len(a) == n else array.array(typecode, bytes(n * a.itemsize))


def ram_cov():
    """executions per work RAM address"""
    return load_u('ram_cov.u32', 'I', 0x10000)


def ram_flags():
    """branch directions per work RAM address (4 taken, 8 fell through)"""
    return load_u('ram_flags.u8', 'B', 0x10000)


def copy_of(addr):
    for c in RAM_CODE:
        if c[0] <= (addr & 0xFFFFFF) < c[0] + c[2]:
            return c
    return None


def decode(base, src, size, entries):
    """{address: instruction} reached from entries (and the executed addresses) by flow"""
    rom = open(ROM, 'rb').read()
    mem = bytearray(0x1000000)
    mem[:len(rom)] = rom
    lo, hi = base & 0xFFFFFF, (base & 0xFFFFFF) + size
    mem[lo:hi] = rom[src:src + size]
    cov = ram_cov()
    work = [e & 0xFFFFFF for e in entries]
    work += [a for a in range(lo, hi) if (a & 0xFF0000) == 0xFF0000 and cov[a & 0xFFFF]]
    seen, insns = set(), {}
    while work:
        a = work.pop()
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
            work.append(i.target & 0xFFFFFF)
    return insns


def main():
    args = [int(x, 16) for x in sys.argv[1:]]
    if not args:
        sys.exit(__doc__)
    c = copy_of(args[0])
    if c and all(copy_of(x) == c for x in args):
        base, src, size, entries = c[0], c[1], c[2], args
    else:
        base, src, size, entries = args[0], args[1], args[2], args[3:] or [args[0]]
    insns = decode(base, src, size, entries)
    cov = ram_cov()
    lo = base & 0xFFFFFF
    targets = {i.target & 0xFFFFFF for i in insns.values() if i.target is not None}
    for a in sorted(insns):
        i = insns[a]
        if a in targets or a in [e & 0xFFFFFF for e in entries]:
            print('loc_%06X:' % a)
        ran = cov[a & 0xFFFF] if (a & 0xFF0000) == 0xFF0000 else 1
        try:
            text = m68k.render(i, lambda t, h: ('loc_%06X' % (t & 0xFFFFFF)) if (t & 0xFFFFFF) in targets else '$%X' % t)
        except m68k.Bad:
            text = i.mnem
        print('\t%-40s ; %06X (ROM %06X)%s' % (text, a, a - lo + src, '' if ran else ' ;!'))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""What the tests do not cover: for each ported routine, the instructions and the branch
directions the recorded runs took (out/evidence) that no test record runs.

A test record runs the C code of its routine and of every ported routine it reaches; FILE.cov
beside each capture (oracle_shim.c) says what each record ran on the 68k. An instruction counts
as tested when a record of a ported routine ran it. A direction the runs took and no record did
is a path no test checks: record a session that takes it (the CheckPause pause branch was one).
A ported routine in code the game copies to work RAM (tools/disasm/ramcode.py's RAM_CODE) is
checked against the runs' executions and branch directions there (ram_cov.u32, ram_flags.u8).

usage: capcov.py [NAME ...] [--all]    (the ported routines by default; --all lists covered ones too)
"""
import argparse, glob, gzip, os, re, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'disasm'))
from program import Program, ROOT  # noqa: E402
from m68k import COND  # noqa: E402
import ramcode  # noqa: E402


def ported():
    out = {}
    for fn in glob.glob(os.path.join(ROOT, 'src', '*.h')):
        for line in open(fn):
            m = re.search(r'\b([A-Za-z_]\w*)\s*\([^)]*\)\s*;\s*/\*\s*\$([0-9A-Fa-f]{6})', line)
            if m:
                out[int(m.group(2), 16)] = m.group(1)
    return out


def load_cov(ported_pcs):
    """address -> flags (1 ran, 4 taken, 8 fell) over the records of ported routines"""
    cov = {}
    for p in glob.glob(os.path.join(ROOT, 'out', 'captures', '*.cap.cov')):
        try:
            data = gzip.open(p).read()
        except EOFError:                       # a capture still being written
            print('(skipping %s: incomplete)' % os.path.basename(p))
            continue
        o = 0
        while o + 12 <= len(data):
            rec, pc, n = struct.unpack_from('<3I', data, o)
            o += 12
            if pc in ported_pcs:
                for (e,) in struct.iter_unpack('<I', data[o:o + 4 * n]):
                    a, f = e & 0xFFFFFF, e >> 24
                    cov[a] = cov.get(a, 0) | f
            o += 4 * n
    return cov


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('names', nargs='*')
    ap.add_argument('--all', action='store_true')
    a = ap.parse_args()
    prog = Program()
    flags = open(os.path.join(ROOT, 'out', 'evidence', 'rom_flags.u8'), 'rb').read()
    port = ported()
    cov = load_cov(set(port))
    names = a.names or [port[e] for e in sorted(port)]
    by_name = {v: k for k, v in port.items()}
    ram_cov, ram_flags = ramcode.ram_cov(), ramcode.ram_flags()
    gaps = 0
    for n in names:
        r = prog.by_name(n)
        c = ramcode.copy_of(by_name.get(n, 0))
        if r:
            body, insn = r.body, prog.insn
            ran, took = (lambda x: x in prog.executed), (lambda x: flags[x])
        elif c:
            insn = ramcode.decode(c[0], c[1], c[2], [by_name[n]])
            body = sorted(insn)
            ran, took = (lambda x: ram_cov[x & 0xFFFF] > 0), (lambda x: ram_flags[x & 0xFFFF])
        else:
            print('%s: not a routine' % n)
            continue
        missing = []
        for x in body:
            i = insn[x]
            if ran(x) and not cov.get(x, 0) & 1:
                missing.append('%06X %s not run' % (x, i.mnem))
                continue
            if i.flow == COND and ran(x):
                for bit, what in ((4, 'taken'), (8, 'not taken')):
                    if took(x) & bit and not cov.get(x, 0) & bit:
                        missing.append('%06X %s %s' % (x, i.mnem, what))
        # report the first instruction of each run of untested ones
        if missing:
            gaps += 1
            print('%-16s %d untested: %s' % (n, len(missing), '; '.join(missing[:6]) + (' ...' if len(missing) > 6 else '')))
        elif a.all:
            print('%-16s covered' % n)
    print('%d of %d routines have paths the runs took and no test record does' % (gaps, len(names)))
    return 1 if gaps else 0


if __name__ == '__main__':
    sys.exit(main())

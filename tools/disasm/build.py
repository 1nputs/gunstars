#!/usr/bin/env python3
"""Build the disassembly: Gunstar Heroes as vasm source that assembles to the ROM byte for byte.

Code is found from the oracle's evidence (out/evidence, tools/hybrid/evidence.py): every ROM
address the 68k executed is an instruction; from those the branches not taken in the recorded
runs are followed statically, as long as they decode and do not run into bytes the game read as
data. The rest is data. Names come from tools/disasm/names.txt (ADDRESS NAME, hex), the others are
made up: sub_XXXXX for routines (called, or reached through a jump table), loc_XXXXX for branch
targets, dat_XXXXX for data the code points at.

Writes build/disasm/: gunstar.s (the source), gunstar.bin (assembled, checked against the ROM),
symbols.json (name -> address), routines.json (each routine: its range, instructions, callees,
whether it ran), code.json (every instruction's address, the executed ones, the entries: what
program.py loads for the analyses), report.txt.

usage: build.py ROM [--evidence DIR] [--out DIR]
"""
import argparse, array, json, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(__file__))
import m68k  # noqa: E402
from m68k import Bad, SEQ, BRANCH, COND, CALL, JUMP, RET, STOP  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
VASM = os.path.join(ROOT, 'ref', 'vasm-src', 'vasm', 'vasmm68k_mot')
ROM_SIZE = 0x100000


# tables of routines the game reaches through data (not a jump table's pattern): every entry is
# code. ObjectRoutines: each object type's routine, by the type ($54C, 409 entries to $BB0)
CODE_TABLES = [(0x00054C, 409)]


def code_table_entries(rom):
    out = []
    for at, n in CODE_TABLES:
        for k in range(n):
            out.append(int.from_bytes(rom[at + 4 * k:at + 4 * k + 4], 'big'))
    return out


def load_u32(path, n):
    a = array.array('I')
    if os.path.exists(path):
        a.frombytes(open(path, 'rb').read())
    else:
        a.frombytes(bytes(4 * n))
    assert len(a) == n, path
    return a


def load_names(path):
    """ADDRESS NAME lines; a name starting with '.' is an inner label: a place a routine jumps to
    through a register inside itself (its own loop), not a routine of its own"""
    names = {}
    if os.path.exists(path):
        for line in open(path):
            line = line.split('#', 1)[0].split()
            if len(line) >= 2:
                names[int(line[0], 16)] = line[1]
    return names


class Disasm:
    def __init__(self, rom, ev):
        self.rom = rom
        self.cov = load_u32(os.path.join(ev, 'rom_cov.u32'), ROM_SIZE)
        p = os.path.join(ev, 'rom_flags.u8')
        self.flags = open(p, 'rb').read() if os.path.exists(p) else bytes(ROM_SIZE)
        self.reader = load_u32(os.path.join(ev, 'rom_reader.u32'), ROM_SIZE)
        self.edges = []
        p = os.path.join(ev, 'edges.txt')
        if os.path.exists(p):
            for line in open(p):
                s, t, k, c = line.split()
                self.edges.append((int(s, 16), int(t, 16), int(k), int(c)))
        # readers that swept the ROM do not make bytes data: the checksum, and a search with no
        # end mark that runs on through the ROM ($5F3A4, an object's): each read where 64 or more
        # executed instructions start (a data reader reads a handful at most)
        counts = {}
        for a, v in enumerate(self.reader):
            if v and self.cov[a]:
                counts[v] = counts.get(v, 0) + 1
        self.sweeps = {v for v, n in counts.items() if n >= 64}
        self.insn = {}                     # address -> Insn
        self.owner = bytearray(ROM_SIZE)   # 1: an executed instruction's byte, 2: a static one's
        self.raw = set()                   # instructions written as dc.w
        self.problems = []

    def is_data(self, a):
        return bool(self.flags[a] & 2) or (self.flags[a] & 1 and self.reader[a] and self.reader[a] not in self.sweeps)

    def take(self, a, executed, forced=False):
        try:
            i = m68k.decode(self.rom, a)
        except Bad as e:
            if executed:
                self.problems.append(f'{a:06X}: executed but does not decode ({e})')
            return None
        if a + i.size > ROM_SIZE:
            return None
        if not executed:
            for b in range(a, a + i.size):
                if self.owner[b] or (self.is_data(b) and not forced):
                    return None
        else:
            for b in range(a, a + i.size):
                if self.owner[b] == 1:
                    self.problems.append(f'{a:06X}: executed instruction overlaps another')
                    return None
        for b in range(a, a + i.size):
            self.owner[b] = 1 if executed else 2
        self.insn[a] = i
        if i.raw:
            self.raw.add(a)
        return i

    def discover(self):
        executed = [a for a in range(ROM_SIZE) if self.cov[a]]
        for a in executed:
            self.take(a, True)
        work = []
        for i in list(self.insn.values()):
            work.extend(self.successors(i))
        for s, t, k, c in self.edges:
            if t < ROM_SIZE:
                work.append(t)
        self.tables = {}
        # code a static call reaches is code even where something also reads it as data (a
        # command list copies ROM that holds routines too): from a call's target, the flow is
        # followed through such bytes; so are the code tables' entries
        forced = set(code_table_entries(self.rom))
        work.extend(forced)
        while True:
            while work:
                a = work.pop()
                if a >= ROM_SIZE or a & 1 or a in self.insn or self.owner[a]:
                    continue
                i = self.take(a, False, a in forced)
                if i:
                    nxt = self.successors(i)
                    if i.flow == CALL and i.target is not None:
                        forced.add(i.target)
                    if a in forced:
                        forced.update(t for t in nxt if t != i.target or i.flow != CALL)
                    work.extend(nxt)
            # jump tables the recorded runs used only in part: the rest of their entries too
            for a, i in list(self.insn.items()):
                if i.flow in (JUMP, CALL) and i.target is None and a not in self.tables:
                    t = self.table_targets(i)
                    if t is not None:
                        self.tables[a] = t
                        work.extend(t)
            if not work:
                break
        self.executed = set(executed)

    def table_targets(self, i):
        """the entries of the jump table i jumps or calls through (movea.l T(pc,Dn),An / jmp (An): long
        pointers; lea T(pc,Dn),An / adda.w (An),An / jmp (An): word offsets from each entry;
        jmp T(pc,Dn): a table of bra.w), or None. The table runs while its entries are code
        addresses (no data, nothing executed inside them)"""
        rom = self.rom
        def before(x):
            for n in (2, 4, 6):
                p = self.insn.get(x - n)
                if p and p.addr + p.size == x:
                    return p
            return None
        op = i.ops[0]
        if op[0] == 'pcx':
            base, kind = op[1], 'bra'
        elif op[0] == 'ind':
            p = before(i.addr)
            if p and p.mnem == 'movea.l' and p.ops[0][0] == 'pcx' and p.ops[1] == ('a', op[1]):
                base, kind = p.ops[0][1], 'long'
            elif p and p.mnem == 'adda.w' and p.ops == [('ind', op[1]), ('a', op[1])]:
                q = before(p.addr)
                if q and q.mnem == 'lea' and q.ops[0][0] == 'pcx' and q.ops[1] == ('a', op[1]):
                    base, kind = q.ops[0][1], 'word'
                else:
                    return None
            else:
                return None
        else:
            return None
        step = 2 if kind == 'word' else 4
        out = []
        for k in range(128):
            e = base + k * step
            if e + step > ROM_SIZE or (self.owner[e] == 1 and kind != 'bra') or (k and e in out):
                break
            if kind == 'long':
                t = int.from_bytes(rom[e:e + 4], 'big')
            elif kind == 'word':
                t = (e + m68k.s16(int.from_bytes(rom[e:e + 2], 'big'))) & 0xFFFFFF
            else:
                t = e
            if t >= ROM_SIZE or t & 1 or self.is_data(t) or (base <= t < e + step and kind != 'bra'):
                break
            try:
                d = m68k.decode(rom, t)
            except Bad:
                break
            if kind == 'bra' and not d.mnem.startswith('bra'):
                break
            out.append(t)
        return out or None

    def successors(self, i):
        out = []
        if i.flow in (SEQ, COND, CALL):
            out.append(i.addr + i.size)
        if i.target is not None and i.target < ROM_SIZE and i.flow in (BRANCH, COND, CALL, JUMP):
            out.append(i.target)
        return out

    def label(self, names):
        """names for every address something refers to"""
        lab = {}
        kind = {}
        entries = {0x200}
        for v in range(1, 64):
            t = int.from_bytes(self.rom[4 * v:4 * v + 4], 'big')
            if t < ROM_SIZE and t in self.insn:
                entries.add(t)
        for s, t, k, c in self.edges:
            if t < ROM_SIZE and (k in (1, 2, 4) or (k == 3 and s in self.insn and self.insn[s].target is None)):
                entries.add(t)
        for i in self.insn.values():
            if i.flow == CALL and i.target is not None and i.target < ROM_SIZE:
                entries.add(i.target)
        for a, ts in self.tables.items():
            entries.update(ts)
        entries.update(code_table_entries(self.rom))
        inner = {a for a, n in names.items() if n.startswith('.')}
        names = {a: n.lstrip('.') for a, n in names.items()}
        self.entries = {e for e in entries if e in self.insn and e not in inner}
        for e in self.entries:
            kind[e] = 'sub'
        for i in self.insn.values():
            for t, how in i.refs():
                if how == 'imm' or not (0 <= t < ROM_SIZE):
                    continue
                if t not in kind:
                    kind[t] = 'loc' if (how == 'target' or t in self.insn) else 'dat'
        for a, k in kind.items():
            lab[a] = names.get(a) or f'{k}_{a:05X}'
        for a, n in names.items():
            if a < ROM_SIZE:
                lab[a] = n
        self.lab = lab
        self.ram_names = {a: n for a, n in names.items() if a >= 0xFF0000}

    def name_of(self, addr, how):
        if how == 'imm':
            return self.lab.get(addr) if addr in self.entries else None
        if addr < ROM_SIZE:
            return self.lab.get(addr)
        a = addr & 0xFFFFFF
        if a >= 0xFF0000 and a in self.ram_names:
            return self.ram_names[a]
        return None

    def emit(self, path):
        """write the source; returns the line -> address map"""
        lines = ['; Gunstar Heroes (U), disassembled by tools/disasm/build.py from the ROM and the oracle\'s',
                 '; evidence. Generated: do not edit (names: tools/disasm/names.txt).',
                 '; ";!" marks code no recorded run executed (found by following branches).', '']
        where = [None] * len(lines)
        # labels that do not fall on an item boundary become equates
        starts = set(self.insn)
        inside = set()
        for a, i in self.insn.items():
            for b in range(a + 1, a + i.size):
                inside.add(b)
        equ = sorted(a for a in self.lab if a in inside)
        for a in equ:
            lines.append(f'{self.lab[a]} equ ${a:X}'); where.append(None)
        for a, n in sorted(self.ram_names.items()):
            lines.append(f'{n} equ ${0xFF000000 | a:08X}'); where.append(None)
        lines.append('\torg 0'); where.append(None)
        a = 0
        rom = self.rom
        while a < ROM_SIZE:
            if a in self.lab and a not in inside:
                if a in self.entries:
                    lines.append(''); where.append(None)
                lines.append(f'{self.lab[a]}:'); where.append(a)
            i = self.insn.get(a)
            if i:
                text = None
                if a not in self.raw:
                    try:
                        text = m68k.render(i, self.name_of)
                    except Bad:
                        self.raw.add(a)
                if text is None:
                    ws = ','.join('$%04X' % (rom[b] << 8 | rom[b + 1]) for b in range(a, a + i.size, 2))
                    try:
                        shown = m68k.render(i, lambda t, h: '$%X' % t)
                    except Bad:
                        shown = i.mnem
                    text = f'dc.w {ws}\t; {shown}'
                mark = '' if a in self.executed else '\t;!'
                lines.append(f'\t{text}{mark}'); where.append(a)
                a += i.size
                continue
            b = a + 1
            while b < ROM_SIZE and b - a < 16 and b not in self.insn and not (b in self.lab and b not in inside):
                b += 1
            lines.append('\tdc.b ' + ','.join('$%02X' % rom[x] for x in range(a, b))); where.append(a)
            a = b
        open(path, 'w').write('\n'.join(lines) + '\n')
        return where

    def routines(self):
        """each entry's instructions by flow inside it (a branch to another entry is a tail call)"""
        out = {}
        dyn = {}
        for s, t, k, c in self.edges:
            dyn.setdefault(s, set()).add(t)
        for e in sorted(self.entries):
            seen, work = set(), [e]
            calls, tails = set(), set()
            while work:
                a = work.pop()
                if a in seen or a not in self.insn:
                    continue
                if a != e and a in self.entries:
                    tails.add(a)
                    continue
                seen.add(a)
                i = self.insn[a]
                if i.flow == CALL:
                    if i.target is not None:
                        calls.add(i.target)
                    calls.update(t for t in dyn.get(a, ()) if t < 0x1000000)
                if i.flow == JUMP and i.target is None:
                    tails.update(t for t in dyn.get(a, ()))
                for s in self.successors(i):
                    if i.flow == CALL and s == i.target:
                        continue
                    work.append(s)
            out[self.lab[e]] = {
                'addr': e, 'end': max(seen) + self.insn[max(seen)].size if seen else e,
                'insns': len(seen), 'ran': sum(1 for a in seen if a in self.executed),
                'calls': sorted(self.lab.get(c, '%06X' % c) for c in calls),
                'tails': sorted(self.lab.get(c, '%06X' % c) for c in tails),
            }
        return out


def assemble(src, binp):
    p = subprocess.run([VASM, '-Fbin', '-no-opt', '-m68000', '-quiet', '-o', binp, src],
                       stdin=subprocess.DEVNULL, capture_output=True, text=True)
    errs = [int(m.group(1)) for m in re.finditer(r'in line (\d+) of', p.stdout + p.stderr)]
    return p.returncode, errs, (p.stdout + p.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--evidence', default=os.path.join(ROOT, 'out', 'evidence'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'build', 'disasm'))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    rom = open(a.rom, 'rb').read()
    d = Disasm(rom, a.evidence)
    d.discover()
    d.label(load_names(os.path.join(os.path.dirname(__file__), 'names.txt')))
    src, binp = os.path.join(a.out, 'gunstar.s'), os.path.join(a.out, 'gunstar.bin')
    for attempt in range(20):
        where = d.emit(src)
        rc, errs, msg = assemble(src, binp)
        bad = set()
        for ln in errs:
            if 0 < ln <= len(where) and where[ln - 1] is not None and where[ln - 1] in d.insn:
                bad.add(where[ln - 1])
        if rc and not bad:
            sys.exit('vasm failed:\n' + msg[:3000])
        if rc == 0:
            out = open(binp, 'rb').read()
            if out == rom:
                break
            diff = [x for x in range(min(len(out), len(rom))) if out[x] != rom[x]]
            if len(out) != len(rom):
                print(f'size {len(out)} against {len(rom)}')
            starts = sorted(d.insn)
            import bisect
            for x in diff:
                k = bisect.bisect_right(starts, x) - 1
                if k >= 0 and starts[k] + d.insn[starts[k]].size > x:
                    bad.add(starts[k])
            if not bad:
                sys.exit(f'{len(diff)} bytes differ outside instructions, first at {diff[0]:06X}')
        new = bad - d.raw
        if not new:
            sys.exit('no progress: ' + msg[:2000])
        d.raw |= new
        print(f'pass {attempt}: {len(new)} instructions written as dc.w')
    else:
        sys.exit('not byte-exact after 20 passes')

    syms = {n: x for x, n in d.lab.items()}
    json.dump(dict(sorted(syms.items(), key=lambda kv: kv[1])), open(os.path.join(a.out, 'symbols.json'), 'w'), indent=0)
    routines = d.routines()
    json.dump(routines, open(os.path.join(a.out, 'routines.json'), 'w'), indent=1)
    json.dump({'insns': sorted(d.insn), 'executed': sorted(d.executed & set(d.insn)), 'entries': sorted(d.entries)},
              open(os.path.join(a.out, 'code.json'), 'w'))
    n_exec = len(d.executed)
    n_static = len(d.insn) - sum(1 for x in d.insn if x in d.executed)
    code_bytes = sum(i.size for i in d.insn.values())
    rep = [f'byte-exact: build/disasm/gunstar.bin == ROM',
           f'instructions: {len(d.insn)} ({n_exec} executed, {n_static} found by following branches), {code_bytes} bytes of code',
           f'routines: {len(d.entries)} ({sum(1 for r in routines.values() if r["ran"])} ran)',
           f'written as dc.w: {len(d.raw)}',
           f'problems: {len(d.problems)}'] + d.problems[:50]
    open(os.path.join(a.out, 'report.txt'), 'w').write('\n'.join(rep) + '\n')
    print('\n'.join(rep[:5]))


if __name__ == '__main__':
    main()

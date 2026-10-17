"""The disassembly as data, for the analyses (regs.py) and the drafts (draft.py).

Program(rom) loads build/disasm/code.json and symbols.json (build.py writes them), decodes every
instruction again from the ROM, and reads the recorded runs' dynamic edges (out/evidence/edges.txt)
for the calls and jumps through registers. A routine is what build.py calls one: an entry and the
instructions reached from it by flow, stopping at other entries (a branch, a jump or a fall into
another entry is a tail call).
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import m68k  # noqa: E402
from m68k import SEQ, BRANCH, COND, CALL, JUMP, RET, STOP  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ROM = os.path.join(ROOT, 'Gunstar Heroes (U) [!].gen')
ROM_SIZE = 0x100000


class Routine:
    __slots__ = ('entry', 'name', 'body', 'succ', 'calls', 'tails', 'table')

    def __init__(self, entry, name):
        self.entry, self.name = entry, name
        self.body = []          # instruction addresses, ascending
        self.succ = {}          # address -> successors inside the routine
        self.calls = {}         # call site -> callee entries
        self.tails = {}         # site -> entries it continues into (branch, jump, fall-in, table)
        self.table = {}         # jump site -> [(case offset, target)] for jump tables


class Program:
    def __init__(self, rom_path=ROM, disasm=os.path.join(ROOT, 'build', 'disasm'),
                 evidence=os.path.join(ROOT, 'out', 'evidence')):
        self.rom = open(rom_path, 'rb').read()
        code = json.load(open(os.path.join(disasm, 'code.json')))
        self.insn = {a: m68k.decode(self.rom, a) for a in code['insns']}
        self.executed = set(code['executed'])
        self.entries = set(code['entries'])
        syms = json.load(open(os.path.join(disasm, 'symbols.json')))
        self.name = {}
        for n, a in syms.items():
            self.name.setdefault(a, n)
        self.addr = syms
        names = os.path.join(os.path.dirname(__file__), 'names.txt')
        for line in open(names) if os.path.exists(names) else ():
            w = line.split('#', 1)[0].split()
            if len(w) >= 2 and int(w[0], 16) >= 0xFF0000:
                self.name[int(w[0], 16)] = w[1]
                self.addr[w[1]] = int(w[0], 16)
        self.dyn = {}           # call or jump site -> targets the recorded runs took
        p = os.path.join(evidence, 'edges.txt')
        if os.path.exists(p):
            for line in open(p):
                s, t, k, c = line.split()
                if int(k) in (1, 3):
                    self.dyn.setdefault(int(s, 16), set()).add(int(t, 16))
        # which way the recorded runs took each conditional branch (rom_flags.u8: 4 taken, 8 fell)
        self.never = set()      # (branch, successor) edges no recorded run followed
        p = os.path.join(evidence, 'rom_flags.u8')
        flags = open(p, 'rb').read() if os.path.exists(p) else bytes(ROM_SIZE)
        for a in self.executed:
            i = self.insn.get(a)
            if i is None or i.flow != COND or not flags[a] & 12:
                continue
            if not flags[a] & 4:
                self.never.add((a, i.target))
            if not flags[a] & 8:
                self.never.add((a, a + i.size))
        self.routines = {}
        for e in sorted(self.entries):
            self.routines[e] = self._routine(e)

    def label(self, a):
        return self.name.get(a) or ('loc_%05X' % a)

    def ram_name(self, a):
        """a RAM variable's name (a: 68k address or RAM offset), or None"""
        return self.name.get(0xFF0000 | (a & 0xFFFF))

    def jump_table(self, site):
        """the cases of a jump through a table just before site: [(offset, target)], or None.
        movea.l T(pc,Dn.w),An / jmp (An): long pointers; lea T(pc,Dn.w),An / adda.w (An),An /
        jmp (An): word offsets from each entry; jmp T(pc,Dn.w): a table of bra.w"""
        i = self.insn[site]
        if i.mnem not in ('jmp', 'jsr'):
            return None
        prev = [self.insn.get(site - n) for n in (2, 4, 6)]
        if i.ops[0][0] == 'pcx':
            base, kind = i.ops[0][1], 'bra'
        elif i.ops[0][0] == 'ind':
            an = i.ops[0][1]
            p1 = next((p for p in prev if p and p.addr + p.size == site), None)
            if p1 and p1.mnem == 'movea.l' and p1.ops[0][0] == 'pcx' and p1.ops[1] == ('a', an):
                base, kind = p1.ops[0][1], 'long'
            elif p1 and p1.mnem == 'adda.w' and p1.ops == [('ind', an), ('a', an)]:
                p2 = next((p for p in [self.insn.get(p1.addr - n) for n in (2, 4, 6)] if p and p.addr + p.size == p1.addr), None)
                if p2 and p2.mnem == 'lea' and p2.ops[0][0] == 'pcx' and p2.ops[1] == ('a', an):
                    base, kind = p2.ops[0][1], 'word'
                else:
                    return None
            else:
                return None
        else:
            return None
        seen = self.dyn.get(site, set())
        step = {'long': 4, 'word': 2, 'bra': 4}[kind]
        cases, off = [], 0
        while True:
            a = base + off
            if off and (a in self.entries or a in self.name) and kind != 'bra':
                break                       # another label: the table ended
            if kind == 'long':
                t = int.from_bytes(self.rom[a:a + 4], 'big') & 0xFFFFFF
            elif kind == 'word':
                t = (a + m68k.s16(int.from_bytes(self.rom[a:a + 2], 'big'))) & 0xFFFFFF
            else:
                b = self.insn.get(a)
                t = b.target if b is not None and b.mnem.startswith('bra') else None
                if t is None and b is not None and b.mnem == 'jmp':
                    t = b.target
                if t is None:
                    break
            if t not in self.insn or (kind != 'bra' and t not in self.entries and t not in seen):
                break
            cases.append((off, t if kind != 'bra' else a))
            off += step
            if len(cases) >= 256:
                break
        if not cases or (seen and not seen <= {t for _, t in cases} | {base + o for o, _ in cases}):
            return None
        return cases

    def successors(self, i):
        out = []
        if i.flow in (SEQ, COND, CALL):
            out.append(i.addr + i.size)
        if i.target is not None and i.flow in (BRANCH, COND, JUMP):
            out.append(i.target)
        return out

    def _routine(self, e):
        r = Routine(e, self.label(e))
        seen, work = set(), [e]
        while work:
            a = work.pop()
            if a in seen:
                continue
            seen.add(a)
            i = self.insn[a]
            succ = []
            if i.flow == CALL:
                tg = {i.target} if i.target is not None else set(self.dyn.get(a, ()))
                if i.target is None:
                    cases = self.jump_table(a)
                    if cases:
                        r.table[a] = cases
                        tg |= {t for _, t in cases}
                r.calls[a] = sorted(t for t in tg if t in self.entries)
            nxt = self.successors(i)
            if i.flow == JUMP and i.target is None:
                cases = self.jump_table(a)
                if cases:
                    r.table[a] = cases
                    nxt = [t for _, t in cases]
                else:
                    r.tails[a] = sorted(t for t in self.dyn.get(a, ()) if t in self.entries)
                    nxt = []
            for s in nxt:
                if s not in self.insn:
                    continue
                if s in self.entries and s != e:
                    r.tails.setdefault(a, []).append(s)
                else:
                    succ.append(s)
                    work.append(s)
            r.succ[a] = succ
        r.body = sorted(seen)
        return r

    def run_succ(self, r):
        """r's successors without the edges no recorded run followed (unexecuted code keeps all)"""
        return {a: [s for s in ss if (a, s) not in self.never] for a, ss in r.succ.items()}

    def by_name(self, name):
        if name in self.addr and self.addr[name] in self.routines:
            return self.routines[self.addr[name]]
        try:
            a = int(name.replace('sub_', '').replace('$', ''), 16)
        except ValueError:
            return None
        return self.routines.get(a)

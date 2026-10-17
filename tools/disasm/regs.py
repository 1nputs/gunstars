#!/usr/bin/env python3
"""Each routine's registers: what it takes, what it returns, what it leaves alone.

A register analysis over the whole call graph (program.py's routines):
  * the stack inside each routine is simulated, so saving a register (move.l d0,-(sp), movem)
    and restoring it later is a copy through a stack slot, not a use;
  * may-define and must-define: the registers a routine (and its callees) can change, and the
    ones it changes on every path; a register saved and restored unchanged is neither;
  * liveness, backwards through each routine, a call reading its callee's parameters and keeping
    whatever the callee does not always define: the parameters are the registers a routine reads
    before writing (and the ones it changes only on some paths that its callers read after: in
    and out), the results are the registers it can change that a caller reads after the call.
Data registers are tracked in three parts (bits 0-7, 8-15, 16-31), so a routine that reads only
d0's low word takes a word. The condition codes and X are registers too: a routine whose callers
branch on its flags returns a condition.

usage: regs.py [NAME ...]        (writes build/disasm/signatures.json; prints the named ones)
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from program import Program, ROOT  # noqa: E402
from m68k import CALL, RET  # noqa: E402

# register sets as bit masks: d0-d7 three parts each, a0-a6, the condition codes, X; stack slots
# above that, numbered per routine
PART = {'b': 1, 'w': 3, 'l': 7}
CC, XF = 1 << 31, 1 << 32
SLOT0 = 33
DREGS = (1 << 24) - 1
AREGS = 0x7F << 24
ALL = DREGS | AREGS | CC | XF


def D(n, size='l'):
    return PART[size] << (3 * n)


def A(n):
    return 0 if n == 7 else 1 << (24 + n)


def X(xreg, xlong):
    return D(xreg, 'l' if xlong else 'w') if xreg < 8 else A(xreg - 8)


def bit_part(n):
    return 1 if n < 8 else 2 if n < 16 else 4


def reg_mask(mask, predec):
    """a movem list as d0..a7 in bits 0-15"""
    if predec:
        mask = int('{:016b}'.format(mask)[::-1], 2)
    return mask


def list_regs(mask):
    return [r for r in range(16) if mask >> r & 1]


def R(r, size='l'):
    """register r of a movem list: 0-7 data, 8-15 address"""
    return D(r, size) if r < 8 else A(r - 8)


def addr_use(o):
    k = o[0]
    if k in ('ind', 'inc', 'dec', 'disp'):
        return A(o[1])
    if k == 'idx':
        return A(o[1]) | X(o[3], o[4])
    if k == 'pcx':
        return X(o[2], o[3])
    return 0


def read(o, size):
    k = o[0]
    if k == 'd':
        return D(o[1], size)
    if k == 'a':
        return A(o[1])
    if k in ('sr', 'ccr'):
        return CC | XF
    return addr_use(o)


def write(o, size):
    """(use, def) of writing operand o"""
    k = o[0]
    if k == 'd':
        return 0, D(o[1], size)
    if k == 'a':
        return 0, A(o[1])
    if k in ('sr', 'ccr'):
        return 0, CC | XF
    if k in ('inc', 'dec'):
        return A(o[1]), A(o[1])
    return addr_use(o), 0


def side(o):
    """the address register an (An)+ or -(An) source changes"""
    return A(o[1]) if o[0] in ('inc', 'dec') else 0


LOW = ('move', 'add', 'sub', 'and', 'or', 'eor', 'addi', 'subi', 'andi', 'ori', 'eori', 'addq', 'subq',
       'neg', 'not', 'negx', 'addx', 'subx', 'asl', 'lsl', 'ext')
MEMORY = ('ind', 'inc', 'dec', 'disp', 'idx', 'absw', 'absl', 'pcd', 'pcx', 'sr', 'ccr', 'usp', 'list')


def usedef(i):
    """(use, def) of one instruction, stack slots aside; calls, jumps and returns add their own"""
    u, d = _usedef(i)
    return u, d


def _usedef(i):
    m = i.mnem
    base, _, sz = m.partition('.')
    ops = i.ops
    if base in ('move', 'movea'):
        s, d = ops
        if s[0] == 'usp' or d[0] == 'usp':
            return 0, 0
        if s[0] == 'sr':                                  # saving the interrupt mask
            return write(d, sz)[0], write(d, sz)[1]
        u = read(s, sz) if s[0] not in ('sr', 'ccr') else 0
        wu, wd = write(d, sz)
        f = CC if base == 'move' and d[0] not in ('a', 'sr', 'ccr') else 0
        return u | wu, wd | side(s) | f
    if base == 'moveq':
        return 0, D(ops[1][1]) | CC
    if base == 'clr':
        wu, wd = write(ops[0], sz)
        return wu, wd | CC
    if base == 'lea':
        return addr_use(ops[0]), A(ops[1][1])
    if base == 'pea':
        return addr_use(ops[0]), 0
    if base in ('add', 'sub', 'and', 'or', 'eor', 'addi', 'subi', 'andi', 'ori', 'eori'):
        s, d = ops
        if d[0] == 'sr':                                  # the interrupt mask: the flags stay
            return 0, 0
        if d[0] == 'ccr':                                 # setting or clearing a returned flag
            return 0, CC | XF
        wu, wd = write(d, sz)
        f = CC | (XF if base in ('add', 'sub', 'addi', 'subi') else 0)
        return read(s, sz) | read(d, sz) | wu, wd | side(s) | f
    if base in ('addq', 'subq'):
        d = ops[1]
        if d[0] == 'a':
            return A(d[1]), A(d[1])
        wu, wd = write(d, sz)
        return read(d, sz) | wu, wd | CC | XF
    if base in ('adda', 'suba'):
        return read(ops[0], sz) | A(ops[1][1]), A(ops[1][1]) | side(ops[0])
    if base in ('cmp', 'cmpi', 'cmpa', 'cmpm'):
        return read(ops[0], sz) | read(ops[1], sz), CC | side(ops[0]) | side(ops[1])
    if base in ('tst', 'chk'):
        return read(ops[-1], sz or 'w') | (read(ops[0], 'w') if base == 'chk' else 0), CC | side(ops[-1])
    if base in ('btst', 'bset', 'bclr', 'bchg'):
        b, d = ops
        u = D(b[1], 'b') if b[0] == 'd' else 0
        if d[0] == 'd':
            part = (bit_part(b[1]) if b[0] == 'q' else 7) << (3 * d[1])
            return u | part, CC | (part if base != 'btst' else 0)
        wu, wd = write(d, 'b')
        return u | read(d, 'b') | wu, CC | (wd if base != 'btst' else side(d))
    if base == 'ext':
        n = ops[0][1]
        return (D(n, 'b') if sz == 'w' else D(n, 'w')), D(n, sz) | CC
    if base == 'swap':
        n = ops[0][1]
        return D(n), D(n) | CC
    if base in ('neg', 'not', 'negx', 'nbcd', 'tas'):
        wu, wd = write(ops[0], sz or 'b')
        f = CC | (XF if base in ('neg', 'negx', 'nbcd') else 0)
        u = read(ops[0], sz or 'b') | wu | (CC | XF if base in ('negx', 'nbcd') else 0)
        return u, wd | f
    if base in ('addx', 'subx', 'abcd', 'sbcd'):
        s, d = ops
        size = sz or 'b'
        wu, wd = write(d, size)
        return read(s, size) | read(d, size) | wu | CC | XF, wd | side(s) | CC | XF
    if base[:2] in ('as', 'ls', 'ro') and base[-1] in 'lr' and len(base) <= 4:
        f = CC | (XF if not base.startswith('ro') or base.startswith('rox') else 0)
        xu = XF if base.startswith('rox') else 0
        if len(ops) == 1:
            wu, wd = write(ops[0], 'w')
            return read(ops[0], 'w') | wu | xu, wd | f
        c, d = ops
        u = D(c[1], 'b') if c[0] == 'd' else 0
        return u | D(d[1], sz) | xu, D(d[1], sz) | f
    if base in ('muls', 'mulu'):
        n = ops[1][1]
        return read(ops[0], 'w') | D(n, 'w'), D(n) | CC | side(ops[0])
    if base in ('divs', 'divu'):
        n = ops[1][1]
        return read(ops[0], 'w') | D(n), D(n) | CC | side(ops[0])
    if base == 'exg':
        a, b = ops
        ra = D(a[1]) if a[0] == 'd' else A(a[1])
        rb = D(b[1]) if b[0] == 'd' else A(b[1])
        return ra | rb, ra | rb
    if base == 'movem':
        s, d = ops
        if s[0] == 'list':
            regs = list_regs(reg_mask(s[1], s[2]))
            u = 0
            for r in regs:
                u |= R(r, sz)
            wu, wd = write(d, sz)
            return u | wu, wd
        regs = list_regs(reg_mask(d[1], d[2]))
        df = 0
        for r in regs:
            df |= R(r)
        return addr_use(s), df | side(s)
    if m.startswith('db'):
        n = ops[0][1]
        return D(n, 'w') | (CC if m != 'dbf' else 0), D(n, 'w')
    if m[0] == 's' and base[1:] in ('t', 'f', 'hi', 'ls', 'cc', 'cs', 'ne', 'eq', 'vc', 'vs', 'pl', 'mi', 'ge', 'lt', 'gt', 'le'):
        wu, wd = write(ops[0], 'b')
        return wu | (CC if base not in ('st', 'sf') else 0), wd
    if m[0] == 'b' and base not in ('bra', 'bsr') and len(base) <= 3:
        return CC, 0
    if base in ('jsr', 'jmp'):
        return addr_use(ops[0]), 0
    if base == 'rtr':
        return 0, CC | XF
    return 0, 0


SIZE = {'b': 1, 'w': 2, 'l': 4}
SLOTS = ~ALL


def value_uses(i, u):
    """for an operation whose result's low parts depend only on its operands' parts at or below
    them (move, add, and, a left shift ...) into a data register: (n, size, the uses that are
    such operands); else None"""
    base, _, sz = i.mnem.partition('.')
    if base not in LOW or not i.ops or i.ops[-1][0] != 'd':
        return None
    n = i.ops[-1][1]
    if base == 'ext':
        return n, sz, D(n, 'b' if sz == 'w' else 'w')
    if base in ('asl', 'lsl'):
        return n, sz, D(n, sz)
    v = D(n, sz) if base != 'move' else 0
    if len(i.ops) == 2 and i.ops[0][0] == 'd':
        v |= D(i.ops[0][1], sz)
    return n, sz, v & u


def is_pure(i):
    """a register operation with no effect but its registers (lea computes an address only)"""
    if i.flow != 'seq':
        return False
    base = i.mnem.partition('.')[0]
    if base in ('pea', 'movem', 'tas', 'nop', 'stop', 'reset', 'trap', 'illegal', 'chk', 'link', 'unlk'):
        return False
    for k, o in enumerate(i.ops):
        if o[0] in MEMORY and not (base == 'lea' and k == 0):
            return False
    return True


def trim(u, need):
    """data register parts in u cut to the parts in need (a part mask 0-7)"""
    out = u & ~DREGS
    for n in range(8):
        p = (u >> (3 * n)) & 7 & need
        out |= p << (3 * n)
    return out


class Facts:
    """one routine's per-instruction facts for the analyses: uses, defs, copies through stack
    slots (a save and its restore), the stack depth"""

    def __init__(self, prog, r):
        self.r = r
        self.notes = []
        self.depth = self._depth(prog, r)
        self.slots = {}             # (offset, size) -> slot bit
        self.ud = {}                # address -> (uses, defs, value uses (n, size, mask) or None, pure, copies)
        self.exits = []
        pushes, pops = {}, {}
        for a in r.body:
            i = prog.insn[a]
            u, d = usedef(i)
            self.ud[a] = [u, d, value_uses(i, u), is_pure(i), []]
            for key, reg in self._pushes(i):
                pushes.setdefault(key, []).append((a, reg))
            for key, reg in self._pops(i):
                pops.setdefault(key, []).append((a, reg))
            self._frame(i)
            if i.flow == RET:
                dp = self.depth.get(a)
                self.exits.append(a)
                if dp not in (0, None):
                    self.notes.append('%06X: %s with the stack %+d (returns past its caller)' % (a, i.mnem, dp))
        # a push whose slot some pop reads is a copy into the slot; that pop a copy out of it
        for key, ps in pushes.items():
            if key not in pops:
                continue
            bit = self.slot(key)
            for a, reg in ps:
                ent = self.ud[a]
                ent[0] &= ~reg
                ent[1] |= bit
                ent[4].append((reg, bit))
            for a, reg in pops[key]:
                ent = self.ud[a]
                ent[4].append((bit, reg))

    def slot(self, key):
        if key not in self.slots:
            self.slots[key] = 1 << (SLOT0 + len(self.slots))
        return self.slots[key]

    def _pushes(self, i):
        dp = self.depth.get(i.addr)
        if dp is None or not i.ops or i.ops[-1] != ('dec', 7):
            return []
        base, _, sz = i.mnem.partition('.')
        if base == 'movem':
            regs = list_regs(reg_mask(i.ops[0][1], True))
            n = SIZE[sz]
            off = dp - n * len(regs)
            return [((off + n * k, sz), R(rg, sz)) for k, rg in enumerate(regs)]
        if base == 'move':
            o = i.ops[0]
            n = 2 if sz == 'b' else SIZE[sz]
            if o[0] == 'd':
                return [((dp - n, sz), D(o[1], sz))]
            if o[0] == 'a':
                return [((dp - n, sz), A(o[1]))]
            if o[0] == 'sr':
                return [((dp - n, sz), CC | XF)]
        return []

    def _pops(self, i):
        dp = self.depth.get(i.addr)
        if dp is None or not i.ops or i.ops[0] != ('inc', 7):
            return []
        base, _, sz = i.mnem.partition('.')
        if base == 'movem':
            regs = list_regs(reg_mask(i.ops[1][1], False))
            n = SIZE[sz]
            return [((dp + n * k, sz), R(rg)) for k, rg in enumerate(regs)]
        if base in ('move', 'movea'):
            o = i.ops[1]
            n = 2 if sz == 'b' else SIZE[sz]
            if o[0] == 'd':
                return [((dp, sz), D(o[1], sz))]
            if o[0] == 'a':
                return [((dp, sz), A(o[1]))]
            if o[0] in ('sr', 'ccr'):
                return [((dp, sz), CC | XF)]
        return []

    def _frame(self, i):
        """note reads and writes of the caller's part of the stack (arguments, the return address)"""
        dp = self.depth.get(i.addr)
        if dp is None:
            return
        for o in i.ops:
            if o[0] in ('disp', 'ind', 'idx') and o[1] == 7:
                off = dp + (o[2] if o[0] in ('disp', 'idx') else 0)
                if off >= 0:
                    self.notes.append('%06X: reaches its caller\'s stack (%+d)' % (i.addr, off))

    def _depth(self, prog, r):
        depth = {r.entry: 0}
        work = [r.entry]
        while work:
            a = work.pop()
            i = prog.insn[a]
            d = depth[a]
            ch = sp_change(i)
            nd = None if d is None or ch is None else d + ch
            if d is not None and ch is None:
                self.notes.append('%06X: the stack pointer is set (%s)' % (a, i.mnem))
            for s in r.succ.get(a, []):
                if s not in depth:
                    depth[s] = nd
                    work.append(s)
                elif depth[s] != nd and depth[s] is not None:
                    self.notes.append('%06X: stack depth %s or %s' % (s, depth[s], nd))
                    depth[s] = None
                    work.append(s)
        return depth


def sp_change(i):
    base, _, sz = i.mnem.partition('.')
    ops = i.ops
    n = 2 if sz == 'b' else SIZE.get(sz, 0)
    if base == 'movem':
        cnt = len(list_regs(reg_mask(ops[0][1], ops[0][2]) if ops[0][0] == 'list' else reg_mask(ops[1][1], False)))
        if ops[1] == ('dec', 7):
            return -cnt * n
        if ops[0] == ('inc', 7):
            return cnt * n
        return 0
    if base == 'pea':
        return -4
    if base == 'lea' and ops[1] == ('a', 7):
        return ops[0][2] if ops[0][0] == 'disp' and ops[0][1] == 7 else None
    if len(ops) == 2 and ops[1] == ('a', 7):
        if base in ('addq', 'adda', 'addi') and ops[0][0] in ('q', 'imm'):
            return ops[0][1]
        if base in ('subq', 'suba', 'subi') and ops[0][0] in ('q', 'imm'):
            return -ops[0][1]
        return None
    c = 0
    for o in ops:
        if o == ('dec', 7):
            c -= n
        elif o == ('inc', 7):
            c += n
    return c


def whole(d):
    """a set with every data register it touches made whole"""
    out = d & ~DREGS
    for n in range(8):
        if (d >> (3 * n)) & 7:
            out |= 7 << (3 * n)
    return out


def transfer(ent, out):
    """live before an instruction, from live after it. A write to part of a data register ends
    the liveness of all of it: what the rest held before is a stale value the routine does not
    mean to take (a word loaded into a register and then shifted as a long), not an argument;
    the tests catch the rare routine that does mean it"""
    u, d, val, pure, copies = ent
    li = out & ~whole(d)
    if pure and not (d & out):
        return li
    if val is not None:
        n, sz, v = val
        if out & d & CC:
            need = PART[sz]
        else:
            lp = (out >> (3 * n)) & PART[sz]
            need = 7 if lp & 4 else 3 if lp & 2 else 1 if lp & 1 else 0
        u = (u & ~v) | trim(v, need & PART[sz])
    for src, dst in copies:
        if out & dst:
            li |= src
    return li | u


class Analysis:
    def __init__(self, prog):
        self.p = prog
        self.facts = {e: Facts(prog, r) for e, r in prog.routines.items()}
        # the flow the analyses follow: the recorded runs' (a branch they never took is left out,
        # so a path the game never runs does not make a routine take a register)
        self.succ = {e: prog.run_succ(r) for e, r in prog.routines.items()}
        self.preds = {}
        for e, r in prog.routines.items():
            pr = {a: [] for a in r.body}
            for a, ss in self.succ[e].items():
                for s in ss:
                    pr[s].append(a)
            self.preds[e] = pr
        self.may = {e: 0 for e in prog.routines}
        self.must = {e: 0 for e in prog.routines}
        self.params = {e: 0 for e in prog.routines}
        self.exit = {e: 0 for e in prog.routines}
        self.full_in = {}
        self.live_after = {}
        self.live_out_at = {}
        self.unknown_calls = {}
        for e, r in prog.routines.items():
            for s, ts in r.calls.items():
                if not ts:
                    self.unknown_calls.setdefault(e, []).append(s)

    # ---- may-define and must-define ------------------------------------------------------------
    def defs(self):
        rounds = 0
        while True:
            rounds += 1
            changed = False
            for e in self.p.routines:
                may, must = self._defs(e)
                if may != self.may[e] or must != self.must[e]:
                    self.may[e], self.must[e] = may, must
                    changed = True
            if not changed:
                return rounds

    def _defs(self, e):
        """forward: the registers changed since entry (may: on some path, must: on every path);
        a slot holds a register as it was at entry when pushed before any change, and a copy
        back from it undoes the change"""
        r = self.p.routines[e]
        f = self.facts[e]
        state = {r.entry: (0, 0, 0)}        # (may, must, slots holding an entry value)
        work = [r.entry]
        may_out, must_out = 0, None
        while work:
            a = work.pop()
            may, must, clean = state[a]
            i = self.p.insn[a]
            u, d, val, pure, copies = f.ud[a]
            back = 0
            for src, dst in copies:
                if src & SLOTS:                              # a restore
                    if src & clean:
                        back |= dst
                elif not (src & may):                        # a save of an unchanged register
                    clean |= dst
                else:
                    clean &= ~dst
            dd = (d & ALL) & ~back
            may = (may | dd) & ~back
            must = (must | dd) & ~back
            if a in r.calls:
                ts = r.calls[a]
                if ts:
                    cm, cs = 0, ALL
                    for t in ts:
                        cm |= self.may[t]
                        cs &= self.must[t]
                    may |= cm
                    must |= cs
                else:
                    may |= ALL
            for t in r.tails.get(a, []):
                may_out |= may | self.may[t]
                m2 = must | self.must[t]
                must_out = m2 if must_out is None else must_out & m2
            if i.flow == RET and f.depth.get(a) in (0, None):
                may_out |= may
                must_out = must if must_out is None else must_out & must
            for s in self.succ[e].get(a, []):
                if s not in state:
                    state[s] = (may, must, clean)
                    work.append(s)
                else:
                    om, os_, oc = state[s]
                    nm, ns, nc = om | may, os_ & must, oc & clean
                    if (nm, ns, nc) != (om, os_, oc):
                        state[s] = (nm, ns, nc)
                        work.append(s)
        return may_out, (must_out or 0)

    # ---- liveness ------------------------------------------------------------------------------
    def live(self, e, full, record=False):
        """backward liveness; returns the live set at entry. full: the routine's exit set is live
        at its returns, and a tail call continues into the target's full live set"""
        r = self.p.routines[e]
        f = self.facts[e]
        pr = self.preds[e]
        live_in = {}
        at_exit = self.exit[e] if full else 0
        work = list(r.body)
        inq = set(work)
        while work:
            a = work.pop()
            inq.discard(a)
            i = self.p.insn[a]
            out = 0
            for s in self.succ[e].get(a, []):
                out |= live_in.get(s, 0)
            for t in r.tails.get(a, []):
                out |= self.full_in.get(t, self.params[t]) if full else self.params[t]
            if i.flow == RET and f.depth.get(a) in (0, None):
                out |= at_exit          # (a return past the caller: what is live there is not known)
            if record:
                self.live_out_at[a] = out
            ent = f.ud[a]
            if a in r.calls:
                if record:
                    self.live_after[a] = out
                ts = r.calls[a]
                cu, cs = 0, ALL
                for t in ts:
                    cu |= self.params[t]
                    cs &= self.must[t]
                if not ts:
                    cs = 0
                out = (out & ~cs) | cu
            li = transfer(ent, out)
            if live_in.get(a) != li:
                live_in[a] = li
                for p in pr.get(a, []):
                    if p not in inq:
                        inq.add(p)
                        work.append(p)
        return live_in.get(r.entry, 0)

    def solve(self):
        """the genuine parameters first (exits live nothing, calls read their callees' genuine
        parameters), then the exits from the callers, then the parameters that are in and out"""
        self.defs()
        rounds = 0
        while True:
            rounds += 1
            changed = False
            for e in self.p.routines:
                g = self.live(e, False) & ALL
                if g != self.params[e]:
                    self.params[e] = g
                    changed = True
            if not changed:
                break
        self.genuine = dict(self.params)
        while True:
            rounds += 1
            ex = {e: 0 for e in self.p.routines}
            self.live_after = {}
            for e in self.p.routines:
                self.full_in[e] = self.live(e, True, record=True) & ALL
            for e, r in self.p.routines.items():
                for s, ts in r.calls.items():
                    for t in ts:
                        ex[t] |= self.live_after.get(s, 0) & ALL
            while True:
                grew = False
                for e, r in self.p.routines.items():
                    for s, ts in r.tails.items():
                        for t in ts:
                            if ex[e] & ~ex[t]:
                                ex[t] |= ex[e]
                                grew = True
                if not grew:
                    break
            if ex == self.exit:
                break
            self.exit = ex
        self.meaningful()
        # the exits again, with calls reading only the parameters some caller sets
        self.params = {e: self.genuine[e] & self.set_by_caller[e] for e in self.p.routines}
        self.exit = {e: 0 for e in self.p.routines}
        while True:
            rounds += 1
            ex = {e: 0 for e in self.p.routines}
            self.live_after = {}
            for e in self.p.routines:
                self.full_in[e] = self.live(e, True, record=True) & ALL
            for e, r in self.p.routines.items():
                for s, ts in r.calls.items():
                    for t in ts:
                        ex[t] |= self.live_after.get(s, 0) & ALL
            while True:
                grew = False
                for e, r in self.p.routines.items():
                    for s, ts in r.tails.items():
                        for t in ts:
                            if ex[e] & ~ex[t]:
                                ex[t] |= ex[e]
                                grew = True
                if not grew:
                    break
            if ex == self.exit:
                break
            self.exit = ex
        for e in self.p.routines:
            inout = self.full_in[e] & self.may[e] & ~self.must[e] & self.set_by_caller[e]
            self.params[e] = (self.genuine[e] & self.set_by_caller[e]) | inout
            self.leaks[e] = self.genuine[e] & ~self.set_by_caller[e]
        return rounds

    def _set_before(self, e):
        """call or tail site -> registers some path from the entry to it has written"""
        r = self.p.routines[e]
        f = self.facts[e]
        state = {r.entry: 0}
        work = [r.entry]
        out = {}
        while work:
            a = work.pop()
            st = state[a]
            if a in r.calls or a in r.tails:
                out[a] = out.get(a, 0) | st
            st |= f.ud[a][1] & ALL
            if a in r.calls:
                for t in r.calls[a]:
                    st |= self.may[t]
                if not r.calls[a]:
                    st |= ALL
            for s in self.succ[e].get(a, []):
                if s not in state or st & ~state[s]:
                    state[s] = state.get(s, 0) | st
                    work.append(s)
        return out

    def meaningful(self):
        """set_by_caller[e]: the registers some chain of callers from a root writes before calling
        e. A root (an interrupt or the reset, or a routine with no recorded caller) sets nothing
        if it is a vector, everything if its callers are unknown"""
        self.leaks = {}
        callers = {e: [] for e in self.p.routines}
        for e, r in self.p.routines.items():
            for s, ts in r.calls.items():
                for t in ts:
                    callers[t].append((e, s))
            for s, ts in r.tails.items():
                for t in ts:
                    callers[t].append((e, s))
        vectors = {int.from_bytes(self.p.rom[4 * v:4 * v + 4], 'big') & 0xFFFFFF for v in range(1, 64)}
        before = {e: self._set_before(e) for e in self.p.routines}
        sbc = {}
        for e in self.p.routines:
            if not callers[e]:
                sbc[e] = 0 if e in vectors else ALL
            else:
                sbc[e] = 0
        changed = True
        while changed:
            changed = False
            for e in self.p.routines:
                if not callers[e]:
                    continue
                v = 0
                for c, s in callers[e]:
                    v |= before[c].get(s, 0) | sbc[c]
                if v & ~sbc[e]:
                    sbc[e] |= v
                    changed = True
        self.set_by_caller = sbc

    def results(self, e):
        return self.may[e] & self.exit[e]


# ---- reporting ---------------------------------------------------------------------------------
def describe(mask):
    out = []
    for n in range(8):
        p = (mask >> (3 * n)) & 7
        if p:
            out.append('d%d%s' % (n, {1: '.b', 3: '.w', 7: '', 2: '.b1', 4: '.hi', 5: '.b+hi', 6: '.b1+hi'}[p]))
    for n in range(7):
        if mask & A(n):
            out.append('a%d' % n)
    if mask & CC:
        out.append('cc')
    if mask & XF:
        out.append('x')
    return out


def signatures(prog, an):
    out = {}
    for e, r in prog.routines.items():
        f = an.facts[e]
        notes = list(dict.fromkeys(f.notes))
        if an.leaks.get(e):
            notes.append('reads %s before writing it on some path, and no caller sets it (a register leak)' % ', '.join(describe(an.leaks[e])))
        if e in an.unknown_calls:
            notes.append('calls through a register with no recorded target at ' + ', '.join('%06X' % s for s in an.unknown_calls[e]))
        pres = ALL & ~an.may[e] & (DREGS | AREGS)
        out[r.name] = {
            'addr': e,
            'in': describe(an.params[e]),
            'out': describe(an.results(e)),
            'changes': describe(an.may[e] & (DREGS | AREGS)),
            'keeps': describe(pres) if pres != (DREGS | AREGS) else ['all'],
            'notes': notes,
        }
    return out


def main():
    prog = Program()
    an = Analysis(prog)
    rounds = an.solve()
    sig = signatures(prog, an)
    path = os.path.join(ROOT, 'build', 'disasm', 'signatures.json')
    json.dump(sig, open(path, 'w'), indent=1)
    names = sys.argv[1:]
    if not names:
        n_in = sum(1 for s in sig.values() if s['in'])
        n_cc = sum(1 for s in sig.values() if 'cc' in s['out'])
        print('%d routines, %d rounds; %d take registers, %d return a condition; %s' % (
            len(sig), rounds, n_in, n_cc, os.path.relpath(path, ROOT)))
    for n in names:
        r = prog.by_name(n)
        if not r:
            print(n, '?')
            continue
        s = sig[r.name]
        print('%s $%06X: %s -> %s   changes %s' % (r.name, s['addr'], ', '.join(s['in']) or '-', ', '.join(s['out']) or '-',
                                                ' '.join(s['changes']) or '-'))
        for x in s['notes']:
            print('   ', x)


if __name__ == '__main__':
    main()

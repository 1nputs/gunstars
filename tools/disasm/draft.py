#!/usr/bin/env python3
"""Draft a routine in C from the disassembly: the first version a hand rewrite starts from.

The draft keeps the 68000's registers as uint32_t locals and does exactly what each instruction
does: sizes, signs, the part of a register a byte or word write leaves alone (only where a later
read can see it), condition codes only where something tests them (folded into the test when the
compared values are still there, otherwise kept in `ccr` with draft.h's helpers), work RAM and
the cartridge through src/mem.h, the hardware through src/hal.h. Its signature comes from regs.py:
the registers it takes, and the ones it returns (one returned, several through pointers). Jumps
become if/else, do/while and switch where the flow allows, goto elsewhere; every statement keeps
its instruction in a comment.

A draft is never committed (build/draft/): the routine is rewritten by hand into the C the
project keeps, with names, types and structure, and both are checked by the same captures.

usage: draft.py NAME...            write build/draft/NAME.c for each, and print them
       draft.py --tree NAME        NAME's callees, leaf first, with what is ported
"""
import argparse, json, os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from program import Program, ROOT  # noqa: E402
import regs  # noqa: E402
from regs import D, A, CC, XF, ALL, DREGS, PART  # noqa: E402
from m68k import CALL, RET, JUMP, BRANCH, COND, SEQ  # noqa: E402

BITS = {'b': 8, 'w': 16, 'l': 32}
UT = {'b': 'uint8_t', 'w': 'uint16_t', 'l': 'uint32_t'}
ST = {'b': 'int8_t', 'w': 'int16_t', 'l': 'int32_t'}
MASK = {'b': 0xFF, 'w': 0xFFFF, 'l': 0xFFFFFFFF}
CONDS = ('t', 'f', 'hi', 'ls', 'cc', 'cs', 'ne', 'eq', 'vc', 'vs', 'pl', 'mi', 'ge', 'lt', 'gt', 'le')
NEG = {'t': 'f', 'f': 't', 'hi': 'ls', 'ls': 'hi', 'cc': 'cs', 'cs': 'cc', 'ne': 'eq', 'eq': 'ne',
       'vc': 'vs', 'vs': 'vc', 'pl': 'mi', 'mi': 'pl', 'ge': 'lt', 'lt': 'ge', 'gt': 'le', 'le': 'gt'}
STRUCTS = {'v_player1': 0x200, 'v_player2': 0x200, 'v_boss': 0x200, 'v_hintCode': 0x200, 'v_vdpRegs': 0x40,
           'v_jobList': 0x20}
HAL_READ = ('vdp_status', 'vdp_read', 'ym_read8', 'z80_ram_read8', 'z80_busack', 'io_read')


def hx(v):
    return '0x%X' % v if v > 9 else str(v)


def shx(v, sz):
    """an immediate as a signed value of size sz"""
    v &= MASK[sz]
    if v & (1 << (BITS[sz] - 1)):
        v -= 1 << BITS[sz]
        return '-' + hx(-v)
    return hx(v)


def single(t):
    """t is one primary expression: a name, a number, a call, a parenthesised expression"""
    t = t.strip()
    m = re.match(r'[A-Za-z_][A-Za-z0-9_]*|0x[0-9A-Fa-f]+|\d+', t)
    k = m.end() if m else 0
    if k == len(t):
        return True
    if t[k] != '(':
        return False
    d = 0
    for j in range(k, len(t)):
        d += t[j] == '('
        d -= t[j] == ')'
        if d == 0:
            return j == len(t) - 1
    return False


TYPED = {'uint8_t': ('rd8(', 'rom8(', 'bus_rd8(', 'ym_read8(', 'z80_ram_read8(', 'io_read8(', 'z80_busack('),
         'uint16_t': ('rd16(', 'rom16(', 'bus_rd16(', 'vdp_status(', 'vdp_read(', 'io_read16('),
         'uint32_t': ('rd32(', 'rom32(', 'bus_rd32(', 'io_read32(')}


def typed(v):
    """the unsigned type expression v already has, or None"""
    v = v.strip()
    for ct in ('uint8_t', 'uint16_t', 'uint32_t'):
        if v.startswith('(%s)' % ct) and single(v[len(ct) + 2:]):
            return ct
        for f in TYPED[ct]:
            if v.startswith(f) and single(v):
                return ct
    return None


def cast(ct, v):
    """v as type ct, without a cast it already has"""
    v = v.strip()
    if re.fullmatch(r'0x[0-9A-F]+|\d+', v):
        return v
    if typed(v) == ct:
        return v
    if ct == 'uint32_t' and re.fullmatch(r'[a-z_][a-z0-9_]*', v):
        return v
    if single(v):
        return '(%s)%s' % (ct, v)
    return '(%s)(%s)' % (ct, v)


def balanced(t):
    d = 0
    for ch in t:
        d += ch == '('
        d -= ch == ')'
        if d < 0:
            return False
    return d == 0


class Expr:
    """a C expression: its text, the registers it reads, whether it reads memory, whether reading
    it again would do something (hardware)"""
    __slots__ = ('t', 'regs', 'mem', 'volatile')

    def __init__(self, t, regs_=0, mem=False, volatile=False):
        self.t, self.regs, self.mem, self.volatile = t, regs_, mem, volatile

    def __str__(self):
        return self.t


class Cond:
    """a branch condition as C text and its negation"""

    def __init__(self, t, n=None):
        self.t = t
        self.n = n

    def neg(self):
        if self.n is not None:
            return Cond(self.n, self.t)
        if self.t == '1':
            return Cond('0', '1')
        if self.t == '0':
            return Cond('1', '0')
        return Cond('!(%s)' % self.t, self.t)


def ccmp(op, a, b):
    inv = {'==': '!=', '!=': '==', '<': '>=', '>=': '<', '>': '<=', '<=': '>'}
    return Cond('%s %s %s' % (a, op, b), '%s %s %s' % (a, inv[op], b))


class Sigs:
    def __init__(self, prog, an):
        self.p, self.an = prog, an

    def of(self, e):
        """(params [(reg, parts)], results [(reg, parts)], cc result)"""
        an = self.an
        return self.split(an.params[e]), self.split(an.results(e) & ~CC & ~XF), bool(an.results(e) & CC)

    @staticmethod
    def split(mask):
        out = []
        for n in range(8):
            p = (mask >> (3 * n)) & 7
            if p:
                out.append(('d%d' % n, p))
        for n in range(7):
            if mask & A(n):
                out.append(('a%d' % n, 7))
        return out

    def decl(self, e, name):
        params, results, cc = self.of(e)
        pn = [r for r, _ in params]
        rn = [r for r, _ in results]
        ret = 'void'
        args = []
        if len(rn) == 1 and not cc:
            ret = 'uint32_t'
        elif not rn and cc:
            ret = 'uint8_t'
        for r, p in params:
            if len(rn) > 1 or (cc and rn):
                if r in rn:
                    continue
            args.append('uint32_t %s%s' % (r, '' if p == 7 else ' /* %s */' % width(p)))
        if len(rn) > 1 or (cc and rn):
            for r in rn:
                args.append('uint32_t *p_%s' % r)
            if cc:
                args.append('uint8_t *p_ccr')
        return ret, args, pn, rn, cc

    def proto(self, e, name):
        ret, args, pn, rn, cc = self.decl(e, name)
        return '%s %s(%s)' % (ret, name, ', '.join(args) or 'void')


def width(p):
    return {1: '.b', 3: '.w', 7: '.l'}.get(p, 'parts %d' % p)


class Draft:
    def __init__(self, prog, an, sigs, r):
        self.p, self.an, self.s, self.r = prog, an, sigs, r
        self.f = an.facts[r.entry]
        self.items = []
        self.locals = set()
        self.temps = 0
        self.notes = []
        self.used_ccr = False
        self.callees = set()
        self.override = {}          # instruction -> the Expr its flags test (a hardware read kept in a temp)
        self.sig = sigs.decl(r.entry, r.name)
        self._live()
        self._consts()
        self._reach_cc()

    # ---- analyses inside the routine ----------------------------------------------------------
    def _live(self):
        """exact liveness after each instruction (a byte write keeps the rest of the register
        live), for where a partial write must keep the rest and where flags are needed"""
        r, an, f = self.r, self.an, self.f
        e = r.entry
        pr = an.preds[e]
        live_in, self.out = {}, {}
        work = list(r.body)
        inq = set(work)
        exit_live = an.exit[e] | CC if self.sig[4] else an.exit[e]
        exit_live |= sum(D(int(x[1])) if x[0] == 'd' else A(int(x[1])) for x in self.sig[3])
        while work:
            a = work.pop()
            inq.discard(a)
            i = self.p.insn[a]
            out = 0
            for s in r.succ.get(a, []):
                out |= live_in.get(s, 0)
            for t in r.tails.get(a, []):
                out |= an.genuine[t] | an.params[t] | an.full_in.get(t, 0)
            if i.flow == RET:
                out |= exit_live
            self.out[a] = out
            u, d, val, pure, copies = f.ud[a]
            if a in r.calls:
                ts = r.calls[a]
                cu, cs = 0, ALL
                for t in ts:
                    cu |= an.genuine[t] | an.params[t]
                    cs &= an.must[t]
                if not ts:
                    cs = 0
                out = (out & ~cs) | cu
            li = (out & ~d) | u
            for src, dst in copies:
                if out & dst:
                    li |= src
            if live_in.get(a) != li:
                live_in[a] = li
                for q in pr.get(a, []):
                    if q not in inq:
                        inq.add(q)
                        work.append(q)
        self.live_in = live_in

    def _consts(self):
        """address registers holding a known address at each instruction (lea, movea #)"""
        r = self.r
        state = {r.entry: {}}
        work = [r.entry]
        while work:
            a = work.pop()
            st = dict(state[a])
            i = self.p.insn[a]
            st = self._const_step(i, st)
            for s in r.succ.get(a, []):
                if s not in state:
                    state[s] = st
                    work.append(s)
                else:
                    old = state[s]
                    new = {k: v for k, v in old.items() if st.get(k) == v}
                    if new != old:
                        state[s] = new
                        work.append(s)
        self.consts = state

    def _const_step(self, i, st):
        base, _, sz = i.mnem.partition('.')
        ops = i.ops
        u, d = regs.usedef(i)
        known = None
        if base == 'lea' and ops[0][0] in ('absw', 'absl', 'pcd'):
            known = (ops[1][1], ops[0][1] & 0xFFFFFF)
        elif base == 'lea' and ops[0][0] == 'disp' and ops[0][1] in st:
            known = (ops[1][1], (st[ops[0][1]] + ops[0][2]) & 0xFFFFFF)
        elif base == 'movea' and ops[0][0] == 'imm':
            v = ops[0][1]
            if sz == 'w' and v & 0x8000:
                v |= 0xFFFF0000
            known = (ops[1][1], v & 0xFFFFFF)
        elif base in ('addq', 'subq', 'adda', 'suba') and ops[1][0] == 'a' and ops[0][0] in ('q', 'imm') and ops[1][1] in st:
            k = ops[0][1] if base in ('addq', 'adda') else -ops[0][1]
            if base in ('adda', 'suba') and sz == 'w' and ops[0][1] & 0x8000:
                k = (ops[0][1] - 0x10000) * (1 if base == 'adda' else -1)
            known = (ops[1][1], (st[ops[1][1]] + k) & 0xFFFFFF)
        for o in ops:
            if o[0] in ('inc', 'dec') and o[1] in st:
                n = 1 if sz == 'b' and o[1] != 7 else 2 if sz in ('b', 'w') else 4
                if base == 'movem':
                    lst = ops[0] if ops[0][0] == 'list' else ops[1]
                    n = (4 if sz == 'l' else 2) * len(regs.list_regs(regs.reg_mask(lst[1], lst[2])))
                st[o[1]] = (st[o[1]] + (n if o[0] == 'inc' else -n)) & 0xFFFFFF
        for n in list(st):
            if d & A(n) and not (known and known[0] == n) and not any(o[0] in ('inc', 'dec') and o[1] == n for o in ops):
                del st[n]
        if i.flow == CALL:
            for t in self.r.calls.get(i.addr, []) or [None]:
                for n in list(st):
                    if t is None or self.an.may[t] & A(n):
                        st.pop(n, None)
        if known:
            st[known[0]] = known[1]
        return st

    def _reach_cc(self):
        """the instruction whose flags each flag test sees, when there is one and the path to the
        test is straight (no other way in): then the test can be folded into a C comparison"""
        r = self.r
        self.cc_setter = {}
        for a in r.body:
            u, d = self.f.ud[a][0], self.f.ud[a][1]
            uses_cc = u & CC or (self.p.insn[a].mnem.startswith('db') and self.p.insn[a].mnem != 'dbf')
            if not uses_cc:
                continue
            between = []
            b = a
            setter = None
            while True:
                preds = self.an.preds[r.entry].get(b, [])
                if len(preds) != 1 or b == r.entry:
                    break
                q = preds[0]
                qi = self.p.insn[q]
                if self.p.successors(qi) and len(r.succ.get(q, [])) != 1 and qi.flow != COND:
                    break
                if q in r.calls:
                    setter = q
                    break
                if self.f.ud[q][1] & CC:
                    setter = q
                    break
                between.append(q)
                b = q
            if setter is not None:
                self.cc_setter[a] = (setter, between)
        self.need_ccr = set()

    # ---- operands -------------------------------------------------------------------------------
    def reg(self, kind, n):
        name = '%s%d' % (kind, n)
        self.locals.add(name)
        return name

    def ram(self, addr):
        """a work RAM offset by name"""
        off = addr & 0xFFFF
        nm = self.p.ram_name(off)
        if nm:
            return nm
        # the closest name below: the next bytes of a variable, or a field of a struct
        for k in range(1, 0x200):
            nm = self.p.ram_name((off - k) & 0xFFFF)
            if nm and (k < 4 or k < STRUCTS.get(nm, 0)):
                return '%s + %s' % (nm, hx(k))
            if nm:
                break
        return '0x%04X' % off

    def const_addr(self, a, at):
        """the address an operand names, when known"""
        c = self.consts.get(at, {})
        k = a[0]
        if k in ('absw', 'absl', 'pcd'):
            return a[1] & 0xFFFFFF
        if k == 'ind' and a[1] in c:
            return c[a[1]]
        if k == 'disp' and a[1] in c:
            return (c[a[1]] + a[2]) & 0xFFFFFF
        return None

    def addr(self, o, sz, at):
        """(pre statements, address Expr, post statements) of a memory operand"""
        k = o[0]
        n = 1 if sz == 'b' else 2 if sz == 'w' else 4
        if k in ('inc', 'dec'):
            an_ = self.reg('a', o[1]) if o[1] != 7 else 'sp'
            if o[1] == 7 and n == 1:
                n = 2
            if k == 'inc':
                return [], Expr(an_, A(o[1])), ['%s += %d;' % (an_, n)]
            return ['%s -= %d;' % (an_, n)], Expr(an_, A(o[1])), []
        if k == 'ind':
            return [], Expr(self.reg('a', o[1]), A(o[1])), []
        if k == 'disp':
            return [], Expr('%s + %s' % (self.reg('a', o[1]), shx(o[2], 'l') if o[2] >= 0 else '(uint32_t)' + str(o[2])), A(o[1])), []
        if k == 'idx':
            x = self.xreg(o[3], o[4])
            disp = '' if o[2] == 0 else (' + %d' % o[2] if o[2] > 0 else ' - %d' % -o[2])
            return [], Expr('%s + %s%s' % (self.reg('a', o[1]), x.t, disp), A(o[1]) | x.regs), []
        if k == 'pcx':
            x = self.xreg(o[2], o[3])
            return [], Expr('0x%X + %s' % (o[1] & 0xFFFFFF, x.t), x.regs), []
        if k in ('absw', 'absl', 'pcd'):
            return [], Expr('0x%X' % (o[1] & 0xFFFFFF)), []
        raise ValueError(o)

    def xreg(self, x, xlong):
        if x < 8:
            r = self.reg('d', x)
            return Expr(r if xlong else '(int16_t)%s' % r, D(x, 'l' if xlong else 'w'))
        r = self.reg('a', x - 8)
        return Expr(r if xlong else '(int16_t)%s' % r, A(x - 8))

    def mem_read(self, o, sz, at):
        """(pre, Expr, post)"""
        pre, ad, post = self.addr(o, sz, at)
        c = self.const_addr(o, at)
        f = {'b': '8', 'w': '16', 'l': '32'}[sz]
        if c is not None:
            hw = self.hw_read(c, sz)
            if hw:
                return pre, Expr(hw, 0, True, True), post
            if c >= 0xFF0000 or (c & 0xE00000) == 0xE00000:
                return pre, Expr('rd%s(%s)' % (f, self.ram(c)), 0, True), post
            if c < 0x400000:
                return pre, Expr('rom%s(0x%X)' % (f, c), 0, False), post
        if o[0] == 'pcx':
            return pre, Expr('rom%s(%s)' % (f, ad.t), ad.regs, False), post
        return pre, Expr('bus_rd%s(%s)' % (f, ad.t), ad.regs, True), post

    def hw_read(self, c, sz):
        if c == 0xC00004 and sz == 'w':
            return 'vdp_status()'
        if c == 0xC00000 and sz == 'w':
            return 'vdp_read()'
        if 0xA04000 <= c < 0xA04004 and sz == 'b':
            return 'ym_read8(%d)' % (c - 0xA04000)
        if 0xA00000 <= c < 0xA02000 and sz == 'b':
            return 'z80_ram_read8(0x%X)' % (c - 0xA00000)
        if c == 0xA11100:
            return 'z80_busack()' if sz == 'b' else '(uint16_t)(z80_busack() << 8)'
        if 0xA10000 <= c < 0xA10020:
            return {'b': 'io_read8(0x%X)', 'w': 'io_read16(0x%X)', 'l': 'io_read32(0x%X)'}[sz] % c
        return None

    def hw_write(self, c, sz, v):
        if c == 0xC00004:
            return {'w': 'vdp_ctrl16(%s);', 'l': 'vdp_ctrl(%s);'}.get(sz, 'VDP_CTRL_BYTE(%s);') % v
        if c == 0xC00000:
            return {'w': 'vdp_data(%s);', 'l': 'vdp_data32(%s);'}.get(sz, 'VDP_DATA_BYTE(%s);') % v
        if c == 0xC00011 and sz == 'b':
            return 'psg_write(%s);' % v
        if 0xA04000 <= c < 0xA04004 and sz == 'b':
            return 'ym_write8(%d, %s);' % (c - 0xA04000, v)
        if 0xA00000 <= c < 0xA02000 and sz == 'b':
            return 'z80_ram_write8(0x%X, %s);' % (c - 0xA00000, v)
        if c == 0xA11100 and sz == 'w':
            return 'z80_busreq(%s);' % v
        if c == 0xA11200 and sz == 'w':
            return 'z80_reset(%s);' % v
        if 0xA10000 <= c < 0xA10020 and sz == 'b':
            return 'io_write8(0x%X, %s);' % (c, v)
        if c == 0xA14000 and sz == 'l':
            return 'tmss_write(%s);' % v
        return None

    def read(self, o, sz, at):
        """(pre, Expr of type UT[sz], post) of reading operand o"""
        k = o[0]
        if k == 'd':
            r = self.reg('d', o[1])
            return [], Expr(r if sz == 'l' else '(%s)%s' % (UT[sz], r), D(o[1], sz)), []
        if k == 'a':
            r = self.reg('a', o[1]) if o[1] != 7 else 'sp'
            return [], Expr(r if sz == 'l' else '(uint16_t)%s' % r, A(o[1])), []
        if k in ('imm', 'q'):
            v = o[1] & MASK[sz] if k == 'imm' else o[1]
            return [], Expr(hx(v & MASK[sz])), []
        return self.mem_read(o, sz, at)

    def write(self, o, sz, v, at):
        """statements writing value text v (already of the size) to operand o: (pre, stmts, post)"""
        k = o[0]
        if k == 'd':
            r = self.reg('d', o[1])
            if sz == 'l':
                return [], ['%s = %s;' % (r, v)], []
            rest = (self.out.get(at, ALL) >> (3 * o[1])) & 7 & ~PART[sz]
            if not rest:
                return [], ['%s = %s;' % (r, cast(UT[sz], v))], []
            return [], ['SET%s(%s, %s);' % (sz.upper(), r, v)], []
        if k == 'a':
            r = self.reg('a', o[1]) if o[1] != 7 else 'sp'
            if sz == 'w':
                return [], ['%s = (uint32_t)(int16_t)(%s);' % (r, v)], []
            return [], ['%s = %s;' % (r, v)], []
        pre, ad, post = self.addr(o, sz, at)
        c = self.const_addr(o, at)
        f = {'b': '8', 'w': '16', 'l': '32'}[sz]
        if c is not None:
            hw = self.hw_write(c, sz, v)
            if hw:
                return pre, [hw], post
            if (c & 0xE00000) == 0xE00000:
                return pre, ['wr%s(%s, %s);' % (f, self.ram(c), v)], post
        return pre, ['bus_wr%s(%s, %s);' % (f, ad.t, v)], post

    # ---- statements ---------------------------------------------------------------------------
    def tmp(self, ctype):
        """a temporary, declared with the locals; the statement assigns it"""
        self.temps += 1
        name = 't%d' % self.temps
        self.locals.add((name, ctype))
        return name, ctype

    def stmt(self, text, i):
        self.items.append(['stmt', text, i.addr])

    def flags_needed(self, a):
        return bool(self.out.get(a, 0) & (CC | XF))

    def emit_all(self):
        prog = self.p
        body = self.r.body
        self.labels = set()
        for a in body:
            i = prog.insn[a]
            for s in self.r.succ.get(a, []):
                if s != a + i.size or i.flow in (BRANCH, JUMP):
                    self.labels.add(s)
        # a body that is not laid out in one run: every block start after a gap is a label
        for k, a in enumerate(body):
            if k and body[k - 1] + prog.insn[body[k - 1]].size != a:
                self.labels.add(a)
        for k, a in enumerate(body):
            i = prog.insn[a]
            if a in self.labels:
                self.items.append(['label', a])
            try:
                self.emit(i)
            except Unsupported as e:
                self.stmt('TODO(%s);' % e, i)
            nxt = a + i.size
            falls = i.flow in (SEQ, COND, CALL) and not (i.flow == CALL and a in self.r.tails)
            if falls and nxt in self.r.succ.get(a, []):
                if k + 1 >= len(body) or body[k + 1] != nxt:
                    self.items.append(['goto', None, nxt])
            for t in self.r.tails.get(a, []):
                if i.flow in (SEQ, COND, CALL) and t == nxt and i.flow != JUMP:
                    self.tail(t, i, None)

    def asm(self, i):
        try:
            import m68k
            return m68k.render(i, lambda t, h: self.p.name.get(t) or ('$%X' % t))
        except Exception:
            return i.mnem

    def emit(self, i):
        base, _, sz = i.mnem.partition('.')
        m = i.mnem
        ops = i.ops
        a = i.addr
        fl = self.flags_needed(a)
        if m in ('rts', 'rte', 'rtr'):
            self.ret(i)
            return
        if m == 'nop':
            return
        if i.flow == CALL:
            self.call(i)
            return
        if base in ('bra', 'jmp'):
            self.jump(i)
            return
        if i.flow == COND and m[0] == 'b':
            c = self.cond(a, m[1:].split('.')[0])
            self.items.append(['if', c, i.target, a])
            return
        if m.startswith('db'):
            self.dbcc(i)
            return
        if base in ('move', 'movea') and (ops[0][0] in ('sr', 'ccr', 'usp') or ops[1][0] in ('sr', 'ccr', 'usp')):
            self.sr(i)
            return
        if base == 'movem':
            self.movem(i)
            return
        if base in ('move', 'movea', 'moveq'):
            if base == 'moveq':
                sz = 'l'
                pre, s, post = [], Expr(hx(ops[0][1] & 0xFFFFFFFF) if ops[0][1] >= 0 else '(uint32_t)%d' % ops[0][1]), []
            else:
                if self.stack_move(i):
                    return
                pre, s, post = self.read(ops[0], sz, a)
            if fl and base != 'movea':
                if s.volatile or s.mem and ops[1][0] not in ('d', 'a'):
                    t, ct = self.tmp(UT[sz])
                    pre = pre + ['%s = %s;' % (t, s.t)]
                    s = Expr(t)
                self.set_flags(i, 'logic', sz, s.t)
            wpre, w, wpost = self.write(ops[1], sz, s.t, a)
            self.out_stmts(i, pre + wpre, w, post + wpost)
            return
        if base == 'clr':
            wpre, w, wpost = self.write(ops[0], sz, '0', a)
            if fl:
                self.set_flags(i, 'logic', sz, '0')
            self.out_stmts(i, wpre, w, wpost)
            return
        if base == 'lea':
            pre, ad, post = self.addr(ops[0], 'l', a)
            c = self.const_addr(ops[0], a)
            v = ad.t
            if c is not None and (c & 0xE00000) == 0xE00000:
                v = '0xFF0000 | %s' % self.ram(c) if self.p.ram_name(c & 0xFFFF) else '0x%X' % (c | 0xFF000000)
            elif c is not None:
                v = self.p.name.get(c) and 'SYM_%s' % self.p.name[c] or '0x%X' % c
            self.stmt('%s = %s;' % (self.reg('a', ops[1][1]) if ops[1][1] != 7 else 'sp', v), i)
            return
        if base == 'pea':
            raise Unsupported('pea')
        if base in ('add', 'sub', 'and', 'or', 'eor', 'addi', 'subi', 'andi', 'ori', 'eori', 'addq', 'subq',
                    'adda', 'suba'):
            self.alu(i)
            return
        if base in ('cmp', 'cmpi', 'cmpa', 'cmpm'):
            self.compare(i)
            return
        if base == 'tst':
            pre, s, post = self.read(ops[0], sz, a)
            if s.volatile:
                t, ct = self.tmp(UT[sz])
                pre = pre + ['%s = %s;' % (t, s.t)]
                s = Expr(t)
            self.set_flags(i, 'logic', sz, s.t, regs_=s.regs, mem=s.mem)
            self.out_stmts(i, pre, [], post)
            return
        if base in ('btst', 'bset', 'bclr', 'bchg'):
            self.bitop(i)
            return
        if base in ('ext', 'swap', 'neg', 'not', 'negx'):
            self.unary(i)
            return
        if base in ('muls', 'mulu', 'divs', 'divu'):
            self.muldiv(i)
            return
        if base[:2] in ('as', 'ls', 'ro') and base[-1] in 'lr':
            self.shift(i)
            return
        if base == 'exg':
            x, y = [self.reg(o[0], o[1]) for o in ops]
            t = 'x'
            self.stmt('{ uint32_t %s = %s; %s = %s; %s = %s; }' % (t, x, x, y, y, t), i)
            return
        if base in ('addx', 'subx', 'abcd', 'sbcd'):
            self.extended(i)
            return
        if m[0] == 's' and base[1:] in CONDS:
            c = self.cond(a, base[1:])
            wpre, w, wpost = self.write(ops[0], 'b', '(%s) ? 0xFF : 0' % c.t, a)
            self.out_stmts(i, wpre, w, wpost)
            return
        raise Unsupported(i.mnem)

    def out_stmts(self, i, pre, main, post):
        lines = pre + main + post
        if lines:
            self.items.append(['stmt', ' '.join(lines), i.addr])

    # ---- flags --------------------------------------------------------------------------------
    def set_flags(self, i, kind, sz, a, b=None, regs_=0, mem=False):
        """record what the flags of instruction i are, for the tests that see them; when one of
        those cannot be folded (or X is needed later), compute ccr here"""
        uses = [u for u, (s, _) in self.cc_setter.items() if s == i.addr]
        info = (kind, sz, a, b)
        self.flag_info = getattr(self, 'flag_info', {})
        self.flag_info[i.addr] = info
        needs = bool(self.out.get(i.addr, 0) & XF)
        for u in uses:
            if not self.foldable(u, i):
                needs = True
        if not uses and self.out.get(i.addr, 0) & CC:
            needs = True
        if needs:
            self.used_ccr = True
            bits = BITS[sz]
            if kind == 'logic':
                text = 'ccr = ccr_logic(ccr, %s, %d);' % (a, bits)
            elif kind == 'cmp':
                text = 'ccr = ccr_cmp(ccr, %s, %s, %d);' % (a, b, bits)
            elif kind == 'sub':
                text = 'ccr = ccr_sub(%s, %s, %d);' % (a, b, bits)
            elif kind == 'add':
                text = 'ccr = ccr_add(%s, %s, %d);' % (a, b, bits)
            elif kind == 'btst':
                text = 'ccr = (uint8_t)((ccr & ~ZF) | ((%s) ? 0 : ZF));' % a
            else:
                text = 'ccr = %s;' % a
            self.items.append(['stmt', text, i.addr])
            self.need_ccr.add(i.addr)

    def foldable(self, use, setter_insn):
        s, between = self.cc_setter[use]
        info = self.fold_info(setter_insn)
        if info is None:
            return False
        kind, sz, exprs = info
        ui = self.p.insn[use]
        cc = ui.mnem[1:].split('.')[0] if ui.mnem[0] == 'b' else ui.mnem[1:] if ui.mnem[0] == 's' else ui.mnem[2:]
        if cc in ('vc', 'vs'):
            return False
        if kind in ('sub', 'add') and cc not in ('eq', 'ne', 'mi', 'pl'):
            return False
        if kind == 'btst' and cc not in ('eq', 'ne'):
            return False
        sdef = self.f.ud[s][1]
        for e in exprs:
            if e.volatile:
                return False
            regs_ = e.regs
            if kind in ('cmp', 'btst') and regs_ & sdef & ~CC:
                return False
            for q in between:
                qd = self.f.ud[q][1]
                if regs_ & qd:
                    return False
                if e.mem and self.writes_memory(self.p.insn[q]):
                    return False
        return True

    def writes_memory(self, i):
        if i.flow == CALL:
            return True
        base = i.mnem.partition('.')[0]
        if base in ('tst', 'cmp', 'cmpi', 'cmpa', 'btst', 'lea', 'bra', 'jmp') or i.mnem[0] == 'b' and len(base) <= 3:
            return False
        return bool(i.ops) and i.ops[-1][0] not in ('d', 'a', 'sr', 'ccr', 'q', 'imm', 'lab')

    def fold_info(self, i):
        """(kind, size, [Expr]) of the values a folded test of i's flags reads, or None"""
        base, _, sz = i.mnem.partition('.')
        ops = i.ops
        a = i.addr
        try:
            if base in ('tst',):
                _, e, _ = self.peek_read(ops[0], sz, a)
                return 'logic', sz, [e]
            if base in ('move', 'moveq', 'and', 'or', 'eor', 'andi', 'ori', 'eori', 'not', 'clr', 'ext', 'swap'):
                if base == 'moveq':
                    return 'logic', 'l', [Expr(self.reg('d', ops[1][1]), D(ops[1][1]))]
                dst = ops[-1]
                if dst[0] == 'd':
                    size = 'w' if base == 'ext' and sz == 'w' else ('l' if base == 'swap' else sz)
                    return 'logic', size, [Expr(self.reg('d', dst[1]), D(dst[1], size))]
                if base == 'move' and ops[0][0] in ('d', 'a', 'imm', 'q'):
                    _, e, _ = self.peek_read(ops[0], sz, a)
                    return 'logic', sz, [e]
                if dst[0] in ('absw', 'absl', 'ind', 'disp'):
                    _, e, _ = self.peek_read(dst, sz, a)
                    if e.regs & self.f.ud[a][1]:
                        return None
                    return 'logic', sz, [e]
                return None
            if base in ('cmp', 'cmpi', 'cmpa'):
                if base == 'cmpa':
                    return None
                _, s, _ = self.peek_read(ops[0], sz, a)
                _, d, _ = self.peek_read(ops[1], sz, a)
                if ops[0][0] in ('inc', 'dec') or ops[1][0] in ('inc', 'dec'):
                    return None
                return 'cmp', sz, [s, d]
            if base in ('sub', 'subi', 'subq', 'add', 'addi', 'addq'):
                dst = ops[-1]
                if dst[0] == 'd':
                    return ('sub' if base.startswith('sub') else 'add'), sz, [Expr(self.reg('d', dst[1]), D(dst[1], sz))]
                if dst[0] in ('absw', 'absl', 'disp', 'ind'):
                    _, e, _ = self.peek_read(dst, sz, a)
                    return ('sub' if base.startswith('sub') else 'add'), sz, [e]
                return None
            if base == 'btst':
                return 'btst', 'b', [self.override.get(a) or self.btst_expr(i)]
        except Unsupported:
            return None
        return None

    def peek_read(self, o, sz, a):
        saved = set(self.locals)
        r = self.read(o, sz, a)
        self.locals = saved | self.locals
        return r

    def btst_expr(self, i):
        b, d = i.ops
        if d[0] == 'd':
            dn = self.reg('d', d[1])
            if b[0] == 'q':
                return Expr('%s & 0x%X' % (dn, 1 << (b[1] & 31)), D(d[1]))
            return Expr('%s & (1u << (%s & 31))' % (dn, self.reg('d', b[1])), D(d[1]) | D(b[1], 'b'))
        _, e, _ = self.peek_read(d, 'b', i.addr)
        if b[0] == 'q':
            return Expr('%s & 0x%X' % (e.t, 1 << (b[1] & 7)), e.regs, e.mem, e.volatile)
        return Expr('%s & (1u << (%s & 7))' % (e.t, self.reg('d', b[1])), e.regs | D(b[1], 'b'), e.mem, e.volatile)

    def cond(self, use, cc):
        """the C condition of test cc at instruction use"""
        if cc == 't':
            return Cond('1', '0')
        if cc == 'f':
            return Cond('0', '1')
        st = self.cc_setter.get(use)
        if st:
            s, between = st
            si = self.p.insn[s]
            if s in self.r.calls:
                return self.ccr_cond(cc)
            if self.foldable(use, si):
                kind, sz, exprs = self.fold_info(si)
                return self.fold(kind, sz, exprs, cc)
        self.used_ccr = True
        return self.ccr_cond(cc)

    def ccr_cond(self, cc):
        self.used_ccr = True
        return Cond('CC_%s(ccr)' % cc.upper(), 'CC_%s(ccr)' % NEG[cc].upper())

    def fold(self, kind, sz, exprs, cc):
        u, s = UT[sz], ST[sz]

        def sv(e):
            if re.fullmatch(r'0x[0-9A-F]+|\d+', e.t):
                return shx(int(e.t, 0), sz)
            t = e.t
            if t.startswith('(%s)' % u) and single(t[len(u) + 2:]):
                t = t[len(u) + 2:]
            return '(%s)%s' % (s, t if single(t) else '(%s)' % t)

        def uv(e):
            return cast(u, e.t)
        if kind == 'logic' or kind in ('sub', 'add'):
            e = exprs[0]
            t = {'eq': ('==', uv(e), '0'), 'ne': ('!=', uv(e), '0'), 'mi': ('<', sv(e), '0'), 'pl': ('>=', sv(e), '0'),
                 'lt': ('<', sv(e), '0'), 'ge': ('>=', sv(e), '0'), 'gt': ('>', sv(e), '0'), 'le': ('<=', sv(e), '0'),
                 'hi': ('!=', uv(e), '0'), 'ls': ('==', uv(e), '0')}.get(cc)
            if t:
                return ccmp(t[0], t[1], t[2])
            return Cond({'cc': '1', 'cs': '0'}[cc], {'cc': '0', 'cs': '1'}[cc])
        if kind == 'cmp':
            src, dst = exprs
            ops = {'eq': '==', 'ne': '!=', 'hi': '>', 'ls': '<=', 'cc': '>=', 'cs': '<',
                   'gt': '>', 'ge': '>=', 'lt': '<', 'le': '<='}
            if cc in ('mi', 'pl'):
                return ccmp('<' if cc == 'mi' else '>=', '(%s)(%s - %s)' % (s, uv(dst), uv(src)), '0')
            if cc in ('gt', 'ge', 'lt', 'le'):
                return ccmp(ops[cc], sv(dst), sv(src))
            return ccmp(ops[cc], uv(dst), uv(src))
        if kind == 'btst':
            e = exprs[0]
            return Cond('!(%s)' % e.t, '%s' % e.t) if cc == 'eq' else Cond(e.t, '!(%s)' % e.t)
        raise Unsupported('fold ' + kind)

    # ---- instruction families -----------------------------------------------------------------
    def alu(self, i):
        base, _, sz = i.mnem.partition('.')
        s_op, d_op = i.ops
        a = i.addr
        op = {'add': '+', 'sub': '-', 'and': '&', 'or': '|', 'eor': '^'}[base.rstrip('iqa') if base not in ('and', 'andi') else 'and']
        if d_op[0] in ('sr', 'ccr'):
            v = s_op[1]
            if d_op[0] == 'sr':
                self.stmt('cpu_set_sr((uint16_t)(cpu_get_sr() %s 0x%X));' % (op, v), i)
            else:
                self.used_ccr = True
                self.stmt('ccr = (uint8_t)(ccr %s 0x%X);' % (op, v), i)
            return
        if d_op[0] == 'a':
            r = self.reg('a', d_op[1]) if d_op[1] != 7 else 'sp'
            pre, s, post = self.read(s_op, sz if base != 'addq' and base != 'subq' else 'l', a)
            sv = s.t if sz == 'l' or s_op[0] == 'q' else '(uint32_t)(int16_t)%s' % s.t
            self.out_stmts(i, pre, ['%s %s= %s;' % (r, op, sv)], post)
            return
        pre, s, post = self.read(s_op, sz, a)
        dpre, dv, dpost = self.read(d_op, sz, a)
        if dpre and pre:
            raise Unsupported('two side effects')
        fl = self.flags_needed(a)
        lines = pre + dpre
        if fl:
            if s.volatile:
                t, ct = self.tmp(UT[sz])
                lines.append('%s = %s;' % (t, s.t))
                s = Expr(t)
            kind = 'sub' if op == '-' else 'add' if op == '+' else 'logic'
            if kind == 'logic':
                res = '(%s)(%s %s %s)' % (UT[sz], dv.t, op, s.t)
                self.items.append(['stmt', ' '.join(lines), a])
                lines = []
                self.set_flags(i, kind, sz, res)
            else:
                self.items.append(['stmt', ' '.join(lines), a])
                lines = []
                self.set_flags(i, kind, sz, dv.t, s.t)
        val = '%s %s %s' % (dv.t, op, s.t)
        wpre, w, wpost = self.write(d_op, sz, val, a)
        self.out_stmts(i, lines + wpre, w, post + dpost + wpost)

    def compare(self, i):
        base, _, sz = i.mnem.partition('.')
        a = i.addr
        if base == 'cmpm':
            raise Unsupported('cmpm')
        pre, s, post = self.read(i.ops[0], sz, a)
        if base == 'cmpa':
            dn = self.reg('a', i.ops[1][1])
            sv = s.t if sz == 'l' else '(uint32_t)(int16_t)%s' % s.t
            self.set_flags(i, 'cmp', 'l', dn, sv)
            self.out_stmts(i, pre, [], post)
            return
        dpre, d, dpost = self.read(i.ops[1], sz, a)
        if s.volatile or d.volatile:
            raise Unsupported('cmp of hardware')
        self.set_flags(i, 'cmp', sz, d.t, s.t)
        self.out_stmts(i, pre + dpre, [], post + dpost)

    def bitop(self, i):
        base, _, sz = i.mnem.partition('.')
        b, d = i.ops
        a = i.addr
        if base == 'btst':
            e = self.btst_expr(i)
            if e.volatile:
                t, ct = self.tmp('uint8_t')
                hw, _, bit = e.t.partition(' & ')
                self.stmt('%s = %s;' % (t, hw), i)
                e = Expr('%s & %s' % (t, bit))
                self.override[i.addr] = e
            self.set_flags(i, 'btst', 'b', e.t)
            return
        if self.flags_needed(a):
            self.set_flags(i, 'btst', 'b', self.btst_expr(i).t)
        if d[0] == 'd':
            dn = self.reg('d', d[1])
            bit = '0x%X' % (1 << (b[1] & 31)) if b[0] == 'q' else '(1u << (%s & 31))' % self.reg('d', b[1])
        else:
            bit = '0x%X' % (1 << (b[1] & 7)) if b[0] == 'q' else '(1u << (%s & 7))' % self.reg('d', b[1])
        opn = {'bset': '| ', 'bclr': '& ~', 'bchg': '^ '}[base]
        if d[0] == 'd':
            self.stmt('%s = %s %s%s;' % (dn, dn, opn, bit), i)
            return
        c = self.const_addr(d, a)
        if c is not None and (c & 0xE00000) == 0xE00000 and b[0] == 'q' and base != 'bchg':
            fn = {'bset': 'set8', 'bclr': 'clr8'}[base]
            self.stmt('%s(%s, %d);' % (fn, self.ram(c), b[1] & 7), i)
            return
        pre, v, post = self.read(d, 'b', a)
        wpre, w, wpost = self.write(d, 'b', '%s %s%s' % (v.t, opn, bit), a)
        self.out_stmts(i, pre, w, post)

    def unary(self, i):
        base, _, sz = i.mnem.partition('.')
        o = i.ops[0]
        a = i.addr
        fl = self.flags_needed(a)
        if base == 'ext':
            n = self.reg('d', o[1])
            if sz == 'w':
                v = '(int8_t)%s' % n
                w = ['SETW(%s, (int16_t)(int8_t)%s);' % (n, n)] if (self.out.get(a, ALL) >> (3 * o[1]) & 4) else ['%s = (uint16_t)(int16_t)(int8_t)%s;' % (n, n)]
            else:
                v = '(int16_t)%s' % n
                w = ['%s = (uint32_t)(int16_t)%s;' % (n, n)]
            self.out_stmts(i, [], w, [])
            if fl:
                self.set_flags(i, 'logic', sz, n if sz == 'l' else '(uint16_t)%s' % n)
            return
        if base == 'swap':
            n = self.reg('d', o[1])
            self.stmt('%s = %s << 16 | %s >> 16;' % (n, n, n), i)
            if fl:
                self.set_flags(i, 'logic', 'l', n)
            return
        if base == 'negx':
            raise Unsupported('negx')
        pre, v, post = self.read(o, sz, a)
        val = ('0 - %s' if base == 'neg' else '~%s') % v.t
        if fl:
            if base == 'neg':
                self.items.append(['stmt', ' '.join(pre), a])
                pre = []
                self.set_flags(i, 'sub', sz, '0', v.t)
            else:
                self.set_flags(i, 'logic', sz, '(%s)(%s)' % (UT[sz], val))
        wpre, w, wpost = self.write(o, sz, val, a)
        self.out_stmts(i, pre + wpre, w, post + wpost)

    def muldiv(self, i):
        base, _, sz = i.mnem.partition('.')
        s_op, d_op = i.ops
        a = i.addr
        pre, s, post = self.read(s_op, 'w', a)
        n = self.reg('d', d_op[1])
        if base == 'muls':
            val = '(uint32_t)((int32_t)(int16_t)%s * (int16_t)%s)' % (n, s.t)
        elif base == 'mulu':
            val = '(uint32_t)(uint16_t)%s * (uint16_t)%s' % (n, s.t)
        elif base == 'divu':
            val = 'divu_w(%s, %s)' % (n, s.t)
        else:
            val = 'divs_w(%s, (int16_t)%s)' % (n, s.t)
        self.out_stmts(i, pre, ['%s = %s;' % (n, val)], post)
        if self.flags_needed(a):
            self.set_flags(i, 'logic', 'l' if base.startswith('mul') else 'w', n if base.startswith('mul') else '(uint16_t)%s' % n)

    def shift(self, i):
        base, _, sz = i.mnem.partition('.')
        a = i.addr
        if len(i.ops) == 1:
            o, cnt = i.ops[0], '1'
            sz = 'w'
        else:
            c, o = i.ops
            cnt = str(c[1]) if c[0] == 'q' else '(%s & 63)' % self.reg('d', c[1])
        bits = BITS[sz]
        pre, v, post = self.read(o, sz, a)
        kind = base[:-1]
        left = base[-1] == 'l'
        fl = self.flags_needed(a)
        lines = list(pre)
        if fl:
            t, ct = self.tmp('uint32_t')
            lines.append('%s = %s;' % (t, v.t))
            v = Expr(t)
        if kind in ('as', 'ls') and left:
            val = '%s << %s' % (v.t, cnt) if cnt.isdigit() else 'lsl_n(%s, %s, %d)' % (v.t, cnt, bits)
        elif kind == 'ls':
            val = '%s >> %s' % (v.t, cnt) if cnt.isdigit() else 'lsr_n(%s, %s, %d)' % (v.t, cnt, bits)
        elif kind == 'as':
            val = '(%s)%s >> %s' % (ST[sz], v.t, cnt) if cnt.isdigit() else 'asr_n(%s, %s, %d)' % (v.t, cnt, bits)
        elif kind == 'ro':
            val = '%s_n(%s, %s, %d)' % ('rol' if left else 'ror', v.t, cnt, bits)
        else:
            raise Unsupported(base)
        if fl:
            self.items.append(['stmt', ' '.join(lines), a])
            lines = []
            out = 'out_left(%s, %s, %d)' % (v.t, cnt, bits) if left else 'out_right(%s, %s, %d)' % (v.t, cnt, bits)
            carry = '(%s ? CF | XF : 0)' % out if kind != 'ro' else '(%s ? CF : 0) | (ccr & XF)' % out
            self.used_ccr = True
            res = '(%s)(%s)' % (UT[sz], val)
            if kind == 'as' and left:
                note = ' /* asl: V not computed */'
            else:
                note = ''
            self.set_flags(i, 'other', sz, '(uint8_t)(ccr_nz(%s, %d) | %s)%s' % (res, bits, carry, note))
        wpre, w, wpost = self.write(o, sz, val, a)
        self.out_stmts(i, lines + wpre, w, post + wpost)

    def extended(self, i):
        base, _, sz = i.mnem.partition('.')
        s_op, d_op = i.ops
        if s_op[0] != 'd':
            raise Unsupported(base + ' -(An)')
        self.used_ccr = True
        s = self.reg('d', s_op[1])
        d = self.reg('d', d_op[1])
        if base in ('abcd', 'sbcd'):
            self.stmt('SETB(%s, %s((uint8_t)%s, (uint8_t)%s, &ccr));' % (d, base, d, s), i)
            return
        bits = BITS[sz]
        sign = '+' if base == 'addx' else '-'
        t = 'r'
        self.stmt('{ uint32_t %s = (%s)%s %s (%s)%s %s ((ccr & XF) != 0); uint8_t z = ccr & ZF; '
                  'ccr = %s((%s)%s, (uint32_t)(%s)%s + ((ccr & XF) != 0), %d); if ((%s)%s) ccr &= ~ZF; else ccr = (ccr & ~ZF) | z; %s }'
                  % (t, UT[sz], d, sign, UT[sz], s, sign, 'ccr_add' if base == 'addx' else 'ccr_sub', UT[sz], d, UT[sz], s, bits,
                     UT[sz], t, 'SET%s(%s, %s);' % (sz.upper(), d, t) if sz != 'l' else '%s = %s;' % (d, t)), i)

    def dbcc(self, i):
        cc = i.mnem[2:]
        n = self.reg('d', i.ops[0][1])
        loop = Cond('(uint16_t)%s != 0xFFFF' % n, '(uint16_t)%s == 0xFFFF' % n)
        if cc == 'f':
            self.items.append(['stmt', 'SETW(%s, %s - 1);' % (n, n), i.addr])
            self.items.append(['if', loop, i.target, i.addr])
            return
        c = self.cond(i.addr, cc)
        self.items.append(['stmt', 'if (%s) goto loc_%05X_exit;' % (c.t, i.addr), i.addr])
        self.items.append(['stmt', 'SETW(%s, %s - 1);' % (n, n), i.addr])
        self.items.append(['if', loop, i.target, i.addr])
        self.items.append(['raw', 'loc_%05X_exit:;' % i.addr])

    def sr(self, i):
        s, d = i.ops
        if d[0] == 'sr':
            if s[0] == 'imm':
                self.stmt('cpu_set_sr(0x%04X);' % s[1], i)
                return
            if s == ('inc', 7):
                self.stmt('cpu_set_sr(%s);' % self.pop_slot(i), i)
                return
        if s[0] == 'sr' and d == ('dec', 7):
            self.stmt('%s = cpu_get_sr();' % self.push_slot(i, 'uint16_t'), i)
            return
        if d[0] == 'ccr':
            pre, v, post = self.read(s, 'w', i.addr)
            self.used_ccr = True
            self.out_stmts(i, pre, ['ccr = (uint8_t)(%s & 0x1F);' % v.t], post)
            return
        raise Unsupported('sr ' + self.asm(i))

    # ---- the stack ----------------------------------------------------------------------------
    def slot_name(self, key):
        off, sz = key
        return 'stk%d' % (-off)

    def push_slot(self, i, ctype):
        dp = self.f.depth.get(i.addr)
        sz = i.mnem.partition('.')[2]
        n = 2 if sz in ('b', 'w') else 4
        name = 'stk%d' % (-(dp - n))
        self.locals.add((name, ctype))
        return name

    def pop_slot(self, i):
        dp = self.f.depth.get(i.addr)
        return 'stk%d' % (-dp)

    def stack_move(self, i):
        """move.x Rn,-(sp) and move.x (sp)+,Rn: a save into a C local and the restore"""
        s, d = i.ops
        sz = i.mnem.partition('.')[2]
        if d == ('dec', 7) and s[0] in ('d', 'a', 'imm'):
            name = self.push_slot(i, 'uint32_t')
            _, v, _ = self.read(s, sz, i.addr)
            self.stmt('%s = %s;' % (name, v.t), i)
            return True
        if s == ('inc', 7) and d[0] in ('d', 'a'):
            name = self.pop_slot(i)
            _, w, _ = self.write(d, sz, '%s' % name if sz == 'l' else '(%s)%s' % (UT[sz], name), i.addr)
            self.stmt(' '.join(w), i)
            return True
        if d == ('dec', 7) or s == ('inc', 7):
            raise Unsupported('stack ' + self.asm(i))
        for o in (s, d):
            if o[0] in ('disp', 'ind') and o[1] == 7:
                raise Unsupported('stack ' + self.asm(i))
        return False

    def movem(self, i):
        s, d = i.ops
        sz = i.mnem.partition('.')[2]
        n = 4 if sz == 'l' else 2
        dp = self.f.depth.get(i.addr)
        if s[0] == 'list' and d == ('dec', 7):
            rl = regs.list_regs(regs.reg_mask(s[1], True))
            if self.paired(i):
                self.items.append(['stmt', '/* saves %s */' % self.asm(i).split(' ', 1)[1].split(',')[0], i.addr])
                return
            off = dp - n * len(rl)
            parts = []
            for k, rg in enumerate(rl):
                name = 'stk%d' % (-(off + n * k))
                self.locals.add((name, 'uint32_t'))
                parts.append('%s = %s;' % (name, self.rname(rg)))
            self.stmt(' '.join(parts), i)
            return
        if s == ('inc', 7) and d[0] == 'list':
            rl = regs.list_regs(regs.reg_mask(d[1], False))
            if self.paired(i):
                self.items.append(['stmt', '/* restores */', i.addr])
                return
            parts = []
            for k, rg in enumerate(rl):
                parts.append('%s = %s;' % (self.rname(rg), 'stk%d' % (-(dp + n * k))))
            self.stmt(' '.join(parts), i)
            return
        # registers to or from memory
        pre, ad, post = self.addr(s if d[0] == 'list' else d, sz, i.addr)
        f = '32' if sz == 'l' else '16'
        lst = d if d[0] == 'list' else s
        rl = regs.list_regs(regs.reg_mask(lst[1], lst[2]))
        out = []
        base = ad.t
        if (s if d[0] == 'list' else d)[0] == 'dec':
            raise Unsupported('movem to -(An)')
        for k, rg in enumerate(rl):
            at = '%s + %d' % (base, n * k) if k else base
            if d[0] == 'list':
                v = 'bus_rd%s(%s)' % (f, at)
                if sz == 'w':
                    v = '(uint32_t)(int16_t)' + v
                out.append('%s = %s;' % (self.rname(rg), v))
            else:
                v = self.rname(rg) if sz == 'l' else '(uint16_t)%s' % self.rname(rg)
                out.append('bus_wr%s(%s, %s);' % (f, at, v))
        if post:
            post = ['%s += %d;' % (self.reg('a', s[1]), n * len(rl))]
        self.out_stmts(i, pre, out, post)

    def rname(self, rg):
        return self.reg('d', rg) if rg < 8 else (self.reg('a', rg - 8) if rg != 15 else 'sp')

    def paired(self, i):
        """a save at the entry that every return restores (or a restore of one)"""
        f = self.f
        for a, cps in ((a, f.ud[a][4]) for a in self.r.body):
            pass
        cps = f.ud[i.addr][4]
        return bool(cps) and all(True for _ in cps)

    # ---- calls, jumps, returns ----------------------------------------------------------------
    def call_text(self, t, name):
        ret, args, pn, rn, cc = self.s.decl(t, name)
        self.callees.add(t)
        params, results, ccr = self.s.of(t)
        argv = []
        for r, p in params:
            if (len(rn) > 1 or (cc and rn)) and r in rn:
                continue
            argv.append(self.reg(r[0], int(r[1])))
        lines = []
        mays = self.an.may[t]
        if len(rn) > 1 or (cc and rn):
            for r in rn:
                argv.append('&' + self.reg(r[0], int(r[1])))
            if cc:
                self.used_ccr = True
                argv.append('&ccr')
            lines.append('%s(%s);' % (name, ', '.join(argv)))
        elif len(rn) == 1:
            r = rn[0]
            reg_ = self.reg(r[0], int(r[1]))
            part = (mays >> (3 * int(r[1]))) & 7 if r[0] == 'd' else 7
            if part == 7 or r[0] == 'a' or (dict(params).get(r, 0) | part) == 7:
                lines.append('%s = %s(%s);' % (reg_, name, ', '.join(argv)))
            else:
                m = 0
                for k, bit in enumerate((0xFF, 0xFF00, 0xFFFF0000)):
                    if part >> k & 1:
                        m |= bit
                lines.append('%s = (%s & 0x%X) | (%s(%s) & 0x%X);' % (reg_, reg_, ~m & 0xFFFFFFFF, name, ', '.join(argv), m))
        elif cc:
            self.used_ccr = True
            lines.append('ccr = %s(%s);' % (name, ', '.join(argv)))
        else:
            lines.append('%s(%s);' % (name, ', '.join(argv)))
        return ' '.join(lines)

    def call(self, i):
        ts = self.r.calls.get(i.addr, [])
        if i.target is not None and len(ts) == 1:
            self.stmt(self.call_text(ts[0], self.p.routines[ts[0]].name), i)
            return
        if i.target is not None and i.target >= 0xFF0000:
            self.stmt('call68k(0x%X);   /* code in RAM */' % i.target, i)
            return
        pre, ad, post = self.addr(i.ops[0], 'l', i.addr)
        names = ', '.join(self.p.routines[t].name for t in ts[:6]) + (' ...' if len(ts) > 6 else '')
        pa = 0
        for t in ts:
            pa |= self.an.params[t]
        self.stmt('call68k(%s);   /* %d targets: %s; they take %s */' % (ad.t, len(ts), names, ', '.join(regs.describe(pa)) or 'nothing'), i)

    def ret_text(self):
        ret, args, pn, rn, cc = self.sig
        if len(rn) > 1 or (cc and rn):
            parts = ['*p_%s = %s;' % (r, self.reg(r[0], int(r[1]))) for r in rn]
            if cc:
                parts.append('*p_ccr = ccr;')
                self.used_ccr = True
            return ' '.join(parts + ['return;'])
        if len(rn) == 1:
            self.reg(rn[0][0], int(rn[0][1]))
            return 'return %s;' % rn[0]
        if cc:
            self.used_ccr = True
            return 'return ccr;'
        return 'return;'

    def ret(self, i):
        dp = self.f.depth.get(i.addr)
        if dp not in (0, None):
            self.items.append(['stmt', 'TODO(returns past its caller: the stack is %+d);' % dp, i.addr])
        self.items.append(['return', self.ret_text(), i.addr])

    def tail(self, t, i, how):
        name = self.p.routines[t].name
        txt = self.call_text(t, name)
        self.items.append(['stmt', txt, i.addr])
        self.items.append(['return', self.ret_text(), i.addr])

    def jump(self, i):
        if i.addr in self.r.table:
            self.switch(i)
            return
        tails = self.r.tails.get(i.addr, [])
        if tails and (i.target is not None or len(tails) == 1):
            self.tail(tails[0], i, 'jump')
            return
        if i.target is not None:
            self.items.append(['goto', None, i.target, i.addr])
            return
        pre, ad, post = self.addr(i.ops[0], 'l', i.addr)
        names = ', '.join(self.p.routines[t].name for t in tails[:6])
        self.stmt('call68k(%s); %s   /* jump: %s */' % (ad.t, self.ret_text(), names or 'no recorded target'), i)

    def switch(self, i):
        cases = self.r.table[i.addr]
        prev = self.p.insn.get(i.addr - 4) or self.p.insn.get(i.addr - 2)
        idx = None
        for back in (2, 4, 6):
            p = self.p.insn.get(i.addr - back)
            if p and p.ops and p.ops[0][0] == 'pcx':
                idx = self.xreg(p.ops[0][2], p.ops[0][3]).t
        if i.ops[0][0] == 'pcx':
            idx = self.xreg(i.ops[0][2], i.ops[0][3]).t
        lines = ['switch (%s) {' % (idx or '?')]
        for off, t in cases:
            if t in self.r.tails.get(i.addr, []) or t in self.p.entries:
                name = self.p.routines[t].name
                lines.append('case %s: %s %s' % (hx(off), self.call_text(t, name), self.ret_text()))
            else:
                lines.append('case %s: goto loc_%05X;' % (hx(off), t))
                self.labels.add(t)
        lines.append('}')
        self.items.append(['stmt', '\n'.join(lines), i.addr])
        self.items.append(['return', self.ret_text(), i.addr])

    # ---- the function -------------------------------------------------------------------------
    def render(self):
        self.emit_all()
        body = structure(self.items, self, self.p)
        ret, args, pn, rn, cc = self.sig
        s = self.an
        e = self.r.entry
        sig = regs.describe(s.params[e]), regs.describe(s.results(e))
        notes = signatures_notes(self)
        head = ['/* $%06X %s: %s -> %s. DRAFT by tools/disasm/draft.py: rewrite by hand before committing. */'
                % (e, self.r.name, ', '.join(sig[0]) or 'nothing', ', '.join(sig[1]) or 'nothing')]
        for n in notes:
            head.append('/*   %s */' % n)
        head.append('%s %s(%s)' % (ret, self.r.name, ', '.join(args) or 'void'))
        head.append('{')
        decl = []
        regs_ = sorted(x for x in self.locals if isinstance(x, str) and x not in pn and x != 'sp')
        if regs_:
            decl.append('    uint32_t %s;' % ', '.join('%s = 0' % x for x in regs_))
        for r in pn:
            if r in rn and (len(rn) > 1 or (cc and rn)):
                decl.append('    uint32_t %s = *p_%s;' % (r, r))
        for x in sorted(x for x in self.locals if isinstance(x, tuple)):
            decl.append('    %s %s = 0;' % (x[1], x[0]))
        if self.used_ccr:
            decl.append('    uint8_t ccr = %s;' % ('*p_ccr' if 'uint8_t *p_ccr' in args else '0'))
        return '\n'.join(head + decl + body + ['}'])


class Unsupported(Exception):
    pass


def signatures_notes(d):
    out = list(dict.fromkeys(d.f.notes))
    if d.an.leaks.get(d.r.entry):
        out.append('reads %s before writing it on some path, and no caller sets it (a register leak)'
                   % ', '.join(regs.describe(d.an.leaks[d.r.entry])))
    return out


# ---- structuring ---------------------------------------------------------------------------------
def structure(items, d, prog):
    """items -> indented lines: if/else and do/while where the gotos allow, goto elsewhere"""
    # a goto to a return: the return
    pos = {}
    for k, it in enumerate(items):
        if it[0] == 'label':
            pos[it[1]] = k
    norm = []
    for it in items:
        if it[0] in ('if', 'goto'):
            tgt = it[2]
            k = pos.get(tgt)
            if k is not None:
                j = k + 1
                while j < len(items) and items[j][0] == 'label':
                    j += 1
                if j < len(items) and items[j][0] == 'return':
                    norm.append(['ifret' if it[0] == 'if' else 'return', it[1] if it[0] == 'if' else items[j][1], items[j][1]] if it[0] == 'if' else ['return', items[j][1], None])
                    continue
            if tgt is not None and tgt not in pos:
                norm.append(it + ['outside'])
                continue
        norm.append(it)
    items = [it for k, it in enumerate(norm)
             if not (it[0] == 'goto' and it[1] is None and k + 1 < len(norm) and norm[k + 1][0] == 'label' and norm[k + 1][1] == it[2])]
    refs = {}
    for it in items:
        if it[0] in ('if', 'goto') and it[2] is not None:
            refs[it[2]] = refs.get(it[2], 0) + 1
    lines = []
    block(items, 0, len(items), refs, lines, 1)
    # labels nobody jumps to
    used = set()
    for l in lines:
        for m in re.finditer(r'goto (loc_[0-9A-F]+)', l):
            used.add(m.group(1))
    return [l for l in lines if not (re.fullmatch(r'\s*loc_[0-9A-F]+:;?', l) and l.strip().rstrip(':;') not in used)]


def find_label(items, i, j, a):
    for k in range(i, j):
        if items[k][0] == 'label' and items[k][1] == a:
            return k
    return None


def entered_only_by(items, i, j, refs, allowed):
    """no goto from outside [i, j) lands on a label inside it, other than the allowed jumps"""
    inside = {}
    for k in range(i, j):
        it = items[k]
        if it[0] in ('if', 'goto') and it[2] is not None:
            inside[it[2]] = inside.get(it[2], 0) + 1
    for k in range(i, j):
        it = items[k]
        if it[0] == 'label':
            a = it[1]
            n = refs.get(a, 0) - inside.get(a, 0) - allowed.get(a, 0)
            if n > 0:
                return False
    return True


def block(items, i, j, refs, out, depth):
    ind = '    ' * depth
    k = i
    while k < j:
        it = items[k]
        kind = it[0]
        if kind == 'label':
            # a loop: the label, a body, a conditional jump back to it
            back = None
            for m in range(j - 1, k, -1):
                if items[m][0] == 'if' and items[m][2] == it[1]:
                    back = m
                    break
            if back is not None and refs.get(it[1], 0) == 1 and entered_only_by(items, k + 1, back, refs, {}):
                out.append(ind + 'do {')
                block(items, k + 1, back, refs, out, depth + 1)
                out.append(ind + '} while (%s);' % items[back][1].t)
                k = back + 1
                continue
            out.append('%sloc_%05X:;' % ('    ' * max(depth - 1, 0), it[1]))
            k += 1
            continue
        if kind == 'if' and len(it) < 5:
            tgt = it[2]
            m = find_label(items, k + 1, j, tgt)
            if m is not None and entered_only_by(items, k + 1, m, refs, {}):
                # if / else: the then part ends with a goto past a second part
                last = items[m - 1] if m - 1 > k else None
                if last and last[0] == 'goto' and last[2] is not None and len(last) < 5:
                    n = find_label(items, m + 1, j, last[2])
                    if n is not None and refs.get(tgt, 0) == 1 and entered_only_by(items, m + 1, n, refs, {tgt: 1}):
                        out.append(ind + 'if (%s) {' % it[1].neg().t)
                        block(items, k + 1, m - 1, refs, out, depth + 1)
                        out.append(ind + '} else {')
                        refs[last[2]] -= 1
                        block(items, m + 1, n, refs, out, depth + 1)
                        out.append(ind + '}')
                        refs[tgt] -= 1
                        k = n
                        continue
                out.append(ind + 'if (%s) {' % it[1].neg().t)
                block(items, k + 1, m, refs, out, depth + 1)
                out.append(ind + '}')
                refs[tgt] -= 1
                k = m
                continue
            out.append(ind + 'if (%s) goto loc_%05X;' % (it[1].t, tgt))
            k += 1
            continue
        if kind == 'if':
            out.append(ind + 'if (%s) goto loc_%05X;   /* outside this routine */' % (it[1].t, it[2]))
        elif kind == 'ifret':
            out.append(ind + ('if (%s) %s' if it[2].count(';') == 1 else 'if (%s) { %s }') % (it[1].t, it[2]))
        elif kind == 'goto':
            out.append(ind + 'goto loc_%05X;' % it[2])
        elif kind == 'return':
            out.append(ind + it[1])
        elif kind == 'raw':
            out.append(ind + it[1])
        elif kind == 'stmt':
            text = it[1]
            if not text.strip():
                k += 1
                continue
            for n, l in enumerate(text.split('\n')):
                out.append(ind + ('    ' if n and not l.startswith('}') and not l.startswith('case') else '') + l)
        k += 1


def tree(prog, an, name, ported):
    """leaf-first order of what name calls"""
    r = prog.by_name(name)
    order, seen = [], set()

    def visit(e):
        if e in seen:
            return
        seen.add(e)
        rr = prog.routines[e]
        for s, ts in sorted(rr.calls.items()):
            for t in ts:
                visit(t)
        for s, ts in sorted(rr.tails.items()):
            for t in ts:
                visit(t)
        order.append(e)
    visit(r.entry)
    return order


def ported_set():
    out = set()
    for fn in os.listdir(os.path.join(ROOT, 'src')):
        if fn.endswith('.c'):
            for m in re.finditer(r'/\* \$([0-9A-F]{6})', open(os.path.join(ROOT, 'src', fn)).read()):
                out.add(int(m.group(1), 16))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('names', nargs='*')
    ap.add_argument('--tree', default='')
    ap.add_argument('--quiet', action='store_true')
    a = ap.parse_args()
    prog = Program()
    an = regs.Analysis(prog)
    an.solve()
    sigs = Sigs(prog, an)
    if a.tree:
        ported = ported_set()
        rows = tree(prog, an, a.tree, ported)
        for e in rows:
            r = prog.routines[e]
            mark = 'ported' if e in ported else ''
            print('%-8s %-14s %5d insns  %-40s %s' % (mark, r.name, len(r.body), sigs.proto(e, r.name)[:60], ''))
        print('%d routines, %d ported' % (len(rows), sum(1 for e in rows if e in ported)))
        return
    os.makedirs(os.path.join(ROOT, 'build', 'draft'), exist_ok=True)
    for n in a.names:
        r = prog.by_name(n)
        if not r:
            print('no routine', n)
            continue
        d = Draft(prog, an, sigs, r)
        text = d.render()
        protos = ['%s;' % sigs.proto(t, prog.routines[t].name) for t in sorted(d.callees) if t != r.entry]
        src = '#include "draft.h"\n' + ''.join(p + '\n' for p in protos) + '\n' + text + '\n'
        open(os.path.join(ROOT, 'build', 'draft', r.name + '.c'), 'w').write(src)
        if not a.quiet:
            print(text)
            print()


if __name__ == '__main__':
    main()

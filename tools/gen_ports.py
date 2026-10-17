#!/usr/bin/env python3
"""Write the tests' adapters (build/ports_gen.inc, included by test/ports.c) from src/game.h.

Every ported routine has its prototype in game.h with a comment naming the routine's address and
registers, which is all an adapter needs:

    uint8_t PadId(uint32_t port);                 /* $003156 a2 -> d0.b */
    void ReadPads(void);                          /* $003066 */
    int Collides(uint16_t obj, uint16_t *hit);    /* $012345 a5 -> Z, a1 */

Before the arrow, one register per parameter that is not a result pointer, in order (a suffix
.b or .w only documents the width; the parameter's C type takes it). After it, the results: the
first is the return value, the rest are the trailing pointer parameters, in order. A register
result is checked at its width (.b, .w, or all of it; an address register on 24 bits); a flag
(Z, N, C, or !Z, !N, !C) means the function returns whether it is set (or clear) as the 68k
left it; ccr means it returns the condition codes (a draft's); "a6=reg_a6" is a register the
C keeps in a global (set from the record, and checked after when it is a result). A routine
whose adapter needs more
is written by hand in test/ports.c and marked "manual" in its comment.

usage: gen_ports.py [DRAFT.c ...] [--out build/ports_gen.inc]
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import protos  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
REG = {**{'d%d' % i: 'R_D%d' % i for i in range(8)}, **{'a%d' % i: 'R_A%d' % i for i in range(8)}}


def adapter(p, draft=False):
    name, ret = p.name, p.ret
    vals, ptrs = p.values(), p.pointers()
    extra = p.results_after_return()
    if len(vals) != len(p.regs_in):
        raise ValueError('%s: %d parameters, %d registers' % (name, len(vals), len(p.regs_in)))
    if len(ptrs) != len(extra):
        raise ValueError('%s: %d result pointers, %d results after the first' % (name, len(ptrs), len(extra)))
    lines = ['static void run_%s(const Record *r)' % name, '{', '    (void)r;']
    args, decl = [], []
    vi = pi = 0
    for ctype, pname in p.params:
        if ctype.endswith('*'):
            base = ctype[:-1].strip()
            reg = extra[pi]
            init = ('(%s)r->regs_in[%s]' % (base, REG[reg[1]]) if reg[0] == 'reg'
                    else '(%s)(r->regs_in[R_SR] & 0x1F)' % base if reg[0] == 'ccr' else '0')
            decl.append('    %s %s = %s;' % (base, pname, init))
            args.append('&' + pname)
            pi += 1
        else:
            reg = p.regs_in[vi]
            if reg[0] != 'reg':
                raise ValueError('%s: a flag as a parameter' % name)
            args.append('(%s)r->regs_in[%s]' % (ctype, REG[reg[1]]))
            vi += 1
    lines += decl
    if draft:
        lines.append('    draft_unwind = 0;')
    for g in p.globs_in + p.globs_out:                   # a register the C keeps in a global
        lines.append('    %s = r->regs_in[%s];' % (g[2], REG[g[1]]))
    call = '%s(%s)' % (name, ', '.join(args))
    checks = []
    types = {pname: ctype[:-1].strip() for ctype, pname in ptrs}
    types['ret'] = ret
    if ret != 'void':
        lines.append('    %s ret = %s;' % (ret, call))
        checks.append((p.regs_out[0], 'ret'))
    else:
        lines.append('    %s;' % call)
    lines.append('    replay_end();                     /* the interrupts after the last access, before the checks */')
    for (ctype, pname), reg in zip(ptrs, extra):
        checks.append((reg, pname))
    for reg, v in checks:
        if reg[0] == 'none':
            lines.append('    (void)%s;' % v)
        elif reg[0] == 'flag':
            bit = {'Z': 'CCR_Z', 'N': 'CCR_N', 'C': 'CCR_C'}[reg[1]]
            want = '!%s(r)' % bit if reg[2] else '%s(r)' % bit
            lines.append('    expect_flag(r, (%s) != 0, %s, "%s%s");' % (v, want, '!' if reg[2] else '', reg[1]))
        elif reg[0] == 'ccr':
            lines.append('    expect_reg(r, R_SR, (uint32_t)%s, 0x1F, "ccr");' % v)
        else:
            r_ = REG[reg[1]]
            if reg[1][0] == 'a':
                m = '0xFFFF' if types.get(v) == 'uint16_t' else '0xFFFFFF'   # a RAM offset: its low word
                lines.append('    expect_reg(r, %s, (uint32_t)%s, %s, "%s");' % (r_, v, m, reg[1]))
            else:
                m = {'b': '0xFF', 'w': '0xFFFF', 'l': '0xFFFFFFFF'}[reg[2]]
                lines.append('    expect_reg(r, %s, (uint32_t)%s, %s, "%s");' % (r_, v, m, reg[1] + ('.' + reg[2] if reg[2] != 'l' else '')))
    for g in p.globs_out:
        lines.append('    expect_reg(r, %s, %s, %s, "%s");' % (REG[g[1]], g[2], '0xFFFFFF' if g[1][0] == 'a' else '0xFFFFFFFF', g[1]))
    lines.append('}')
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('drafts', nargs='*', help='drafts under test (build/draft/NAME.c): their prototypes too')
    ap.add_argument('--out', default=os.path.join(ROOT, 'build', 'ports_gen.inc'))
    ap.add_argument('--header', default=os.path.join(ROOT, 'src', 'game.h'))
    a = ap.parse_args()
    out = ['/* Generated by tools/gen_ports.py from src/game.h: the tests\' adapters. Do not edit. */']
    rows = []
    found = protos.load(a.header, *a.drafts)
    drafted = protos.load(*a.drafts) if a.drafts else {}
    for addr in sorted(drafted):
        p = drafted[addr]
        out.append('%s %s(%s);' % (p.ret, p.name, ', '.join('%s %s' % x for x in p.params) or 'void'))
    if drafted:
        out.append('extern int draft_unwind;')
    for addr in sorted(found):
        p = found[addr]
        if p.manual:
            continue
        out.append(adapter(p, addr in drafted))
        rows.append('    { 0x%06XU, "%s", run_%s },' % (addr, p.name, p.name))
    out.append('#define PORTS_GEN \\')
    out.append(' \\\n'.join(rows))
    out.append('')
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    text = '\n'.join(out) + '\n'
    if not os.path.exists(a.out) or open(a.out).read() != text:
        open(a.out, 'w').write(text)
    # call68k's cases: every ported routine that takes and returns nothing but the registers the
    # C keeps in globals (game.h's only)
    cases = ['/* Generated by tools/gen_ports.py from src/game.h: call68k\'s cases. Do not edit. */']
    for addr in sorted(protos.load(a.header)):
        p = found[addr]
        if p.ret == 'void' and not p.params:
            cases.append('    case 0x%06XU: %s(); return;' % (addr, p.name))
    disp = os.path.join(os.path.dirname(a.out), 'dispatch_gen.inc')
    dtext = '\n'.join(cases) + '\n'
    if not os.path.exists(disp) or open(disp).read() != dtext:
        open(disp, 'w').write(dtext)
    # call_object's: the ones that take an object (a5) and return nothing
    cases = ['/* Generated by tools/gen_ports.py from src/game.h: call_object\'s cases. Do not edit. */']
    for addr in sorted(protos.load(a.header)):
        p = found[addr]
        if p.ret == 'void' and len(p.params) == 1 and p.regs_in == [('reg', 'a5', 'l')] and not p.regs_out:
            cases.append('    case 0x%06XU: %s(obj); return;' % (addr, p.name))
    disp = os.path.join(os.path.dirname(a.out), 'dispatch_obj_gen.inc')
    dtext = '\n'.join(cases) + '\n'
    if not os.path.exists(disp) or open(disp).read() != dtext:
        open(disp, 'w').write(dtext)


if __name__ == '__main__':
    main()

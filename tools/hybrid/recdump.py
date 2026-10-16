#!/usr/bin/env python3
"""Dump one capture record: registers in / out, the nested calls with their names, optional RAM words.
usage: recdump.py FILE.cap INDEX [--ram ADDR[,ADDR...]] [--obj ADDR]"""
import argparse, json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from oracle import read_capture_at, ROOT  # noqa: E402

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('file'); ap.add_argument('index', type=int)
    ap.add_argument('--ram', default='', help='hex addresses (low 16 bits) to print as words, in and out')
    ap.add_argument('--obj', default='', help='an object\'s RAM address (hex): its first $80 bytes, in and out')
    a = ap.parse_args()
    syms = json.load(open(os.path.join(ROOT, 'build', 'disasm', 'symbols.json')))
    syms = syms.get('symbols', syms)
    inv = {}
    for k, v in syms.items():
        ad = v if isinstance(v, int) else v.get('addr')
        inv.setdefault(ad, k)
    r = read_capture_at(a.file, a.index)
    print(f'record {a.index}: {inv.get(r.pc, "?")} pc={r.pc:x} frame={r.frame} flags={r.flags} calls={len(r.calls)}')
    print('in :', ' '.join(f'{n}={v:08x}' for n, v in r.regs_in.items()))
    print('out:', ' '.join(f'{n}={v:08x}' for n, v in r.regs_out.items()))
    for c in r.calls:
        if c.pc & 0x20000000:
            st = bytes(c.stack) if not isinstance(c.stack, bytes) else c.stack
            print(f'  {"  " * (c.depth & 0xFF)}IRQ {c.pc & 0xFFFFFF:06x} at {int.from_bytes(st[2:6], "big"):06x} diff={len(c.diff) if hasattr(c.diff, "__len__") else c.diff}')
            continue
        if c.pc & 0xC0000000:
            ci = c.regs_in if isinstance(c.regs_in, dict) else dict(zip(r.regs_in.keys(), c.regs_in))
            kind = 'read ' if c.pc & 0x80000000 else 'write'
            print(f'  {"  " * (c.depth & 0xFF)}io {kind} {c.pc & 0xFFFFFF:06x}.{ci["d1"]} = {ci["d0"]:0{2 * ci["d1"]}x}')
            continue
        ci = c.regs_in if isinstance(c.regs_in, dict) else dict(zip(r.regs_in.keys(), c.regs_in))
        co = c.regs_out if isinstance(c.regs_out, dict) else dict(zip(r.regs_in.keys(), c.regs_out))
        nd = len(c.diff) if hasattr(c.diff, '__len__') else c.diff
        print(f'  {"  " * (c.depth & 0xFF)}{inv.get(c.pc, "?"):22s} d0={ci["d0"]:08x} d1={ci["d1"]:08x} d2={ci["d2"]:08x} d4={ci["d4"]:08x} a0={ci["a0"]:08x} a2={ci["a2"]:08x} a3={ci["a3"]:08x} -> d0={co["d0"]:08x} d1={co["d1"]:08x} diff={nd}')
    def w(ram, off): return ram[off] << 8 | ram[off + 1]
    for h in filter(None, a.ram.split(',')):
        o = int(h, 16) & 0xFFFF
        print(f'  RAM {o:04X}: in {w(r.ram_in, o):04x} out {w(r.ram_out, o):04x}')
    if a.obj:
        o = int(a.obj, 16) & 0xFFFF
        for row in range(0, 0x80, 16):
            ins = r.ram_in[o + row:o + row + 16].hex(' ')
            outs = r.ram_out[o + row:o + row + 16].hex(' ')
            print(f'  {o + row:04X} in  {ins}\n       out {outs}' if ins != outs else f'  {o + row:04X}     {ins}')

if __name__ == '__main__':
    main()

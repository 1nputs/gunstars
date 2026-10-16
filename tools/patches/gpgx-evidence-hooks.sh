#!/usr/bin/env bash
# Two more places for GPGX's CPU hook (HOOK_CPU builds only), for the disassembly's evidence
# (tools/hybrid/oracle_shim.c, "disassembly evidence"):
#  - the 68k's pc-relative data reads (move.w table(pc,d0.w),d0 and the like) reach the hook as
#    HOOK_M68K_R; GPGX reads them straight from memory, so the shim would not see tables read so;
#  - the Z80 calls it with HOOK_Z80_E before each instruction, with its PC.
# Idempotent. The core must be rebuilt after it (make -f Makefile.libretro HOOK_CPU=1).
set -e
H="$1/core/m68k/m68kcpu.h"
Z="$1/core/z80/z80.c"
if ! grep -q 'm68k_pcrel_hook' "$H"; then
  python3 - "$H" <<'EOF'
import sys
p = sys.argv[1]; s = open(p).read()
old = '''#define m68k_read_pcrelative_8(address)  READ_BYTE(m68ki_cpu.memory_map[((address)>>16)&0xff].base, (address) & 0xffff)
#define m68k_read_pcrelative_16(address) m68k_read_immediate_16(address)
#define m68k_read_pcrelative_32(address) m68k_read_immediate_32(address)'''
new = '''#ifdef HOOK_CPU
static inline unsigned int m68k_pcrel_hook(unsigned int address, int width, unsigned int value)
{
  if (cpu_hook) cpu_hook(HOOK_M68K_R, width, address, value);
  return value;
}
#define m68k_read_pcrelative_8(address)  m68k_pcrel_hook(address, 1, READ_BYTE(m68ki_cpu.memory_map[((address)>>16)&0xff].base, (address) & 0xffff))
#define m68k_read_pcrelative_16(address) m68k_pcrel_hook(address, 2, m68k_read_immediate_16(address))
#define m68k_read_pcrelative_32(address) m68k_pcrel_hook(address, 4, m68k_read_immediate_32(address))
#else
#define m68k_read_pcrelative_8(address)  READ_BYTE(m68ki_cpu.memory_map[((address)>>16)&0xff].base, (address) & 0xffff)
#define m68k_read_pcrelative_16(address) m68k_read_immediate_16(address)
#define m68k_read_pcrelative_32(address) m68k_read_immediate_32(address)
#endif'''
assert old in s, 'pcrel macros not found'
open(p, 'w').write(s.replace(old, new))
EOF
fi
if ! grep -q 'HOOK_Z80_E' "$Z"; then
  python3 - "$Z" <<'EOF'
import sys
p = sys.argv[1]; s = open(p).read()
old = '''    Z80.after_ei = FALSE;
    R++;
    EXEC_INLINE(op,ROP());
  }
} '''
new = '''    Z80.after_ei = FALSE;
#ifdef HOOK_CPU
    if (cpu_hook) cpu_hook(HOOK_Z80_E, 0, PCD, 0);
#endif
    R++;
    EXEC_INLINE(op,ROP());
  }
} '''
assert old in s, 'z80_run loop not found'
open(p, 'w').write(s.replace(old, new))
EOF
fi
grep -q 'm68k_pcrel_hook' "$H" && grep -q 'HOOK_Z80_E' "$Z" && echo "evidence hooks patched"

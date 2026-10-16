/* Recorded stand-ins for routines not ported yet (test/stubs.c): weak, so a port in src/
 * overrides its stub at link time. A stub takes the routine's next recorded call, checks the
 * arguments the C caller passed against the registers the 68k caller had, applies the routine's
 * RAM changes and returns its results:
 *
 *     STUB void sub_01234(uint16_t d0)
 *     {
 *         TAKE(sub_01234); ARG16(R_D0, d0, sub_01234);
 *         replay_apply(c);
 *     }
 */
#pragma once
#include "replay.h"
#include "symbols_gen.h"

#define STUB __attribute__((weak))
#define TAKE(name) const Call *c = replay_next(SYM_##name); if (!c) { replay_fail("no recorded call of " #name); return; }
#define TAKE_RET(name, val) const Call *c = replay_next(SYM_##name); if (!c) { replay_fail("no recorded call of " #name); return val; }
#define ARG8(reg, v, name) if ((uint8_t)c->regs_in[reg] != (uint8_t)(v)) replay_fail(#name ": " #reg " = %02x, recorded %02x", (uint8_t)(v), (uint8_t)c->regs_in[reg])
#define ARG16(reg, v, name) if ((uint16_t)c->regs_in[reg] != (uint16_t)(v)) replay_fail(#name ": " #reg " = %04x, recorded %04x", (uint16_t)(v), (uint16_t)c->regs_in[reg])
#define ARG32(reg, v, name) if (c->regs_in[reg] != (uint32_t)(v)) replay_fail(#name ": " #reg " = %08x, recorded %08x", (uint32_t)(v), c->regs_in[reg])
/* a 68k address argument: compared on 24 bits ($FFFFxxxx and $00FFxxxx are the same RAM) */
#define ARGADDR(reg, v, name) if ((c->regs_in[reg] & 0xFFFFFF) != ((uint32_t)(v) & 0xFFFFFF)) replay_fail(#name ": " #reg " = %06x, recorded %06x", (uint32_t)(v) & 0xFFFFFF, c->regs_in[reg] & 0xFFFFFF)
/* the flags a routine returned (its caller branches on them) */
#define CCR_Z(c) (((c)->regs_out[R_SR] >> 2) & 1)
#define CCR_N(c) (((c)->regs_out[R_SR] >> 3) & 1)
#define CCR_C(c) ((c)->regs_out[R_SR] & 1)

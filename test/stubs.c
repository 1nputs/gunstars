/* Stubs for the routines the ported ones call that are not ported yet (stubs.h has the pattern).
 * A routine with no stub and no port makes its callers' records fail with its recorded calls left
 * over, so every callee of a ported routine is in one place or the other. */
#include "game.h"
#include "stubs.h"
#include "ports.h"

/* call68k (a jsr through an address the game computed) to a routine dispatch.c does not know:
 * its recorded call */
void call68k_missing(uint32_t addr)
{
    for (const Port *p = ports; p->run; p++)       /* ported, so dispatch.c should have run it */
        if (p->pc == (addr & 0xFFFFFF)) { replay_fail("call68k has no case for %s, ported", p->name); return; }
    const Call *c = replay_next(addr & 0xFFFFFF);
    if (!c) { replay_fail("no recorded call of %06x (call68k)", addr & 0xFFFFFF); return; }
    replay_apply(c);
}

/* call_object's: the same, with the object checked against the recorded a5 */
void call_object_missing(uint32_t addr, uint16_t obj)
{
    for (const Port *p = ports; p->run; p++)
        if (p->pc == (addr & 0xFFFFFF)) { replay_fail("call_object has no case for %s, ported", p->name); return; }
    const Call *c = replay_next(addr & 0xFFFFFF);
    if (!c) { replay_fail("no recorded call of %06x (call_object)", addr & 0xFFFFFF); return; }
    if ((uint16_t)c->regs_in[R_A5] != obj) replay_fail("%06x: a5 = %04x, recorded %04x", addr & 0xFFFFFF, obj, (uint16_t)c->regs_in[R_A5]);
    replay_apply(c);
}


/* a routine of an object (a5) not ported yet */
#define OBJ_STUB(name) STUB void name(uint16_t obj) { TAKE(name); ARG16(R_A5, obj, name); replay_apply(c); }

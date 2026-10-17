/* Stubs for the routines the ported ones call that are not ported yet (stubs.h has the pattern).
 * A routine with no stub and no port makes its callers' records fail with its recorded calls left
 * over, so every callee of a ported routine is in one place or the other. */
#include "game.h"
#include "stubs.h"

/* call68k (a jsr through an address the game computed) to a routine dispatch.c does not know:
 * its recorded call */
void call68k_missing(uint32_t addr)
{
    const Call *c = replay_next(addr & 0xFFFFFF);
    if (!c) { replay_fail("no recorded call of %06x (call68k)", addr & 0xFFFFFF); return; }
    replay_apply(c);
}

/* $000CCA: the game's frame (every mode's routines) */
STUB void GameFrame(void)
{
    TAKE(GameFrame);
    replay_apply(c);
}

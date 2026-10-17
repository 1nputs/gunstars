/* jsr (a0) / jmp (a0) to an address the game computed (a table, a mode's routine): the C
 * function for it, for every ported routine that takes and returns nothing (the cases are
 * generated from game.h: build/dispatch_gen.inc). Addresses no port knows go to
 * call68k_missing, which the tests answer with the recorded call. */
#include "game.h"

void call68k(uint32_t addr)
{
    switch (addr & 0xFFFFFF) {
#include "dispatch_gen.inc"
    default: call68k_missing(addr); return;
    }
}

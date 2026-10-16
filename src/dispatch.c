/* jsr (a0) / jmp (a0) to an address the game computed (a table, an object's routine): the C
 * function for it. Addresses no port knows go to call68k_missing, which the tests answer with the
 * recorded call. */
#include "game.h"

void call68k(uint32_t addr)
{
    switch (addr & 0xFFFFFF) {
    case SYM_HintNone: HintNone(); return;
    case SYM_CheckPause: CheckPause(); return;
    default: call68k_missing(addr); return;
    }
}

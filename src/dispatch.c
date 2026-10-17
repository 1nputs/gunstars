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

/* The same for an object's routine (ObjectRoutines, a state table): the ones that take the
 * object (a5) and return nothing (build/dispatch_obj_gen.inc). */
void call_object(uint32_t addr, uint16_t obj)
{
    switch (addr & 0xFFFFFF) {
#include "dispatch_obj_gen.inc"
    default: call_object_missing(addr, obj); return;
    }
}

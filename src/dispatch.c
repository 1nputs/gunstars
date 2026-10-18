/* jsr (a0) / jmp (a0) to an address the game computed (a table, a mode's routine): the C
 * function for it, for every ported routine that takes and returns nothing (the cases are
 * generated from game.h: build/dispatch_gen.inc). Addresses no port knows go to
 * call68k_missing, which the tests answer with the recorded call. */
#include <stddef.h>
#include "game.h"

/* a routine that is a bare rts (gen_symbols.py's trivial_syms: the object table's empty entry
 * $BB0, a state that waits): nothing to do */
static int trivial(uint32_t addr)
{
    for (size_t i = 0; i < sizeof trivial_syms / sizeof *trivial_syms; i++)
        if (trivial_syms[i] == addr) return 1;
    return 0;
}

void call68k(uint32_t addr)
{
    switch (addr & 0xFFFFFF) {
#include "dispatch_gen.inc"
    default:
        if (!trivial(addr & 0xFFFFFF)) call68k_missing(addr);
        return;
    }
}

/* The same for an object's routine (ObjectRoutines, a state table): the ones that take the
 * object (a5) and return nothing (build/dispatch_obj_gen.inc). */
void call_object(uint32_t addr, uint16_t obj)
{
    switch (addr & 0xFFFFFF) {
#include "dispatch_obj_gen.inc"
    default:
        if (!trivial(addr & 0xFFFFFF)) call_object_missing(addr, obj);
        return;
    }
}

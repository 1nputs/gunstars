/* One adapter per ported routine: the recorded call's registers into the C function's arguments,
 * the call, and its register results checked. Most are generated from game.h's prototypes
 * (tools/gen_ports.py, build/ports_gen.inc); the ones that need more are written here, their
 * prototypes marked "manual". */
#include "game.h"
#include "ports.h"
#include "stubs.h"
#include "ports_gen.inc"

const Port ports[] = {
    PORTS_GEN
    { 0, NULL, NULL },
};

/* the sprite mappings' per-piece continuations, reached by jmp (a0) */
const uint32_t inner_labels[] = { 0x001FE0, 0x001FF0, 0x0020A8, 0x0020B6, 0x00216E, 0x00217C, 0 };

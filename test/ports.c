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

/* labels that are not routines of their own: the sprite mappings' per-piece continuations
 * (reached by jmp (a0)) */
const uint32_t inner_labels[] = { 0x001FE0, 0x001FF0, 0x0020A8, 0x0020B6, 0x00216E, 0x00217C,
    /* the Treasure logo's fill in RAM: its masks, a band's merge, an edge's walk, the next cell,
     * the runs of and.l it calls into */
    0xFF2000, 0xFF2148, 0xFF2212, 0xFF2348,
    0xFF2378, 0xFF237A, 0xFF237C, 0xFF237E, 0xFF2380, 0xFF2382, 0xFF2384, 0xFF2386, 0xFF2388, 0 };

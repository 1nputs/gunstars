/* One adapter per ported routine: the recorded call's registers into the C function's arguments,
 * the call, and its register results checked. Most are generated from game.h's prototypes
 * (tools/gen_ports.py, build/ports_gen.inc); the ones that need more are written here, their
 * prototypes marked "manual". */
#include "game.h"
#include "ports.h"
#include "stubs.h"
#include "ports_gen.inc"

/* the collision lists' ends as the 68k keeps them, in d4, a0, a1, a2, a3, d5, d6, d7 */
static const int ends_reg[8] = { R_D4, R_A0, R_A1, R_A2, R_A3, R_D5, R_D6, R_D7 };

static void ends_in(const Record *r, uint16_t ends[8])
{
    for (int i = 0; i < 8; i++) ends[i] = (uint16_t)r->regs_in[ends_reg[i]];
}

static void ends_out(const Record *r, const uint16_t ends[8])
{
    for (int i = 0; i < 8; i++) expect_reg(r, ends_reg[i], ends[i], 0xFFFF, "a list's end");
}

/* $00B250: the drawn objects into the lists */
static void run_CollisionListsDrawn(const Record *r)
{
    uint16_t ends[8];
    ends_in(r, ends);
    CollisionListsDrawn(ends);
    replay_end();
    ends_out(r, ends);
}

/* $00B260: an object into the lists; entered from $00B250's loop too, when it goes on with the
 * rest of the draw list (d1 more, from a4) */
static void run_CollisionAdd(const Record *r)
{
    uint16_t ends[8];
    ends_in(r, ends);
    uint16_t obj = (uint16_t)r->regs_in[R_A5], n = (uint16_t)r->regs_in[R_D1], at = (uint16_t)r->regs_in[R_A4];
    for (;;) {
        CollisionAdd(obj, ends);
        if (n-- == 0) break;
        obj = rd16(at);
        at = (uint16_t)(at + 2);
    }
    replay_end();
    ends_out(r, ends);
}

const Port ports[] = {
    PORTS_GEN
    { 0x00B250U, "CollisionListsDrawn", run_CollisionListsDrawn },
    { 0x00B260U, "CollisionAdd", run_CollisionAdd },
    { 0, NULL, NULL },
};

/* labels that are not routines of their own: the sprite mappings' per-piece continuations
 * (reached by jmp (a0)) */
const uint32_t inner_labels[] = { 0x001FE0, 0x001FF0, 0x0020A8, 0x0020B6, 0x00216E, 0x00217C,
    /* the Treasure logo's fill in RAM: its masks, a band's merge, an edge's walk, the next cell,
     * the runs of and.l it calls into */
    0xFF2000, 0xFF2148, 0xFF2212, 0xFF2348,
    0xFF2378, 0xFF237A, 0xFF237C, 0xFF237E, 0xFF2380, 0xFF2382, 0xFF2384, 0xFF2386, 0xFF2388, 0 };

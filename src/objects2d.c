/* Type $1F8: the soldiers' captain, a soldier fought where the camera is held (a spawner becomes
 * one, $0321A0).
 *
 * It is the soldier (soldier.c) made bigger, with its own copy of the soldier's code: a kind (+6,
 * from the spawner: an offset into the table at $2F764) gives its decision row, its flags (+$56:
 * byte $56 bit 1 guards, bit 2 grabs, bit 4 a companion ($570) at the higher levels; byte $57
 * bit 0 drops in from above, bit 7 a status bar event when it is gone), its vitality, its energy
 * (+$4E, copied to +$5C) and where it starts. Every 8 frames on the ground it looks at the nearer
 * player and takes the action its row gives for the distance. Hits count down a counter shared
 * by the captains ($8092, 6 to 9): spent, it blinks unhurt for a while (+$5A, counting down by
 * $100) and is knocked back. It is kept between x $80 and $1C0 (and below y $8C); beaten, it
 * falls, blows apart, drops an item and lets the camera go. A step (+4) is one of the 73
 * routines of the table at $2D612. */
#include "game.h"

#define C_TIMER   0x48           /* the step's counter (word; byte $49 for the grab's shake) */
#define C_COUNT   0x4A
#define C_AUX     0x4C
#define C_ENERGY0 0x4E
#define C_CLOCK   0x50           /* counts down by 1 a frame (a random start): a clock */
#define C_MASK    0x54           /* added to the chance rolls ($7F) */
#define C_KIND    0x56           /* bytes $56 and $57: the kind's flags */
#define C_LINK    0x58           /* the helper it spawned (its knife, its kick) */
#define C_BLINK   0x5A           /* its blinking: counts down by $100; negative: none */
#define C_ENERGY  0x5C
#define C_MODE    0x5E           /* byte: bits 0-4 what it may do now */

#define BOSS_FLAGS 0x8080        /* bit 0: it is beaten, bit 1: it died */
#define HITS       0x8092        /* word: the hits left before a captain blinks */
#define LEVEL      0xA20C        /* word: the level (0-$E, by 2) */
#define C_TABLE    0x02D612u     /* the steps */
#define C_BLINKS   0x02F75C      /* the blinking's time by difficulty */
#define C_THINK    0x02F4BA      /* the decisions: offsets from here */

static void set_state(uint16_t obj, uint16_t s) { wr16(obj + O_STATE, s); }
static void next(uint16_t obj, uint16_t by) { wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + by)); }
static void look(uint16_t obj, uint32_t map) { wr32(obj + 0x08, map); wr16(obj + 0x0C, 0); }
static void mode(uint16_t obj, uint8_t keep, uint8_t set)
{
    wr8(obj + C_MODE, (uint8_t)((rd8(obj + C_MODE) & keep) | set));
}
static void spend(uint16_t obj, uint16_t n) { wr16(obj + C_ENERGY, (uint16_t)(rd16(obj + C_ENERGY) - n)); }
static int dec(uint16_t obj, uint16_t field)
{
    wr16(obj + field, (uint16_t)(rd16(obj + field) - 1));
    return (rd16(obj + field) & 0x8000) != 0;   /* now below 0 */
}
static void body(uint16_t obj, uint32_t box)
{
    wr32(obj + 0x28, box);
    wr8(obj + 0x21, 0x2A);
}
static int flipped(uint16_t obj) { return bit8(obj + 0x0E, 3); }
static int facing_left(uint16_t obj) { return (rd16(obj + 0x0E) & 0x800) != 0; }
static uint16_t clock(uint16_t obj) { return rd16(obj + C_CLOCK); }
static uint16_t seed(void) { return rd16(v_randomSeed); }

/* its floor (16 below): 0 when none; d3, d4 its attribute and kind */
static int floor16(uint16_t obj, uint16_t *d3, uint16_t *d4) { return !ObjectFloor16(obj, d3, d4); }
static int floor16_any(uint16_t obj) { uint16_t d3 = 0, d4 = 0; return floor16(obj, &d3, &d4); }
static int walls8(uint16_t obj) { uint16_t d3 = 0, d4 = 0; return !ObjectWalls8(obj, &d3, &d4); }
static uint16_t walls_up(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectWalls(obj, 0xFFF8, &d3, &d4);
    return d3;
}
static int ceiling16(uint16_t obj) { uint16_t d3 = 0, d4 = 0; return !ObjectCeiling(obj, 0xFFF0, &d3, &d4); }
static int floor_up16(uint16_t obj) { uint16_t d3 = 0, d4 = 0; return !ObjectFloorUp16(obj, &d3, &d4); }

/* ---- the captain's transitions (each sets a step and its look) ---- */

/* loc_02EC04: guarding (unless already) */
static void guard(uint16_t obj)
{
    uint16_t s = rd16(obj + O_STATE);
    if (s == 0x9C || s == 0xA0 || s == 0xA4) return;
    set_state(obj, 0x9C);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DDA04);
    mode(obj, 0xE0, 0);
    body(obj, 0xF410F808);
}

/* loc_02EBC0: standing (or guarding, for a guard) */
static void stand(uint16_t obj)
{
    if (bit8(obj + C_KIND, 1)) { guard(obj); return; }
    set_state(obj, 4);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DD970);
    mode(obj, 0xE0, 0);
    body(obj, 0xF410F808);
}

/* loc_02F01E: falling, its counters kept */
static void fall_keep(uint16_t obj)
{
    set_state(obj, 0x40);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DDB78);
    mode(obj, 0xF6, 0x16);
}

/* $02F012. Falling. */
void CaptainFall(uint16_t obj)
{
    wr16(obj + C_TIMER, 0);
    wr16(obj + C_COUNT, 0);
    fall_keep(obj);
}

/* $02EC96. Tired (its energy spent): resting $50 frames in a pose by the floor's kind (no
 * floor: falling). */
void CaptainTire(uint16_t obj)
{
    set_state(obj, 0x30);
    wr16(obj + O_FLAGS, 0xED00);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    wr16(obj + C_COUNT, 0x50);
    mode(obj, 0xE2, 2);
    body(obj, 0xF410F808);
    wr16(obj + C_ENERGY, rd16(obj + C_ENERGY0));
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { CaptainFall(obj); return; }
    if (d4 == 4 || d4 == 0x10 || d4 == 0x14) { wr32(obj + 0x08, 0x0DDC34); return; }
    if (!flipped(obj)) wr32(obj + 0x08, d4 == 0xC ? 0x0DE24C : 0x0DE298);
    else wr32(obj + 0x08, d4 == 8 ? 0x0DE24C : 0x0DE298);
}

/* $02ED44. Tired, crouching. */
void CaptainTireCrouch(uint16_t obj)
{
    set_state(obj, 0x30);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DE02C);
    wr16(obj + C_ENERGY, rd16(obj + C_ENERGY0));
    wr32(obj + 0x18, 0);
    wr16(obj + C_COUNT, 0x50);
    mode(obj, 0xE2, 2);
    body(obj, 0x10F808);
}

/* loc_02EDD2: about to shoot ($32 frames), its look by the floor's slope and its facing */
static void aim(uint16_t obj)
{
    set_state(obj, 0x34);
    wr16(obj + O_FLAGS, 0xED00);
    wr16(obj + 0x0C, 0);
    wr16(obj + C_TIMER, 0x32);
    wr32(obj + 0x18, 0);
    mode(obj, 0xF7, 0x17);
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { CaptainFall(obj); return; }
    uint32_t m = 0x0DE198;
    if (d4 == 8) m = flipped(obj) ? 0x0DE48C : 0x0DE198;
    else if (d4 == 0xC) m = flipped(obj) ? 0x0DE198 : 0x0DE48C;
    wr32(obj + 0x08, m);
}

/* loc_02EE5E: aiming up ($1E frames) */
static void aim_up(uint16_t obj)
{
    set_state(obj, 0xE0);
    wr16(obj + O_FLAGS, 0xCD00);
    wr16(obj + 0x0C, 0);
    wr16(obj + C_TIMER, 0x1E);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x08, 0x0DDDB0);
    mode(obj, 0xF7, 0x17);
}

/* loc_02EEA4, loc_02EEF2, loc_02EF12: jumping (6, 4.5 or 7.5 up, less a random 0-1) */
static void jump_with(uint16_t obj, uint32_t base)
{
    set_state(obj, 0x4C);
    spend(obj, 0x10);
    wr32(obj + 0x1C, (uint32_t)(seed() & 0xFF00) + base);
    wr16(obj + C_TIMER, 0);
    wr16(obj + C_COUNT, 0);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DDB2C);
    mode(obj, 0xF6, 0x16);
}
static void jump_a(uint16_t obj) { jump_with(obj, 0xFFFA0000); }
static void jump_b(uint16_t obj) { jump_with(obj, 0xFFFB8000); }
static void jump_c(uint16_t obj) { jump_with(obj, 0xFFF88000); }

/* loc_02EF3C: landing hard */
static void land(uint16_t obj)
{
    set_state(obj, 0x70);
    look(obj, 0x0DDDA8);
    wr32(obj + 0x18, 0);
    wr16(obj + C_LINK, 0);
    wr16(obj + C_TIMER, 0xC);
    wr16(obj + C_COUNT, 8);
    mode(obj, 0xF6, 0x16);
}

/* loc_02EF78: jumping up to a ledge, unless a ceiling cell is 30 above */
static void jump_up(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectFloorUp30(obj, &d3, &d4);
    if (d3 & 8) return;
    set_state(obj, 0x4C);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DDBC4);
    wr32(obj + 0x1C, 0xFFFB8000);
    spend(obj, 0xA);
    mode(obj, 0xF6, 0x16);
}

/* loc_02EFBC, loc_02EFC6: crouching (on a ledge, $D4; under one, $118) */
static void crouch(uint16_t obj, uint16_t state)
{
    set_state(obj, state);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DDDA8);
    mode(obj, 0xE6, 6);
    body(obj, 0xF410F808);
}

/* loc_02F046: a little hop back */
static void hop_back(uint16_t obj)
{
    set_state(obj, 0xE8);
    mode(obj, 0xF6, 0x16);
    wr16(obj + C_COUNT, 2);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DEA64);
    wr32(obj + 0x1C, 0xFFFC8000);
    wr32(obj + 0x18, flipped(obj) ? 0x8000 : 0xFFFF8000);
}

/* loc_02EFFC: off a ledge: jumping (on odd randoms), hopping, or falling */
static void off_ledge(uint16_t obj)
{
    if (bit8(v_randomSeed + 1, 0)) { jump_a(obj); return; }
    if (!(clock(obj) & 3)) { hop_back(obj); return; }
    CaptainFall(obj);
}

/* $02F098. A small hop (not under a ceiling cell). */
void CaptainHop(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectFloor16(obj, &d3, &d4);
    if (d3 & 8) return;
    wr16(obj + C_TIMER, 0x10);
    wr16(obj + C_COUNT, 0x10);
    set_state(obj, 0x40);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DDBC4);
    mode(obj, 0xF6, 0x16);
}

/* loc_02F0DA: throwing (a grenade) */
static void throw_start(uint16_t obj)
{
    set_state(obj, 0x48);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DE734);
    wr32(obj + 0x18, 0);
    wr16(obj + C_COUNT, 0x20);
    spend(obj, 0x20);
    mode(obj, 0xF6, 0x16);
}

/* loc_02F116, loc_02F120: throwing from a crouch */
static void throw_low(uint16_t obj, uint16_t state)
{
    set_state(obj, state);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DE890);
    wr32(obj + 0x18, 0);
    wr16(obj + C_COUNT, 0x20);
    spend(obj, 0x20);
    mode(obj, 0xF6, 0x16);
}

/* loc_02F15C: setting up a mortar ($1FC) beside it */
static void mortar(uint16_t obj)
{
    set_state(obj, 0x50);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DE02C);
    wr32(obj + 0x18, 0);
    spend(obj, 8);
    mode(obj, 0xF7, 0x17);
    body(obj, 0x10F808);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1FC);
    wr16(slot + O_FLAGS, 0x8100);
    wr16(slot + 0x0E, 0xE6A4);
    wr16(slot + 0x08, 0x500);
    wr16(slot + 0x0A, 0xF8F8);
    wr16(slot + 0x4A, 4);
    wr16(slot + 0x4C, 0x50);
    wr16(slot + 0x58, obj);
    uint16_t n = rom16(0x02F216 + (uint32_t)(int16_t)rd16(v_difficulty));
    wr16(slot + 0x48, n);
    wr16(obj + C_COUNT, n);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10));
    if (flipped(obj)) wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) - 0xC));
    else wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + 0xC));
}

/* loc_02F21E: crouching to shoot */
static void crouch_shoot(uint16_t obj)
{
    set_state(obj, 0x5C);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DDE8C);
    wr32(obj + 0x18, 0);
    wr16(obj + C_COUNT, 0x40);
    spend(obj, 8);
    mode(obj, 0xF6, 0x16);
}

/* loc_02F288: a charge (spending a lot), its box reaching forward */
static void charge(uint16_t obj)
{
    set_state(obj, 0x90);
    spend(obj, 0x80);
    mode(obj, 0xF6, 0x16);
    wr32(obj + 0x2C, 0xF808E818);
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x2B);
}

/* loc_02F2B8: a punch */
static void punch(uint16_t obj)
{
    set_state(obj, 0xF0);
    mode(obj, 0xF6, 0x16);
    wr32(obj + 0x2C, 0xF808E818);
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x2B);
}

/* $02F2E2. Staggered (a hit on the ground). */
void CaptainStagger(uint16_t obj)
{
    set_state(obj, 0xB4);
    wr16(obj + O_FLAGS, 0xCD00);
    look(obj, 0x0DE9A8);
    wr16(obj + C_TIMER, 7);
    wr32(obj + 0x18, 0);
    mode(obj, 0xF7, 0x17);
}

/* $02F324. Knocked back up (away from its facing), gaining energy (8-39). */
void CaptainKnocked(uint16_t obj)
{
    set_state(obj, 0xC0);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DE978);
    wr32(obj + 0x1C, 0xFFFC8000);
    wr16(obj + C_ENERGY, (uint16_t)(rd16(obj + C_ENERGY) + (uint16_t)((rd8(v_randomSeed + 1) & 0x1F) + 8)));
    mode(obj, 0xF7, 0x17);
    body(obj, 0xF410F808);
    wr16(obj + C_TIMER, 0x40);
    wr16(obj + C_COUNT, 0);
    wr32(obj + 0x18, facing_left(obj) ? 0xC000 : 0xFFFF4000);
}

/* loc_02F318: knocked back by `by`'s hit: vitality its +$4A less the hit's power */
static void knocked_by(uint16_t obj, uint16_t by)
{
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + C_COUNT) - rd16(by + 0x26)));
    CaptainKnocked(obj);
}

/* $02F39A. Grabbing a player: shaking it $10 frames (its vitality kept in +$4A). */
void CaptainGrab(uint16_t obj)
{
    ObjectRelease(obj);
    set_state(obj, 0x108);
    wr16(obj + O_FLAGS, 0xCD00);
    look(obj, 0x0DEDF8);
    wr16(obj + C_TIMER, 0x10);
    wr32(obj + 0x18, 0);
    wr16(obj + C_COUNT, rd16(obj + O_VITALITY));
    mode(obj, 0xFF, 0x1F);
    body(obj, 0xF410F808);
}

/* loc_02F3E4: beaten: points, the boss's flag (+$8080 bit 1), falling back harmless (step $C4) */
static void beaten(uint16_t obj)
{
    AddScore(obj, 0x3300);
    set8(BOSS_FLAGS, 1);
    wr8(obj + 0x21, 0);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_VITALITY, 0);
    set_state(obj, 0xC4);
    wr16(obj + O_FLAGS, 0xCD00);
    wr32(obj + 0x08, 0x0DEAE0);
    wr16(obj + C_COUNT, 0x20);
    mode(obj, 0xFF, 0x1F);
    wr32(obj + 0x1C, 0xFFFE0000);
    wr32(obj + 0x18, flipped(obj) ? 0x8000 : 0xFFFF8000);
}

/* loc_02F44E: knocked flying by a hit from below: thrown, it hurts the enemies it meets */
static void knocked_up(uint16_t obj)
{
    set_state(obj, 0x80);
    look(obj, 0x0DDBC4);
    wr16(obj + O_FLAGS, 0xED00);
    wr16(obj + C_TIMER, 0x10);
    wr16(obj + 0x26, 0x20);
    wr32(obj + 0x2C, 0xF808F808);
    wr8(obj + 0x21, 0x4A);
    wr8(obj + 0x23, 0x20);
    mode(obj, 0xF6, 0x16);
}

/* loc_02F496: against a wall: turning back */
static void turn_back(uint16_t obj)
{
    set_state(obj, 0x100);
    wr32(obj + 0x18, flipped(obj) ? 0xFFFF0000 : 0x10000);
}

/* loc_02F60A: down: standing, or falling */
static void settle(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    if (!ObjectFloor(obj, 0x10, &d3, &d4)) stand(obj);
    else CaptainFall(obj);
}

/* loc_02DDD6 (and its copies loc_02DEB0, loc_02DF38): stepping off (a ledge's end) at 1/2,
 * falling */
static void step_off(uint16_t obj)
{
    wr16(obj + C_TIMER, 0x20);
    wr16(obj + C_COUNT, 0);
    wr32(obj + 0x18, flipped(obj) ? 0xFFFF8000 : 0x8000);
    fall_keep(obj);
}

/* loc_02DD34: let go: its vitality back, standing */
static void let_go(uint16_t obj)
{
    wr16(obj + O_VITALITY, rd16(obj + C_COUNT));
    stand(obj);
}

/* ---- what the captain spawns ---- */

/* loc_02EA6A: its knife ($A8) in a slot of $CE00-$D3FF, linked both ways, +$48 its time */
static void knife(uint16_t obj, uint16_t time)
{
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + 0x48, time);
    wr16(slot + 0x4C, 0);
    wr16(slot + 0x58, obj);
    wr16(obj + C_LINK, slot);
    wr16(slot + O_TYPE, 0xA8);
    wr8(slot + 0x21, 0x10);
    wr16(slot + 0x26, 6);
    if (seed() == 0x7650) wr16(slot + 0x26, 0x32);
    if (flipped(obj)) {
        wr16(slot + 0x4A, 0xFFE4);
        wr32(slot + 0x2C, 0xF609F618);
        return;
    }
    wr16(slot + 0x4A, 0x1C);
    wr32(slot + 0x2C, 0xF609E80A);
}

/* loc_02EAD2: its knife thrust at `reach` */
static void thrust(uint16_t obj, uint16_t reach)
{
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + 0x48, 0x1C);
    wr16(slot + 0x4A, reach);
    wr16(slot + 0x4C, 8);
    wr16(slot + 0x58, obj);
    wr16(obj + C_LINK, slot);
    wr16(slot + O_TYPE, 0xA8);
    wr8(slot + 0x21, 0x10);
    wr32(slot + 0x2C, 0xFC03FA06);
    wr16(slot + 0x26, rd16(obj + C_COUNT) ? 8 : 4);
}

/* $02EB84. A spark ($100: its speeds vx, vy as words, its animation list `data`) at its feet. */
void CaptainSpark(uint16_t obj, uint16_t vx, uint16_t vy, uint32_t data)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_X, (uint16_t)(rd16(obj + O_X) - 4));
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) + 0xC));
    wr16(slot + O_TYPE, 0x100);
    wr16(slot + 0x18, vx);
    wr16(slot + 0x1C, vy);
    wr32(slot + 0x48, data);
}

/* $02EB1E. Set to shoot at the player it goes after: the shot's type (d7: $90; $91 on a slope,
 * faced up it), its tables (a1 the facing's mappings, a2 the muzzles); off the ground it falls
 * instead (d7, a1, a2 left as they were). */
void CaptainAimSetup(uint16_t obj, uint16_t *target, uint16_t *type, uint32_t *faces, uint32_t *muzzles)
{
    *target = PlayerWeaker();
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { CaptainFall(obj); return; }
    if (d4 == 8) set8(obj + 0x0E, 3);
    else if (d4 == 0xC) clr8(obj + 0x0E, 3);
    else {
        *type = 0x90;
        *faces = 0x02AD36;
        *muzzles = 0x02AD56;
        return;
    }
    *type = 0x91;
    *faces = 0x02AD86;
    *muzzles = 0x02ADA6;
}

/* ---- the captain's decisions (by its row and the player's distance) ---- */

/* $02F4BA */
void CaptainThinkNone(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)obj; (void)adx; (void)dx; (void)dy;
}

/* $02F4BC. Near: a charge; else jump at a player above, hop under one below, throw. */
void CaptainThinkA(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)dx;
    if (!ChanceRoll(obj, 0x6F)) {
        if ((int16_t)(adx - 0xC) >= 0) return;
        charge(obj);
        return;
    }
    if ((int16_t)(dy + 0x18) >= 0) {
        if ((int16_t)(dy - 0x18) >= 0) { CaptainHop(obj); return; }
        if (!ChanceRoll(obj, 0x98)) { throw_start(obj); return; }
    }
    if (!ChanceRoll(obj, 0xA0)) jump_a(obj);
}

/* $02F504. Jump at a player above, hop under one below; else (by chance) a punch. */
void CaptainThinkB(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx;
    if ((int16_t)(dy + 0x18) < 0) { jump_a(obj); return; }
    if ((int16_t)(dy - 0x18) >= 0) { CaptainHop(obj); return; }
    if (!ChanceRoll(obj, 0x74)) punch(obj);
}

/* $02F550. A mortar (on the hardest difficulties), or a jump. */
void CaptainThinkC(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if ((int16_t)(rd16(v_difficulty) - 4) >= 0 && !ChanceRoll(obj, 8)) { mortar(obj); return; }
    if (!ChanceRoll(obj, 0x80)) jump_a(obj);
}

/* $02F578 (reached only through the row table at $2F6EC: no run has decoded it). Guarding. */
void CaptainThinkD(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!ChanceRoll(obj, 8)) guard(obj);
}

/* $02F588. 1 in 16, about to shoot. */
void CaptainThinkE(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!(seed() & 0xF)) aim(obj);
}

/* $02F596. Jump at a player above (by chance), hop under one below. */
void CaptainThinkF(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx;
    if ((int16_t)(dy + 0x18) < 0) {
        if (!ChanceRoll(obj, 0xA0)) jump_a(obj);
        return;
    }
    if ((int16_t)(dy - 0x18) >= 0) CaptainHop(obj);
}

/* $02F5B8. The same, the jump lower. */
void CaptainThinkG(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx;
    if ((int16_t)(dy + 0x18) < 0) {
        if (!ChanceRoll(obj, 0xA0)) jump_b(obj);
        return;
    }
    if ((int16_t)(dy - 0x18) >= 0) CaptainHop(obj);
}

/* $02F5DA. Crouching to shoot. */
void CaptainThinkH(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!ChanceRoll(obj, 8)) crouch_shoot(obj);
}

/* $02F5EA. A throw from a crouch. */
void CaptainThinkI(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!ChanceRoll(obj, 0x98)) throw_low(obj, 0xDC);
}

/* $02F5FA (reached only through the table at $2F666: no run has decoded it). Aiming up. */
void CaptainThinkJ(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!ChanceRoll(obj, 8)) aim_up(obj);
}

/* a decision by its address (an offset from $2F4BA in the row tables) */
static void think_at(uint32_t at, uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    switch (at) {
    case 0x02F4BA: CaptainThinkNone(obj, adx, dx, dy); return;
    case 0x02F4BC: CaptainThinkA(obj, adx, dx, dy); return;
    case 0x02F504: CaptainThinkB(obj, adx, dx, dy); return;
    case 0x02F550: CaptainThinkC(obj, adx, dx, dy); return;
    case 0x02F578: CaptainThinkD(obj, adx, dx, dy); return;
    case 0x02F588: CaptainThinkE(obj, adx, dx, dy); return;
    case 0x02F596: CaptainThinkF(obj, adx, dx, dy); return;
    case 0x02F5B8: CaptainThinkG(obj, adx, dx, dy); return;
    case 0x02F5DA: CaptainThinkH(obj, adx, dx, dy); return;
    case 0x02F5EA: CaptainThinkI(obj, adx, dx, dy); return;
    case 0x02F5FA: CaptainThinkJ(obj, adx, dx, dy); return;
    default: call68k_missing(at); return;
    }
}

/* loc_02F6A8: every 8 frames (else on to $2F4BA, a return): the decision for the nearer
 * player's distance (in 32s, within $100), from the row table at $2F6EC */
static void think(uint16_t obj)
{
    if (clock(obj) & 7) return;
    uint16_t dx, dy, player;
    uint16_t adx = NearestPlayer(obj, &dx, &dy, &player);
    uint16_t d3 = adx & 0x7FE0;
    if ((int16_t)(d3 - 0x100) >= 0) return;
    d3 = (uint16_t)((uint16_t)(((int16_t)d3 >> 4) + rd16(obj + 6)) & 0xFFFE);
    think_at(C_THINK + rom16(0x02F6EC + (uint32_t)(int16_t)d3), obj, adx, dx, dy);
}

/* loc_02F61C: on a ledge, every 16 frames: falling to a player well below, or the decision from
 * the table at $2F666 */
static void think_air(uint16_t obj)
{
    if (clock(obj) & 0xF) return;
    uint16_t dx, dy, player;
    uint16_t adx = NearestPlayer(obj, &dx, &dy, &player);
    if (!(dy & 0x8000) && (int16_t)(dy - 0x18) >= 0) { CaptainFall(obj); return; }
    uint16_t d3 = adx & 0x7FE0;
    if ((int16_t)(d3 - 0x100) >= 0) return;
    d3 = (uint16_t)((uint16_t)(((int16_t)d3 >> 4) + rd16(obj + 6)) & 0x3E);
    think_at(C_THINK + (uint32_t)(int16_t)rom16(0x02F666 + d3), obj, adx, dx, dy);
}

/* ---- walking ---- */

/* loc_02D89E: walking right on floor kind d4 (its step, look and speed by the slope); then the
 * decision */
static void walk_right(uint16_t obj, uint16_t d4)
{
    d4 = (uint16_t)(d4 - 4);
    uint16_t s = rom16(0x02D904 + (uint32_t)(int16_t)((int16_t)d4 >> 1));
    if (s != rd16(obj + O_STATE)) {
        wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
        set_state(obj, s);
        wr32(obj + 0x08, rom32(0x02D8DC + (uint32_t)(int16_t)d4));
        wr32(obj + 0x18, rom32(0x02D8F0 + (uint32_t)(int16_t)d4));
    }
    think(obj);
}

/* loc_02D958: and left */
static void walk_left(uint16_t obj, uint16_t d4)
{
    d4 = (uint16_t)(d4 - 4);
    uint16_t s = rom16(0x02D9BE + (uint32_t)(int16_t)((int16_t)d4 >> 1));
    if (s != rd16(obj + O_STATE)) {
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
        set_state(obj, s);
        wr32(obj + 0x08, rom32(0x02D996 + (uint32_t)(int16_t)d4));
        wr32(obj + 0x18, rom32(0x02D9AA + (uint32_t)(int16_t)d4));
    }
    think(obj);
}

/* the player it goes after is left of it */
static int player_left(uint16_t obj)
{
    uint16_t p = PlayerWeaker();
    return (int16_t)(rd16(p + O_X) - rd16(obj + O_X)) < 0;
}

/* $02D808. Standing: toward its player. */
void CaptainS04(uint16_t obj)
{
    walls8(obj);
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { off_ledge(obj); return; }
    if (player_left(obj)) walk_left(obj, d4);
    else walk_right(obj, d4);
}

/* $02D82E. Standing the other way: away from it. */
void CaptainS104(uint16_t obj)
{
    walls8(obj);
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { off_ledge(obj); return; }
    if (player_left(obj)) walk_right(obj, d4);
    else walk_left(obj, d4);
}

/* $02D87C. Walking right: a wall turns it; off the floor it falls. */
void CaptainS0C(uint16_t obj)
{
    if (walls8(obj)) { turn_back(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { CaptainFall(obj); return; }
    if ((int16_t)(d3 - 0x20) < 0) return;
    spend(obj, 1);
    walk_right(obj, d4);
}

/* $02D854. Up a steep slope right (3 more energy). */
void CaptainS14(uint16_t obj)
{
    spend(obj, 3);
    CaptainS0C(obj);
}

/* $02D85C. Down a slope right, a spark every 8 frames. */
void CaptainS1C(uint16_t obj)
{
    if (!(clock(obj) & 7)) CaptainSpark(obj, 0xFFFF, 0xFFFF, 0x0565BA);
    CaptainS0C(obj);
}

/* $02D936. Walking left: off the floor it may jump. */
void CaptainS08(uint16_t obj)
{
    if (walls8(obj)) { turn_back(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { off_ledge(obj); return; }
    if ((int16_t)(d3 - 0x20) < 0) return;
    spend(obj, 1);
    walk_left(obj, d4);
}

/* $02D90E. Up a steep slope left. */
void CaptainS18(uint16_t obj)
{
    spend(obj, 3);
    CaptainS08(obj);
}

/* $02D916. Down a slope left (sparks). */
void CaptainS10(uint16_t obj)
{
    if (!(clock(obj) & 7)) CaptainSpark(obj, 1, 0xFFFF, 0x0565BA);
    CaptainS08(obj);
}

/* ---- guarding: walking with the guard up ---- */

/* $02D9F6. Right (slower up slopes, sparks up them). */
void CaptainSA0(uint16_t obj)
{
    if (walls8(obj)) { turn_back(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { off_ledge(obj); return; }
    if (d4 == 8) {
        spend(obj, 4);
        wr32(obj + 0x18, 0xE000);
        if (!(clock(obj) & 0xF)) CaptainSpark(obj, 0xFFFF, 1, 0x0565D4);
    } else if (d4 == 0x10) {
        spend(obj, 3);
        wr32(obj + 0x18, 0x18000);
    } else {
        spend(obj, 2);
        wr32(obj + 0x18, 0x22000);
    }
    think(obj);
}

/* $02DA76. Left. */
void CaptainSA4(uint16_t obj)
{
    if (walls8(obj)) { turn_back(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { off_ledge(obj); return; }
    if (d4 == 0xC) {
        spend(obj, 4);
        wr32(obj + 0x18, 0xFFFF2000);
        if (!(clock(obj) & 0xF)) CaptainSpark(obj, 1, 1, 0x0565D4);
    } else if (d4 == 0x14) {
        spend(obj, 3);
        wr32(obj + 0x18, 0xFFFE8000);
    } else {
        spend(obj, 2);
        wr32(obj + 0x18, 0xFFFDE000);
    }
    think(obj);
}

/* $02D9C8. Guarding: toward its player. */
void CaptainS9C(uint16_t obj)
{
    walls8(obj);
    if (!floor16_any(obj)) { off_ledge(obj); return; }
    if (player_left(obj)) {
        set_state(obj, 0xA4);
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
        CaptainSA4(obj);
        return;
    }
    set_state(obj, 0xA0);
    wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
    CaptainSA0(obj);
}

/* ---- running ---- */

/* $02DB3A. A jump left: the ceiling checked from twice its height (the walls' probe, 8 below,
 * left in d1). Its y speed is set to -2.5 each frame (where the soldier's x speed is), so it never
 * comes down: kept (no step leads here). */
void CaptainS10C(uint16_t obj)
{
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
    wr32(obj + 0x1C, 0xFFFD8000);
    uint16_t d1 = (uint16_t)(8 + rd16(obj + O_Y));
    walls8(obj);
    uint16_t d3 = 0, d4 = 0;
    ObjectCeiling(obj, d1, &d3, &d4);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) next(obj, 4);
}

/* $02DB66. Falling, running left, until a floor. */
void CaptainS110(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
    wr32(obj + 0x18, 0xFFFD8000);
    walls8(obj);
    if (floor16_any(obj)) set_state(obj, 0xA8);
}

/* $02DB94. Turned back from a wall: jumping (higher when its player is above), or a hop. */
void CaptainS100(uint16_t obj)
{
    walls8(obj);
    floor16_any(obj);
    uint16_t p = PlayerWeaker();
    int pick;
    if ((int16_t)(rd16(p + O_Y) - rd16(obj + O_Y)) >= 0) {
        if (!(seed() & 7)) { jump_a(obj); return; }
        pick = 0;
    } else {
        pick = (rd8(v_randomSeed) & 7) != 0;
    }
    if (!pick) {
        CaptainHop(obj);
        if (rd16(obj + O_STATE) != 0x100) return;
    }
    uint16_t r = seed() & 0xC;
    if ((int16_t)(r - 8) >= 0) jump_a(obj);
    else if (r == 4) jump_c(obj);
    else jump_b(obj);
}

/* ---- tired, hit, grabbing ---- */

/* $02DBF6. Resting: then standing. */
void CaptainS30(uint16_t obj)
{
    if (!floor16_any(obj)) { CaptainFall(obj); return; }
    if (dec(obj, C_COUNT)) stand(obj);
}

/* $02DC5A. Pushed the way `by` moves (at 4). */
void CaptainPushed(uint16_t obj, uint16_t by)
{
    if (!(rd16(by + 0x18) & 0x8000)) {
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
        wr32(obj + 0x18, 0x40000);
        return;
    }
    wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
    wr32(obj + 0x18, 0xFFFC0000);
}

/* $02DC0A. Grabbing: hit (+$22 bit 5) while it shakes, it is pushed off (thrown up if the
 * player's hit rises). */
void CaptainS108(uint16_t obj)
{
    if (dec(obj, C_TIMER)) { let_go(obj); return; }
    if (!bit8(obj + 0x22, 5)) return;
    uint16_t by = rd16(obj + 0x42);
    CaptainPushed(obj, by);
    uint16_t vy = rd16(by + 0x1C);
    if (vy && !(vy & 0x8000)) {
        set_state(obj, 0xB8);
        return;
    }
    wr32(obj + 0x08, 0x0DEE20);
    set_state(obj, 0xBC);
    body(obj, 0x10F808);
}

/* loc_02DCD2: pushed along the ground (slowing by 1/4), sparks every 4 frames; stopped: let go */
static void pushed_slide(uint16_t obj)
{
    if (!floor16_any(obj)) { CaptainFall(obj); return; }
    uint32_t v = rd32(obj + 0x18);
    if (!v) {
        if (bit8(obj + 0x22, 5)) return;
        let_go(obj);
        return;
    }
    if (v & 0x80000000u) wr32(obj + 0x18, v + 0x4000);
    else wr32(obj + 0x18, v - 0x4000);
    if (clock(obj) & 3) return;
    uint16_t vy = (uint16_t)((seed() & 3) - 2);
    CaptainSpark(obj, 0xFFFF, vy, 0x0565BA);
}

/* $02DC82. Pushed: hit from below by a falling player or one landing on it: knocked back. */
void CaptainSB8(uint16_t obj)
{
    if (bit8(obj + 0x22, 5)) {
        uint16_t by = rd16(obj + 0x42);
        uint16_t vy = rd16(by + 0x1C);
        if (vy & 0x8000) { knocked_by(obj, by); return; }
        if (!vy) {
            uint16_t d = (uint16_t)(rd16(by + O_Y) - rd16(obj + O_Y));
            if (!(d & 0x8000) && (int16_t)(d - 3) >= 0) { knocked_by(obj, by); return; }
        }
    }
    pushed_slide(obj);
}

/* $02DCB4. Pushed: a rising hit knocks it back. */
void CaptainSBC(uint16_t obj)
{
    if (bit8(obj + 0x22, 5)) {
        uint16_t by = rd16(obj + 0x42);
        uint16_t vy = rd16(by + 0x1C);
        if (vy && !(vy & 0x8000)) { knocked_by(obj, by); return; }
    }
    pushed_slide(obj);
}

/* $02DD3E. Staggered: 7 frames (its look at 3), its energy back. */
void CaptainSB4(uint16_t obj)
{
    if (dec(obj, C_TIMER)) {
        wr16(obj + C_ENERGY, rd16(obj + C_ENERGY0));
        stand(obj);
        return;
    }
    if (rd16(obj + C_TIMER) == 3) wr32(obj + 0x08, 0x0DE9D0);
}

/* ---- on a ledge, under one ---- */

/* $02DD64. Landed hard ($70): on a ledge, then by the player: crouching, jumping up, or
 * stepping off. */
void CaptainS70(uint16_t obj)
{
    if (!(clock(obj) & 3)) {
        spend(obj, 1);
        if (rd16(obj + C_ENERGY) & 0x8000) goto off;
    }
    if (!dec(obj, C_TIMER)) return;
    {
        uint16_t dx, dy, p;
        NearestPlayer(obj, &dx, &dy, &p);
        if ((int16_t)(dy + 0x10) < 0) {
            if (!(seed() & 7)) { crouch(obj, 0xD4); return; }
            if ((int16_t)(rd16(obj + C_ENERGY) - 0xC) >= 0) jump_up(obj);
            else step_off(obj);
            return;
        }
        if ((int16_t)(dy - 0x10) >= 0) goto off;
        if (clock(obj) & 3) { crouch(obj, 0xD4); return; }
        if (bit8(v_gameFrames + 1, 0)) step_off(obj);
        else jump_up(obj);
        return;
    }
off:
    if (!(seed() & 3)) { crouch(obj, 0xD4); return; }
    step_off(obj);
}

/* $02DE06. Crouching on a ledge (above, $D4): toward its player along it. */
void CaptainSD4(uint16_t obj)
{
    walls_up(obj);
    if (!floor_up16(obj)) { CaptainFall(obj); return; }
    if (player_left(obj)) {
        set8(obj + 0x0E, 3);
        wr32(obj + 0x18, 0xFFFEE000);
    } else {
        clr8(obj + 0x0E, 3);
        wr32(obj + 0x18, 0x12000);
    }
    set_state(obj, 0xD8);
    wr16(obj + O_FLAGS, 0xED00);
    look(obj, 0x0DDC80);
}

/* $02DE66. Crawling along the ledge: at a wall, deciding; then jumping up or off. */
void CaptainSD8(uint16_t obj)
{
    wr16(obj + C_TIMER, walls_up(obj));
    if (!floor_up16(obj)) { CaptainFall(obj); return; }
    if (!rd16(obj + C_TIMER)) { think_air(obj); return; }
    uint16_t p = PlayerWeaker();
    if ((int16_t)(rd16(p + O_Y) - rd16(obj + O_Y)) >= 0) {
        if (!(seed() & 3)) { jump_up(obj); return; }
        step_off(obj);
        return;
    }
    if ((int16_t)(rd16(obj + C_ENERGY) - 0xC) >= 0) jump_up(obj);
    else step_off(obj);
}

/* $02DEE0. Crouched: its knife out at 8, then crouching again. */
void CaptainSDC(uint16_t obj)
{
    if (dec(obj, C_COUNT)) { crouch(obj, 0xD4); return; }
    if (rd16(obj + C_COUNT) == 8) knife(obj, 6);
}

/* $02DEFC. Hanging (under a ledge, $118 on): crouching on unless (1 in 4, its player below or
 * its energy spent) it drops. */
void CaptainS114(uint16_t obj)
{
    if (!(clock(obj) & 3)) {
        spend(obj, 1);
        if (rd16(obj + C_ENERGY) & 0x8000) goto roll;
    }
    if (!dec(obj, C_TIMER)) return;
    {
        uint16_t dx, dy, p;
        NearestPlayer(obj, &dx, &dy, &p);
        if (dy & 0x8000) { crouch(obj, 0x118); return; }
    }
roll:
    if (seed() & 3) { crouch(obj, 0x118); return; }
    step_off(obj);
}

/* $02DF68. Hanging: along under the ceiling toward its player. */
void CaptainS118(uint16_t obj)
{
    walls_up(obj);
    if (!ceiling16(obj)) { step_off(obj); return; }
    if (player_left(obj)) {
        set8(obj + 0x0E, 3);
        wr32(obj + 0x18, 0xFFFEE000);
    } else {
        clr8(obj + 0x0E, 3);
        wr32(obj + 0x18, 0x12000);
    }
    set_state(obj, 0x11C);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DDC80);
}

/* $02DFCC. Along until a wall (then dropping when its player is level or below, or its energy
 * is spent) or its player is under it: a throw from there. */
void CaptainS11C(uint16_t obj)
{
    wr16(obj + C_TIMER, walls_up(obj));
    if (!ceiling16(obj)) { step_off(obj); return; }
    if (rd16(obj + C_TIMER)) {
        uint16_t p = PlayerWeaker();
        if ((int16_t)(rd16(p + O_Y) - rd16(obj + O_Y)) >= 0 || (rd16(obj + C_ENERGY) & 0x8000)) step_off(obj);
        return;
    }
    uint16_t dx, dy, p;
    if ((int16_t)(NearestPlayer(obj, &dx, &dy, &p) - 0x10) < 0) throw_low(obj, 0x120);
}

/* $02E01A. Hanging, its knife out at 8. */
void CaptainS120(uint16_t obj)
{
    if (dec(obj, C_COUNT)) { crouch(obj, 0x118); return; }
    if (rd16(obj + C_COUNT) == 8) knife(obj, 6);
}

/* ---- jumping and falling ---- */

/* $02E086. At the top of a jump: a slash ($18) unless its player is well below. */
void CaptainSB0(uint16_t obj)
{
    uint16_t dx, dy, p;
    NearestPlayer(obj, &dx, &dy, &p);
    if ((dy & 0x8000) && (int16_t)(dy + 0x18) < 0) { CaptainFall(obj); return; }
    set_state(obj, 0xD0);
    look(obj, 0x0DE7F4);
    wr16(obj + C_COUNT, 0);
    wr16(obj + C_TIMER, 0x10);
    wr16(obj + C_AUX, 0x18);
    knife(obj, 0x18);
}

/* $02E036. Jumping: a ceiling cuts it short (landing on it, $70); at the top, a slash (by
 * chance) or falling. */
void CaptainS4C(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectWallsAny(obj, 0xFFF8, &d3, &d4);
    ObjectWallsAny8(obj, &d3, &d4);
    if (!ObjectCeiling(obj, 0xFFF0, &d3, &d4)) { land(obj); return; }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (!ChanceRoll(obj, 0x30)) { CaptainSB0(obj); return; }
    CaptainFall(obj);
    wr32(obj + 0x08, 0x0DDBC4);
    set_state(obj, 0xD0);
    wr16(obj + C_AUX, 0xC);
}

/* $02E0E4. Falling (to 6): onto a ledge above by chance once +$48 runs out ($70), or onto the
 * floor once +$4A does: landing ($44). */
void CaptainS40(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C);
    if ((v & 0x80000000u) || (int32_t)(v - 0x60000) < 0) wr32(obj + 0x1C, v + 0x4000);
    uint16_t d3 = 0, d4 = 0;
    ObjectWallsAny(obj, 0xFFF8, &d3, &d4);
    ObjectWallsAny8(obj, &d3, &d4);
    if (dec(obj, C_TIMER) && !ChanceRoll(obj, 0x84)) {
        if (floor_up16(obj)) { land(obj); return; }
    }
    if (!dec(obj, C_COUNT)) return;
    if (!floor16_any(obj)) return;
    set_state(obj, 0x44);
    wr16(obj + C_COUNT, 8);
    look(obj, 0x0DE02C);
    wr32(obj + 0x18, 0);
    wr16(obj + C_LINK, 0);
    body(obj, 0x10F808);
    mode(obj, 0xE0, 0);
}

/* $02E0C8. A slash in the air: +$4C frames, then falling. */
void CaptainSD0(uint16_t obj)
{
    if (dec(obj, C_AUX)) {
        set_state(obj, 0x40);
        look(obj, 0x0DDB78);
    }
    CaptainS40(obj);
}

/* $02E17C. Landed: crouching a moment. */
void CaptainS44(uint16_t obj)
{
    if (!floor16_any(obj)) { CaptainFall(obj); return; }
    if (dec(obj, C_COUNT)) settle(obj);
}

/* $02E190. Knocked back: rising, then falling ($28 frames before it may land on a ledge),
 * still unhurt. */
void CaptainSC0(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectWallsAny(obj, 0xFFF8, &d3, &d4);
    ObjectWallsAny8(obj, &d3, &d4);
    ObjectCeiling(obj, 0xFFF0, &d3, &d4);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    CaptainFall(obj);
    wr16(obj + C_TIMER, 0x28);
    wr8(obj + C_MODE, (uint8_t)(rd8(obj + C_MODE) | 0x17));
}

/* $02E1C8. The hop back: bouncing twice, then tired. */
void CaptainSE8(uint16_t obj)
{
    walls8(obj);
    uint32_t v = rd32(obj + 0x1C);
    if ((v & 0x80000000u) || (int32_t)(v - 0x60000) < 0) wr32(obj + 0x1C, v + 0x4000);
    if (bit8(obj + 0x1C, 7)) return;
    if (!floor16_any(obj)) return;
    if (dec(obj, C_COUNT)) { CaptainTire(obj); return; }
    wr32(obj + 0x1C, 0xFFFE0000);
}

/* ---- shooting ---- */

/* $02E210. About to shoot. */
void CaptainS34(uint16_t obj)
{
    if (!dec(obj, C_TIMER)) return;
    next(obj, 4);
    wr16(obj + C_TIMER, 0x10);
}

/* $02E224. Aiming at its player $10 frames, then the shot. */
void CaptainS38(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xCD00);
    uint16_t target = 0, type = 0;
    uint32_t faces = 0, muzzles = 0;
    CaptainAimSetup(obj, &target, &type, &faces, &muzzles);
    if (rd16(obj + O_STATE) != 0x38) return;
    if (!dec(obj, C_TIMER)) {
        uint16_t d0, d5, d6, d7 = type;
        FaceToward(obj, target, faces, &d0, &d5, &d6, &d7);
        return;
    }
    next(obj, 4);
    wr16(obj + C_TIMER, 0x10);
    AimedSpawn(obj, target, faces, muzzles, type);
}

/* $02E258. After the shot. */
void CaptainS3C(uint16_t obj)
{
    if (dec(obj, C_TIMER)) settle(obj);
}

/* $02E262. Aiming up at its player, then the shot. */
void CaptainSE0(uint16_t obj)
{
    uint16_t target = PlayerWeaker();
    if (!dec(obj, C_TIMER)) {
        uint16_t d0, d5, d6, d7 = 0x90;
        FaceToward(obj, target, 0x02AE66, &d0, &d5, &d6, &d7);
        return;
    }
    next(obj, 4);
    wr16(obj + C_TIMER, 0x10);
    AimedSpawn(obj, target, 0x02AE66, 0x02AE86, 0x90);
}

/* $02E296. Then crouching. */
void CaptainSE4(uint16_t obj)
{
    if (dec(obj, C_TIMER)) crouch(obj, 0xD4);
}

/* $02E2A0. Throwing a grenade: the knife's hand at 8. */
void CaptainS48(uint16_t obj)
{
    if (dec(obj, C_COUNT)) { settle(obj); return; }
    if (rd16(obj + C_COUNT) == 8) knife(obj, 6);
}

/* $02E2BC. The mortar's wait; then (its sound) a dash forward (thrusting, by chance its box
 * ahead). */
void CaptainS50(uint16_t obj)
{
    if (!floor16_any(obj)) { CaptainFall(obj); return; }
    if (!dec(obj, C_COUNT)) return;
    QueueSound(0xB9);
    wr16(obj + 0x0C, 0);
    spend(obj, 8);
    if (!ChanceRoll(obj, 0x10)) {
        body(obj, 0xF410F808);
        wr32(obj + 0x08, 0x0DDE64);
        wr16(obj + C_COUNT, 1);
    } else {
        wr32(obj + 0x08, 0x0DDDD0);
        wr16(obj + C_COUNT, 0);
    }
    if (facing_left(obj)) {
        wr32(obj + 0x18, 0xFFF98000);
        next(obj, 4);
        thrust(obj, 0xFFF0);
        return;
    }
    wr32(obj + 0x18, 0x68000);
    next(obj, 8);
    thrust(obj, 0x10);
}

/* the dash's sparks (every 4 frames) */
static void dash_sparks(uint16_t obj)
{
    if (clock(obj) & 3) return;
    CaptainSpark(obj, 0, 0xFFFF, rd16(obj + C_COUNT) ? 0x0565D4 : 0x05652E);
}

/* $02E356. Dashing left, slowing to a stop. */
void CaptainS54(uint16_t obj)
{
    dash_sparks(obj);
    walls8(obj);
    if (!floor16_any(obj)) { CaptainFall(obj); return; }
    wr32(obj + 0x18, rd32(obj + 0x18) + 0x2000);
    if (rd32(obj + 0x18) & 0x80000000u) return;
    wr32(obj + 0x18, 0);
    settle(obj);
}

/* $02E3B2. Right. */
void CaptainS58(uint16_t obj)
{
    dash_sparks(obj);
    walls8(obj);
    if (!floor16_any(obj)) { CaptainFall(obj); return; }
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x2000);
    if (!(rd32(obj + 0x18) & 0x80000000u)) return;
    wr32(obj + 0x18, 0);
    settle(obj);
}

/* $02E40E. Crouched to shoot: at $24 left a grenade ($98) thrown at its player. */
void CaptainS5C(uint16_t obj)
{
    if (dec(obj, C_COUNT)) { settle(obj); return; }
    if (rd16(obj + C_COUNT) != 0x24) return;
    ThrowAt(obj, PlayerWeaker(), 0x98);
}

/* ---- holding a player ---- */

/* $02E4F0. Holding the player (+$40) in front of it, shaking (by its +$49 bit 1); each time the
 * player mashes the pad (+$69 changing) the count runs down (at $F a grenade ($98) pops up from
 * it): out, the player breaks free. */
void CaptainSF4(uint16_t obj)
{
    wr8(obj + 0x20, 0x70);
    uint16_t p = rd16(obj + 0x40);
    if (bit8(p + 0x23, 2) || !bit8(p + 0x21, 7)) { stand(obj); return; }
    uint16_t pad = rd8(p + 0x69) & 0x7C;
    if (pad != rd16(obj + C_COUNT)) {
        if (rd16(obj + C_TIMER) == 0xF) {
            uint16_t slot, t;
            if (SlotFreeHigh(&slot, &t)) {
                wr16(slot + O_TYPE, 0x98);
                wr16(slot + O_X, rd16(obj + O_X));
                wr16(slot + O_Y, rd16(obj + O_Y));
                wr32(slot + 0x1C, 0xFFFDF000);
            }
        }
        if (dec(obj, C_TIMER)) {
            wr32(p + 0x1C, 0xFFFC0000);
            CaptainTire(obj);
            return;
        }
    }
    wr16(obj + C_COUNT, pad);
    set8(p + 0x23, 1);
    clr8(p + 0x23, 4);
    if (bit8(obj + C_TIMER + 1, 1)) wr16(p + O_Y, (uint16_t)(rd16(obj + O_Y) + 2));
    else wr16(p + O_Y, (uint16_t)(rd16(obj + O_Y) - 3));
}

/* $02E432. Grabbed something (+$40): a player is held in front of it, still (both facing);
 * anything else is let go and it throws. Its hold by the difficulty (the table at $2E59A). */
void CaptainSF0(uint16_t obj)
{
    body(obj, 0xF410F808);
    uint16_t p = rd16(obj + 0x40);
    if (!p) { throw_start(obj); return; }
    if (p != v_player1 && p != v_player2) {
        wr8(p + 0x22, rd8(p + 0x22));
        wr16(p + 0x42, 0);
        throw_start(obj);
        return;
    }
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr32(p + 0x18, 0);
    wr32(p + 0x1C, 0);
    wr16(p + O_Y, rd16(obj + O_Y));
    uint16_t x = rd16(obj + O_X);
    if ((int16_t)(x - rd16(p + O_X)) < 0) {
        clr8(obj + 0x0E, 3);
        clr8(p + 0x0E, 3);
        x = (uint16_t)(x + 0x10);
    } else {
        set8(obj + 0x0E, 3);
        set8(p + 0x0E, 3);
        x = (uint16_t)(x - 0x10);
    }
    wr16(p + O_X, x);
    next(obj, 4);
    look(obj, 0x0DE540);
    wr16(obj + O_FLAGS, 0xED00);
    wr16(obj + C_COUNT, 0);
    wr8(obj + C_COUNT, rd8(p + 0x69));
    wr16(obj + C_TIMER, rom16(0x02E59A + (uint32_t)(int16_t)rd16(v_difficulty)));
    CaptainSF4(obj);
}

/* ---- the jump kick (no step leads here) ---- */

/* $02E5A2. Rising; at the top its kick ($A8, linked) out. */
void CaptainS60(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2E00);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xCD00);
    look(obj, 0x0DDE24);
    wr16(obj + C_COUNT, 0);
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + 0x48, 0x20);
    wr16(slot + 0x4A, 0);
    wr16(slot + 0x4C, 0);
    wr16(slot + 0x58, obj);
    wr16(obj + C_LINK, slot);
    wr16(slot + O_TYPE, 0xA8);
    wr16(slot + 0x26, 0xA);
    wr8(slot + 0x21, 0x10);
    wr32(slot + 0x2C, 0xFBFFF010);
}

/* $02E60A. Falling on the kick: a bounce, then a stop. */
void CaptainS78(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C);
    if ((int32_t)(v - 0x60000) < 0) {
        v += 0x3800;
        wr32(obj + 0x1C, v);
        if (v & 0x80000000u) return;
    }
    if (!floor16_any(obj)) return;
    if (rd16(obj + C_COUNT)) {
        next(obj, 4);
        wr32(obj + 0x18, 0);
        wr16(obj + C_COUNT, 0x10);
        return;
    }
    wr16(obj + C_COUNT, (uint16_t)(rd16(obj + C_COUNT) + 1));
    wr16(obj + C_LINK, 0);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x1C, 0xFFFD0000);
}

/* $02E66A */
void CaptainS7C(uint16_t obj)
{
    if (dec(obj, C_COUNT)) settle(obj);
}

/* ---- held by a player ---- */

/* $02E674. Held (+$23 bit 1, cleared): while held, nothing; thrown (bit 4): flying ($84),
 * hurting by 4; dropped: a jump. */
void CaptainS80(uint16_t obj)
{
    uint8_t s = rd8(obj + 0x23);
    wr8(obj + 0x23, (uint8_t)(s & ~2));
    if (s & 2) return;
    s = rd8(obj + 0x23);
    wr8(obj + 0x23, (uint8_t)(s & ~0x10));
    if (!(s & 0x10)) { jump_a(obj); return; }
    next(obj, 4);
    wr16(obj + 0x26, 4);
}

/* $02E694. Thrown: rising (against walls and ceilings); then falling ($88). */
void CaptainS84(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectWalls(obj, 0xFFF8, &d3, &d4);
    ObjectWalls8(obj, &d3, &d4);
    ObjectCeiling(obj, 0xFFF0, &d3, &d4);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    wr32(obj + 0x08, 0x0DDBC4);
    set_state(obj, 0x88);
    wr16(obj + C_COUNT, 0);
    wr16(obj + C_AUX, 0xC);
}

/* $02E6D6. Falling (to 6) until a floor. Thrown ($88): a bounce up (its x speed by 1/8), the
 * hits' counter reset, blinking (by the difficulty, $10 more), $64 vitality lost. Knocked down
 * ($A8): landed, crouching ($44). */
void CaptainS88(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C);
    if ((v & 0x80000000u) || (int32_t)(v - 0x60000) < 0) wr32(obj + 0x1C, v + 0x4000);
    uint16_t d3 = 0, d4 = 0;
    ObjectWalls(obj, 0xFFF8, &d3, &d4);
    ObjectWalls8(obj, &d3, &d4);
    if (!floor16_any(obj)) return;
    if (rd16(obj + O_STATE) == 0xA8) {
        set_state(obj, 0x44);
        wr16(obj + C_COUNT, 0x10);
        look(obj, 0x0DE560);
        wr32(obj + 0x18, 0);
        body(obj, 0x10F808);
        mode(obj, 0xE0, 0);
        return;
    }
    next(obj, 4);
    wr32(obj + 0x1C, 0xFFFC8000);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 3));
    wr16(HITS, (uint16_t)((rd8(v_randomSeed) & 3) + 6));
    wr16(obj + C_BLINK, (uint16_t)(rom16(C_BLINKS + (uint32_t)(int16_t)rd16(v_difficulty)) + 0x1000));
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0x64));
}

/* $02E78E. Knocked up: rising; then falling ($A8). */
void CaptainS8C(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectWalls(obj, 0xFFF8, &d3, &d4);
    ObjectWalls8(obj, &d3, &d4);
    ObjectCeiling(obj, 0xFFF0, &d3, &d4);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    wr32(obj + 0x08, 0x0DDBC4);
    set_state(obj, 0xA8);
    wr16(obj + C_COUNT, 0);
    wr16(obj + C_AUX, 0xC);
}

/* $02E7D2. Grabbed something to throw: let go (whatever it is), and a jump. */
void CaptainS90(uint16_t obj)
{
    body(obj, 0xF410F808);
    uint16_t p = rd16(obj + 0x40);
    if (p) {
        wr8(p + 0x22, rd8(p + 0x22));
        wr16(p + 0x42, 0);
    }
    jump_a(obj);
}

/* loc_02E8C6: the held player at the swing's point (+$4C's entry of the table at $2E914) */
static void swing_hold(uint16_t obj)
{
    uint16_t p = rd16(obj + 0x40);
    if (!p) return;
    set8(p + 0x23, 1);
    uint16_t i = (uint16_t)(rd16(obj + C_AUX) << 1);
    uint16_t dx = rom16(0x02E914 + (uint32_t)(int16_t)i);
    if (facing_left(obj)) dx = (uint16_t)~dx;
    wr16(p + O_X, (uint16_t)(rd16(obj + O_X) + dx));
    i = (uint16_t)(i + 2);
    wr16(p + O_Y, (uint16_t)(rom16(0x02E914 + (uint32_t)(int16_t)i) + rd16(obj + O_Y)));
}

/* $02E844. Swinging the player over its head (frames by the table at $2E90E), then throwing it
 * up and away (no step leads here). */
void CaptainS94(uint16_t obj)
{
    if (!dec(obj, C_COUNT)) { swing_hold(obj); return; }
    wr16(obj + C_AUX, (uint16_t)(rd16(obj + C_AUX) + 2));
    uint16_t n = rom16(0x02E90E + (uint32_t)(int16_t)rd16(obj + C_AUX));
    if (!(n & 0x8000)) {
        wr16(obj + C_COUNT, n);
        swing_hold(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + C_COUNT, 0x20);
    uint16_t p = rd16(obj + 0x40);
    if (!p) return;
    clr8(obj + 0x21, 0);
    wr16(obj + 0x40, 0);
    wr8(obj + 0x22, 0);
    wr8(obj + 0x23, 0);
    clr8(p + 0x23, 1);
    set8(p + 0x23, 4);
    wr32(p + 0x1C, 0xFFF80000);
    wr32(p + 0x18, facing_left(obj) ? 0xFFFE0000 : 0x20000);
}

/* $02E91C */
void CaptainS98(uint16_t obj)
{
    if (dec(obj, C_COUNT)) settle(obj);
}

/* ---- its end ---- */

/* $02E926. Beaten: falling (slowly) to the floor: then hidden. */
void CaptainSC4(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (!floor16_any(obj)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xD00);
}

/* $02E94A. Blown apart: an item dropped, ten pieces ($100) by the tables at $5818C/$581AC (the
 * first five at half speed), the screen shaking; it is beaten (+$8080 bit 0), gone in $80
 * frames. */
void CaptainSC8(uint16_t obj)
{
    DropItem(obj);
    wr16(v_shakeSprites, 0x10);
    QueueSound(0xBE);
    uint16_t d3 = 0;
    for (int16_t k = 9; k >= 0; k--) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) break;
        uint32_t vy = rom32(0x05818C + d3), vx = rom32(0x0581AC + d3);
        if ((int16_t)(k - 5) >= 0) {
            vy = (uint32_t)((int32_t)vy >> 1);
            vx = (uint32_t)((int32_t)vx >> 1);
        }
        wr32(slot + 0x18, vx);
        wr32(slot + 0x1C, vy);
        wr16(slot + O_TYPE, 0x100);
        wr32(slot + 0x48, 0x05639C);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        d3 = (uint16_t)((d3 - 0x10) & 0x7C);
        if ((int16_t)(d3 - 0x40) < 0) d3 = 0;
    }
    next(obj, 4);
    set8(BOSS_FLAGS, 0);
    wr16(obj + C_TIMER, 0x80);
    wr16(obj + C_BLINK, 0xFFFF);
    wr16(obj + O_FLAGS, 0);
}

/* $02E9E4. Then gone, the camera free (and, by byte $57 bit 7, the status bar's event $58). */
void CaptainSCC(uint16_t obj)
{
    if (!dec(obj, C_TIMER)) return;
    wr16(obj + O_FLAGS, 0x1000);
    clr8(v_cameraHold, 7);
    if (bit8(obj + 0x57, 7)) wr16(0x80C2, 0x58);
}

/* $02EA0A. Dropped in from above (+$57 bit 0): down through the cells 8 at a time (a floor cell,
 * after +$48 of them, stops it); below $15F, gone. */
void CaptainSEC(uint16_t obj)
{
    uint16_t x = rd16(obj + O_X);
    for (int k = 0; k < 8; k++) {
        uint16_t a = MapAttribute(x, rd16(obj + O_Y));
        if ((int16_t)(a - 0x30) >= 0 && ((a & 8) || dec(obj, C_TIMER))) {
            wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10));
            if (floor16_any(obj)) stand(obj);
            else CaptainFall(obj);
            return;
        }
        wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 8));
    }
    if ((int16_t)(rd16(obj + O_Y) - 0x15F) >= 0) wr16(obj + O_FLAGS, 0x1000);
}

/* ---- the start ---- */

/* $02D736. Step 0: its kind's row, flags, vitality, energy and place; its clock's high byte from
 * the random; the hits' counter (6-9); a companion ($570, tied to it by +$48) for a kind with
 * byte $56 bit 4 from level 6. Dropping in from above, or standing (falling with no floor). */
void CaptainS00(uint16_t obj)
{
    uint32_t k = 0x02F764 + (uint32_t)(int16_t)rd16(obj + 6);
    wr16(obj + 6, rom16(k));
    wr16(obj + C_KIND, rom16(k + 2));
    wr16(obj + O_VITALITY, rom16(k + 4));
    wr16(obj + C_ENERGY0, rom16(k + 6));
    wr16(obj + O_X, rom16(k + 8));
    wr16(obj + O_Y, rom16(k + 10));
    wr8(obj + C_CLOCK, rd8(v_randomSeed + 1));
    wr16(obj + C_MASK, 0x7F);
    wr16(obj + 0xDA, 0x20);
    /* +$57 bit 1 (the soldier's palette) is tested, and both ways go on alike */
    wr16(obj + 0x0E, (uint16_t)(1 | rd16(0x808A)));
    wr16(obj + C_ENERGY, rd16(obj + C_ENERGY0));
    wr16(HITS, (uint16_t)((rd8(v_randomSeed) & 3) + 6));
    if (bit8(obj + C_KIND, 4) && (int16_t)(rd16(LEVEL) - 6) >= 0) {
        uint16_t slot, t;
        if (SlotFreeHigh(&slot, &t)) {
            wr16(slot + O_TYPE, 0x570);
            wr16(slot + 0x48, obj);
            wr16(slot + O_X, rd16(obj + O_X));
            wr16(slot + O_Y, rd16(obj + O_Y));
        }
    }
    if (bit8(obj + 0x57, 0)) {
        set_state(obj, 0xEC);
        wr16(obj + O_FLAGS, 0);
        wr16(obj + C_TIMER, seed() & 1);
        wr16(obj + O_Y, 0xA0);
        return;
    }
    uint16_t d3 = 0, d4 = 0;
    if (ObjectFloor(obj, 0x10, &d3, &d4)) { CaptainFall(obj); return; }
    stand(obj);
}

/* ---- the type ---- */

/* $02D470. Its frame: the clock; kept inside x $80-$1C0 (and, between, below y $8C); at y $200
 * gone (the camera let go). Its vitality gone (unless +$5E bit 3): beaten. Knocked flying by a
 * hit from below (+$22 bit 1). Hit (+$22 bit 5, unless +$5E bit 0): the hits' counter down (8
 * when thrown, +$23 bit 4): spent, it is reset and it blinks, knocked back; else staggered on
 * the ground, knocked back in the air. Its energy gone (unless +$5E bit 1): tired. A grabber
 * (byte $56 bit 2; not while +$5E bit 2), not blinking: a player near enough is grabbed. Then
 * its step. */
void CaptainFrame(uint16_t obj)
{
    wr16(obj + C_CLOCK, (uint16_t)(rd16(obj + C_CLOCK) - 1));
    wr8(obj + 0x20, 0x80);
    if ((int16_t)(rd16(obj + O_X) - 0x80) < 0) wr16(obj + O_X, 0x80);
    else if ((int16_t)(rd16(obj + O_X) - 0x1C0) >= 0) wr16(obj + O_X, 0x1C0);
    else if ((int16_t)(rd16(obj + O_Y) - 0x8C) < 0) wr16(obj + O_Y, 0x8C);
    if ((int16_t)(rd16(obj + O_Y) - 0x200) >= 0) {
        wr16(obj + O_FLAGS, 0x1000);
        clr8(v_cameraHold, 7);
        return;
    }
    if ((rd16(obj + O_VITALITY) & 0x8000) && !bit8(obj + C_MODE, 3)) {
        ObjectRelease(obj);
        beaten(obj);
        return;
    }
    if (bit8(obj + 0x22, 1)) { knocked_up(obj); return; }
    if (bit8(obj + 0x22, 5) && !bit8(obj + C_MODE, 0)) {
        int thrown = bit8(obj + 0x23, 4);
        wr16(HITS, (uint16_t)(rd16(HITS) - (thrown ? 8 : 1)));
        if (rd16(HITS) & 0x8000) {
            wr16(HITS, (uint16_t)((rd8(v_randomSeed) & 3) + 6));
            wr16(obj + C_BLINK, rom16(C_BLINKS + (uint32_t)(int16_t)rd16(v_difficulty)));
        } else if (!thrown) {
            uint16_t d3 = 0, d4 = 0;
            if (!ObjectFloor(obj, 0x10, &d3, &d4)) {
                spend(obj, 1);
                ObjectRelease(obj);
                CaptainStagger(obj);
                goto step;
            }
        }
        clr8(obj + 0x23, 4);
        ObjectRelease(obj);
        CaptainKnocked(obj);
        goto step;
    }
    if ((rd16(obj + C_ENERGY) & 0x8000) && !bit8(obj + C_MODE, 1)) {
        if (clock(obj) & 3) CaptainTire(obj);
        else CaptainTireCrouch(obj);
        goto step;
    }
    if (bit8(obj + C_KIND, 2) && !bit8(obj + C_MODE, 2)) {
        uint16_t dx, dy, p;
        uint16_t adx = NearestPlayer(obj, &dx, &dy, &p);
        if ((int16_t)(adx - 0x28) < 0 && (int16_t)(dy + 0x30) >= 0 && (int16_t)(dy - 0x20) < 0
            && bit8(p + 0x21, 6) && bit8(p + 0x23, 5) && bit8(obj + C_BLINK, 7))
            CaptainGrab(obj);
    }
step:
    call_object(rom32(C_TABLE + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $02D40E. Type $1F8. The random stirred twice (with the frame count, into $FF0A); its frame;
 * then its blinking (+$5A down by $100 while not negative: running, flagged in +$23 bit 2 and
 * shown on odd clocks; run out, shown). */
void Captain(uint16_t obj)
{
    Random();
    wr16(v_randomSeed, (uint16_t)(rd16(v_randomSeed) + rd16(v_gameFrames)));
    wr16(0xFF0A, (uint16_t)(rd16(0xFF0A) + rd16(v_randomSeed)));
    CaptainFrame(obj);
    uint16_t b = rd16(obj + C_BLINK);
    if (!(b & 0x8000)) {
        wr16(obj + C_BLINK, (uint16_t)(b - 0x100));
        if (!(rd16(obj + C_BLINK) & 0x8000)) {
            set8(obj + 0x23, 2);
            if (bit8(obj + C_CLOCK + 1, 0)) set8(obj + O_FLAGS, 7);
            else clr8(obj + O_FLAGS, 7);
            return;
        }
        set8(obj + O_FLAGS, 7);
    }
    clr8(obj + 0x23, 2);
}

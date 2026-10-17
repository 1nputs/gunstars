/* Type $48: the soldier, the game's commonest enemy; and $110 and $114, what a soldier becomes
 * when it is thrown or knocked flying.
 *
 * A soldier's kind (+6, from the spawner: an offset into the table at $2D376) gives its
 * decision row, its flags (+$56: byte $56 bit 0 never retreats, bit 1 guards, bit 2 grabs;
 * byte $57 bit 0 drops in from above, bit 1 its palette), its vitality and its energy (+$4E,
 * copied to +$5C: each action spends some; spent, it tires). Every 8 frames on the ground it
 * looks at the nearer player and takes the action its row gives for the distance: walk, jump,
 * shoot, throw, guard, crouch. A step (+4) is one of the 73 routines of the table at $2B0B8. */
#include "game.h"

#define S_TIMER   0x48           /* the step's counter (word; byte $49 for the grab's shake) */
#define S_COUNT   0x4A
#define S_AUX     0x4C           /* word or long (the slide's distance) */
#define S_ENERGY0 0x4E
#define S_CLOCK   0x50           /* counts down from $200-$2FF: its life on screen, and a clock */
#define S_MASK    0x54           /* added to the chance rolls (a random part by difficulty) */
#define S_KIND    0x56           /* bytes $56 and $57: the kind's flags */
#define S_LINK    0x58           /* the helper it spawned (its knife, its throw) */
#define S_ENERGY  0x5C
#define S_MODE    0x5E           /* byte: bits 0-4 what it may do now */

#define BOSS_FLAGS 0x8080        /* bit 0: the boss is beaten (the soldiers run), bit 1 */
#define AGGRESSION 0x80C0        /* added to the chance rolls */

static void set_state(uint16_t obj, uint16_t s) { wr16(obj + O_STATE, s); }
static void next(uint16_t obj, uint16_t by) { wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + by)); }
static void look(uint16_t obj, uint32_t map) { wr32(obj + 0x08, map); wr16(obj + 0x0C, 0); }
static void mode(uint16_t obj, uint8_t keep, uint8_t set)
{
    wr8(obj + S_MODE, (uint8_t)((rd8(obj + S_MODE) & keep) | set));
}
static void spend(uint16_t obj, uint16_t n) { wr16(obj + S_ENERGY, (uint16_t)(rd16(obj + S_ENERGY) - n)); }
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
static uint16_t clock(uint16_t obj) { return rd16(obj + S_CLOCK); }
static uint16_t seed(void) { return rd16(v_randomSeed); }

/* $02D018. A chance roll: C set when chance (plus the aggression and its mask) is below a random
 * byte. */
int ChanceRoll(uint16_t obj, uint16_t chance)
{
    uint16_t d6 = (uint16_t)(chance + rd16(AGGRESSION) + rd16(obj + S_MASK));
    return d6 < rd8(v_randomSeed + 1);
}

/* the soldier's floor (16 below): 0 when none; d3, d4 its attribute and kind */
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
static int floor_up16(uint16_t obj, uint16_t *d3) { uint16_t d4 = 0; return !ObjectFloorUp16(obj, d3, &d4); }

/* ---- what a soldier turns into ---- */

/* $02AAE2: dead: a dying type ($7C falling, $70, $74, $54) by the random, still moving if it
 * was falling */
static void become_dying(uint16_t obj)
{
    wr16(obj + S_LINK, 0);
    set_state(obj, 0);
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0);
    if (rd32(obj + 0x1C)) { wr16(obj + O_TYPE, 0x7C); return; }
    uint16_t r = seed();
    if (!(r & 0xF00)) wr32(obj + 0x1C, 0xFFFC8000);
    int16_t d1 = (int16_t)((r & 0x1E) - 0x18);
    if (d1 == 0) {
        uint16_t d0 = rd16(v_gameFrames) & 7;
        if ((int16_t)(d0 - 3) < 0) d0 = 5;
        wr16(obj + 0x1C, (uint16_t)~d0);
        wr16(obj + O_TYPE, 0x7C);
        return;
    }
    if (d1 < 0) { wr16(obj + O_TYPE, 0x54); return; }
    if (!(seed() & 0x7000)) wr32(obj + 0x18, 0);
    wr16(obj + O_TYPE, rom16(0x02AB70 + (uint32_t)d1));
}

/* $02AB94: blown up: type $8C */
static void become_8C(uint16_t obj)
{
    wr16(obj + S_LINK, 0);
    set_state(obj, 0);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + O_TYPE, 0x8C);
    wr16(obj + S_TIMER, 0x68);
    wr16(obj + S_COUNT, 7);
    wr16(obj + S_AUX, 0x10);
}

/* ---- type $110: thrown by a player ---- */

/* $02A9B4: its next step, touching as the thrower's: a player's throw hits enemies (+$21 $40,
 * +$23 $20), anything else's hits players */
static void thrown_touch(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x0C, 0);
    uint16_t by = rd16(obj + 0x42);
    if (by == v_player1 || by == v_player2) {
        wr32(obj + 0x2C, 0xF808F808);
        wr8(obj + 0x21, 0x40);
        wr8(obj + 0x23, 0x20);
        return;
    }
    wr32(obj + 0x2C, 0xF808F808);
    wr32(obj + 0x28, 0xF808F808);
    wr8(obj + 0x21, 2);
}

/* $02A92A. A dust effect ($11C) where it was; flying away from the thrower's facing (4 more,
 * at least 1), turned to match, up at 4.5. */
void ThrownStart(uint16_t obj)
{
    uint16_t slot, t;
    if (SlotFreeHigh(&slot, &t)) {
        wr16(slot + O_TYPE, 0x11C);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
    }
    uint16_t by = rd16(obj + 0x42);
    int left_map;
    if (rd16(by + 6) & 0x8000) {
        wr16(obj + 0x18, (uint16_t)(rd16(obj + 0x18) - 4));
        if (!(rd16(obj + 0x18) & 0x8000)) wr32(obj + 0x18, 0xFFFF0000);
        left_map = !bit8(obj + 0x0E, 3);
    } else {
        wr16(obj + 0x18, (uint16_t)(rd16(obj + 0x18) + 4));
        if (rd16(obj + 0x18) & 0x8000) wr32(obj + 0x18, 0x10000);
        left_map = bit8(obj + 0x0E, 3);
    }
    wr32(obj + 0x08, left_map ? 0x0E0896 : 0x0E0882);
    wr16(obj + 0x26, 1);
    wr16(obj + O_FLAGS, 0xEF00);
    wr32(obj + 0x1C, 0xFFFB8000);
    thrown_touch(obj);
}

/* $02AA00. Bouncing off the screen's sides, falling (to 6); on the ground, dead. */
void ThrownFly(uint16_t obj)
{
    if (bit8(obj + 0x18, 7)) {
        if ((int16_t)(rd16(obj + O_X) - 0x88) < 0) wr32(obj + 0x18, ~rd32(obj + 0x18));
    } else if ((int16_t)(rd16(obj + O_X) - 0x1B8) >= 0) {
        wr32(obj + 0x18, ~rd32(obj + 0x18));
    }
    uint32_t v = rd32(obj + 0x1C);
    if ((int32_t)(v - 0x60000) < 0) wr32(obj + 0x1C, v + 0x4000);
    if (MapAttribute(rd16(obj + O_X), (uint16_t)(rd16(obj + O_Y) + 0xC))) {
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        become_dying(obj);
    }
}

/* $02A918. Type $110. */
void Thrown(uint16_t obj)
{
    call_object(rom32(0x02A922 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $114: knocked flying (by a player's hit from below, +$22 bit 1) ---- */

/* $02ABFA */
void KnockedStart(uint16_t obj)
{
    next(obj, 4);
    look(obj, 0x0DF4A4);
    wr16(obj + O_FLAGS, 0xEF00);
    wr8(obj + 0x21, 0);
}

/* $02AC1A. Still held (+$23 bit 1, cleared): wait; thrown (bit 4): flying, hurting; else dead. */
void KnockedHeld(uint16_t obj)
{
    uint8_t s = rd8(obj + 0x23);
    wr8(obj + 0x23, (uint8_t)(s & ~2));
    if (s & 2) return;
    if (!bit8(obj + 0x23, 4)) { become_dying(obj); return; }
    wr16(obj + 0x26, 0x28);
    wr16(obj + S_TIMER, 0xC);
    thrown_touch(obj);
}

/* $02AD00: on something: dead */
static void knocked_stop(uint16_t obj)
{
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    become_dying(obj);
}

/* $02AC5E. Rising (slowing); at the top its falling look. A ceiling cell (bit 3) 8 above stops
 * it. */
void KnockedRise(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) {
        next(obj, 4);
        look(obj, 0x0DFFEE);
    }
    uint16_t a = MapAttribute(rd16(obj + O_X), (uint16_t)(rd16(obj + O_Y) - 8));
    if (a && (a & 8)) knocked_stop(obj);
}

/* $02AC44. $C frames thrown straight, then rising. */
void KnockedFly(uint16_t obj)
{
    if (!dec(obj, S_TIMER)) { KnockedRise(obj); return; }
    next(obj, 4);
    look(obj, 0x0DFFCE);
    KnockedRise(obj);
}

/* $02AC9A. Falling (to 6) until the ground: then a bounce, half as fast. */
void KnockedFall(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C);
    if ((int32_t)(v - 0x60000) < 0) wr32(obj + 0x1C, v + 0x4000);
    if (!MapAttribute(rd16(obj + O_X), (uint16_t)(rd16(obj + O_Y) + 0xC))) return;
    next(obj, 4);
    wr16(obj + 0x26, 0);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x1C, 0xFFFD0000);
    look(obj, 0x0DF4A4);
}

/* $02ACF4. The bounce; down again: dead. */
void KnockedBounce(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    knocked_stop(obj);
}

/* $02ABBA. Type $114. Hit by a blast (+$22 bit 6): shaking the screen, blown up. */
void Knocked(uint16_t obj)
{
    if (bit8(obj + 0x22, 6)) {
        if (!rd16(v_shakeSprites)) wr16(v_shakeSprites, 8);
        become_8C(obj);
        return;
    }
    call_object(rom32(0x02ABE2 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $02AB78: knocked flying */
static void become_114(uint16_t obj)
{
    wr16(obj + S_LINK, 0);
    ObjectRelease(obj);
    set_state(obj, 0);
    wr16(obj + O_TYPE, 0x114);
    Knocked(obj);
}

/* ---- the soldier's transitions (each sets a step and its look) ---- */

static void stand(uint16_t obj);

/* $02C704: standing's look */
static void stand_look(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF2D0);
    mode(obj, 0xE0, 0);
    body(obj, 0xF410F808);
}

/* $02C738: guarding (unless already) */
static void guard(uint16_t obj)
{
    uint16_t s = rd16(obj + O_STATE);
    if (s == 0x9C || s == 0xA0 || s == 0xA4) return;
    set_state(obj, 0x9C);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF344);
    mode(obj, 0xE0, 0);
    body(obj, 0xF410F808);
}

/* $02C6F4: standing (or guarding, for a guard) */
static void stand(uint16_t obj)
{
    if (bit8(obj + S_KIND, 1)) { guard(obj); return; }
    set_state(obj, 4);
    stand_look(obj);
}

/* $02C72E: standing, the other way's step */
static void stand_104(uint16_t obj)
{
    set_state(obj, 0x104);
    stand_look(obj);
}

/* $02C786: running away left (its time up) */
static void retreat(uint16_t obj)
{
    set_state(obj, 0xA8);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF3B8);
    wr32(obj + 0x18, 0xFFFD8000);
    wr16(obj + S_COUNT, 0x200);
    mode(obj, 0xF7, 0x17);
    body(obj, 0xF410F808);
}

/* $02CB80: falling */
void SoldierFall(uint16_t obj)
{
    wr16(obj + S_TIMER, 0);
    wr16(obj + S_COUNT, 0);
    set_state(obj, 0x40);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF468);
    mode(obj, 0xF6, 0x16);
}

/* $02CB8C: falling, its counters kept */
static void fall_keep(uint16_t obj)
{
    set_state(obj, 0x40);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF468);
    mode(obj, 0xF6, 0x16);
}

/* $02C7CA. Tired (its energy spent): resting $50 frames in a pose by the floor's kind. */
void SoldierTire(uint16_t obj)
{
    set_state(obj, 0x30);
    wr16(obj + O_FLAGS, 0xEF00);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    wr16(obj + S_COUNT, 0x50);
    mode(obj, 0xE2, 2);
    body(obj, 0xF410F808);
    wr16(obj + S_ENERGY, rd16(obj + S_ENERGY0));
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { SoldierFall(obj); return; }
    if (d4 == 4 || d4 == 0x10 || d4 == 0x14) {
        wr32(obj + 0x08, 0x0DF4FC);
        uint16_t r = seed();
        if (!(r & 0x8000)) wr32(obj + 0x08, rom32(0x02C848 + (r & 0x1C)));
        return;
    }
    if (!flipped(obj)) wr32(obj + 0x08, d4 == 0xC ? 0x0DF9C6 : 0x0DFA02);
    else wr32(obj + 0x08, d4 == 8 ? 0x0DF9C6 : 0x0DFA02);
}

/* $02C8AA. Tired, crouching. */
void SoldierTireCrouch(uint16_t obj)
{
    set_state(obj, 0x30);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF81A);
    wr16(obj + S_ENERGY, rd16(obj + S_ENERGY0));
    wr32(obj + 0x18, 0);
    wr16(obj + S_COUNT, 0x50);
    mode(obj, 0xE2, 2);
    body(obj, 0x10F808);
}

/* $02C938: about to shoot ($32 frames), its look by the floor's slope and its facing */
static void aim(uint16_t obj)
{
    set_state(obj, 0x34);
    wr16(obj + O_FLAGS, 0xEF00);
    wr16(obj + 0x0C, 0);
    wr16(obj + S_TIMER, 0x32);
    wr32(obj + 0x18, 0);
    mode(obj, 0xF7, 0x17);
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { SoldierFall(obj); return; }
    uint32_t m = 0x0DF93A;
    if (d4 == 8) m = flipped(obj) ? 0x0DFB8E : 0x0DF93A;
    else if (d4 == 0xC) m = flipped(obj) ? 0x0DF93A : 0x0DFB8E;
    wr32(obj + 0x08, m);
}

/* $02C9C4: aiming up ($1E frames) */
static void aim_up(uint16_t obj)
{
    set_state(obj, 0xE0);
    wr16(obj + O_FLAGS, 0xCF00);
    wr16(obj + 0x0C, 0);
    wr16(obj + S_TIMER, 0x1E);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x08, 0x0DF628);
    mode(obj, 0xF7, 0x17);
}

/* $02CA26: a jump up at `vy`, at once checked against the ceiling */
static void jump_from(uint16_t obj, uint32_t vy)
{
    wr32(obj + 0x1C, vy);
    wr16(obj + S_TIMER, 0);
    wr16(obj + S_COUNT, 0);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF42C);
    mode(obj, 0xF6, 0x16);
    uint16_t d3 = 0, d4 = 0;
    ObjectCeiling(obj, 0xFFF0, &d3, &d4);
}

/* $02CA0A, $02CA60, $02CA80: jumping (6, 4.5 or 7.5 up, less a random 0-1) */
static void jump_with(uint16_t obj, uint32_t base)
{
    set_state(obj, 0x4C);
    spend(obj, 0x10);
    jump_from(obj, (uint32_t)(seed() & 0xFF00) + base);
}
static void jump_a(uint16_t obj) { jump_with(obj, 0xFFFA0000); }
static void jump_b(uint16_t obj) { jump_with(obj, 0xFFFB8000); }
static void jump_c(uint16_t obj) { jump_with(obj, 0xFFF88000); }

/* $02CAB0: landing hard */
static void land(uint16_t obj, uint16_t state)
{
    set_state(obj, state);
    look(obj, 0x0DF620);
    wr32(obj + 0x18, 0);
    wr16(obj + S_LINK, 0);
    wr16(obj + S_TIMER, 0xC);
    wr16(obj + S_COUNT, 8);
    mode(obj, 0xF6, 0x16);
}

/* $02CAE6: jumping up to a ledge, unless a ceiling cell is above */
static void jump_up(uint16_t obj)
{
    uint16_t d3 = 0;
    floor_up16(obj, &d3);
    if (d3 & 8) return;
    set_state(obj, 0x4C);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF4A4);
    wr32(obj + 0x1C, 0xFFFB8000);
    spend(obj, 0xA);
    mode(obj, 0xF6, 0x16);
}

/* $02CB3A: crouching */
static void crouch(uint16_t obj, uint16_t state)
{
    set_state(obj, state);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF620);
    mode(obj, 0xE6, 6);
    body(obj, 0xF410F808);
}

/* $02CBB4: a little hop back */
static void hop_back(uint16_t obj)
{
    set_state(obj, 0xE8);
    mode(obj, 0xF6, 0x16);
    wr16(obj + S_COUNT, 2);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0E002A);
    wr32(obj + 0x1C, 0xFFFC8000);
    wr32(obj + 0x18, flipped(obj) ? 0x8000 : 0xFFFF8000);
}

/* $02CB6A: off a ledge: jumping (on odd randoms), hopping, or falling */
static void off_ledge(uint16_t obj)
{
    if (bit8(v_randomSeed + 1, 0)) { jump_a(obj); return; }
    if (!(clock(obj) & 3)) { hop_back(obj); return; }
    SoldierFall(obj);
}

/* $02CC06. A small hop (not under a ceiling cell). */
void SoldierHop(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectFloor16(obj, &d3, &d4);
    if (d3 & 8) return;
    wr16(obj + S_TIMER, 0x10);
    wr16(obj + S_COUNT, 0x10);
    set_state(obj, 0x40);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF4A4);
    mode(obj, 0xF6, 0x16);
}

/* $02CC48: throwing (a grenade) */
static void throw_start(uint16_t obj)
{
    set_state(obj, 0x48);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DFDA8);
    wr32(obj + 0x18, 0);
    wr16(obj + S_COUNT, 0x20);
    spend(obj, 0x20);
    mode(obj, 0xF6, 0x16);
}

/* $02CC94: throwing from a crouch */
static void throw_low(uint16_t obj, uint16_t state)
{
    set_state(obj, state);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DFEB8);
    wr32(obj + 0x18, 0);
    wr16(obj + S_COUNT, 0x20);
    spend(obj, 0x20);
    mode(obj, 0xF6, 0x16);
}

/* $02CCCA: setting up a mortar ($1FC) beside it */
static void mortar(uint16_t obj)
{
    set_state(obj, 0x50);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF81A);
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
    uint16_t n = rom16(0x02CD84 + (uint32_t)(int16_t)rd16(v_difficulty));
    wr16(slot + 0x48, n);
    wr16(obj + S_COUNT, n);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10));
    if (flipped(obj)) wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) - 0xC));
    else wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + 0xC));
}

/* $02CD8C: crouching to shoot */
static void crouch_shoot(uint16_t obj)
{
    set_state(obj, 0x5C);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF6D4);
    wr32(obj + 0x18, 0);
    wr16(obj + S_COUNT, 0x40);
    spend(obj, 8);
    mode(obj, 0xF6, 0x16);
}

/* $02CDC8: sliding along the ground */
static void slide(uint16_t obj)
{
    set_state(obj, 0x60);
    if (!(rd16(obj + 0x18) & 0x8000)) next(obj, 4);
    wr16(obj + O_FLAGS, 0xCF00);
    wr16(obj + 0x0C, 0);
    wr16(obj + S_TIMER, 0);
    wr32(obj + S_AUX, rd32(obj + 0x18));
    wr32(obj + 0x08, 0x0DF4B4);
    mode(obj, 0xE0, 0x16);
}

/* $02CE06: a charge (spending a lot), its box reaching forward */
static void charge(uint16_t obj)
{
    set_state(obj, 0x90);
    spend(obj, 0x80);
    mode(obj, 0xF6, 0x16);
    wr32(obj + 0x2C, 0xF808E818);
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x2B);
}

/* $02CE36: a punch */
static void punch(uint16_t obj)
{
    set_state(obj, 0xF0);
    mode(obj, 0xF6, 0x16);
    wr32(obj + 0x2C, 0xF808E818);
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x2B);
}

/* $02CE60. Staggered (a hit from behind). */
void SoldierStagger(uint16_t obj)
{
    set_state(obj, 0xB4);
    wr16(obj + O_FLAGS, 0xCF00);
    look(obj, 0x0DFF96);
    wr16(obj + S_TIMER, 7);
    wr32(obj + 0x18, 0);
    mode(obj, 0xF7, 0x17);
}

/* $02CEA2. Knocked back up (away from its facing), gaining energy (8-39). */
void SoldierKnocked(uint16_t obj)
{
    set_state(obj, 0xC0);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DFF6E);
    wr32(obj + 0x1C, 0xFFFC8000);
    wr16(obj + S_ENERGY, (uint16_t)(rd16(obj + S_ENERGY) + (uint16_t)((rd8(v_randomSeed + 1) & 0x1F) + 8)));
    mode(obj, 0xF7, 0x17);
    body(obj, 0xF410F808);
    wr16(obj + S_TIMER, 0x40);
    wr16(obj + S_COUNT, 0);
    wr32(obj + 0x18, facing_left(obj) ? 0xC000 : 0xFFFF4000);
}

/* $02CE96: knocked back by `by`'s hit: vitality its +$4A less the hit's power */
static void knocked_by(uint16_t obj, uint16_t by)
{
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + S_COUNT) - rd16(by + 0x26)));
    SoldierKnocked(obj);
}

/* $02CF18. Grabbing a player: shaking it $10 frames (its vitality kept in +$4A). */
void SoldierGrab(uint16_t obj)
{
    ObjectRelease(obj);
    set_state(obj, 0x108);
    wr16(obj + O_FLAGS, 0xCF00);
    look(obj, 0x0E02FC);
    wr16(obj + S_TIMER, 0x10);
    wr32(obj + 0x18, 0);
    wr16(obj + S_COUNT, rd16(obj + O_VITALITY));
    mode(obj, 0xFF, 0x1F);
    body(obj, 0xF410F808);
}

/* $02CFF4: against a wall: turning back */
static void turn_back(uint16_t obj)
{
    set_state(obj, 0x100);
    wr32(obj + 0x18, flipped(obj) ? 0xFFFF0000 : 0x10000);
}

/* $02D1C0: down: standing, or falling */
static void settle(uint16_t obj)
{
    if (floor16_any(obj)) stand(obj);
    else SoldierFall(obj);
}

/* ---- what the soldier spawns ---- */

/* $02C53A: its knife ($A8) in a slot of $CE00-$D3FF, linked both ways, +$48 its time */
static void knife(uint16_t obj, uint16_t time)
{
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + 0x48, time);
    wr16(slot + 0x4C, 0);
    wr16(slot + 0x58, obj);
    wr16(obj + S_LINK, slot);
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

/* $02C5A2: its knife thrust at `reach` */
static void thrust(uint16_t obj, uint16_t reach)
{
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + 0x48, 0x1C);
    wr16(slot + 0x4A, reach);
    wr16(slot + 0x4C, 8);
    wr16(slot + 0x58, obj);
    wr16(obj + S_LINK, slot);
    wr16(slot + O_TYPE, 0xA8);
    wr8(slot + 0x21, 0x10);
    wr32(slot + 0x2C, 0xFC03FA06);
    wr16(slot + 0x26, rd16(obj + S_COUNT) ? 8 : 4);
}

/* $02C6B8. A spark ($100: its speeds vx, vy as words, its animation list `data`) at its feet. */
void SoldierSpark(uint16_t obj, uint16_t vx, uint16_t vy, uint32_t data)
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

/* $02C5EE. Sliding: every 8 pixels along (+$4C, long) its next frame. */
void SoldierSlide(uint16_t obj)
{
    uint32_t d0 = rd32(obj + S_AUX), d1 = d0, vx = rd32(obj + 0x18);
    int beyond;
    if (!(rd16(obj + 0x18) & 0x8000)) {
        d1 -= 0x80000;
        beyond = !(d1 & 0x80000000u);
    } else {
        d1 += 0x80000;
        beyond = (d1 & 0x80000000u) != 0;
    }
    if (!beyond) {
        wr32(obj + S_AUX, d0 + vx);
        return;
    }
    wr32(obj + S_AUX, d1 + vx);
    uint16_t f = (uint16_t)((rd16(obj + S_TIMER) + 4) & 0xC);
    wr16(obj + S_TIMER, f);
    wr32(obj + 0x08, rom32(0x02C642 + f));
}

/* $02C652. Set to shoot at its player: the shot's type (d7: $90; $91 on a slope, faced up it),
 * its tables (a1 the facing's mappings, a2 the muzzles); off the ground it falls instead. */
void SoldierAimSetup(uint16_t obj, uint16_t *target, uint16_t *type, uint32_t *faces, uint32_t *muzzles)
{
    *target = PlayerBySlot(obj);
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { SoldierFall(obj); return; }
    if (d4 == 8) set8(obj + 0x0E, 3);
    else if (d4 == 0xC) clr8(obj + 0x0E, 3);
    else {
        *type = 0x90;
        *faces = 0x02AD16;
        *muzzles = 0x02AD56;
        return;
    }
    *type = 0x91;
    *faces = 0x02AD66;
    *muzzles = 0x02ADA6;
}

/* ---- the soldier's decisions (by its row and the player's distance) ---- */

/* $02D02A */
void SoldierThinkNone(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)obj; (void)adx; (void)dx; (void)dy;
}

/* $02D02C. Near: a charge; else jump at a player above, hop under one below, throw. */
void SoldierThinkA(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)dx;
    if (!ChanceRoll(obj, 0x6F)) {
        if ((int16_t)(adx - 0xC) >= 0) return;
        charge(obj);
        return;
    }
    if ((int16_t)(dy + 0x18) >= 0) {
        if ((int16_t)(dy - 0x18) >= 0) { SoldierHop(obj); return; }
        if (!ChanceRoll(obj, 0x98)) { throw_start(obj); return; }
    }
    if (!ChanceRoll(obj, 0xA0)) jump_a(obj);
}

/* $02D072 */
void SoldierThinkB(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx;
    if ((int16_t)(dy + 0x18) < 0) {
        if (!ChanceRoll(obj, 0xA0)) jump_a(obj);
        return;
    }
    if ((int16_t)(dy - 0x18) >= 0) { SoldierHop(obj); return; }
    throw_start(obj);
}

/* $02D094 */
void SoldierThinkC(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx;
    if ((int16_t)(dy + 0x18) < 0) { jump_a(obj); return; }
    if ((int16_t)(dy - 0x18) >= 0) { SoldierHop(obj); return; }
    if (!ChanceRoll(obj, 0x74)) punch(obj);
}

/* $02D0B2. Turning to face the player. */
void SoldierThinkD(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx;
    if (!(dy & 0x8000) && (int16_t)(dy - 0x18) >= 0) { SoldierHop(obj); return; }
    if (dx & 0x8000) {
        if (!flipped(obj)) stand(obj);
        return;
    }
    if (flipped(obj)) stand(obj);
}

/* $02D0DE */
void SoldierThinkE(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dy;
    if (dx & 0x8000) {
        if (flipped(obj)) stand_104(obj);
        return;
    }
    if (!flipped(obj)) stand_104(obj);
}

/* $02D0FE */
void SoldierThinkF(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!ChanceRoll(obj, 8)) { mortar(obj); return; }
    if (!ChanceRoll(obj, 0x80)) jump_a(obj);
}

/* $02D118 */
void SoldierThinkG(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!ChanceRoll(obj, 8)) guard(obj);
}

/* $02D126 */
void SoldierThinkH(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!ChanceRoll(obj, 8)) aim(obj);
}

/* $02D134 */
void SoldierThinkI(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!ChanceRoll(obj, 8)) slide(obj);
}

/* $02D142 */
void SoldierThinkJ(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx;
    if ((int16_t)(dy + 0x18) < 0) {
        if (!ChanceRoll(obj, 0xA0)) jump_a(obj);
        return;
    }
    if ((int16_t)(dy - 0x18) >= 0) SoldierHop(obj);
}

/* $02D162 */
void SoldierThinkK(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx;
    if ((int16_t)(dy + 0x18) < 0) {
        if (!ChanceRoll(obj, 0xA0)) jump_b(obj);
        return;
    }
    if ((int16_t)(dy - 0x18) >= 0) SoldierHop(obj);
}

/* $02D182. Crouching to shoot (on the hardest difficulties anywhere, else only below y $70). */
void SoldierThinkL(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if ((int16_t)(rd16(v_difficulty) - 4) < 0 && (int16_t)(rd16(obj + O_Y) - 0x70) < 0) return;
    if (!ChanceRoll(obj, 8)) crouch_shoot(obj);
}

/* $02D1A4 */
void SoldierThinkM(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!ChanceRoll(obj, 0x98)) throw_low(obj, 0xDC);
}

/* $02D1B2 */
void SoldierThinkN(uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    (void)adx; (void)dx; (void)dy;
    if (!ChanceRoll(obj, 8)) aim_up(obj);
}

/* a decision by its address (an offset from $2D02A in the row tables) */
static void think_at(uint32_t at, uint16_t obj, uint16_t adx, uint16_t dx, uint16_t dy)
{
    switch (at) {
    case 0x02D02A: SoldierThinkNone(obj, adx, dx, dy); return;
    case 0x02D02C: SoldierThinkA(obj, adx, dx, dy); return;
    case 0x02D072: SoldierThinkB(obj, adx, dx, dy); return;
    case 0x02D094: SoldierThinkC(obj, adx, dx, dy); return;
    case 0x02D0B2: SoldierThinkD(obj, adx, dx, dy); return;
    case 0x02D0DE: SoldierThinkE(obj, adx, dx, dy); return;
    case 0x02D0FE: SoldierThinkF(obj, adx, dx, dy); return;
    case 0x02D118: SoldierThinkG(obj, adx, dx, dy); return;
    case 0x02D126: SoldierThinkH(obj, adx, dx, dy); return;
    case 0x02D134: SoldierThinkI(obj, adx, dx, dy); return;
    case 0x02D142: SoldierThinkJ(obj, adx, dx, dy); return;
    case 0x02D162: SoldierThinkK(obj, adx, dx, dy); return;
    case 0x02D182: SoldierThinkL(obj, adx, dx, dy); return;
    case 0x02D1A4: SoldierThinkM(obj, adx, dx, dy); return;
    case 0x02D1B2: SoldierThinkN(obj, adx, dx, dy); return;
    default: call68k_missing(at); return;
    }
}

/* $02D25A: every 8 frames: the decision for the nearer player's distance (in 32s, within $100) */
static void think(uint16_t obj)
{
    if (clock(obj) & 7) return;
    uint16_t dx, dy, player;
    uint16_t adx = NearestPlayer(obj, &dx, &dy, &player);
    uint16_t d3 = adx & 0x7FE0;
    if ((int16_t)(d3 - 0x100) >= 0) return;
    d3 = (uint16_t)((uint16_t)(((int16_t)d3 >> 4) + rd16(obj + 6)) & 0xFFFE);
    think_at(0x02D02A + rom16(0x02D29E + (uint32_t)(int16_t)d3), obj, adx, dx, dy);
}

/* $02D1CE: in the air (on a ledge's edge) every 16 frames: falling to a player below, or the
 * decision from the second table */
static void think_air(uint16_t obj)
{
    if (clock(obj) & 0xF) return;
    uint16_t dx, dy, player;
    uint16_t adx = NearestPlayer(obj, &dx, &dy, &player);
    if (!(dy & 0x8000) && (int16_t)(dy - 0x20) >= 0) { SoldierFall(obj); return; }
    uint16_t d3 = adx & 0x7FE0;
    if ((int16_t)(d3 - 0x100) >= 0) return;
    d3 = (uint16_t)((uint16_t)(((int16_t)d3 >> 4) + rd16(obj + 6)) & 0x3E);
    think_at(0x02D02A + (uint32_t)(int16_t)rom16(0x02D218 + d3), obj, adx, dx, dy);
}

/* ---- walking ---- */

/* $02B332: walking right on floor kind d4 (its step, look and speed by the slope); then the
 * decision */
static void walk_right(uint16_t obj, uint16_t d4)
{
    d4 = (uint16_t)(d4 - 4);
    uint16_t s = rom16(0x02B398 + (uint32_t)(int16_t)((int16_t)d4 >> 1));
    if (s != rd16(obj + O_STATE)) {
        wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
        set_state(obj, s);
        wr32(obj + 0x08, rom32(0x02B370 + (uint32_t)(int16_t)d4));
        wr32(obj + 0x18, rom32(0x02B384 + (uint32_t)(int16_t)d4));
    }
    think(obj);
}

/* $02B3EC: and left */
static void walk_left(uint16_t obj, uint16_t d4)
{
    d4 = (uint16_t)(d4 - 4);
    uint16_t s = rom16(0x02B452 + (uint32_t)(int16_t)((int16_t)d4 >> 1));
    if (s != rd16(obj + O_STATE)) {
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
        set_state(obj, s);
        wr32(obj + 0x08, rom32(0x02B42A + (uint32_t)(int16_t)d4));
        wr32(obj + 0x18, rom32(0x02B43E + (uint32_t)(int16_t)d4));
    }
    think(obj);
}

static int player_left(uint16_t obj)
{
    uint16_t p = PlayerBySlot(obj);
    return (int16_t)(rd16(p + O_X) - rd16(obj + O_X)) < 0;
}

/* $02B29C. Standing: toward its player. */
void SoldierS04(uint16_t obj)
{
    walls8(obj);
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { off_ledge(obj); return; }
    if (player_left(obj)) walk_left(obj, d4);
    else walk_right(obj, d4);
}

/* $02B2C2. Standing the other way: away from it. */
void SoldierS104(uint16_t obj)
{
    walls8(obj);
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { off_ledge(obj); return; }
    if (player_left(obj)) walk_right(obj, d4);
    else walk_left(obj, d4);
}

/* $02B310. Walking right: a wall turns it; off the floor it falls. */
void SoldierS0C(uint16_t obj)
{
    if (walls8(obj)) { turn_back(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { SoldierFall(obj); return; }
    if ((int16_t)(d3 - 0x20) < 0) return;
    spend(obj, 1);
    walk_right(obj, d4);
}

/* $02B2E8. Up a steep slope right (3 more energy). */
void SoldierS14(uint16_t obj)
{
    spend(obj, 3);
    SoldierS0C(obj);
}

/* $02B2F0. Down a slope right, a spark every 8 frames. */
void SoldierS1C(uint16_t obj)
{
    if (!(clock(obj) & 7)) SoldierSpark(obj, 0xFFFF, 0xFFFF, 0x0565BA);
    SoldierS0C(obj);
}

/* $02B3CA. Walking left: off the floor it may jump. */
void SoldierS08(uint16_t obj)
{
    if (walls8(obj)) { turn_back(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { off_ledge(obj); return; }
    if ((int16_t)(d3 - 0x20) < 0) return;
    spend(obj, 1);
    walk_left(obj, d4);
}

/* $02B3A2 */
void SoldierS18(uint16_t obj)
{
    spend(obj, 3);
    SoldierS08(obj);
}

/* $02B3AA */
void SoldierS10(uint16_t obj)
{
    if (!(clock(obj) & 7)) SoldierSpark(obj, 1, 0xFFFF, 0x0565BA);
    SoldierS08(obj);
}

/* ---- guarding: walking with the guard up ---- */

/* $02B48A. Right (slower up slopes, sparks down them). */
void SoldierSA0(uint16_t obj)
{
    if (walls8(obj)) { turn_back(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { off_ledge(obj); return; }
    if (d4 == 8) {
        spend(obj, 4);
        wr32(obj + 0x18, 0xE000);
        if (!(clock(obj) & 0xF)) SoldierSpark(obj, 0xFFFF, 1, 0x0565D4);
    } else if (d4 == 0x10) {
        spend(obj, 3);
        wr32(obj + 0x18, 0x18000);
    } else {
        spend(obj, 2);
        wr32(obj + 0x18, 0x22000);
    }
    think(obj);
}

/* $02B50A. Left. */
void SoldierSA4(uint16_t obj)
{
    if (walls8(obj)) { turn_back(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (!floor16(obj, &d3, &d4)) { off_ledge(obj); return; }
    if (d4 == 0xC) {
        spend(obj, 4);
        wr32(obj + 0x18, 0xFFFF2000);
        if (!(clock(obj) & 0xF)) SoldierSpark(obj, 1, 1, 0x0565D4);
    } else if (d4 == 0x14) {
        spend(obj, 3);
        wr32(obj + 0x18, 0xFFFE8000);
    } else {
        spend(obj, 2);
        wr32(obj + 0x18, 0xFFFDE000);
    }
    think(obj);
}

/* $02B45C. Guarding: toward its player. */
void SoldierS9C(uint16_t obj)
{
    walls8(obj);
    if (!floor16_any(obj)) { off_ledge(obj); return; }
    if (player_left(obj)) {
        set_state(obj, 0xA4);
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
        SoldierSA4(obj);
        return;
    }
    set_state(obj, 0xA0);
    wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
    SoldierSA0(obj);
}

/* ---- running away ---- */

/* $02B57E. Left at 2.5 for $200 frames: a wall makes it jump, no floor makes it fall. */
void SoldierSA8(uint16_t obj)
{
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
    wr32(obj + 0x18, 0xFFFD8000);
    if (dec(obj, S_COUNT)) {
        wr16(obj + O_VITALITY, 0xFFFF);
        return;
    }
    if (walls8(obj)) {
        set_state(obj, 0x10C);
        wr32(obj + 0x1C, 0xFFFA0000);
        return;
    }
    if (!floor16_any(obj)) set_state(obj, 0x110);
}

/* $02B5D0. Its jump (the ceiling checked from twice its height: the walls' probe left in d1). */
void SoldierS10C(uint16_t obj)
{
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
    wr32(obj + 0x18, 0xFFFD8000);
    uint16_t d1 = (uint16_t)(8 + rd16(obj + O_Y));
    walls8(obj);
    uint16_t d3 = 0, d4 = 0;
    ObjectCeiling(obj, d1, &d3, &d4);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) next(obj, 4);
}

/* $02B5FC. Falling, still running, until a floor. */
void SoldierS110(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
    wr32(obj + 0x18, 0xFFFD8000);
    walls8(obj);
    if (floor16_any(obj)) set_state(obj, 0xA8);
}

/* $02B62A. Turned back from a wall: jumping (higher when its player is above), or a hop. */
void SoldierS100(uint16_t obj)
{
    walls8(obj);
    floor16_any(obj);
    uint16_t p = PlayerBySlot(obj);
    int pick;
    if ((int16_t)(rd16(p + O_Y) - rd16(obj + O_Y)) >= 0) {
        if (!(seed() & 7)) { jump_a(obj); return; }
        pick = 0;
    } else {
        pick = (rd8(v_randomSeed) & 7) != 0;
    }
    if (!pick) {
        SoldierHop(obj);
        if (rd16(obj + O_STATE) != 0x100) return;
    }
    uint16_t r = seed() & 0xC;
    if ((int16_t)(r - 8) >= 0) jump_a(obj);
    else if (r == 4) jump_c(obj);
    else jump_b(obj);
}

/* ---- tired, hit, grabbing ---- */

/* $02B68C. Resting: then standing. */
void SoldierS30(uint16_t obj)
{
    if (!floor16_any(obj)) { SoldierFall(obj); return; }
    if (dec(obj, S_COUNT)) stand(obj);
}

/* $02B6F0. Pushed the way `by` moves (at 4). */
void SoldierPushed(uint16_t obj, uint16_t by)
{
    if (!(rd16(by + 0x18) & 0x8000)) {
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
        wr32(obj + 0x18, 0x40000);
        return;
    }
    wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
    wr32(obj + 0x18, 0xFFFC0000);
}

/* $02B7CA: let go: its vitality back, standing */
static void let_go(uint16_t obj)
{
    wr16(obj + O_VITALITY, rd16(obj + S_COUNT));
    stand(obj);
}

/* $02B6A0. Grabbing: hit (+$22 bit 5) while it shakes, it is pushed off (thrown up if the
 * player's hit rises). */
void SoldierS108(uint16_t obj)
{
    if (dec(obj, S_TIMER)) { let_go(obj); return; }
    if (!bit8(obj + 0x22, 5)) return;
    uint16_t by = rd16(obj + 0x42);
    SoldierPushed(obj, by);
    uint16_t vy = rd16(by + 0x1C);
    if (vy && !(vy & 0x8000)) {
        set_state(obj, 0xB8);
        return;
    }
    wr32(obj + 0x08, 0x0E031C);
    set_state(obj, 0xBC);
    body(obj, 0x10F808);
}

/* $02B768: pushed along the ground (slowing by 1/4), sparks; stopped: let go */
static void pushed_slide(uint16_t obj)
{
    if (!floor16_any(obj)) { SoldierFall(obj); return; }
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
    SoldierSpark(obj, 0xFFFF, vy, 0x0565BA);
}

/* $02B718. Pushed: hit from below by a falling player or one landing on it: knocked back. */
void SoldierSB8(uint16_t obj)
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

/* $02B74A. Pushed: a rising hit knocks it back. */
void SoldierSBC(uint16_t obj)
{
    if (bit8(obj + 0x22, 5)) {
        uint16_t by = rd16(obj + 0x42);
        uint16_t vy = rd16(by + 0x1C);
        if (vy && !(vy & 0x8000)) { knocked_by(obj, by); return; }
    }
    pushed_slide(obj);
}

/* $02B7D4. Staggered: 7 frames (its look at 3), its energy back. */
void SoldierSB4(uint16_t obj)
{
    if (dec(obj, S_TIMER)) {
        wr16(obj + S_ENERGY, rd16(obj + S_ENERGY0));
        stand(obj);
        return;
    }
    if (rd16(obj + S_TIMER) == 3) wr32(obj + 0x08, 0x0DFFB6);
}

/* $02B86C: falling on (a step off its ledge at 1/2) */
static void step_off(uint16_t obj)
{
    wr16(obj + S_TIMER, 0x20);
    wr16(obj + S_COUNT, 0);
    wr32(obj + 0x18, flipped(obj) ? 0xFFFF8000 : 0x8000);
    fall_keep(obj);
}

/* $02B7FA. Landed hard ($70): on a ledge, then by the player: crouching, jumping up, or
 * stepping off. */
void SoldierS70(uint16_t obj)
{
    if (!(clock(obj) & 3)) {
        spend(obj, 1);
        if (rd16(obj + S_ENERGY) & 0x8000) goto off;
    }
    if (!dec(obj, S_TIMER)) return;
    {
        uint16_t dx, dy, p;
        NearestPlayer(obj, &dx, &dy, &p);
        if ((int16_t)(dy + 0x10) < 0) {
            if (!(seed() & 7)) { crouch(obj, 0xD4); return; }
            if ((int16_t)(rd16(obj + S_ENERGY) - 0xC) >= 0) jump_up(obj);
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
    if (!(seed() & 7)) { crouch(obj, 0xD4); return; }
    step_off(obj);
}

/* $02B89C. Crouching on a ledge (above, $D4): toward its player along it. */
void SoldierSD4(uint16_t obj)
{
    walls_up(obj);
    uint16_t d3 = 0;
    if (!floor_up16(obj, &d3)) { SoldierFall(obj); return; }
    if (player_left(obj)) {
        set8(obj + 0x0E, 3);
        wr32(obj + 0x18, 0xFFFEE000);
    } else {
        clr8(obj + 0x0E, 3);
        wr32(obj + 0x18, 0x12000);
    }
    set_state(obj, 0xD8);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF538);
}

/* $02B946: off the ledge's end */
static void ledge_off(uint16_t obj)
{
    wr16(obj + S_TIMER, 0x20);
    wr16(obj + S_COUNT, 0);
    wr32(obj + 0x18, flipped(obj) ? 0xFFFF8000 : 0x8000);
    fall_keep(obj);
}

/* $02B8FC. Crawling along the ledge: at a wall, deciding; then jumping up or off. */
void SoldierSD8(uint16_t obj)
{
    wr16(obj + S_TIMER, walls_up(obj));
    uint16_t d3 = 0;
    if (!floor_up16(obj, &d3)) { SoldierFall(obj); return; }
    if (!rd16(obj + S_TIMER)) { think_air(obj); return; }
    uint16_t p = PlayerBySlot(obj);
    if ((int16_t)(rd16(p + O_Y) - rd16(obj + O_Y)) >= 0) {
        if (!(seed() & 3)) { jump_up(obj); return; }
        ledge_off(obj);
        return;
    }
    if ((int16_t)(rd16(obj + S_ENERGY) - 0xC) >= 0) jump_up(obj);
    else ledge_off(obj);
}

/* $02B974. Crouched: its knife out at 8, then crouching again. */
void SoldierSDC(uint16_t obj)
{
    if (dec(obj, S_COUNT)) { crouch(obj, 0xD4); return; }
    if (rd16(obj + S_COUNT) == 8) knife(obj, 6);
}

/* $02B990. Hanging (under a ledge, $118 on): dropping when its energy is spent or its player is
 * below and near. */
void SoldierS114(uint16_t obj)
{
    if (!(clock(obj) & 3)) {
        spend(obj, 1);
        if (rd16(obj + S_ENERGY) & 0x8000) goto drop;
    }
    if (!dec(obj, S_TIMER)) return;
    {
        uint16_t dx, dy, p;
        uint16_t adx = NearestPlayer(obj, &dx, &dy, &p);
        if ((int16_t)(dy - 0x10) < 0 || (int16_t)(adx - 0x18) >= 0 || (seed() & 3)) {
            crouch(obj, 0x118);
            return;
        }
    }
drop:
    ledge_off(obj);
}

/* $02BA04. Hanging: along under the ceiling toward its player. */
void SoldierS118(uint16_t obj)
{
    walls_up(obj);
    if (!ceiling16(obj)) { ledge_off(obj); return; }
    if (player_left(obj)) {
        set8(obj + 0x0E, 3);
        wr32(obj + 0x18, 0xFFFEE000);
    } else {
        clr8(obj + 0x0E, 3);
        wr32(obj + 0x18, 0x12000);
    }
    set_state(obj, 0x11C);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF538);
}

/* $02BA68. Along until a wall or its player is under it: then a throw from there. */
void SoldierS11C(uint16_t obj)
{
    wr16(obj + S_TIMER, walls_up(obj));
    if (!ceiling16(obj)) { ledge_off(obj); return; }
    uint16_t dx, dy, p;
    if (rd16(obj + S_TIMER)) {
        uint16_t adx = NearestPlayer(obj, &dx, &dy, &p);
        if ((int16_t)(adx - 0x18) < 0 || (rd16(obj + S_ENERGY) & 0x8000)) ledge_off(obj);
        return;
    }
    if ((int16_t)(NearestPlayer(obj, &dx, &dy, &p) - 0x10) < 0) throw_low(obj, 0x120);
}

/* $02BAB2. Hanging, its knife out at 8. */
void SoldierS120(uint16_t obj)
{
    if (dec(obj, S_COUNT)) { crouch(obj, 0x118); return; }
    if (rd16(obj + S_COUNT) == 8) knife(obj, 6);
}

/* ---- jumping and falling ---- */

/* $02BB1E. At the top of a jump: a slash ($18) unless its player is well below. */
void SoldierSB0(uint16_t obj)
{
    uint16_t dx, dy, p;
    NearestPlayer(obj, &dx, &dy, &p);
    if ((dy & 0x8000) && (int16_t)(dy + 0x18) < 0) { SoldierFall(obj); return; }
    set_state(obj, 0xD0);
    look(obj, 0x0DFE3E);
    wr16(obj + S_COUNT, 0);
    wr16(obj + S_TIMER, 0x10);
    wr16(obj + S_AUX, 0x18);
    knife(obj, 0x18);
}

/* $02BACE. Jumping: a ceiling cuts it short (landing on it, $114); at the top, a slash (by
 * chance) or falling. */
void SoldierS4C(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectWallsAny(obj, 0xFFF8, &d3, &d4);
    ObjectWallsAny8(obj, &d3, &d4);
    if (!ObjectCeiling(obj, 0xFFF0, &d3, &d4)) { land(obj, 0x114); return; }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (!ChanceRoll(obj, 0x30)) { SoldierSB0(obj); return; }
    SoldierFall(obj);
    wr32(obj + 0x08, 0x0DF4A4);
    set_state(obj, 0xD0);
    wr16(obj + S_AUX, 0xC);
}

/* $02BB7C. Falling (to 6): onto a ledge above by chance once +$48 runs out ($70), or onto the
 * floor once +$4A does: landing ($44). */
void SoldierS40(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C);
    if ((v & 0x80000000u) || (int32_t)(v - 0x60000) < 0) wr32(obj + 0x1C, v + 0x4000);
    uint16_t d3 = 0, d4 = 0;
    ObjectWallsAny(obj, 0xFFF8, &d3, &d4);
    ObjectWallsAny8(obj, &d3, &d4);
    if (dec(obj, S_TIMER) && !ChanceRoll(obj, 0x84)) {
        uint16_t u = 0;
        if (floor_up16(obj, &u)) { land(obj, 0x70); return; }
    }
    if (!dec(obj, S_COUNT)) return;
    if (!floor16_any(obj)) return;
    set_state(obj, 0x44);
    wr16(obj + S_COUNT, 0xF);
    look(obj, 0x0DF81A);
    wr32(obj + 0x18, 0);
    wr16(obj + S_LINK, 0);
    body(obj, 0x10F808);
    mode(obj, 0xE0, 0);
}

/* $02BB60. A slash in the air: $4C frames, then falling. */
void SoldierSD0(uint16_t obj)
{
    if (dec(obj, S_AUX)) {
        set_state(obj, 0x40);
        look(obj, 0x0DF468);
    }
    SoldierS40(obj);
}

/* $02BC14. Landed: crouching a moment. */
void SoldierS44(uint16_t obj)
{
    if (!floor16_any(obj)) { SoldierFall(obj); return; }
    if (dec(obj, S_COUNT)) settle(obj);
}

/* $02BC28. Knocked back: rising, then falling. */
void SoldierSC0(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectWallsAny(obj, 0xFFF8, &d3, &d4);
    ObjectWallsAny8(obj, &d3, &d4);
    ObjectCeiling(obj, 0xFFF0, &d3, &d4);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) SoldierFall(obj);
}

/* $02BC50. The hop back: bouncing twice, then tired. */
void SoldierSE8(uint16_t obj)
{
    walls8(obj);
    uint32_t v = rd32(obj + 0x1C);
    if ((v & 0x80000000u) || (int32_t)(v - 0x60000) < 0) wr32(obj + 0x1C, v + 0x4000);
    uint16_t d3 = 0, d4 = 0;
    if (bit8(obj + 0x1C, 7)) {
        ObjectWalls(obj, 0xFFF8, &d3, &d4);
        ObjectCeiling(obj, 0xFFF0, &d3, &d4);
        return;
    }
    if (!floor16_any(obj)) return;
    if (dec(obj, S_COUNT)) { SoldierTire(obj); return; }
    wr32(obj + 0x1C, 0xFFFE0000);
}

/* ---- shooting ---- */

/* $02BCAE. About to shoot. */
void SoldierS34(uint16_t obj)
{
    if (!dec(obj, S_TIMER)) return;
    next(obj, 4);
    wr16(obj + S_TIMER, 0x10);
}

/* $02BCC2. Aiming at its player $10 frames, then the shot. */
void SoldierS38(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xCF00);
    uint16_t target = 0, type = 0;
    uint32_t faces = 0, muzzles = 0;
    SoldierAimSetup(obj, &target, &type, &faces, &muzzles);
    if (rd16(obj + O_STATE) != 0x38) return;
    if (!dec(obj, S_TIMER)) {
        uint16_t d0, d5, d6, d7 = type;
        FaceToward(obj, target, faces, &d0, &d5, &d6, &d7);
        return;
    }
    next(obj, 4);
    wr16(obj + S_TIMER, 0x10);
    AimedSpawn(obj, target, faces, muzzles, type);
}

/* $02BCF6. After the shot. */
void SoldierS3C(uint16_t obj)
{
    if (dec(obj, S_TIMER)) settle(obj);
}

/* $02BD00. Aiming up at its player, then the shot. */
void SoldierSE0(uint16_t obj)
{
    uint16_t target = PlayerBySlot(obj);
    if (!dec(obj, S_TIMER)) {
        uint16_t d0, d5, d6, d7 = 0x90;
        FaceToward(obj, target, 0x02AE46, &d0, &d5, &d6, &d7);
        return;
    }
    next(obj, 4);
    wr16(obj + S_TIMER, 0x10);
    AimedSpawn(obj, target, 0x02AE46, 0x02AE86, 0x90);
}

/* $02BD34. Then crouching. */
void SoldierSE4(uint16_t obj)
{
    if (dec(obj, S_TIMER)) crouch(obj, 0xD4);
}

/* $02BD3E. Throwing a grenade: the knife's hand at 8. */
void SoldierS48(uint16_t obj)
{
    if (dec(obj, S_COUNT)) { settle(obj); return; }
    if (rd16(obj + S_COUNT) == 8) knife(obj, 6);
}

/* $02BD5A. The mortar's wait; then a dash forward (thrusting, by chance its box ahead). */
void SoldierS50(uint16_t obj)
{
    if (!floor16_any(obj)) { SoldierFall(obj); return; }
    if (!dec(obj, S_COUNT)) return;
    wr16(obj + 0x0C, 0);
    spend(obj, 8);
    if (!ChanceRoll(obj, 0x10)) {
        body(obj, 0xF410F808);
        wr32(obj + 0x08, 0x0DF6B4);
        wr16(obj + S_COUNT, 1);
    } else {
        wr32(obj + 0x08, 0x0DF640);
        wr16(obj + S_COUNT, 0);
    }
    if (facing_left(obj)) {
        wr32(obj + 0x18, 0xFFFB0000);
        next(obj, 4);
        thrust(obj, 0xFFF0);
        return;
    }
    wr32(obj + 0x18, 0x50000);
    next(obj, 8);
    thrust(obj, 0x10);
}

/* the dash's sparks (every 16 frames) */
static void dash_sparks(uint16_t obj)
{
    if (clock(obj) & 0xF) return;
    SoldierSpark(obj, 0, 0xFFFF, rd16(obj + S_COUNT) ? 0x0565D4 : 0x05652E);
}

/* $02BDEA. Dashing left, slowing to a stop. */
void SoldierS54(uint16_t obj)
{
    dash_sparks(obj);
    walls8(obj);
    if (!floor16_any(obj)) { SoldierFall(obj); return; }
    wr32(obj + 0x18, rd32(obj + 0x18) + 0x2000);
    if (rd32(obj + 0x18) & 0x80000000u) return;
    wr32(obj + 0x18, 0);
    settle(obj);
}

/* $02BE46. Right. */
void SoldierS58(uint16_t obj)
{
    dash_sparks(obj);
    walls8(obj);
    if (!floor16_any(obj)) { SoldierFall(obj); return; }
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x2000);
    if (!(rd32(obj + 0x18) & 0x80000000u)) return;
    wr32(obj + 0x18, 0);
    settle(obj);
}

/* $02BEA2. Crouched to shoot: at $24 left a grenade ($98) thrown at its player. */
void SoldierS5C(uint16_t obj)
{
    if (dec(obj, S_COUNT)) { settle(obj); return; }
    if (rd16(obj + S_COUNT) != 0x24) return;
    ThrowAt(obj, PlayerBySlot(obj), 0x98);
}

/* ---- holding a player ---- */

/* $02BF84. Holding the player (+$40) in front of it, shaking (by its +$49 bit 1); the player
 * mashing the pad (+$69 changing) breaks free when the count runs out. */
void SoldierSF4(uint16_t obj)
{
    wr8(obj + 0x20, 0x70);
    uint16_t p = rd16(obj + 0x40);
    if (bit8(p + 0x23, 2) || !bit8(p + 0x21, 7)) { stand(obj); return; }
    uint16_t pad = rd8(p + 0x69) & 0x7C;
    if (pad != rd16(obj + S_COUNT) && dec(obj, S_TIMER)) {
        wr32(p + 0x1C, 0xFFFC0000);
        SoldierTire(obj);
        return;
    }
    wr16(obj + S_COUNT, pad);
    set8(p + 0x23, 1);
    clr8(p + 0x23, 4);
    if (bit8(obj + S_TIMER + 1, 1)) wr16(p + O_Y, (uint16_t)(rd16(obj + O_Y) + 2));
    else wr16(p + O_Y, (uint16_t)(rd16(obj + O_Y) - 3));
}

/* $02BEC6. Grabbed something (+$40): a player is held in front of it, still (both facing);
 * anything else is let go and it throws. */
void SoldierSF0(uint16_t obj)
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
    wr16(p + O_Y, rd16(p + O_Y));
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
    look(obj, 0x0DFC1A);
    wr16(obj + O_FLAGS, 0xEF00);
    wr16(obj + S_COUNT, 0);
    wr8(obj + S_COUNT, rd8(p + 0x69));
    wr16(obj + S_TIMER, rom16(0x02BFF8 + (uint32_t)(int16_t)rd16(v_difficulty)));
    SoldierSF4(obj);
}

/* ---- sliding ---- */

/* $02C168: a wall stops the slide: up */
static void slide_wall(uint16_t obj)
{
    set_state(obj, 0x68);
    wr32(obj + 0x1C, 0xFFFC8000);
}

/* $02C000. Sliding left, braking toward the slope's speed. */
void SoldierS60(uint16_t obj)
{
    if (!(clock(obj) & 0xF)) SoldierSpark(obj, 2, 0xFFFF, 0x0565BA);
    SoldierSlide(obj);
    if (walls8(obj)) { slide_wall(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (ObjectFloor(obj, 0xC, &d3, &d4)) { next(obj, 8); return; }
    if (!(rd16(obj + 0x18) & 0x8000)) {
        wr32(obj + 0x18, rd32(obj + 0x18) - 0x800);
        return;
    }
    int32_t target = (int32_t)rom32(0x02C06C + (uint32_t)(int16_t)(uint16_t)(d4 - 4));
    if ((int32_t)((uint32_t)target - rd32(obj + 0x18)) >= 0) wr32(obj + 0x18, rd32(obj + 0x18) + 0x400);
    else wr32(obj + 0x18, rd32(obj + 0x18) - 0x800);
}

/* $02C080. Right. */
void SoldierS64(uint16_t obj)
{
    if (!(clock(obj) & 0xF)) SoldierSpark(obj, 0xFFFE, 0xFFFF, 0x0565BA);
    SoldierSlide(obj);
    if (walls8(obj)) { slide_wall(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (ObjectFloor(obj, 0xC, &d3, &d4)) { next(obj, 4); return; }
    if (rd16(obj + 0x18) & 0x8000) {
        wr32(obj + 0x18, rd32(obj + 0x18) + 0x800);
        return;
    }
    int32_t target = (int32_t)rom32(0x02C0EC + (uint32_t)(int16_t)(uint16_t)(d4 - 4));
    if ((int32_t)((uint32_t)target - rd32(obj + 0x18)) >= 0) wr32(obj + 0x18, rd32(obj + 0x18) - 0x400);
    else wr32(obj + 0x18, rd32(obj + 0x18) + 0x800);
}

/* $02C100. Off the end: rising. */
void SoldierS68(uint16_t obj)
{
    SoldierSlide(obj);
    walls8(obj);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) next(obj, 4);
}

/* $02C11C. Falling, still sliding: onto a floor, sliding on. */
void SoldierS6C(uint16_t obj)
{
    SoldierSlide(obj);
    uint32_t v = rd32(obj + 0x1C);
    if ((v & 0x80000000u) || (int32_t)(v - 0x60000) < 0) wr32(obj + 0x1C, v + 0x4000);
    if (walls8(obj)) { turn_back(obj); return; }
    uint16_t d3 = 0, d4 = 0;
    if (ObjectFloor(obj, 0xC, &d3, &d4)) return;
    set_state(obj, 0x60);
    if (!(rd16(obj + 0x18) & 0x8000)) next(obj, 4);
}

/* ---- the jump kick ---- */

/* $02C178. Rising; at the top its kick ($A8, linked) out. */
void SoldierS74(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2E00);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xCF00);
    look(obj, 0x0DF684);
    wr16(obj + S_COUNT, 0);
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + 0x48, 0x20);
    wr16(slot + 0x4A, 0);
    wr16(slot + 0x4C, 0);
    wr16(slot + 0x58, obj);
    wr16(obj + S_LINK, slot);
    wr16(slot + O_TYPE, 0xA8);
    wr16(slot + 0x26, 0xA);
    wr8(slot + 0x21, 0x10);
    wr32(slot + 0x2C, 0xFBFFF010);
}

/* $02C1D8. Falling on the kick: a bounce, then a stop. */
void SoldierS78(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C);
    if ((int32_t)(v - 0x60000) < 0) {
        v += 0x3800;
        wr32(obj + 0x1C, v);
        if (v & 0x80000000u) return;
    }
    if (!floor16_any(obj)) return;
    if (rd16(obj + S_COUNT)) {
        next(obj, 4);
        wr32(obj + 0x18, 0);
        wr16(obj + S_COUNT, 0x10);
        return;
    }
    wr16(obj + S_COUNT, (uint16_t)(rd16(obj + S_COUNT) + 1));
    wr16(obj + S_LINK, 0);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x1C, 0xFFFD0000);
}

/* $02C232 */
void SoldierS7C(uint16_t obj)
{
    if (dec(obj, S_COUNT)) settle(obj);
}

/* ---- held by a player ---- */

/* $02C23C. Held (+$23 bit 1, cleared): while held, nothing; thrown (bit 4): flying, hurting;
 * dropped: a jump. */
void SoldierS80(uint16_t obj)
{
    uint8_t s = rd8(obj + 0x23);
    wr8(obj + 0x23, (uint8_t)(s & ~2));
    if (s & 2) return;
    s = rd8(obj + 0x23);
    wr8(obj + 0x23, (uint8_t)(s & ~0x10));
    if (!(s & 0x10)) { jump_a(obj); return; }
    next(obj, 4);
    wr16(obj + 0x26, 4);
    wr32(obj + 0x2C, 0xF808F808);
    wr8(obj + 0x21, 0x40);
    wr8(obj + 0x23, 0x20);
}

/* $02C270. Thrown: rising. */
void SoldierS84(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) next(obj, 4);
}

/* $02C282. Falling: on a floor, standing. */
void SoldierS88(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C);
    if ((int32_t)(v - 0x60000) < 0) wr32(obj + 0x1C, v + 0x4000);
    if (floor16_any(obj)) stand(obj);
}

/* $02C2AA */
void SoldierS8C(uint16_t obj)
{
    (void)obj;
}

/* $02C3B0: the held player at the swing's point (+$4C's entry of the table at $2C3FE) */
static void swing_hold(uint16_t obj)
{
    uint16_t p = rd16(obj + 0x40);
    if (!p) return;
    set8(p + 0x23, 1);
    uint16_t i = (uint16_t)(rd16(obj + S_AUX) << 1);
    uint16_t dx = rom16(0x02C3FE + (uint32_t)(int16_t)i);
    if (facing_left(obj)) dx = (uint16_t)~dx;
    wr16(p + O_X, (uint16_t)(rd16(obj + O_X) + dx));
    i = (uint16_t)(i + 2);
    wr16(p + O_Y, (uint16_t)(rom16(0x02C3FE + (uint32_t)(int16_t)i) + rd16(obj + O_Y)));
}

/* $02C32E. Swinging the player over its head (frames by the table at $2C3F8), then throwing it
 * up and away. */
void SoldierS94(uint16_t obj)
{
    if (!dec(obj, S_COUNT)) { swing_hold(obj); return; }
    wr16(obj + S_AUX, (uint16_t)(rd16(obj + S_AUX) + 2));
    uint16_t n = rom16(0x02C3F8 + (uint32_t)(int16_t)rd16(obj + S_AUX));
    if (!(n & 0x8000)) {
        wr16(obj + S_COUNT, n);
        swing_hold(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + S_COUNT, 0x20);
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

/* $02C2AC. Grabbed something to throw: a player is lifted (facing the screen's middle); anything
 * else is let go and it jumps. */
void SoldierS90(uint16_t obj)
{
    body(obj, 0xF410F808);
    uint16_t p = rd16(obj + 0x40);
    if (!p) { jump_a(obj); return; }
    if (p != v_player1 && p != v_player2) {
        wr8(p + 0x22, rd8(p + 0x22));
        wr16(p + 0x42, 0);
        jump_a(obj);
        return;
    }
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr32(p + 0x18, 0);
    wr32(p + 0x1C, 0);
    next(obj, 4);
    look(obj, 0x0DF74E);
    wr16(obj + S_COUNT, 0);
    wr16(obj + S_AUX, 0xFFFE);
    if ((int16_t)(rd16(obj + O_X) - 0x120) >= 0) wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
    else wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
    SoldierS94(obj);
}

/* $02C406 */
void SoldierS98(uint16_t obj)
{
    if (dec(obj, S_COUNT)) settle(obj);
}

/* ---- the boss's guard's end (+$56: never seen in a run) ---- */

/* $02C410. Falling (slowly) to the floor: then hidden. */
void SoldierSC4(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (!floor16_any(obj)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xD00);
}

/* $02C434. Blown apart: ten pieces ($100) by the tables at $5818C/$581AC (the first five at half
 * speed), the screen shaking; the boss is beaten. */
void SoldierSC8(uint16_t obj)
{
    wr16(v_shakeSprites, 0x10);
    QueueSound(0xCE);
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
    wr16(obj + S_TIMER, 0x80);
}

/* $02C4BC. Then gone, the camera free. */
void SoldierSCC(uint16_t obj)
{
    if (!dec(obj, S_TIMER)) return;
    wr16(obj + O_FLAGS, 0x1000);
    clr8(v_cameraHold, 7);
}

/* $02C4D2. Dropped in from above (+$57 bit 0): down through the cells 8 at a time (a floor cell,
 * after +$48 of them, stops it); below $15F, gone. */
void SoldierSEC(uint16_t obj)
{
    if (rd8(obj + 0x47)) { stand(obj); return; }
    uint16_t x = rd16(obj + O_X);
    for (int k = 0; k < 8; k++) {
        uint16_t a = MapAttribute(x, rd16(obj + O_Y));
        if ((int16_t)(a - 0x30) >= 0 && ((a & 8) || dec(obj, S_TIMER))) {
            wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10));
            if (floor16_any(obj)) stand(obj);
            else SoldierFall(obj);
            return;
        }
        wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 8));
    }
    if ((int16_t)(rd16(obj + O_Y) - 0x15F) >= 0) wr16(obj + O_FLAGS, 0x1000);
}

/* ---- the start ---- */

/* $02B1DC. Step 0: its kind's row, flags, vitality and energy; its palette; its life ($200 plus
 * a random byte); its chance mask by difficulty. Dropping in from above, or standing (falling
 * with no floor). */
void SoldierS00(uint16_t obj)
{
    uint32_t k = 0x02D376 + rd16(obj + 6);
    wr16(obj + 6, rom16(k));
    wr16(obj + S_KIND, rom16(k + 2));
    wr16(obj + O_VITALITY, rom16(k + 4));
    wr16(obj + S_ENERGY0, rom16(k + 6));
    uint16_t sprite = 0x300;
    if (bit8(obj + 0x57, 1)) sprite |= 0x2000;
    wr16(obj + 0x0E, sprite | rd16(0x808A));
    wr16(obj + S_CLOCK, (uint16_t)(rd8(v_randomSeed + 1) + 0x200));
    wr16(obj + S_MASK, rd8(v_randomSeed) & rom16(0x02B294 + (uint32_t)(int16_t)rd16(v_difficulty)));
    wr16(obj + S_ENERGY, rd16(obj + S_ENERGY0));
    if (bit8(obj + 0x57, 0)) {
        set_state(obj, 0xEC);
        wr8(obj + 0x21, 8);
        wr32(obj + 0x28, 0xF010F808);
        wr8(obj + 0x47, 0);
        wr16(obj + O_FLAGS, 0);
        uint16_t r = seed() & 3;
        if (r == 3) r = 0;
        wr16(obj + S_TIMER, r);
        wr16(obj + O_Y, 0xA0);
        return;
    }
    if (!floor16_any(obj)) { SoldierFall(obj); return; }
    stand(obj);
}

/* ---- the type ---- */

/* $02AECE. Type $48. The boss beaten (+$8080 bit 0): shown ones die, hidden ones go. Its life up
 * (unless it never retreats): running away. Its vitality gone: dead (thrown, if it was).
 * Knocked flying by a player; hit: staggered or knocked back; its energy gone: tired; a player
 * near enough (grabbers): grabbed. Then its step. */
void Soldier(uint16_t obj)
{
    Random();
    if (bit8(BOSS_FLAGS, 0)) {
        if (!bit8(obj + O_FLAGS, 7)) {
            ObjectRelease(obj);
            wr16(obj + O_FLAGS, 0x1000);
            return;
        }
        clr8(obj + S_MODE, 3);
        wr16(obj + O_VITALITY, 0xFFFF);
    } else if (bit8(BOSS_FLAGS, 1)) {
        wr16(obj + S_CLOCK, 0);
    }
    if (dec(obj, S_CLOCK)) {
        if (!bit8(obj + O_FLAGS, 7)) goto step;
        if (!bit8(obj + S_MODE, 4) && !bit8(obj + S_KIND, 0)) {
            retreat(obj);
            return;
        }
    }
    wr8(obj + 0x20, (uint8_t)(((rd16(obj + S_CLOCK) << 2) & 0xC) + 0xA0));
    if ((rd16(obj + O_VITALITY) & 0x8000) && !bit8(obj + S_MODE, 3)) {
        AddScore(obj, 0x180);
        ObjectRelease(obj);
        if (!bit8(obj + 0x23, 4)) { become_dying(obj); return; }
        set_state(obj, 0);
        wr16(obj + S_LINK, 0);
        wr16(obj + O_TYPE, 0x110);
        Thrown(obj);
        return;
    }
    if (bit8(obj + 0x22, 1)) {
        clr8(obj + 0x22, 1);
        uint16_t by = rd16(obj + 0x42);
        if (by == v_player1 || by == v_player2) {
            AddScore(obj, 0x180);
            become_114(obj);
            return;
        }
    }
    if (bit8(obj + 0x22, 5) && !bit8(obj + S_MODE, 0)) {
        if (!bit8(obj + 0x23, 4) && floor16_any(obj)) {
            spend(obj, 1);
            ObjectRelease(obj);
            SoldierStagger(obj);
        } else {
            clr8(obj + 0x23, 4);
            ObjectRelease(obj);
            SoldierKnocked(obj);
        }
        goto step;
    }
    if ((rd16(obj + S_ENERGY) & 0x8000) && !bit8(obj + S_MODE, 1)) {
        if (clock(obj) & 3) SoldierTire(obj);
        else SoldierTireCrouch(obj);
        goto step;
    }
    if (!rd16(v_difficulty) && (rd16(v_gameFrames) & 0x7F)) goto step;
    if (bit8(obj + S_KIND, 2) && !bit8(obj + S_MODE, 2)) {
        uint16_t dx, dy, p;
        uint16_t adx = NearestPlayer(obj, &dx, &dy, &p);
        if ((int16_t)(adx - 0x28) < 0 && (int16_t)(dy + 0x30) >= 0 && (int16_t)(dy - 0x20) < 0
            && bit8(p + 0x21, 6) && bit8(p + 0x23, 5))
            SoldierGrab(obj);
    }
step:
    call_object(rom32(0x02B0B8 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

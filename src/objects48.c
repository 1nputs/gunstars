/* Object types from $48044 to $4BF97: a fighter and the spinning fire of Black's dice rooms
 * (types $4C, $360), the dice rooms' bosses ($364, $368, $43C, $658), stage 4's director and its
 * camera events ($444), the sky ship's sinking ($440), and stage 4's fliers, turrets, lines of
 * fire and its boss ($448-$484, $4A4, $4C0-$4DC, $508, $518-$530). The helpers they share with
 * the types before and after them (the brawler of $47xxx, the turrets' library at $4BF98) are
 * at the end. */
#include "game.h"

static void next(uint16_t obj, uint16_t by) { wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + by)); }
/* subq.w #1 on a word, then bpl: 1 when it is now below 0 */
static int dec(uint16_t a)
{
    wr16(a, (uint16_t)(rd16(a) - 1));
    return (rd16(a) & 0x8000) != 0;
}
static void add32(uint16_t a, uint32_t v) { wr32(a, rd32(a) + v); }
static void add16(uint16_t a, uint16_t v) { wr16(a, (uint16_t)(rd16(a) + v)); }
static uint16_t seed(void) { return rd16(v_randomSeed); }
/* a word table's entry by a word index (d0.w, sign-extended) */
static uint16_t tab16(uint32_t table, uint16_t i) { return rom16(table + (uint32_t)(int16_t)i); }
/* the sine and cosine tables ($2977C, $297FC: 256 words) */
#define SINES   0x02977Cu
#define COSINES 0x0297FCu
static int32_t sine(uint16_t i) { return (int16_t)tab16(SINES, i); }
static int32_t cosine(uint16_t i) { return (int16_t)tab16(COSINES, i); }
/* movea.w: a RAM address as the 68k holds it in an address register */
static uint32_t areg(uint16_t a) { return (uint32_t)(int32_t)(int16_t)a; }
/* the object reg_a6 points at */
static uint16_t a6obj(void) { return (uint16_t)reg_a6; }

/* ---- shared helpers ---- */

/* the brawler of the $47xxx type ($35C) and type $4C: its animation (sprite bit 11: facing
 * right; the one at $C980 keeps its sprite's bit 13), its fall, the room's walls */

/* $047F8C. Its animation list stepped; but for the one at $C980, its sprite's bit 13 cleared. */
void BrawlerAnim(uint16_t obj)
{
    AnimLoop(obj);
    if (obj != 0xC980) wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xDFFF);
}

/* $047F86. The same from the list's start. */
void BrawlerAnimStart(uint16_t obj)
{
    wr16(obj + 0x4C, 0);
    BrawlerAnim(obj);
}

/* $047F7A. Stepped, facing right. */
void BrawlerAnimOnFlipped(uint16_t obj)
{
    BrawlerAnim(obj);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
}

/* $047F70. Stepped, facing right when moving left (knocked back). */
void BrawlerAnimOnBySpeed(uint16_t obj)
{
    if (!bit8(obj + 0x18, 7)) {
        BrawlerAnim(obj);
        return;
    }
    BrawlerAnimOnFlipped(obj);
}

/* loc_047F56: from the start, facing right */
static void brawler_anim_start_flipped(uint16_t obj)
{
    BrawlerAnimStart(obj);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
}

/* $047F4C. From the start, facing right when moving left. */
void BrawlerAnimBySpeed(uint16_t obj)
{
    if (!bit8(obj + 0x18, 7)) {
        BrawlerAnimStart(obj);
        return;
    }
    brawler_anim_start_flipped(obj);
}

/* $047F3E. From the start, the way it faces. */
void BrawlerAnimFacing(uint16_t obj)
{
    if (bit8(obj + 0x0E, 3)) brawler_anim_start_flipped(obj);
    else BrawlerAnimStart(obj);
}

/* loc_047F62: stepped, the way it faces */
static void brawler_anim_on(uint16_t obj)
{
    if (bit8(obj + 0x0E, 3)) BrawlerAnimOnFlipped(obj);
    else BrawlerAnim(obj);
}

/* $047F0E. Falling ($4000 a frame): 1 (Z) when it lands at y $148 (then still). */
int BrawlerFall(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (bit8(obj + 0x1C, 7)) return 0;
    if ((int16_t)(rd16(obj + O_Y) - 0x148) < 0) return 0;
    wr16(obj + O_Y, 0x148);
    wr32(obj + 0x1C, 0);
    return 1;
}

/* $047E50. Kept in the room, x $A0-$1A0 (unless +$50 bit 3): +$50 bit 2 set at the left wall,
 * bit 1 at the right. */
void BrawlerWalls(uint16_t obj)
{
    wr8(obj + 0x50, rd8(obj + 0x50) & 0xF9);
    if ((int16_t)(rd16(obj + O_X) - 0xA0) < 0) {
        set8(obj + 0x50, 2);
        if (!bit8(obj + 0x50, 3)) wr16(obj + O_X, 0xA1);
        return;
    }
    if ((int16_t)(rd16(obj + O_X) - 0x1A0) < 0) return;
    set8(obj + 0x50, 1);
    if (!bit8(obj + 0x50, 3)) wr16(obj + O_X, 0x1A1);
}

/* $047E9A. Hit (+$22 bit 5) by a blow (+$23 bit 4), unless already knocked: knocked up and
 * back. */
void BrawlerHitJump(uint16_t obj)
{
    if (!bit8(obj + 0x22, 5) || !bit8(obj + 0x23, 4) || bit8(obj + 0x50, 0)) return;
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFEE000 : 0x12000);
    wr8(obj + 0x21, 0x20);
    wr8(obj + 0x50, 1);
    wr32(obj + 0x1C, 0xFFFD8000);
    wr32(obj + 0x48, 0x048024);
    BrawlerAnimFacing(obj);
    wr16(obj + O_STATE, obj == 0xC980 ? 0x24 : 0x10);
}

/* $047CE0. Within $30 across and $28 up or down of the player (a6): attacking (1, Z). */
int BrawlerNear(uint16_t obj)
{
    uint16_t a6 = a6obj();
    uint16_t d = (uint16_t)(rd16(a6 + O_X) - rd16(obj + O_X));
    if (d & 0x8000) d = (uint16_t)-d;
    if ((int16_t)(d - 0x30) >= 0) return 0;
    d = (uint16_t)(rd16(a6 + O_Y) - rd16(obj + O_Y));
    if (d & 0x8000) d = (uint16_t)-d;
    if ((int16_t)(d - 0x28) >= 0) return 0;
    wr16(obj + O_STATE, obj == 0xC980 ? 0x54 : 0x30);
    wr8(obj + 0x21, 0x30);
    wr32(obj + 0x48, 0x048004);
    BrawlerAnimFacing(obj);
    wr16(obj + 0x5E, 0x10);
    return 1;
}

/* $047D4C. A puff (type $100) at random within 16 of (+$5C, y), less 8. */
void BrawlerPuff(uint16_t obj)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x056450);
    wr16(slot + O_X, (uint16_t)((rd8(v_randomSeed) & 0xF) + (uint16_t)(rd16(obj + 0x5C) - 8)));
    wr16(slot + O_Y, (uint16_t)((rd8(0xFF09) & 0xF) + (uint16_t)(rd16(obj + O_Y) - 8)));
}

/* loc_047C40: crouched before a jump (8 frames) */
static void brawler_crouch(uint16_t obj)
{
    wr8(obj + 0x50, 0);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x5E, 8);
    wr32(obj + 0x48, 0x047FF4);
    BrawlerAnimFacing(obj);
}

/* loc_047912: landed: standing ($10 frames) */
static void brawler_rest(uint16_t obj)
{
    wr16(obj + 0x5C, 0x10);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x50, 0);
    wr32(obj + 0x48, 0x047FF4);
    BrawlerAnimFacing(obj);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x4C, 0x7FFF);
    wr16(obj + O_STATE, obj == 0xC980 ? 0x18 : 4);
}

/* loc_047DE0: standing up (still) */
static void brawler_stand(uint16_t obj)
{
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x50, 0);
    wr32(obj + 0x48, 0x047FA2);
    BrawlerAnimBySpeed(obj);
    wr32(obj + 0x18, 0);
    wr16(obj + O_STATE, obj == 0xC980 ? 0x18 : 4);
}

/* loc_047DDA: at a wall: standing ($20 frames) */
static void brawler_stop(uint16_t obj)
{
    wr16(obj + 0x5C, 0x20);
    brawler_stand(obj);
}

/* loc_047E18: landed from a knock: lying ($20 frames) */
static void brawler_lie(uint16_t obj)
{
    wr16(obj + 0x5C, 0x20);
    wr8(obj + 0x50, 1);
    wr32(obj + 0x48, 0x04802C);
    BrawlerAnimBySpeed(obj);
    wr32(obj + 0x18, 0);
    wr16(obj + O_STATE, obj == 0xC980 ? 0x28 : 0x14);
}

/* Black's dice rooms: their end (the room's object becomes the board's type $260, step $48) */

/* $041044. The room's objects from $C9E0 on gone (types $10). */
void DiceRoomClear(void)
{
    ObjectsVanishFrom(0xC9E0, 0, 0);
}

/* $041E4A. The room's player ($80D8) has done its square ($8050 bit 0 or 1); unless $8052, its
 * square of the board ($9800, at $8040 or $8042) marked done (bit 5, its low bits cleared);
 * $8052 cleared. */
void DiceRoomMark(void)
{
    uint16_t d0;
    if (rd16(0x80D8) == 0xA400) {
        d0 = rd16(0x8040);
        set8(0x8050, 0);
    } else {
        d0 = rd16(0x8042);
        set8(0x8050, 1);
    }
    if (!rd16(0x8052)) {
        uint16_t a = (uint16_t)(0x9800 + d0);
        wr16(a, (uint16_t)((rd16(a) | 0x20) & 0xFFE0));
    }
    wr16(0x8052, 0);
}

/* $041E10. A room ended: the players' sprites' bit 7 and the planes' flags as the board's, no
 * H-interrupt effect, the room's objects gone; the object (a5) becomes the board (type $260,
 * step $48); the square marked. */
void DiceRoomEnd(uint16_t obj)
{
    clr8(0xA40E, 7);
    clr8(0xA60E, 7);
    wr8(v_planeFlagsA, 0xC0);
    wr8(v_planeFlagsB, 0xC0);
    wr16(v_hintEffect, 0);
    wr16(v_hintState, 0);
    DiceRoomClear();
    wr16(obj + O_TYPE, 0x260);
    wr16(obj + O_STATE, 0x48);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + 6, 0);
    wr16(0x80C2, 0);
    DiceRoomMark();
}

/* $02A262. Every 8th frame, sound $CA (a blast). */
void BlastSound8(void)
{
    if (rd16(v_gameFrames) & 7) return;
    QueueSound(0xCA);
}

/* ---- type $4C: a brawler of Black's dice room (the room's player, $80D8, in a6) ---- */

/* loc_048224: landed: standing ($10 frames), its animation held */
static void brawler_land(uint16_t obj)
{
    wr16(obj + 0x5C, 0x10);
    wr16(obj + O_STATE, 4);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x50, 0);
    wr32(obj + 0x48, 0x047FF4);
    BrawlerAnimFacing(obj);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x4C, 0x7FFF);
}

/* loc_04805C: a puff (type $100) */
static void brawler4c_puff(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x056450);
    wr16(obj + O_STATE, 0);
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xCE);
}

/* loc_048314: within $50 (d0) of the player: a jump at it */
static void brawler4c_close(uint16_t obj, uint16_t d0)
{
    if ((int16_t)(d0 - 0x50) >= 0) return;
    wr16(obj + O_STATE, 0x24);
    brawler_crouch(obj);
}

/* $048120. Its set-up: flags, vitality $12, boxes, its look; then waiting. */
void Brawler4CStart(uint16_t obj)
{
    wr16(obj + O_STATE, 0x20);
    wr16(obj + O_FLAGS, 0x8C00);
    wr8(obj + 0x50, 0);
    wr16(obj + O_VITALITY, 0x12);
    wr8(obj + 0x21, 0x22);
    wr16(obj + 0x26, 5);
    wr32(obj + 0x2C, 0xFC04F010);
    wr32(obj + 0x28, 0xFD04F40C);
    wr32(obj + 0x48, 0x048034);
    BrawlerAnimBySpeed(obj);
}

/* $048160. Standing ($5C frames), then walking toward the player at 2. */
void Brawler4CStand(uint16_t obj)
{
    if (!dec(obj + 0x5C)) {
        brawler_anim_on(obj);
        return;
    }
    wr32(obj + 0x48, 0x047FCE);
    if ((int16_t)(rd16(a6obj() + O_X) - rd16(obj + O_X)) >= 0) {
        next(obj, 8);
        wr32(obj + 0x18, 0x20000);
        brawler_anim_start_flipped(obj);
        return;
    }
    next(obj, 4);
    wr32(obj + 0x18, 0xFFFE0000);
    BrawlerAnimStart(obj);
}

/* $0481A0. Walking left: at the room's wall it stops; within $50 of the player, a jump. */
void Brawler4CWalkLeft(uint16_t obj)
{
    if (bit8(obj + 0x50, 2)) {
        brawler_stop(obj);
        return;
    }
    BrawlerAnim(obj);
    brawler4c_close(obj, (uint16_t)~(uint16_t)(rd16(a6obj() + O_X) - rd16(obj + O_X)));
}

/* $0481BE. Walking right, the same. */
void Brawler4CWalkRight(uint16_t obj)
{
    if (bit8(obj + 0x50, 1)) {
        brawler_stop(obj);
        return;
    }
    BrawlerAnimOnFlipped(obj);
    brawler4c_close(obj, (uint16_t)(rd16(a6obj() + O_X) - rd16(obj + O_X)));
}

/* $0481D8. Knocked up: falling, then lying. */
void Brawler4CKnocked(uint16_t obj)
{
    if (BrawlerFall(obj)) brawler_lie(obj);
}

/* $0481E2. Lying ($5C frames), then standing. */
void Brawler4CLying(uint16_t obj)
{
    if (dec(obj + 0x5C)) brawler_stand(obj);
}

/* $0481EC. Hit: while +$23 bit 1 stays set; then thrown. */
void Brawler4CHit(uint16_t obj)
{
    int was = bit8(obj + 0x23, 1);
    clr8(obj + 0x23, 1);
    if (was) return;
    next(obj, 4);
    BrawlerAnimOnBySpeed(obj);
}

/* $048200. Thrown: against a wall of the room, or on landing, its vitality runs out. */
void Brawler4CThrown(uint16_t obj)
{
    if (!(rd8(obj + 0x50) & 6) && !BrawlerFall(obj)) return;
    wr16(obj + O_VITALITY, 0xFFFF);
}

/* $04821C. Falling, then standing ($10 frames). */
void Brawler4CFall(uint16_t obj)
{
    if (BrawlerFall(obj)) brawler_land(obj);
}

/* $048258. Crouched ($5E frames); then a jump at a random speed (up $4C000 less a random 0-7
 * quarter, across -2 plus a random 0-7 eighth), facing the player. The jump's speed up takes the
 * caller's d0's high word, shifted, as its fraction (the 68k swaps d0 having set its low word
 * only): d0 comes in. */
void Brawler4CCrouch(uint16_t obj, uint32_t d0)
{
    if (dec(obj + 0x5E)) {
        next(obj, 4);
        d0 = (uint32_t)(seed() & 7) << 16 | d0 >> 16;
        wr32(obj + 0x1C, (d0 >> 2) + 0xFFFB4000u);
        d0 = (uint32_t)(rd8(v_randomSeed) & 0x38) << 16;
        wr32(obj + 0x18, (d0 >> 4) - 0x20000);
        if ((int16_t)(rd16(a6obj() + O_X) - rd16(obj + O_X)) >= 0) set8(obj + 0x0E, 3);
        else clr8(obj + 0x0E, 3);
    }
    brawler_anim_on(obj);
}

/* $0482B2. Jumping: near the player it attacks; else on the way down, falling. */
void Brawler4CJump(uint16_t obj)
{
    BrawlerFall(obj);
    if (BrawlerNear(obj)) return;
    if (bit8(obj + 0x1C, 7)) return;
    next(obj, 4);
    wr32(obj + 0x48, 0x048034);
    BrawlerAnimFacing(obj);
}

/* $0482DA. Falling from the jump: landed, it stands; else near the player it attacks. */
void Brawler4CJumpFall(uint16_t obj)
{
    if (BrawlerFall(obj)) {
        brawler_rest(obj);
        return;
    }
    BrawlerNear(obj);
}

/* $0482E8. Attacking ($5E frames, falling), then crouching again. */
void Brawler4CAttack(uint16_t obj)
{
    if (BrawlerFall(obj)) {
        brawler_land(obj);
        return;
    }
    if (!dec(obj + 0x5E)) {
        brawler_anim_on(obj);
        return;
    }
    wr16(obj + O_STATE, 0x20);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x48, 0x048034);
    BrawlerAnimFacing(obj);
}

/* $048044. Type $4C: gone (a puff) when the boss timer runs or its vitality does (16 points);
 * kept in the room; hit by a player's grab it is thrown, else knocked by a hit; then its step
 * (the room's player in a6). */
void Brawler4C(uint16_t obj)
{
    if (!(rd16(v_bossTimer) & 0x8000)) {
        brawler4c_puff(obj);
        return;
    }
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 16);
        brawler4c_puff(obj);
        return;
    }
    BrawlerWalls(obj);
    if (bit8(obj + 0x23, 1)) {
        clr8(obj + 0x23, 1);
        wr16(obj + O_STATE, 0x18);
        wr8(obj + 0x21, 0x20);
        wr8(obj + 0x50, 1);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr32(obj + 0x48, 0x048024);
        BrawlerAnimFacing(obj);
    } else {
        BrawlerHitJump(obj);
    }
    reg_a6 = areg(rd16(0x80D8));
    uint32_t fn = rom32(0x0480EC + (uint32_t)(int16_t)rd16(obj + O_STATE));
    /* d0's high word is the previous object's (ObjectsUpdate's loop keeps none): taken as 0 */
    if (fn == 0x048258) Brawler4CCrouch(obj, 0);
    else call_object(fn, obj);
}

/* ---- type $360: a fire spinning out of the dice room's floor ---- */

/* $048328. Its speed's fraction and power from its kind (+$48: 0-3); then every other frame a
 * flame (type $100, hurting) on a turning radius of $20 by $40 about it, turning by $22 each
 * way of its speed. */
void FireSpinner(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0xA00);
        uint16_t k = rd16(obj + 0x48);
        uint16_t d1 = rom8(0x048370 + (uint32_t)(int16_t)k);
        uint16_t d2 = rom8(0x048372 + (uint32_t)(int16_t)k);
        if (bit8(obj + 0x18, 7)) d1 = (uint16_t)-d1;
        wr16(obj + 0x18, rd16(obj + 0x18) | d1);
        wr16(obj + 0x26, d2);
        return;
    }
    if (!bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x048400);
    wr8(slot + 0x21, 0x10);
    wr32(slot + 0x2C, 0xFF01FF01);
    wr16(slot + 0x26, rd16(obj + 0x26));
    wr8(slot + 0x23, 0x20);
    uint16_t i = rd16(obj + 0x48) & 0x1FE;
    uint16_t dy = (uint16_t)(((uint32_t)sine(i) << 5) >> 16);
    uint16_t dx = (uint16_t)(((uint32_t)cosine(i) << 6) >> 16);
    wr16(slot + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
    wr16(slot + O_X, (uint16_t)(dx + rd16(obj + O_X)));
    if (bit8(obj + 0x18, 7)) add16(obj + 0x48, (uint16_t)-0x22);
    else add16(obj + 0x48, 0x22);
}

/* The objects' shared helpers: free slots, links between objects. An object's routine finds a
 * slot for what it spawns by searching a range of the $60-byte slots for a type of 0. */
#include "game.h"

/* $055F6A. A free slot from $CE00 below $D400: Z set and the slot, or d0 1 and $D400. */
int SlotFreeMid(uint16_t *slot, uint16_t *type)
{
    uint16_t a = 0xCE00;
    for (;;) {
        *type = rd16(a);
        if (!*type) { *slot = a; return 1; }
        a = (uint16_t)(a + 0x60);
        if ((int16_t)(a - 0xD400) >= 0) break;
    }
    *slot = a;
    *type = 1;
    return 0;
}

/* n slots from *slot, each $60 on but the last: Z set at a free one, else the last's type in d0
 * (the 68k's unrolled search, entered at one of its points) */
static int slots_from(uint16_t *slot, uint16_t *type, int n)
{
    uint16_t a = *slot;
    for (int k = 0;; k++) {
        *type = rd16(a);
        if (!*type || k == n - 1) break;
        a = (uint16_t)(a + 0x60);
    }
    *slot = a;
    return *type == 0;
}

/* $055FC0. A free slot from $D400 to $DE20 (28). */
int SlotFreeHigh(uint16_t *slot, uint16_t *type)
{
    *slot = 0xD400;
    return slots_from(slot, type, 28);
}

/* $055F84. A free slot from $D400 to $DE20; none, a puff's (type $100 or $104) taken, cleared
 * (d0 then -1; the clearing leaves d1 0 and a1 past the slot, the caller's as they are); none of
 * those either: d0 1, no slot (Z clear). */
int SlotFreeOrPuff(uint16_t *slot, uint16_t *type, uint16_t *d1, uint32_t *a1)
{
    if (SlotFreeHigh(slot, type)) return 1;
    for (uint16_t a = 0xD400; (int16_t)(a - 0xDE80) < 0; a = (uint16_t)(a + 0x60)) {
        if (rd16(a) == 0x100 || rd16(a) == 0x104) {
            *slot = a;
            for (uint16_t k = 0; k < 0x60; k += 4) wr32((uint16_t)(a + k), 0);
            *type = 0xFFFF;
            *d1 = 0;
            *a1 = 0xFFFF0000u | (uint16_t)(a + 0x60);
            return 1;
        }
    }
    *slot = 0xDE80;
    *type = 1;
    return 0;
}

/* $055FEC. The same search from a slot with 24 left. */
int SlotFree24(uint16_t *slot, uint16_t *type) { return slots_from(slot, type, 24); }

/* $056064. 12 left. */
int SlotFree12(uint16_t *slot, uint16_t *type) { return slots_from(slot, type, 12); }

/* $056082. 9 left. */
int SlotFree9(uint16_t *slot, uint16_t *type) { return slots_from(slot, type, 9); }

/* $0560A0. 6 left. */
int SlotFree6(uint16_t *slot, uint16_t *type) { return slots_from(slot, type, 6); }

/* $0560B4. The last 4. */
int SlotFree4(uint16_t *slot, uint16_t *type) { return slots_from(slot, type, 4); }

/* $029E9C. The player an object goes after: player 1 for $400 frames, player 2 the next $400;
 * the other when that one is out of the game, or neither blinking nor shown. Player 1 when
 * neither plays. */
uint16_t PlayerTarget(void)
{
    uint8_t p = rd8(v_players);
    if (!p) return v_player1;
    uint16_t want = (rd16(v_gameFrames) & 0x400) ? v_player2 : v_player1;
    uint16_t other = want == v_player1 ? v_player2 : v_player1;
    if (!(p & (want == v_player1 ? 1 : 2))) return other;
    if (bit8(want + 0x23, 2)) return want;
    if (!bit8(want + O_FLAGS, 7)) return other;
    return want;
}

/* $04C4AC. An object's fields from a list: its flags, sprite, mappings (+8, +$A), +$20; then (count
 * permitting) +$21, its box (+$28), vitality; then +$2C, +$26. The list and count move on. */
void ObjectSetup(uint16_t obj, uint32_t *data, uint16_t *count)
{
    uint32_t a = *data;
    wr16(obj + O_FLAGS, bus_rd16(a)); a += 2;
    wr16(obj + 0x0E, bus_rd16(a)); a += 2;
    wr16(obj + 0x08, bus_rd16(a)); a += 2;
    wr16(obj + 0x0A, bus_rd16(a)); a += 2;
    wr8(obj + 0x20, bus_rd8(a)); a += 1;
    *count = (uint16_t)(*count - 1);
    if (!(*count & 0x8000)) {
        wr8(obj + 0x21, bus_rd8(a)); a += 1;
        wr32(obj + 0x28, bus_rd32(a)); a += 4;
        wr16(obj + O_VITALITY, bus_rd16(a)); a += 2;
        *count = (uint16_t)(*count - 1);
        if (!(*count & 0x8000)) {
            wr32(obj + 0x2C, bus_rd32(a)); a += 4;
            wr16(obj + 0x26, bus_rd16(a)); a += 2;
        }
    }
    *data = a;
}

/* $04C4A8. The same, the object's step on 4. */
void ObjectSetupNext(uint16_t obj, uint32_t *data, uint16_t *count)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    ObjectSetup(obj, data, count);
}

/* $04C4A6. All three parts. */
void ObjectSetupAll(uint16_t obj, uint32_t *data, uint16_t *count)
{
    *count = 2;
    ObjectSetupNext(obj, data, count);
}

/* $02A252. An angle (d2, $10000 a turn) as an even index of 256 (d2) and its nearest 16th (d0). */
uint16_t AngleIndex(uint16_t *angle)
{
    *angle = (uint16_t)(((int16_t)*angle >> 7) & 0x1FE);
    return (uint16_t)((*angle + 8) & 0x1F0);
}

/* $04C3EA. Off the screen (x $70-$1CF, y $88-$1AF): removed. */
void ObjectRemoveOffscreen(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_Y) - 0x88) < 0 || (int16_t)(rd16(obj + O_Y) - 0x1B0) >= 0
        || (int16_t)(rd16(obj + O_X) - 0x70) < 0 || (int16_t)(rd16(obj + O_X) - 0x1D0) >= 0)
        wr16(obj + O_FLAGS, 0x1000);
}

/* $02A10E. An object (not of types keep1, keep2) turned into type $10, to be removed. */
void ObjectVanish(uint16_t slot, uint16_t keep1, uint16_t keep2)
{
    uint16_t t = rd16(slot);
    if (!t || t == keep1 || t == keep2) return;
    wr16(slot + O_TYPE, 0x10);
    wr16(slot + O_FLAGS, 0x1000);
}

/* $02A0E0. Every object from `from` to the slots' end, the one at $A800 and those from $C800
 * below the boss's. */
void ObjectsVanishFrom(uint16_t from, uint16_t keep1, uint16_t keep2)
{
    uint16_t a = from;
    do {
        ObjectVanish(a, keep1, keep2);
        a = (uint16_t)(a + 0x60);
    } while ((int16_t)(a - 0xE000) < 0);
    ObjectVanish(0xA800, keep1, keep2);
    for (a = 0xC800; (int16_t)(a - 0xC980) < 0; a = (uint16_t)(a + 0x60)) ObjectVanish(a, keep1, keep2);
}

/* ---- helpers of the players' effects (an effect's +$23 bit 7: player 1's) ---- */

/* $019D88. The effect's player, kept at +$4C. */
uint16_t EffectOwner(uint16_t obj)
{
    uint16_t p = bit8(obj + 0x23, 7) ? v_player1 : v_player2;
    wr16(obj + 0x4C, p);
    return p;
}

/* $019DA6. At the player's hand (its +$9E, +$A0 from it). */
void EffectAtHand(uint16_t obj, uint16_t player)
{
    wr16(obj + O_X, (uint16_t)(rd16(player + 0x9E) + rd16(player + O_X)));
    wr16(obj + O_Y, (uint16_t)(rd16(player + 0xA0) + rd16(player + O_Y)));
}

/* $019DFA. In front of the planes as its player is. */
void EffectPriority(uint16_t obj)
{
    if (rd16(rd16(obj + 0x4C) + 0x0E) & 0x8000) set8(obj + 0x0E, 7);
    else clr8(obj + 0x0E, 7);
}

/* $019E16. With two players, each one's effects shown (and hitting) on alternate frames. */
void EffectFlicker(uint16_t obj)
{
    int show = 1;
    if (rd8(v_players) == 3) {
        int odd = bit8(v_gameFrames + 1, 0);
        show = bit8(obj + 0x23, 7) ? odd : !odd;
    }
    if (show) {
        wr8(obj + 0x21, 0x40);
        set8(obj + O_FLAGS, 7);
    } else {
        wr8(obj + 0x21, 0);
        clr8(obj + O_FLAGS, 7);
    }
}

/* $019142. Its speeds from its angle (+6, a 256th of a turn) by the sine table at $297FC,
 * shifted left by `shift`. */
void SpeedFromAngle(uint16_t obj, uint16_t shift)
{
    uint16_t i = (uint16_t)(rd8(obj + 6) << 1);
    unsigned n = shift & 63;
    int32_t s = (int16_t)rom16(0x0297FC + i), c = (int16_t)rom16(0x0297FC - 0x80 + i);
    wr32(obj + 0x18, n >= 32 ? 0 : (uint32_t)s << n);
    wr32(obj + 0x1C, n >= 32 ? 0 : (uint32_t)c << n);
}

/* ---- finding the players, spawning ---- */

/* $029F7A. The player nearer in x (of those in the game; a blinking or shown one preferred to a
 * hidden one): |dx| returned, dx, dy (to it from the object) and the player. Neither playing:
 * $FFFF, $FFFF, 0 and player 1. */
uint16_t NearestPlayer(uint16_t obj, uint16_t *dx, uint16_t *dy, uint16_t *player)
{
    uint8_t p = rd8(v_players);
    uint16_t a = v_player1, d0;
    *player = a;
    if (!(p & 3)) {
        *dx = 0xFFFF;
        *dy = 0;
        return 0xFFFF;
    }
    int both = (p & 1) && (p & 2);
    if (!(p & 1)) {
        a = v_player2;
    } else if (both) {
        int seen1 = bit8(v_player1 + 0x23, 2) || bit8(v_player1 + O_FLAGS, 7);
        int seen2 = bit8(v_player2 + 0x23, 2) || bit8(v_player2 + O_FLAGS, 7);
        if (!seen1) a = v_player2;
        else if (seen2) {
            uint16_t x1 = (uint16_t)(rd16(v_player1 + O_X) - rd16(obj + O_X));
            uint16_t x2 = (uint16_t)(rd16(v_player2 + O_X) - rd16(obj + O_X));
            uint16_t a1 = (x1 & 0x8000) ? (uint16_t)-x1 : x1, a2 = (x2 & 0x8000) ? (uint16_t)-x2 : x2;
            if ((int16_t)(a1 - a2) < 0) { d0 = a1; *dx = x1; }
            else { d0 = a2; *dx = x2; a = v_player2; }
            *player = a;
            *dy = (uint16_t)(rd16(a + O_Y) - rd16(obj + O_Y));
            return d0;
        }
    }
    *player = a;
    *dx = (uint16_t)(rd16(a + O_X) - rd16(obj + O_X));
    d0 = (*dx & 0x8000) ? (uint16_t)-*dx : *dx;
    *dy = (uint16_t)(rd16(a + O_Y) - rd16(obj + O_Y));
    return d0;
}

/* $056AF4. An object of `type` at (dx, dy) from this one, `arg` at its +$48, in a free slot of
 * $D400 on: Z set and the slot, if there was one. */
int SpawnAt(uint16_t obj, uint16_t type, uint16_t dy, uint16_t dx, uint16_t arg, uint16_t *slot)
{
    uint16_t t;
    if (!SlotFreeHigh(slot, &t)) return 0;
    wr16(*slot + O_TYPE, type);
    wr16(*slot + O_X, (uint16_t)(dx + rd16(obj + O_X)));
    wr16(*slot + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
    wr16(*slot + 0x48, arg);
    return 1;
}

/* $04C16E. An object $490 here, aimed at the player it goes after (its +$48 the angle in 32nds,
 * times 4). */
int SpawnTowardPlayer(uint16_t obj, uint16_t *slot)
{
    uint16_t target = PlayerTarget();
    uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
    uint16_t d0 = AngleIndex(&a);
    d0 = (uint16_t)(((int16_t)d0 >> 2) & 0x7C);
    return SpawnAt(obj, 0x490, 0, 0, d0, slot);
}

/* $056206. An animation list (+$48) started: its first frame's time, sprite, mappings. The list's
 * frames take the palette at $FF808A. */
void AnimListStart(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8D00);
    uint32_t a = rd32(obj + 0x48);
    wr16(obj + 0x4C, bus_rd16(a)); a += 2;
    wr16(obj + 0x0E, bus_rd16(a)); a += 2;
    wr16(obj + 0x08, bus_rd16(a)); a += 2;
    wr16(obj + 0x0A, bus_rd16(a)); a += 2;
    wr32(obj + 0x48, a);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(0x808A));
}

/* $05801A. A looping list of frames (+$58, started over from +$5C): each a count of frames to
 * show it, its sprite word and the two words at +8 (a negative count: start over); +$56 counts
 * the frames down. */
void FrameList(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 1));
    if (!(rd16(obj + 0x56) & 0x8000)) return;
    for (;;) {
        uint32_t a = rd32(obj + 0x58);
        wr16(obj + 0x56, bus_rd16(a));
        if (!(rd16(obj + 0x56) & 0x8000)) {
            wr16(obj + 0x0E, bus_rd16(a + 2));
            wr16(obj + 0x08, bus_rd16(a + 4));
            wr16(obj + 0x0A, bus_rd16(a + 6));
            wr32(obj + 0x58, a + 8);
            return;
        }
        wr32(obj + 0x58, rd32(obj + 0x5C));
    }
}

/* where an emitter's object goes: around the emitter, by the random bytes at $FF08 (x, masked by
 * +$4C) and $FF09 (y, by +$4E), $FF0A's and $FF0B's signs taking the side. d0's top byte is
 * SlotFreeHigh's; d1's is taken as 0 (+$4E is below $100 for every caller) */
static void emit_place(uint16_t obj, uint16_t slot, uint16_t t, uint32_t *flip)
{
    uint16_t d0 = (uint16_t)((t & 0xFF00) | rd8(0xFF08));
    uint16_t d1 = rd8(0xFF09);
    d0 &= rd16(obj + 0x4C);
    d1 &= rd16(obj + 0x4E);
    if (rd8(0xFF0A) & 0x80) {
        d0 = (uint16_t)~d0;
        if (flip) *flip = ~*flip;
    }
    if (!(rd8(0xFF0B) & 0x80)) d1 = (uint16_t)~d1;
    wr16(slot + O_TYPE, rd16(obj + 0x58));
    wr32(slot + 0x48, rd32(obj + 0x54));
    wr16(slot + O_X, (uint16_t)(d0 + rd16(obj + O_X)));
    wr16(slot + O_Y, (uint16_t)(d1 + rd16(obj + O_Y)));
}

/* $057BC2. Every +$4A+1 frames an object (type +$58, its animation list +$54) around this one
 * (see emit_place), thrown out sideways and up; a sound every 8th frame. */
void DebrisBurst(uint16_t obj)
{
    wr16(obj + 0x5A, (uint16_t)(rd16(obj + 0x5A) - 1));
    if (!(rd16(obj + 0x5A) & 0x8000)) return;
    wr16(obj + 0x5A, rd16(obj + 0x4A));
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    uint32_t vx = 0x18000;
    emit_place(obj, slot, t, &vx);
    wr32(slot + 0x18, vx);
    wr16(slot + 0x1C, 0xFFFC);
    if (rd16(v_gameFrames) & 7) return;
    QueueSound(0xCE);
}

/* loc_0577BA (in $057790): the same, its speed's whole parts +$50, +$52 */
void DebrisEmit(uint16_t obj)
{
    wr16(obj + 0x5A, (uint16_t)(rd16(obj + 0x5A) - 1));
    if (!(rd16(obj + 0x5A) & 0x8000)) return;
    wr16(obj + 0x5A, rd16(obj + 0x4A));
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    emit_place(obj, slot, t, 0);
    wr16(slot + 0x18, rd16(obj + 0x50));
    wr16(slot + 0x1C, rd16(obj + 0x52));
}

/* $0561F4. An object that plays its animation list once ($20 = $B0), then is removed. */
void AnimList(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 1));
        wr8(obj + 0x20, 0xB0);
        AnimListStart(obj);
        return;
    }
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if (rd16(obj + 0x4C)) return;
    uint32_t a = rd32(obj + 0x48);
    wr16(obj + 0x4C, bus_rd16(a)); a += 2;
    if (rd16(obj + 0x4C) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr16(obj + 0x0E, bus_rd16(a)); a += 2;
    wr16(obj + 0x08, bus_rd16(a)); a += 2;
    wr16(obj + 0x0A, bus_rd16(a)); a += 2;
    wr32(obj + 0x48, a);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(0x808A));
}

/* $032288. A free slot from *slot below $CE00: Z set; else d0 1. */
int SlotFreeBelowCE(uint16_t *slot, uint16_t *type)
{
    for (;;) {
        *type = rd16(*slot);
        if (!*type) return 1;
        *slot = (uint16_t)(*slot + 0x60);
        if ((int16_t)(*slot - 0xCE00) >= 0) break;
    }
    *type = 1;
    return 0;
}

/* $02A234. An index of 256 (even) as its nearest eighth, times $40. */
uint16_t AngleEighthOf(uint16_t index)
{
    return (uint16_t)((index + 0x20) & 0x1C0);
}

/* $02A22E. An angle ($10000 a turn) as an index of 256 (d2) and its nearest eighth (d0). */
uint16_t AngleEighth(uint16_t *angle)
{
    *angle = (uint16_t)(((int16_t)*angle >> 7) & 0x1FE);
    return AngleEighthOf(*angle);
}

/* $04C2F0. An object $4A8 (a small one that hurts: $26 6) at x, y, moving 8 times the tables'
 * entry `angle` (ytable for y, xtable for x). */
int Spawn4A8(uint16_t angle, uint16_t x, uint16_t y, uint32_t ytable, uint32_t xtable, uint16_t *slot)
{
    uint16_t t;
    if (!SlotFreeHigh(slot, &t)) return 0;
    uint16_t a = *slot;
    wr16(a + O_TYPE, 0x4A8);
    wr16(a + O_FLAGS, 0x8C00);
    wr16(a + 0x08, 0);
    wr16(a + 0x0A, 0xFCFC);
    wr8(a + 0x20, 0x80);
    wr8(a + 0x21, 0x10);
    wr32(a + 0x2C, 0xFF01FF01);
    wr16(a + 0x26, 6);
    wr16(a + O_X, x);
    wr16(a + O_Y, y);
    int32_t vy = (int16_t)bus_rd16(ytable + (uint32_t)(int16_t)angle);
    int32_t vx = (int16_t)bus_rd16(xtable + (uint32_t)(int16_t)angle);
    wr32(a + 0x1C, (uint32_t)vy << 3);
    wr32(a + 0x18, (uint32_t)vx << 3);
    return 1;
}

/* $04C2E4. The same by the sine table (y by its cosine). */
int Spawn4A8Round(uint16_t angle, uint16_t x, uint16_t y, uint16_t *slot)
{
    return Spawn4A8(angle, x, y, 0x02977C, 0x0297FC, slot);
}

/* ---- facing: an object's mappings (+8) for one of 8 directions, its flip ---- */

/* $02A6CA. The mappings for direction `dir` (0-$1C by 4, from `table` of longs): the left half
 * drawn as the right half flipped (bit 11 of the sprite word), straight up and down ($8, $18)
 * as they are. With d7 bit 0 (cleared) the flip is kept and only the mappings follow it. d6 is
 * left as the entry taken. */
void FaceAngle(uint16_t obj, uint16_t dir, uint32_t table, uint16_t *d6, uint16_t *d7)
{
    *d6 = dir;
    if (*d7 & 1) {
        *d7 = (uint16_t)(*d7 & ~1u);
        if (!(rd8(obj + 0x0E) & 8)) goto take;
    } else {
        if (dir == 0x18 || dir == 8) goto take;
        if ((int16_t)(dir - 0x18) >= 0 || (int16_t)(dir - 8) < 0) {
            wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
            wr32(obj + 0x08, bus_rd32(table + (uint32_t)(int16_t)dir));
            return;
        }
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
    }
    if (dir & 4) {
        *d6 ^= 8;
        goto take;
    }
    if (dir & 8) goto take;
    *d6 ^= 0x10;
take:
    wr32(obj + 0x08, bus_rd32(table + (uint32_t)(int16_t)*d6));
}

/* $02A6A8. Facing toward `target`: the angle's eighth (d0, times 4) and 32nd (d5, times 4). */
void FaceToward(uint16_t obj, uint16_t target, uint32_t table, uint16_t *d0, uint16_t *d5, uint16_t *d6, uint16_t *d7)
{
    uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
    uint16_t e = AngleEighth(&a);
    e = (uint16_t)((int16_t)e >> 2);
    *d5 = e;
    e = (uint16_t)((int16_t)e >> 2);
    *d0 = e;
    FaceAngle(obj, e, table, d6, d7);
}

/* ---- an object on the level's floor (its +$47 bit 1: held on a floor already) ---- */

/* the floor's kinds: from the attribute's cell (x, y the point found in it, mask 7) the object
 * put on its surface; d4 the kind (4 flat, 8 and $C the steep slopes, $10 and $14 the gentle
 * ones); Z set when the attribute was 0 */
static int floor_done(uint16_t obj, uint16_t attr, uint16_t kind, uint16_t *d4)
{
    *d4 = kind;
    (void)obj;
    return attr == 0;
}

static uint16_t cell(uint16_t v, uint16_t map, uint16_t mask) { return (uint16_t)((uint16_t)(v + rd16(map)) & mask); }

/* $029CD8 */
int ObjFloor4A(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4)
{
    (void)x;
    wr32(obj + 0x1C, 0);
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - cell(y, v_mapY, mask)));
    return floor_done(obj, attr, 4, d4);
}

/* $029CF2 */
int ObjFloor4B(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4)
{
    (void)x;
    wr32(obj + 0x1C, 0);
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - cell(y, v_mapY, mask)));
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 7));
    return floor_done(obj, attr, 4, d4);
}

/* $029D10 */
int ObjFloor4C(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4)
{
    (void)x;
    wr32(obj + 0x1C, 0);
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - (uint16_t)(cell(y, v_mapY, mask) + 4)));
    return floor_done(obj, attr, 4, d4);
}

/* $029D2C */
int ObjFloor8(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4)
{
    wr32(obj + 0x1C, 0);
    uint16_t d = (uint16_t)((cell(x, v_mapX, mask) ^ mask) - cell(y, v_mapY, mask));
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + d));
    return floor_done(obj, attr, 8, d4);
}

/* $029D4E */
int ObjFloorC(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4)
{
    wr32(obj + 0x1C, 0);
    uint16_t d = (uint16_t)(cell(x, v_mapX, mask) - cell(y, v_mapY, mask));
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + d));
    return floor_done(obj, attr, 0xC, d4);
}

/* the gentle slopes' half-cells: (x's cell / 2) & 3, flipped (xor mask) for $10 */
static uint16_t half(uint16_t x, uint16_t mask, int flip)
{
    uint16_t h = (uint16_t)(((int16_t)(uint16_t)(x + rd16(v_mapX)) >> 1) & 3);
    return flip ? (uint16_t)(h ^ mask) : h;
}

static int gentle(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4, int flip, int low, uint16_t kind)
{
    wr32(obj + 0x1C, 0);
    uint16_t d = (uint16_t)(half(x, mask, flip) - cell(y, v_mapY, mask));
    if (low) d = (uint16_t)(d - 4);
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + d));
    return floor_done(obj, attr, kind, d4);
}

/* $029D6E */
int ObjFloor10A(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4) { return gentle(obj, x, y, attr, mask, d4, 1, 0, 0x10); }
/* $029D94 */
int ObjFloor10B(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4) { return gentle(obj, x, y, attr, mask, d4, 1, 1, 0x10); }
/* $029DBC */
int ObjFloor14A(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4) { return gentle(obj, x, y, attr, mask, d4, 0, 0, 0x14); }
/* $029DE0 */
int ObjFloor14B(uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t mask, uint16_t *d4) { return gentle(obj, x, y, attr, mask, d4, 0, 1, 0x14); }

/* a floor kind by its routine's address (a table's entry) */
static int floor_kind(uint32_t fn, uint16_t obj, uint16_t x, uint16_t y, uint16_t attr, uint16_t *d4)
{
    switch (fn) {
    case 0x029CD8: return ObjFloor4A(obj, x, y, attr, 7, d4);
    case 0x029CF2: return ObjFloor4B(obj, x, y, attr, 7, d4);
    case 0x029D10: return ObjFloor4C(obj, x, y, attr, 7, d4);
    case 0x029D2C: return ObjFloor8(obj, x, y, attr, 7, d4);
    case 0x029D4E: return ObjFloorC(obj, x, y, attr, 7, d4);
    case 0x029D6E: return ObjFloor10A(obj, x, y, attr, 7, d4);
    case 0x029D94: return ObjFloor10B(obj, x, y, attr, 7, d4);
    case 0x029DBC: return ObjFloor14A(obj, x, y, attr, 7, d4);
    case 0x029DE0: return ObjFloor14B(obj, x, y, attr, 7, d4);
    default: call68k_missing(fn); return 1;   /* an attribute the tables do not cover */
    }
}

/* $029E06 / $029E1E: a wall's edge: x put to the cell's right (kind 4, $14) or left (8, $10) */
static int wall_right(uint16_t obj, uint16_t x, uint16_t *d3, uint16_t *d4)
{
    uint16_t c = (uint16_t)((uint16_t)(x + rd16(v_mapX)) & 7);
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + (uint16_t)(8 - c)));
    *d4 = 4;
    *d3 = 0x14;
    return 0;
}

static int wall_left(uint16_t obj, uint16_t x, uint16_t *d3, uint16_t *d4)
{
    uint16_t c = (uint16_t)(((uint16_t)(x + rd16(v_mapX)) & 7) + 1);
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - c));
    *d4 = 8;
    *d3 = 0x10;
    return 0;
}

/* $029BEE: the floor an attribute found at (x, y) is: the object put on it. A floor kind's
 * routine is left in a0 (when a0 is wanted). */
static int floor_attr(uint16_t obj, uint16_t x, uint16_t y, uint16_t a, uint16_t *d3, uint16_t *d4, uint32_t *a0)
{
    if ((int16_t)(a - 0x30) < 0) {
        if (a == 0x18 || a == 0x19) {
            wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 8));
            return wall_left(obj, x, d3, d4);
        }
        if (a == 0x1C || a == 0x1D) {
            wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + 8));
            return wall_right(obj, x, d3, d4);
        }
        *d4 = 0;
        *d3 = 0;
        return 1;
    }
    uint32_t fn = rom32(0x029C08 + (uint32_t)(int16_t)(uint16_t)((a - 0x30) << 2));
    if (a0) *a0 = fn;
    return floor_kind(fn, obj, x, y, a, d4);
}

/* $029B82. The floor at dy below the object, looked for from 8 above to 8 below (the object
 * moved by 8 when found there): the object put on it, its kind in d4, its attribute in d3; Z
 * set when there is none (also for an attribute of exactly $60). Attributes $18/$19 and $1C/$1D
 * are a wall's edge. */
int ObjectFloor(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4)
{
    return ObjectFloorA0(obj, dy, d3, d4, 0);
}

/* ObjectFloor, and a0 as it leaves it: a floor kind's routine, or unchanged */
int ObjectFloorA0(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4, uint32_t *a0)
{
    uint16_t y = (uint16_t)(dy + rd16(obj + O_Y)), x = rd16(obj + O_X), a = 0;
    if (bit8(obj + 0x47, 1)) {
        wr32(obj + 0x1C, 0);
        *d3 = 0x38;
        *d4 = 4;
        return 0;
    }
    int k;
    for (k = 0; k < 3; k++) {
        a = MapAttr(x, (uint16_t)(y - 8 + 8 * k));
        *d3 = a;
        if (a && (int16_t)(a - 0x60) < 0) break;
        if (k == 2) return a == 0 || a == 0x60;
    }
    if (k == 0) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 8));
    if (k == 2) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 8));
    return floor_attr(obj, x, (uint16_t)(y - 8 + 8 * k), a, d3, d4, a0);
}

/* $029AFC: the same, a ceiling's cell (bit 3) not a floor; none found below one: d3, d4 0 */
static int floor_not_ceiling(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4)
{
    uint16_t y = (uint16_t)(dy + rd16(obj + O_Y)), x = rd16(obj + O_X), a = 0;
    int k;
    for (k = 0; k < 3; k++) {
        a = MapAttr(x, (uint16_t)(y - 8 + 8 * k));
        *d3 = a;
        if (!a) {
            if (k == 2) return 1;
            continue;
        }
        if (a & 8) {
            if (k == 2) {
                *d3 = 0;
                *d4 = 0;
                return 1;
            }
            continue;
        }
        if ((int16_t)(a - 0x60) < 0) break;
        if (k == 2) return a == 0x60;
    }
    if (k == 0) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 8));
    if (k == 2) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 8));
    return floor_attr(obj, x, (uint16_t)(y - 8 + 8 * k), a, d3, d4, 0);
}

/* $029AF0. The floor 16 above (not a ceiling). */
int ObjectFloorUp16(uint16_t obj, uint16_t *d3, uint16_t *d4)
{
    return floor_not_ceiling(obj, 0xFFF0, d3, d4);
}

/* $029AF8. And 30 above. */
int ObjectFloorUp30(uint16_t obj, uint16_t *d3, uint16_t *d4)
{
    return floor_not_ceiling(obj, 0xFFE2, d3, d4);
}

/* a wall (an attribute with bit 3; for ObjectWalls below $20) 8 to the right or left at dy:
 * the object put against it, d3 $10 (right) or $14 (left); none: d3, d4 0 (Z set). With +$47 bit 2
 * or 3 (against a wall already) d3 $1C or $18. */
static int walls(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4, int below20)
{
    uint16_t y = (uint16_t)(dy + rd16(obj + O_Y));
    if (bit8(obj + 0x47, 2)) { *d3 = 0x1C; return 0; }
    if (bit8(obj + 0x47, 3)) { *d3 = 0x18; return 0; }
    uint16_t x = (uint16_t)(rd16(obj + O_X) + 8);
    uint16_t a = MapAttr(x, y);
    *d3 = a;
    if ((a & 8) && (!below20 || (int16_t)(a - 0x20) < 0)) return wall_left(obj, x, d3, d4);
    x = (uint16_t)(rd16(obj + O_X) - 8);
    a = MapAttr(x, y);
    *d3 = a;
    if ((a & 8) && (!below20 || (int16_t)(a - 0x20) < 0)) return wall_right(obj, x, d3, d4);
    *d4 = 0;
    *d3 = 0;
    return 1;
}

/* $029A42 */
int ObjectWalls(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4) { return walls(obj, dy, d3, d4, 1); }
/* $029A40. At 8 below. */
int ObjectWalls8(uint16_t obj, uint16_t *d3, uint16_t *d4) { return walls(obj, 8, d3, d4, 1); }
/* $029AA2. Any wall cell. */
int ObjectWallsAny(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4) { return walls(obj, dy, d3, d4, 0); }
/* $029AA0 */
int ObjectWallsAny8(uint16_t obj, uint16_t *d3, uint16_t *d4) { return walls(obj, 8, d3, d4, 0); }

/* $029F04. The player an object goes after by its slot (alternate slots: alternate players),
 * the other when that one is hidden and not blinking; the one playing alone. */
uint16_t PlayerBySlot(uint16_t obj)
{
    uint8_t p = rd8(v_players);
    if (!(p & 1)) return (p & 2) ? v_player2 : v_player1;
    if (!(p & 2)) return v_player1;
    uint16_t want = (obj & 0x20) ? v_player2 : v_player1, other = want == v_player1 ? v_player2 : v_player1;
    if (bit8(want + 0x23, 2)) return want;
    if (!bit8(want + O_FLAGS, 7)) return other;
    return want;
}

/* $029B7E. The floor at 16 below. */
int ObjectFloor16(uint16_t obj, uint16_t *d3, uint16_t *d4)
{
    return ObjectFloor(obj, 0x10, d3, d4);
}

/* $029C48. A ceiling (an attribute with bit 3) at dy, looked for from 8 below to 8 above. */
int ObjectCeiling(uint16_t obj, uint16_t dy, uint16_t *d3, uint16_t *d4)
{
    uint16_t y = (uint16_t)(dy + rd16(obj + O_Y)), x = rd16(obj + O_X), a = 0;
    int k;
    for (k = 0; k < 3; k++) {
        a = MapAttr(x, (uint16_t)(y + 8 - 8 * k));
        *d3 = a;
        if (a & 8) break;
        if (k == 2) return 1;
    }
    if (k == 0) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 8));
    if (k == 2) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 8));
    y = (uint16_t)(y + 8 - 8 * k);
    if ((int16_t)(a - 0x20) < 0 || (int16_t)(a - 0x38) >= 0) return ObjFloor4B(obj, x, y, a, 7, d4);
    uint32_t fn = rom32(0x029CB8 + (uint32_t)(int16_t)(uint16_t)((a - 0x28) << 2));
    return floor_kind(fn, obj, x, y, a, d4);
}

/* $02A906. What the object holds (+$40) let go: returned (0: nothing). */
uint16_t ObjectRelease(uint16_t obj)
{
    uint16_t held = rd16(obj + 0x40);
    if (held) wr16(obj + 0x40, 0);
    return held;
}

/* $02A758: the shot (in `slot`, type `type`, angle `angle` at +$48) placed at the muzzle: the
 * offsets table's entry for the direction (a byte of x, a byte of y), mirrored as the object is
 * flipped */
static void shot_at_muzzle(uint16_t obj, uint16_t slot, uint32_t offsets, uint16_t angle, uint16_t dir, uint16_t type)
{
    wr16(slot + 0x48, angle);
    uint16_t o = bus_rd16(offsets + (uint32_t)(int16_t)(uint16_t)((int16_t)dir >> 1));
    int16_t dy = (int8_t)(o & 0xFF), dx = (int8_t)(o >> 8);
    wr16(slot + O_TYPE, type);
    if (bit8(obj + 0x0E, 3)) dx = (int16_t)-dx;
    wr16(slot + O_X, (uint16_t)(rd16(obj + O_X) + dx));
    if (bit8(obj + 0x0E, 4)) dy = (int16_t)-dy;
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) + dy));
}

/* $02A742 (entered by jump): facing `target` (`faces`: the mappings by direction), a shot of
 * `type` from a slot of $CE00-$D3FF at the muzzle (`offsets`). */
void AimedSpawn(uint16_t obj, uint16_t target, uint32_t faces, uint32_t offsets, uint16_t type)
{
    uint16_t d0, d5, d6, d7 = type;
    FaceToward(obj, target, faces, &d0, &d5, &d6, &d7);
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    shot_at_muzzle(obj, slot, offsets, d5, d6, d7);
}

/* $02A884 (entered by jump): something of `type` thrown at `target`: up at 5, across so as to
 * land on it (its distance over 85 frames; the soldiers' $1F8 kind with +$56 bit 3 lower and
 * twice as fast above the easiest difficulty), the thrower facing the throw. */
void ThrowAt(uint16_t obj, uint16_t target, uint16_t type)
{
    int32_t d = (int32_t)(rd32(target + O_X) - rd32(obj + O_X)) >> 8;
    uint32_t vx = 0;
    if (d) {
        int32_t q = d / 85;
        if (q >= -32768 && q <= 32767) vx = (uint32_t)(int32_t)(int16_t)q << 8;
        else vx = (uint32_t)(int32_t)(int16_t)(uint16_t)d << 8;    /* divs overflow: d1 kept */
    }
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    QueueSound(0xD4);
    wr16(slot + O_TYPE, type);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x1C, 0xFFFB0000);
    if (rd16(obj + O_TYPE) == 0x1F8 && bit8(obj + 0x56, 3) && rd16(v_difficulty)) {
        wr32(slot + 0x1C, 0xFFFD8000);
        vx <<= 1;
    }
    wr32(slot + 0x18, vx);
    if (vx & 0x80000000u) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
}

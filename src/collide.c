/* The objects' collisions. Each frame every object with collision classes (P_CLASSES, its +$21)
 * goes into the lists of its classes, with its two boxes on the screen: the box it is hit by
 * (from +$28: top, bottom, left, right offsets, flipped with the sprite) for the classes in bits
 * 7, 5, 3, 1, the box it hits with (from +$2C) for bits 6, 4, 2, 0. Then four pairs of lists
 * are checked, every member of one against every member of the other:
 *   7 (hurt) by 4 (attacks): the hit flags, the attacker's strength off the vitality;
 *   5 (touched) by 6: the same, without the players' special cases;
 *   1 (grabbing) by 0 (grabbable);
 *   3 (stood on solids) by 2 (solids): pushed out of them (players by their state's rules).
 * The lists stay for the frame (the chaser picks its targets from list 5). */
#include "game.h"

#define P_SPRITE  0x0E
#define P_X_FRAC  0x12
#define P_Y_FRAC  0x16
#define P_VX      0x18
#define P_VY      0x1C
#define P_CLASSES 0x21
#define P_COLL    0x22           /* the collisions this frame, a bit per class pair */
#define P_STATUS  0x23
#define P_POWER   0x26           /* an attack's strength */
#define P_HURT_BOX 0x28          /* signed bytes: top, bottom, left, right */
#define P_HIT_BOX 0x2C
#define P_HURT_T  0x30           /* the boxes on the screen: top, bottom, left, right */
#define P_HURT_B  0x32
#define P_HURT_L  0x34
#define P_HURT_R  0x36
#define P_HIT_T   0x38
#define P_HIT_B   0x3A
#define P_HIT_L   0x3C
#define P_HIT_R   0x3E
#define P_BY      0x40           /* what collided with it */
#define P_WITH    0x42           /* what it collided with */
#define P_SOLID_BOX 0x44         /* 0: stands on solids by its hurt box; 4: a player's rules */
#define P_SOLID   0x46           /* a solid's sides: bit 7 all of it, 0 top, 1 bottom, 2 left, 3 right */
#define P_HITS    0x47
#define P_HELD    0x69
#define P_FLOOR   0x82
#define P_WALL    0x84
#define P_PUSHED  0xB0
#define P_GUARD   0xCC

static void collide_solids(void);
static void solid_bottom(uint16_t obj, uint16_t solid);
static void solid_hang(uint16_t obj, uint16_t solid);

/* the lists and their counts */
static const uint16_t list_at[8] = { 0xB800, 0xB8C0, 0xB980, 0xBA40, 0xBB00, 0xBBC0, 0xBC80, 0xBD40 };
static const uint16_t count_at[8] = { 0xF734, 0xF736, 0xF738, 0xF73A, 0xF73C, 0xF73E, 0xF740, 0xF742 };

/* $00B188. The lists made, the pairs checked (not while paused). */
void Collisions(void)
{
    if (rd8(v_pauseFlags) & 0x80) return;
    CollisionLists();
    CollideGrab();
    CollideHurt();
    CollideTouch();
    collide_solids();
}

/* $00B1A6. The players, the object at $FFA800, then the drawn ones. (The lists' ends, as they
 * fill, are the 68k's d4, a0, a1, a2, a3, d5, d6, d7.) */
void CollisionLists(void)
{
    uint16_t ends[8];
    for (int i = 0; i < 8; i++) ends[i] = list_at[i];
    CollisionAdd(v_player1, ends);
    CollisionAdd(v_player2, ends);
    CollisionAdd(0xA800, ends);
    CollisionListsDrawn(ends);
    for (int i = 0; i < 8; i++) wr16(count_at[i], (uint16_t)((uint16_t)(ends[i] - list_at[i]) >> 1));
}

/* $00B250 */
void CollisionListsDrawn(uint16_t ends[8])
{
    uint16_t n = rd16(v_drawCount);
    for (uint16_t i = 0; i < n; i++) CollisionAdd(rd16((uint16_t)(v_drawList + 2 * i)), ends);
}

/* a box on the screen from its offsets: top, bottom from y (swapped, negated upside down),
 * left, right from x (likewise flipped) */
static void box(uint16_t obj, uint16_t offs, uint16_t field)
{
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y), to = (uint16_t)(obj + field);
    int8_t t = (int8_t)rd8(obj + offs), b = (int8_t)rd8(obj + offs + 1);
    int8_t l = (int8_t)rd8(obj + offs + 2), r = (int8_t)rd8(obj + offs + 3);
    if (bit8(obj + P_SPRITE, 3)) {
        wr16(to + 4, (uint16_t)(x - r));
        wr16(to + 6, (uint16_t)(x - l));
    } else {
        wr16(to + 4, (uint16_t)(x + l));
        wr16(to + 6, (uint16_t)(x + r));
    }
    if (bit8(obj + P_SPRITE, 4)) {
        wr16(to, (uint16_t)(y - b));
        wr16(to + 2, (uint16_t)(y - t));
    } else {
        wr16(to, (uint16_t)(y + t));
        wr16(to + 2, (uint16_t)(y + b));
    }
}

static void add(uint16_t ends[8], int list, uint16_t obj)
{
    wr16(ends[list], obj);
    ends[list] = (uint16_t)(ends[list] + 2);
}

/* $00B260. An object into its classes' lists, its boxes on the screen; its collisions cleared. */
void CollisionAdd(uint16_t obj, uint16_t ends[8])
{
    uint8_t c = rd8(obj + P_CLASSES);
    if (!c) return;
    wr8(obj + P_COLL, 0);
    if (c & 0xAA) {
        if (c & 0x80) add(ends, 0, obj);
        if (c & 0x20) add(ends, 2, obj);
        if (c & 0x08) add(ends, 4, obj);
        if (c & 0x02) add(ends, 6, obj);
        box(obj, P_HURT_BOX, P_HURT_T);
    }
    if (c & 0x55) {
        if (c & 0x40) add(ends, 1, obj);
        if (c & 0x10) add(ends, 3, obj);
        if (c & 0x04) add(ends, 5, obj);
        if (c & 0x01) add(ends, 7, obj);
        box(obj, P_HIT_BOX, P_HIT_T);
    }
}

static int is_player(uint16_t obj) { return obj == v_player1 || obj == v_player2; }

/* whether a's hurt box (t, b, l, r) and b's hit box overlap, by the 68k's four tests */
static int overlap(uint16_t t, uint16_t b, uint16_t l, uint16_t r, uint16_t other)
{
    return !((int16_t)l > (int16_t)rd16(other + P_HIT_R) || (int16_t)b < (int16_t)rd16(other + P_HIT_T)
             || (int16_t)r < (int16_t)rd16(other + P_HIT_L) || (int16_t)t > (int16_t)rd16(other + P_HIT_B));
}

/* the hit's side and owner taken from the attacker's status */
static void take_side(uint16_t victim, uint16_t from)
{
    uint8_t d = (uint8_t)((rd8(from + P_STATUS) & 0x20) >> 1);
    wr8(victim + P_STATUS, (uint8_t)((rd8(victim + P_STATUS) & 0xEF) | d));
}

static void take_owner(uint16_t victim, uint16_t from)
{
    uint8_t o = rd8(from + P_STATUS) & 0xC0;
    wr8(victim + P_STATUS, (uint8_t)((rd8(victim + P_STATUS) & 0x3F) | o));
}

/* $00B3F8. Class 7 hit by class 4: an item touched (not the players'), else a hit: the
 * attacker's strength off the vitality (half for a guarding player). */
void CollideHurt(void)
{
    uint16_t na = rd16(count_at[0]), nb = rd16(count_at[3]);
    if (!na || !nb) return;
    for (uint16_t i = 0; i < na; i++) {
        uint16_t a = rd16((uint16_t)(list_at[0] + 2 * i));
        if (bit8(a + P_COLL, 7) || bit8(a + P_COLL, 1)) continue;
        uint16_t t = rd16(a + P_HURT_T), b = rd16(a + P_HURT_B), l = rd16(a + P_HURT_L), r = rd16(a + P_HURT_R);
        for (uint16_t j = 0; j < nb; j++) {
            uint16_t o = rd16((uint16_t)(list_at[3] + 2 * j));
            /* (the 68k's tests: its right before the other's left, its top below the other's
               bottom, its left after the other's right, its bottom above the other's top) */
            if ((int16_t)r < (int16_t)rd16(o + P_HIT_L) || (int16_t)t > (int16_t)rd16(o + P_HIT_B)
                || (int16_t)l > (int16_t)rd16(o + P_HIT_R) || (int16_t)b < (int16_t)rd16(o + P_HIT_T)) continue;
            if (bit8(o + P_COLL, 4) || o == a) continue;
            if (bit8(o + P_STATUS, 3) && !is_player(o)) {
                wr16(o + P_BY, a);
                wr16(a + P_WITH, o);
                set8(o + P_COLL, 4);
                set8(a + P_STATUS, 3);
                take_owner(o, a);
                continue;
            }
            if (bit8(a + P_STATUS, 2)) continue;
            wr16(o + P_BY, a);
            wr16(a + P_WITH, o);
            set8(o + P_COLL, 4);
            set8(a + P_COLL, 7);
            clr8(a + P_STATUS, 3);
            take_side(a, o);
            if (!(rd16(a + O_VITALITY) & 0x8000)) {
                uint16_t p = rd16(o + P_POWER);
                if (p) {
                    if (is_player(a)) {
                        if (rd16(a + O_TYPE) == 0x540) goto next;
                        if (rd8(a + P_GUARD) & 0x80) p = (uint16_t)((int16_t)p >> 1);
                    }
                    wr16(a + O_VITALITY, (uint16_t)(rd16(a + O_VITALITY) - p));
                }
            }
            break;
        next:
            continue;
        }
    }
}

/* $00B532. Class 5 touched by class 6. */
void CollideTouch(void)
{
    uint16_t na = rd16(count_at[2]), nb = rd16(count_at[1]);
    if (!na || !nb) return;
    for (uint16_t i = 0; i < na; i++) {
        uint16_t a = rd16((uint16_t)(list_at[2] + 2 * i));
        if (bit8(a + P_COLL, 5) || bit8(a + P_COLL, 1) || bit8(a + P_STATUS, 2)) continue;
        uint16_t t = rd16(a + P_HURT_T), b = rd16(a + P_HURT_B), l = rd16(a + P_HURT_L), r = rd16(a + P_HURT_R);
        for (uint16_t j = 0; j < nb; j++) {
            uint16_t o = rd16((uint16_t)(list_at[1] + 2 * j));
            if (!overlap(t, b, l, r, o)) continue;
            if (bit8(o + P_COLL, 6) || o == a) continue;
            set8(a + P_COLL, 5);
            set8(o + P_COLL, 6);
            wr16(o + P_BY, a);
            wr16(a + P_WITH, o);
            take_side(a, o);
            take_owner(a, o);
            if (!(rd16(a + O_VITALITY) & 0x8000))
                wr16(a + O_VITALITY, (uint16_t)(rd16(a + O_VITALITY) - rd16(o + P_POWER)));
            break;
        }
    }
}

/* $00B60E. Class 1 grabbing class 0; an enemy grabbing a player takes its side. */
void CollideGrab(void)
{
    uint16_t na = rd16(count_at[6]), nb = rd16(count_at[7]);
    if (!na || !nb) return;
    for (uint16_t i = 0; i < na; i++) {
        uint16_t a = rd16((uint16_t)(list_at[6] + 2 * i));
        if (bit8(a + P_COLL, 1) || bit8(a + P_COLL, 0) || bit8(a + P_STATUS, 2)) continue;
        uint16_t t = rd16(a + P_HURT_T), b = rd16(a + P_HURT_B), l = rd16(a + P_HURT_L), r = rd16(a + P_HURT_R);
        for (uint16_t j = 0; j < nb; j++) {
            uint16_t o = rd16((uint16_t)(list_at[7] + 2 * j));
            if (!overlap(t, b, l, r, o)) continue;
            if (bit8(o + P_COLL, 0) || bit8(o + P_COLL, 1) || o == a) continue;
            set8(a + P_COLL, 1);
            set8(o + P_COLL, 0);
            wr16(o + P_BY, a);
            wr16(a + P_WITH, o);
            if (!is_player(a)) {
                if (o == v_player1) wr8(a + P_STATUS, (uint8_t)((rd8(a + P_STATUS) & 0x3F) | 0x80));
                else if (o == v_player2) wr8(a + P_STATUS, (uint8_t)((rd8(a + P_STATUS) & 0x3F) | 0x40));
            }
            break;
        }
    }
}

/* $00B6FA (the end of Collisions). Class 3 against the solids (class 2). */
static void collide_solids(void)
{
    uint16_t na = rd16(count_at[4]), nb = rd16(count_at[5]);
    if (!na || !nb) return;
    for (uint16_t i = 0; i < na; i++) {
        uint16_t a = rd16((uint16_t)(list_at[4] + 2 * i));
        uint16_t t, b, l, r;
        SolidBox(a, &t, &b, &l, &r);
        for (uint16_t j = 0; j < nb; j++) {
            uint16_t s = rd16((uint16_t)(list_at[5] + 2 * j));
            if (!overlap(t, b, l, r, s) || s == a) continue;
            SolidPush(a, s);
            set8(s + P_COLL, 2);
            set8(a + P_COLL, 3);
        }
    }
}

/* $00B764. The box an object stands on solids by: its hurt box, or a player's (by P_SOLID_BOX). */
void SolidBox(uint16_t obj, uint16_t *t, uint16_t *b, uint16_t *l, uint16_t *r)
{
    switch (0x00B76C + (uint32_t)(int16_t)rd16(obj + P_SOLID_BOX)) {
    case 0x00B76C: SolidBoxHurt(obj, t, b, l, r); return;
    case 0x00B770: SolidBoxPlayer(obj, t, b, l, r); return;
    default: call_object_missing(0x00B76C + (uint32_t)(int16_t)rd16(obj + P_SOLID_BOX), obj); return;
    }
}

/* $00B76C */
void SolidBoxHurt(uint16_t obj, uint16_t *t, uint16_t *b, uint16_t *l, uint16_t *r)
{
    *t = rd16(obj + P_HURT_T);
    *b = rd16(obj + P_HURT_B);
    *l = rd16(obj + P_HURT_L);
    *r = rd16(obj + P_HURT_R);
}

/* $00B770 (then $012558): a player's: from $12 above its top to 8 below its bottom, 8 either side
 * of its x */
void SolidBoxPlayer(uint16_t obj, uint16_t *t, uint16_t *b, uint16_t *l, uint16_t *r)
{
    *t = (uint16_t)(rd16(obj + P_HURT_T) - 0x12);
    *b = (uint16_t)(rd16(obj + P_HURT_B) + 8);
    *l = (uint16_t)(rd16(obj + O_X) - 8);
    *r = (uint16_t)(rd16(obj + O_X) + 8);
}

/* $00B78C. Pushed out of a solid: by its hurt box, or by a player's state's rules. */
void SolidPush(uint16_t obj, uint16_t solid)
{
    switch (0x00B798 + (uint32_t)(int16_t)rd16(obj + P_SOLID_BOX)) {
    case 0x00B798: SolidPushBox(obj, solid); return;
    case 0x00B79C: SolidPushPlayer(obj, solid); return;
    default: call_object_missing(0x00B798 + (uint32_t)(int16_t)rd16(obj + P_SOLID_BOX), obj); return;
    }
}

/* the solid's speed given to what it pushed */
static void carried(uint16_t obj, uint16_t solid)
{
    wr32(obj + O_X, rd32(obj + O_X) + rd32(solid + P_VX));
    wr32(obj + O_Y, rd32(obj + O_Y) + rd32(solid + P_VY));
}

static void push_y(uint16_t obj, int16_t d)
{
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + d));
    wr16(obj + P_HURT_T, (uint16_t)(rd16(obj + P_HURT_T) + d));
    wr16(obj + P_HURT_B, (uint16_t)(rd16(obj + P_HURT_B) + d));
}

static void push_x(uint16_t obj, int16_t d)
{
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + d));
    wr16(obj + P_HURT_R, (uint16_t)(rd16(obj + P_HURT_R) + d));
    wr16(obj + P_HURT_L, (uint16_t)(rd16(obj + P_HURT_L) + d));
}

/* $00B798 (then $00B7A0). An object's hurt box out of a solid: onto its top or under its bottom
 * when x is within it (8 of slack unless all solid), else off its sides. */
void SolidPushBox(uint16_t obj, uint16_t solid)
{
    uint8_t s = rd8(solid + P_SOLID);
    int16_t x = (int16_t)rd16(obj + O_X);
    int left;                                            /* off the left side, else the right */
    if (s & 3) {
        if (x > (int16_t)rd16(solid + P_HIT_R)) left = 0;
        else if (x < (int16_t)rd16(solid + P_HIT_L)) left = 1;
        else {
            int top = 1;
            if ((s & 2) && (!(s & 1) || (int16_t)rd16(obj + O_Y) > (int16_t)rd16(solid + O_Y))) top = 0;
            if (top) {
                if (!(s & 1)) return;
                if (!(s & 0x80) && (int16_t)(rd16(obj + P_HURT_B) - 8) > (int16_t)rd16(solid + P_HIT_T)) return;
                push_y(obj, (int16_t)(rd16(solid + P_HIT_T) - rd16(obj + P_HURT_B)));
                set8(obj + P_HITS, 1);
                set8(solid + P_HITS, 4);
            } else {
                if (!(s & 2)) return;
                if (!(s & 0x80) && (int16_t)(rd16(obj + P_HURT_T) + 8) < (int16_t)rd16(solid + P_HIT_B)) return;
                push_y(obj, (int16_t)(rd16(solid + P_HIT_B) - rd16(obj + P_HURT_T)));
                set8(obj + P_HITS, 0);
                set8(solid + P_HITS, 5);
            }
            carried(obj, solid);
            return;
        }
    } else if (!(s & 4)) left = 0;
    else if (!(s & 8)) left = 1;
    else left = x < (int16_t)rd16(solid + O_X);
    if (left) {
        if (!(s & 4) || !(s & 0x80)) return;
        push_x(obj, (int16_t)(rd16(solid + P_HIT_L) - rd16(obj + P_HURT_R)));
        set8(obj + P_HITS, 3);
        set8(solid + P_HITS, 6);
    } else {
        if (!(s & 8)) return;
        push_x(obj, (int16_t)(rd16(solid + P_HIT_R) - rd16(obj + P_HURT_L)));
        set8(obj + P_HITS, 2);
        set8(solid + P_HITS, 7);
    }
    carried(obj, solid);
}

/* $00B79C (then $00B93A). A player: by its state ($012574). */
void SolidPushPlayer(uint16_t obj, uint16_t solid) { PlayerOnSolid(obj, solid); }

/* ---- the players on solids ---- */


/* $012574. A player against a solid, by its state (the table at $01258C). */
void PlayerOnSolid(uint16_t obj, uint16_t solid)
{
    if (rd16(v_noSolids)) return;
    uint32_t at = 0x01258C + (uint32_t)((int16_t)rd16(obj + O_STATE) >> 1);
    uint32_t fn = at + (uint32_t)(int16_t)rom16(at);
    switch (fn) {
    case 0x0125E2: SolidNone(obj, solid); return;
    case 0x0125E4: SolidGround(obj, solid); return;
    case 0x0125F0: SolidAir(obj, solid); return;
    case 0x01261A: SolidAirNoGrab(obj, solid); return;
    case 0x01262A: SolidHanging(obj, solid); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $0125E2 */
void SolidNone(uint16_t obj, uint16_t solid) { (void)obj; (void)solid; }

/* $0125E4. On the ground: its sides, its top, its bottom. */
void SolidGround(uint16_t obj, uint16_t solid)
{
    SolidSides(obj, solid);
    SolidTop(obj, solid);
    solid_bottom(obj, solid);
}

/* $0125F0. In the air: its sides; going up its bottom, else its top, and with Up hanging under
 * it. */
void SolidAir(uint16_t obj, uint16_t solid)
{
    SolidSides(obj, solid);
    if (rd16(obj + P_VY) & 0x8000) { solid_bottom(obj, solid); return; }
    SolidTop(obj, solid);
    if (bit8(obj + P_HITS, 1) || !bit8(obj + P_HELD, 0)) return;
    solid_hang(obj, solid);
}

/* $01261A */
void SolidAirNoGrab(uint16_t obj, uint16_t solid)
{
    SolidSides(obj, solid);
    if (rd16(obj + P_VY) & 0x8000) { solid_bottom(obj, solid); return; }
    SolidTop(obj, solid);
}

/* $01262A. Hanging: its sides, its top; with no floor, hanging under it. */
void SolidHanging(uint16_t obj, uint16_t solid)
{
    SolidSides(obj, solid);
    SolidTop(obj, solid);
    if (!rd16(obj + P_FLOOR)) solid_hang(obj, solid);
}

/* $01263C. A wholly solid object's sides: the player 8 off the side it is nearer (between its
 * top and bottom), the wall attribute $18 or $1C. */
void SolidSides(uint16_t obj, uint16_t solid)
{
    uint8_t s = rd8(solid + P_SOLID);
    if (!(s & 0x80)) return;
    int left;
    if (s & 3) {
        if ((int16_t)((int8_t)rd8(obj + 0x29) + rd16(obj + O_Y)) <= (int16_t)rd16(solid + P_HIT_T)) return;
        if ((int16_t)((int8_t)rd8(obj + 0x28) + rd16(obj + O_Y) - 0xA) >= (int16_t)rd16(solid + P_HIT_B)) return;
        int16_t x = (int16_t)rd16(obj + O_X);
        if (x > (int16_t)rd16(solid + P_HIT_R)) left = 0;
        else if (x >= (int16_t)rd16(solid + P_HIT_L)) return;
        else left = 1;
    } else if (!(s & 8)) left = 1;
    else if (!(s & 4)) left = 0;
    else left = (int16_t)rd16(obj + O_X) < (int16_t)rd16(solid + O_X);
    if (left) {
        if (!(s & 4)) return;
        wr16(obj + O_X, (uint16_t)(rd16(solid + P_HIT_L) - 8));
        wr16(obj + P_X_FRAC, 0);
        set8(obj + P_HITS, 3);
        set8(solid + P_HITS, 6);
        wr16(obj + P_WALL, 0x18);
    } else {
        if (!(s & 8)) return;
        wr16(obj + O_X, (uint16_t)(rd16(solid + P_HIT_R) + 8));
        wr16(obj + P_X_FRAC, 0);
        set8(obj + P_HITS, 2);
        set8(solid + P_HITS, 7);
        wr16(obj + P_WALL, 0x1C);
    }
}

/* stood on: the player's feet on y, carried by the solid (put down this frame unless the solid
 * moves by its own flags: P_STATUS bit 0) */
static void stand_on(uint16_t obj, uint16_t solid, uint16_t y, uint16_t floor)
{
    wr16(obj + P_FLOOR, floor);
    wr16(obj + O_Y, y);
    wr16(obj + P_Y_FRAC, 0);
    set8(obj + P_HITS, 1);
    set8(solid + P_HITS, 4);
    wr32(obj + P_PUSHED, rd32(solid + P_VX));
    wr32(obj + P_PUSHED + 4, rd32(solid + P_VY));
    if (!bit8(solid + O_FLAGS, 0)) set8(obj + P_STATUS, 0);
}

/* $012710. Its top, within its sides: the player stands on it (a floor $30, or a thin floor $38
 * the player was above, a top-only solid within 8). */
void SolidTop(uint16_t obj, uint16_t solid)
{
    uint8_t s = rd8(solid + P_SOLID);
    if (!(s & 1)) return;
    int16_t x = (int16_t)rd16(obj + O_X);
    if (x > (int16_t)rd16(solid + P_HIT_R) || x < (int16_t)rd16(solid + P_HIT_L)) return;
    int16_t feet = (int16_t)((int8_t)rd8(obj + 0x29) + rd16(obj + O_Y) + 4);
    uint16_t d = (uint16_t)(feet - rd16(solid + P_HIT_T));
    uint16_t floor;
    if (!(s & 0x80)) {
        if (d >= 8) return;
        floor = 0x30;
    } else {
        if (d & 0x8000) return;
        if ((s & 2) && (int16_t)rd16(obj + O_Y) > (int16_t)rd16(solid + O_Y)) return;
        floor = 0x38;
    }
    stand_on(obj, solid, (uint16_t)(rd16(solid + P_HIT_T) - (int8_t)rd8(obj + 0x29)), floor);
}

/* $0127C6. A wholly solid object's bottom, within its sides: the player's head stopped under it. */
static void solid_bottom(uint16_t obj, uint16_t solid)
{
    uint8_t s = rd8(solid + P_SOLID);
    if (!(s & 0x80) || !(s & 2)) return;
    int16_t x = (int16_t)rd16(obj + O_X);
    if (x > (int16_t)rd16(solid + P_HIT_R) || x < (int16_t)rd16(solid + P_HIT_L)) return;
    if (s & 1) {
        uint16_t d = (uint16_t)((int8_t)rd8(obj + 0x28) + rd16(obj + O_Y) - rd16(solid + P_HIT_B));
        if (d < 0xFFF8) return;
    }
    wr16(obj + O_Y, (uint16_t)(rd16(solid + P_HIT_B) - (int8_t)rd8(obj + 0x28)));
    wr16(obj + P_Y_FRAC, 0);
    set8(obj + P_HITS, 0);
    set8(solid + P_HITS, 5);
    wr32(obj + P_VY, 0);
}

/* $012832. Hanging under it (the head's grip $A below the box's top): from a top-only solid's
 * top (a floor $30), or its bottom ($28 wholly solid, else $20). */
static void solid_hang(uint16_t obj, uint16_t solid)
{
    uint8_t s = rd8(solid + P_SOLID);
    int16_t x = (int16_t)rd16(obj + O_X);
    if (x > (int16_t)rd16(solid + P_HIT_R) || x < (int16_t)rd16(solid + P_HIT_L)) return;
    int16_t grip = (int16_t)((int8_t)rd8(obj + 0x28) + rd16(obj + O_Y) - 0xA + 4);
    uint16_t edge, floor;
    if (!(s & 0x80) && (s & 1) && (uint16_t)(grip - rd16(solid + P_HIT_T)) < 8) {
        edge = rd16(solid + P_HIT_T);
        floor = 0x30;
    } else {
        if (!(s & 2) || (uint16_t)(grip - rd16(solid + P_HIT_B)) >= 8) return;
        edge = rd16(solid + P_HIT_B);
        floor = (s & 0x80) ? 0x28 : 0x20;
    }
    wr16(obj + O_Y, (uint16_t)(edge - (int8_t)rd8(obj + 0x28) + 0xA));
    wr16(obj + P_Y_FRAC, 0);
    wr32(obj + P_PUSHED, rd32(solid + P_VX));
    wr32(obj + P_PUSHED + 4, rd32(solid + P_VY));
    if (!bit8(solid + O_FLAGS, 0)) set8(obj + P_STATUS, 0);
    wr16(obj + P_FLOOR, floor);
}

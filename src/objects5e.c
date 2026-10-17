/* Object types from $5E092 to $6237E, most of them stage 6's (the final battle's): the jet $580
 * and the grab box $584 of the object at $CB60, the shard $588, the bomb $604; the boss $58C
 * (at $CB60) with its parts $590/$594, $598, $5A0 and the hands $59C it sends; the boss $5B4
 * with its four orbs $5D4 (and their shots $5EC, $5F0 and its trail $5F4, $5F8, the chain
 * $5E8), its shots $5DC, $5E0, $5E4 and its falling pieces $5FC; the ending's director $614
 * and its actors $618. The routines they call outside this range that were not ported yet are
 * under "shared helpers" at the end.
 *
 * Types $67C ($06FF26) and $6F0 ($066700) are not object types: the table at $54C ends with
 * type $660 ($664 on is VBlank's code), and those two "addresses" are bytes of code. */
#include "game.h"

static void next(uint16_t obj, uint16_t by) { wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + by)); }
static void back(uint16_t obj, uint16_t by) { wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - by)); }
/* subq.w #1 on a word, then bpl: 1 when it is now below 0 */
static int dec(uint16_t a)
{
    wr16(a, (uint16_t)(rd16(a) - 1));
    return (rd16(a) & 0x8000) != 0;
}
static void add16(uint16_t a, uint16_t v) { wr16(a, (uint16_t)(rd16(a) + v)); }
static void add32(uint16_t a, uint32_t v) { wr32(a, rd32(a) + v); }
/* addi.l then bmi: 1 when the long is now negative */
static int add32_neg(uint16_t a, uint32_t v)
{
    add32(a, v);
    return (rd32(a) & 0x80000000u) != 0;
}
/* cmpi.w #v,(a) then bmi: 1 when the word is below v (the N flag of the difference) */
static int below(uint16_t a, uint16_t v) { return (int16_t)(rd16(a) - v) < 0; }
/* cmpi.w #b,dn then bmi */
static int lt(uint16_t a, uint16_t b) { return (int16_t)(a - b) < 0; }
/* an animation from its start: +8 the list, +$C its frame */
static void look(uint16_t obj, uint32_t map)
{
    wr32(obj + 0x08, map);
    wr16(obj + 0x0C, 0);
}
static uint32_t neg32(uint32_t v) { return 0u - v; }
static uint32_t asr32(uint32_t v, int n) { return (uint32_t)((int32_t)v >> n); }
static uint32_t swap32(uint32_t v) { return v << 16 | v >> 16; }
/* a word index, sign-extended (as an index register .w is) */
static uint32_t sx(uint16_t w) { return (uint32_t)(int32_t)(int16_t)w; }
/* btst #n,($FFFFA001).w: the frame count's low byte */
static int frame_bit(int n) { return bit8(v_gameFrames + 1, n); }
/* the sprite word, +8 and +$A from three words of the ROM */
static void sprite3(uint16_t obj, uint32_t at)
{
    wr16(obj + 0x0E, rom16(at));
    wr16(obj + 0x08, rom16(at + 2));
    wr16(obj + 0x0A, rom16(at + 4));
}

#define SINES   0x02977Cu        /* the sine table (words, $1FE the turn) */
#define COSINES 0x0297FCu
#define AGGRESSION 0xA20C        /* word: the difficulty's step (engine.c) */

/* ---- type $580: a jet of the object at $CB60 (while its +$5C bit 5 is set) ---- */

/* $05E174. On even frames a spark (type $2CC, $5663A) at it, thrown the way its hitter (+$42)
 * moves, up or down at random. */
void Jet580Sparks(uint16_t obj)
{
    uint16_t s, t;
    if (frame_bit(0)) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x2CC);
    wr32(s + 0x48, 0x05663A);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    wr16(s + 0x18, 2);
    wr16(s + 0x5A, 0x1000);
    uint16_t by = rd16(obj + 0x42);
    if (rd16(by + 0x18) & 0x8000) {
        wr16(s + 0x18, 0xFFFE);
        wr32(s + 0x58, 0xFFFFF000);
    }
    if (!(rd16(v_randomSeed) & 0x8000)) {
        wr16(s + 0x1C, 0xFFFC);
        wr16(s + 0x5E, 0x3000);
    } else {
        wr16(s + 0x1C, 4);
        wr32(s + 0x5C, 0xFFFFD000);
    }
}

/* $05E092. Type $580: hidden and harmless until the object at $CB60 sets bit 5 of its +$5C;
 * then it can be touched, and once hit (+$22 bit 5) it burns for 8 frames (+$48): shown, its
 * frame from $566E0 by the frame count, sparks, the sprites shaking, $CB60's +$5C bit 6 set. */
void Jet580(uint16_t obj)
{
    uint16_t a0 = 0xCB60;
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0);
        wr16(obj + 0x48, 0xFFFF);
        wr32(obj + 0x28, 0xEC20EC14);
    }
    if (!bit8(a0 + 0x5C, 5)) {
        wr8(obj + 0x21, 0);
        wr16(obj + 0x48, 0xFFFF);
        clr8(a0 + 0x5C, 6);
        wr16(obj + O_FLAGS, 0);
        return;
    }
    wr8(obj + 0x21, 0x20);
    if (rd16(obj + 0x48) & 0x8000) {
        int hit = bit8(obj + 0x22, 5);
        clr8(obj + 0x22, 5);
        if (!hit) {
            clr8(a0 + 0x5C, 6);
            wr16(obj + O_FLAGS, 0);
            return;
        }
        wr16(obj + 0x48, 7);
        set8(a0 + 0x5C, 6);
        return;
    }
    add16(obj + 0x48, 0xFFFF);
    if (!(rd16(v_gameFrames) & 0xF)) QueueSound(0xB7);
    wr8(obj + 0x5C, 0x20);
    set8(a0 + 0x5C, 6);
    wr16(obj + O_FLAGS, 0x8080);
    wr16(v_shakeSprites, 2);
    sprite3(obj, 0x0566E0 + (uint16_t)((rd16(v_gameFrames) << 3) & 0x18));
    Jet580Sparks(obj);
    wr8(obj + 0x20, frame_bit(0) ? 0x88 : 0x08);
}

/* ---- type $584: a box on the object at $CB60 that a player can grab ---- */

/* $05E1EC. Type $584: at $CB60 while its +$5C bit 4 is set (else harmless); touched by a
 * player, it sets $CB60's +$5C bit 7, gives it the player's x speed and its +$5D bit 2 when the
 * player is above. */
void Grab584(uint16_t obj)
{
    uint16_t a0 = 0xCB60;
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0);
        wr32(obj + 0x28, 0xF410F010);
    }
    if (!bit8(a0 + 0x5C, 4)) {
        wr8(obj + 0x21, 0);
        clr8(a0 + 0x5C, 7);
        return;
    }
    wr8(obj + 0x21, 0x20);
    wr16(obj + O_X, rd16(a0 + O_X));
    wr16(obj + O_Y, rd16(a0 + O_Y));
    int hit = bit8(obj + 0x22, 5);
    clr8(obj + 0x22, 5);
    uint16_t by = rd16(obj + 0x42);
    if (!hit || (by != 0xA400 && by != 0xA600)) {
        clr8(a0 + 0x5C, 7);
        return;
    }
    set8(a0 + 0x5C, 7);
    wr32(a0 + 0x18, rd32(by + 0x18));
    if ((int16_t)(rd16(by + O_Y) - rd16(obj + O_Y)) < 0) set8(a0 + 0x5D, 2);
    else clr8(a0 + 0x5D, 2);
}

/* ---- type $588: a shard ---- */

/* $05E27A. Type $588: a small harmful shard ($647C) flying; in step 4, past y $12C or x $1A0 it
 * bounces back (a quarter of its speed, harmless, falling from then on); hit, it becomes a
 * burst ($565D4, type $100) going back the way it came. */
void Shard588(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0x8E80);
        wr16(obj + 0x0E, 0x647C);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
        wr8(obj + 0x21, 0x30);
        wr32(obj + 0x2C, 0xFF01FF01);
        wr32(obj + 0x28, 0xFE02FE02);
        wr16(obj + 0x26, 2);
    }
    if (rd16(obj + O_STATE) != 4) {
        add32(obj + 0x1C, 0x4000);
        return;
    }
    if (!below(obj + O_Y, 0x12C) || !below(obj + O_X, 0x1A0)) {
        next(obj, 4);
        wr32(obj + 0x1C, 0xFFFD8000);
        wr32(obj + 0x18, asr32(neg32(rd32(obj + 0x18)), 2));
        wr8(obj + 0x21, 0);
        QueueSound(0xDC);
        return;
    }
    if (bit8(obj + 0x22, 4)) {
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x0565D4);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, neg32(rd32(obj + 0x18)));
        wr32(obj + 0x1C, neg32(rd32(obj + 0x1C)));
    }
}

/* ---- type $604: a bomb dropped by the object at $CB60 (the steps' a1): it bounces twice,
 * blinks, flashes the screen white, bursts in rings of puffs, removes $CB60 and fades back ---- */

#define BOMB_HOST 0xCB60         /* a1: Bomb604 sets it for every step */

/* $05E342. Type $604, by its step (a table of longs at $5E350). */
void Bomb604(uint16_t obj)
{
    call_object(rom32(0x05E350 + sx(rd16(obj + O_STATE))), obj);
}

/* $05E368. Step 0: below the host, $18 to the side it faces from ($30 the other way when it
 * faces left), thrown up; two bounces down to $E below the host's y (+$4A). */
void Bomb604Step00(uint16_t obj)
{
    uint16_t a1 = BOMB_HOST;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x8C00);
    wr16(obj + 0x0E, 0x447C);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr16(obj + 0x4A, rd16(a1 + O_Y));
    add16(obj + 0x4A, 0xE);
    wr16(obj + 0x4C, 2);
    wr16(obj + 0x1C, 0xFFFD);
    wr16(obj + O_X, rd16(a1 + O_X));
    wr16(obj + O_Y, rd16(a1 + O_Y));
    add16(obj + O_Y, 0xC);
    wr16(obj + 0x18, 0xFFFF);
    add16(obj + O_X, (uint16_t)-0x18);
    if (bit8(a1 + 0x0E, 3)) {
        wr16(obj + 0x18, 1);
        add16(obj + O_X, 0x30);
    }
    Bomb604Step04(obj);
}

/* $05E3D0. Falling; at the floor (+$4A) it bounces (half its x speed) until +$4C runs out, then
 * lies still for $80 frames. */
void Bomb604Step04(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x4000)) return;
    if ((int16_t)(rd16(obj + O_Y) - rd16(obj + 0x4A)) < 0) return;
    if (!dec(obj + 0x4C)) {
        wr32(obj + 0x1C, 0xFFFE4000);
        wr32(obj + 0x18, asr32(rd32(obj + 0x18), 1));
        return;
    }
    next(obj, 4);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_Y, rd16(obj + 0x4A));
    wr16(obj + 0x4A, 0x80);
}

/* $05E41E. Its last $30 frames blinking; then the palettes to white ($8002, $8014-$801A),
 * the sprites shaking, a blast. */
void Bomb604Step08(uint16_t obj)
{
    if (below(obj + 0x4A, 0x30)) {
        if (frame_bit(0)) set8(obj + O_FLAGS, 7);
        else clr8(obj + O_FLAGS, 7);
    }
    if (!dec(obj + 0x4A)) return;
    next(obj, 4);
    clr8(obj + O_FLAGS, 7);
    wr16(obj + 0x48, 0);
    wr16(obj + 0x4A, 0);
    wr16(0x8002, 0xF000);
    wr32(0x8014, 0);
    wr32(0x8018, 0);
    wr16(v_shakeSprites, 0x10);
    QueueSound(0xBE);
}

/* $05E47C. Brighter every other frame up to 8 (then $40 frames); every 5th frame a ring of
 * puffs (type $100, $5663A) from the list at $5E516 (+$4A: a count less 1, then (dx, dy) bytes;
 * a puff with no slot leaves its pair unread, its index counted). */
void Bomb604Step0C(uint16_t obj)
{
    if (!frame_bit(0)) {
        add16(0x8014, 1);
        add16(0x8016, 1);
        add16(0x8018, 1);
        add16(0x801A, 1);
        if (!below(0x8014, 8)) {
            next(obj, 4);
            wr16(obj + 0x48, 0x40);
            PalettesFull();
            return;
        }
    }
    PalettesFull();
    if (!dec(obj + 0x48)) return;
    wr16(obj + 0x48, 4);
    uint32_t a2 = 0x05E516 + sx(rd16(obj + 0x4A));
    uint16_t d7 = bus_rd16(a2);
    a2 += 2;
    add16(obj + 0x4A, 2);
    do {
        uint16_t s, t;
        if (SlotFreeHigh(&s, &t)) {
            wr16(s + O_TYPE, 0x100);
            wr32(s + 0x48, 0x05663A);
            uint16_t w = bus_rd16(a2);
            a2 += 2;
            uint16_t dx = (uint16_t)(int16_t)(int8_t)(w >> 8), dy = (uint16_t)(int16_t)(int8_t)w;
            wr16(s + O_X, (uint16_t)(dx + rd16(obj + O_X)));
            wr16(s + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
        }
        add16(obj + 0x4A, 2);
    } while (d7-- != 0);
}

/* $05E54A. White for $40 frames; then the host removed. */
void Bomb604Step10(uint16_t obj)
{
    PalettesFull();
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr16(BOMB_HOST + O_FLAGS, 0x1000);
}

/* $05E564. Back from white every other frame; at 0 it is removed. */
void Bomb604Step14(uint16_t obj)
{
    if (!frame_bit(0)) {
        add16(0x8014, 0xFFFF);
        add16(0x8016, 0xFFFF);
        add16(0x8018, 0xFFFF);
        add16(0x801A, 0xFFFF);
        if (!rd16(0x801A)) set8(obj + O_FLAGS, 4);
    }
    PalettesFull();
}

/* ---- type $58C: a boss (at $CB60) with its upper body at $DEE0 (a3, type $10), its arm's hit
 * box at $DF40 (a4, type $590) and its parts at $CBC0 ($598) and $DE80 ($5A0). Its step is a
 * table of longs at $5E6B4; +$48 the step's frames, +$4A, +$4E a leap's speeds, +$58 its flash,
 * +$5C its flags (bit 0 can be beaten, bit 1 flashes when hit, bit 2 kept between x $B0 and
 * $190). Steps 8-$44 its fight, $48-$4C its entrance, $50-$5C its end. ---- */

#define B58C_TOP 0xDEE0          /* a3 in its steps */
#define B58C_ARM 0xDF40          /* a4 */

/* $05EF0A. It faces the nearer player. */
void Boss58CFace(uint16_t obj)
{
    FaceNearest(obj);
}

/* $05EF10. The nearer player's distance in x (NearestPlayer). */
uint16_t Boss58CNearest(uint16_t obj)
{
    uint16_t dx, dy, p;
    return NearestPlayer(obj, &dx, &dy, &p);
}

/* $05E5BA. Hit (+$22 bit 5) while +$5C bit 1 is set: 3 frames of the flash palette bits
 * ($6000 in its sprite word); else they are cleared. */
void Boss58CFlash(uint16_t obj)
{
    if (!(rd16(obj + 0x58) & 0x8000)) {
        add16(obj + 0x58, 0xFFFF);
    } else if (bit8(obj + 0x5C, 1) && bit8(obj + 0x22, 5)) {
        wr16(obj + 0x58, 3);
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x6000);
        return;
    }
    wr16(obj + 0x0E, rd16(obj + 0x0E) & 0x9FFF);
}

/* $05E5F4. Kept within x $B0-$190 (+$5C bit 2); beaten (+$5C bit 0, vitality below 0): points,
 * step $50, still and harmless, its parts gone ($CBC0, $DEE0, $DF40); then its step. */
void Boss58CThink(uint16_t obj)
{
    if (rd16(obj + O_STATE)) {
        if (bit8(obj + 0x5C, 2)) {
            if (below(obj + O_X, 0xB0)) {
                wr16(obj + O_X, 0xB2);
                wr32(obj + 0x18, 0);
            } else if (!below(obj + O_X, 0x190)) {
                wr16(obj + O_X, 0x18E);
                wr32(obj + 0x18, 0);
            }
        }
        if (bit8(obj + 0x5C, 0) && (rd16(obj + O_VITALITY) & 0x8000)) {
            AddScore(obj, 0x47909);
            wr16(obj + O_STATE, 0x50);
            wr16(obj + O_FLAGS, 0xEC00);
            wr8(obj + 0x20, 0xBC);
            wr8(0xDEA0, 0xB8);
            wr16(obj + O_VITALITY, 0);
            wr32(obj + 0x18, 0);
            wr32(obj + 0x1C, 0);
            wr8(obj + 0x21, 0);
            wr8(obj + 0x5C, 0);
            wr16(obj + 0x48, 0xC0);
            wr16(obj + 0x4A, rd16(obj + O_Y));
            look(obj, 0x0E3B20);
            set8(0xCBC2, 4);
            set8(B58C_TOP + O_FLAGS, 4);
            set8(B58C_ARM + O_FLAGS, 4);
        }
    }
    call_object(rom32(0x05E6B4 + sx(rd16(obj + O_STATE))), obj);
}

/* $05E58E. Type $58C: its step, its flash; its upper body ($DEE0) takes its flash and flip bits
 * and its place. */
void Boss58C(uint16_t obj)
{
    Boss58CThink(obj);
    Boss58CFlash(obj);
    uint16_t a0 = B58C_TOP;
    uint16_t d0 = rd16(obj + 0x0E) & 0x6800;
    wr16(a0 + 0x0E, rd16(a0 + 0x0E) & 0x7FF);
    wr16(a0 + 0x0E, rd16(a0 + 0x0E) | d0);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
}

/* $05E714. Step 0: hidden at ($200, $100), its tiles' job ($5E7AA), its parts made: $598 at
 * $CBC0, the upper body (type $10) at $DEE0, $590 at $DF40, $5A0 at $DE80; $40 frames. */
void Boss58CStep00(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + 0x0E, 0xB56);
    wr32(obj + 0x08, 0x0E2CC0);
    wr8(obj + 0x20, 0x10);
    wr16(obj + 0x48, 0x40);
    wr16(obj + 0x52, 0);
    wr16(obj + 0x58, 0);
    wr8(obj + 0x5C, 0);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xF010D828);
    wr16(obj + O_Y, 0x100);
    wr16(obj + O_X, 0x200);
    uint32_t list = 0x05E7AA;
    NextJob(&list);
    wr16(0xCBC0 + O_TYPE, 0x598);
    wr16(0xCBC0 + O_STATE, 0);
    wr16(B58C_TOP + O_TYPE, 0x10);
    wr16(B58C_TOP + O_FLAGS, 0);
    wr8(B58C_TOP + 0x20, 4);
    wr16(B58C_ARM + O_TYPE, 0x590);
    wr16(B58C_ARM + O_STATE, 0);
    wr16(0xDE80 + O_TYPE, 0x5A0);
    wr16(0xDE80 + O_STATE, 0);
}

/* $05E7B4. Once the frames are out and the status bar's event done ($80C2): shown, walking in
 * from the right (step $48). */
void Boss58CStep04(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    if (rd16(0x80C2)) return;
    wr16(obj + O_STATE, 0x48);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(B58C_TOP + O_FLAGS, 0xE000);
    wr16(B58C_TOP + 0x0E, 0x356);
    look(B58C_TOP, 0x0E3F84);
    wr32(obj + 0x18, 0xFFFF4000);
    QueueSound(0xF6);
}

/* $05E7FA. Step $48: walking in until x $190; then stops, $80 frames. */
void Boss58CStep48(uint16_t obj)
{
    if (!below(obj + O_X, 0x190)) return;
    next(obj, 4);
    look(obj, 0x0E2F98);
    look(B58C_TOP, 0x0E40E2);
    wr16(obj + 0x48, 0x80);
    wr32(obj + 0x18, 0);
    QueueSound(0x91);
}

/* $05E834. Then the fight: vitality $1387, its flags 7, the status bar's boss bar on it. */
void Boss58CStep4C(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 8);
    wr16(obj + O_VITALITY, 0x1387);
    wr8(obj + 0x5C, 7);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, obj);
}

/* $05E8C2. On even frames debris (type $104, $5639C) from its middle, thrown up and to the
 * side by the frame count ($5E930). */
void Boss58CDebris(uint16_t obj)
{
    uint16_t s, t;
    if (frame_bit(0)) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x104);
    wr32(s + 0x48, 0x05639C);
    wr16(s + O_X, (uint16_t)((rd8(0xFF09) & 0x1F) + 0x10 + rd16(obj + O_X)));
    wr16(s + O_Y, (uint16_t)((rd8(v_randomSeed) & 0xF) + 8 + rd16(obj + O_Y)));
    wr32(s + 0x1C, 0xFFFB8000);
    uint16_t i = (uint16_t)(((int16_t)rd16(v_gameFrames) >> 1) & 7);
    /* ext.w, ext.l, swap: the sign word lands in the low word */
    wr32(s + 0x18, swap32((uint32_t)(int32_t)(int8_t)rom8(0x05E930 + i)));
    add16(s + O_X, (uint16_t)-0x1C);
}

/* $05E85A. Step $50, beaten: blasts and debris, the sprites shaking, it shudders ($4A its y,
 * 3 lower every 4 frames); after $C0 frames hidden, every object but $5A0 and $3D4 vanishing
 * (Stage6Clear). */
void Boss58CStep50(uint16_t obj)
{
    BlastSound8();
    Boss58CDebris(obj);
    wr16(v_shakeSprites, 8);
    if (dec(obj + 0x48)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0);
        wr16(v_shakeSprites, 0x10);
        Stage6Clear(0x5A0, 0x3D4);
        QueueSound(0xBE);
        return;
    }
    if (!frame_bit(2)) {
        wr16(obj + O_Y, rd16(obj + 0x4A));
        wr16(obj + 0x58, 8);
    } else {
        wr16(obj + O_Y, (uint16_t)(rd16(obj + 0x4A) + 3));
        wr16(obj + 0x58, 0xFFFF);
    }
}

/* $05E9BA. d7+1 puffs (type $100, $563F6) at (d6, d5), falling at d4, each $20000 faster up
 * (stops at the first missing slot). Its loop branches back to its own entry. */
void Boss58CPuffs(uint32_t d4, uint16_t d5, uint16_t d6, uint16_t d7)
{
    do {
        uint16_t s, t;
        if (!SlotFreeHigh(&s, &t)) return;
        wr16(s + O_TYPE, 0x100);
        wr32(s + 0x48, 0x0563F6);
        wr16(s + O_X, d6);
        wr16(s + O_Y, d5);
        wr32(s + 0x1C, d4);
        d4 -= 0x20000;
    } while (d7-- != 0);
}

/* $05E938. Step $54: the pilot thrown out (the part at $DE80 becomes type $10, $E49CC, flying
 * up and back), three columns of puffs; $80 frames. */
void Boss58CStep54(uint16_t obj)
{
    next(obj, 4);
    wr16(v_shakeSprites, 0x10);
    wr16(obj + 0x48, 0x80);
    uint16_t a0 = 0xDE80;
    wr16(a0 + O_TYPE, 0x10);
    wr16(a0 + 0x1C, 0xFFF9);
    wr16(a0 + O_FLAGS, 0xEE00);
    look(a0, 0x0E49CC);
    uint16_t d5 = (uint16_t)(rd16(obj + O_Y) + 0x20), d6 = rd16(obj + O_X);
    wr32(a0 + 0x18, bit8(obj + 0x0E, 3) ? 0xC000 : 0xFFFF4000);
    Boss58CPuffs(0xFFFF8000, d5, d6, 5);
    d6 = (uint16_t)(d6 - 0x10);
    Boss58CPuffs(0x10000, d5, d6, 5);
    d6 = (uint16_t)(d6 + 0x20);
    Boss58CPuffs(0x10000, d5, d6, 5);
}

/* $05E9E8. Then the wreck ($659C) falls from ($120, $80). */
void Boss58CStep58(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x8400);
    wr16(obj + 0x0E, 0x659C);
    wr16(obj + 0x08, 0);
    wr16(obj + 0x0A, 0xF9F8);
    wr8(obj + 0x20, 0xBC);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0x80);
    wr32(obj + 0x1C, 0x30000);
}

/* $05EA28. Slowing; when it would rise it becomes a burst ($566BC, type $100). */
void Boss58CStep5C(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x1000);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) return;
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0566BC);
    wr16(obj + O_STATE, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xDC);
}

/* loc_05EDEE: step $10, a punch ($E30D4) */
static void b58c_punch(uint16_t obj)
{
    wr16(obj + O_STATE, 0x10);
    look(obj, 0x0E30D4);
    look(B58C_TOP, 0x0E41B2);
    wr16(obj + 0x48, 0x12);
    wr32(obj + 0x18, 0);
}

/* loc_05EE18: step $1C, the other punch ($E3536) */
static void b58c_punch2(uint16_t obj)
{
    wr16(obj + O_STATE, 0x1C);
    look(obj, 0x0E3536);
    look(B58C_TOP, 0x0E4404);
    wr16(obj + 0x48, 0x28);
    wr32(obj + 0x18, 0);
}

/* loc_05EE42: step $28, harmless, its arm's box on */
static void b58c_dive(uint16_t obj)
{
    wr16(obj + O_STATE, 0x28);
    wr8(obj + 0x21, 0);
    look(obj, 0x0E39B8);
    look(B58C_TOP, 0x0E4634);
    wr16(obj + 0x48, 0x12);
    wr32(obj + 0x18, 0);
    wr16(B58C_ARM + O_STATE, 4);
}

/* loc_05EE76: step $30, crouching before a leap at (+$4A, +$4E) ($4A mirrored facing left) */
static void b58c_crouch(uint16_t obj)
{
    wr16(obj + O_STATE, 0x30);
    wr32(obj + 0x18, 0);
    look(obj, 0x0E3B20);
    look(B58C_TOP, 0x0E46DC);
    wr16(obj + 0x48, 0x14);
    if (bit8(obj + 0x0E, 3)) wr32(obj + 0x4A, neg32(rd32(obj + 0x4A)));
}

/* loc_05EEE0: step $38, $20 frames before step +$4A (its word) */
static void b58c_pause(uint16_t obj)
{
    wr16(obj + O_STATE, 0x38);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x48, 0x20);
    look(obj, 0x0E3E7E);
    look(B58C_TOP, 0x0E4866);
}

/* loc_05EED6: a pause, then step $3C */
static void b58c_pause3c(uint16_t obj)
{
    wr16(obj + 0x4A, 0x3C);
    b58c_pause(obj);
}

/* loc_05EA5E: its choice by the nearer player's distance (d0), its vitality and the
 * difficulty's step */
static void b58c_choose(uint16_t obj)
{
    uint16_t d0 = Boss58CNearest(obj);
    if (below(obj + O_VITALITY, 0x960)) {
        if (!below(AGGRESSION, 6) && (int16_t)(d0 - 0xC0) >= 0) {
            b58c_dive(obj);
            return;
        }
        if ((int16_t)(d0 - 0x5C) >= 0) {
            if (below(obj + O_X, 0xB3) || !below(obj + O_X, 0x18D)) {
                b58c_pause3c(obj);
                return;
            }
            wr16(obj + O_STATE, 0x44);
            look(obj, 0x0E2F70);
            look(B58C_TOP, 0x0E40BA);
            return;
        }
    } else if ((int16_t)(d0 - 0x40) >= 0) {
        if (!(rd16(v_gameFrames) & 0x7F)) {
            wr32(obj + 0x4A, 0x33000);
            wr32(obj + 0x4E, 0xFFFC8000);
            b58c_crouch(obj);
            return;
        }
        if (!(rd16(v_randomSeed) & 0xFF)) {
            b58c_pause3c(obj);
            return;
        }
        if (rd16(obj + O_STATE) == 0xC) return;
        wr16(obj + O_STATE, 0xC);
        look(obj, 0x0E2CC0);
        look(B58C_TOP, 0x0E3F84);
        return;
    } else {
        if (!(rd16(v_gameFrames) & 3)) {
            wr32(obj + 0x4A, 0xFFFE0000);
            wr32(obj + 0x4E, 0xFFFBC000);
            b58c_crouch(obj);
            return;
        }
        if (!rd16(v_difficulty)) {
            b58c_punch(obj);
            return;
        }
    }
    if (bit8(0xFF09, 0)) b58c_punch2(obj);
    else b58c_punch(obj);
}

/* $05EA52. Step 8: standing; when the frames are out it faces the nearer player and chooses. */
void Boss58CStep08(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    Boss58CFace(obj);
    b58c_choose(obj);
}

/* $05EB28. Step $C: walking ($C000 the way it faces), choosing every frame. */
void Boss58CStep0C(uint16_t obj)
{
    wr32(obj + 0x18, 0xC000);
    if (bit8(obj + 0x0E, 3)) wr32(obj + 0x18, neg32(rd32(obj + 0x18)));
    b58c_choose(obj);
}

/* $05EB42. Step $44: backing away ($B000) until out of x $B3-$18D: then step 8. */
void Boss58CStep44(uint16_t obj)
{
    if (below(obj + O_X, 0xB3) || !below(obj + O_X, 0x18D)) {
        wr16(obj + O_STATE, 8);
        wr16(obj + 0x48, 0);
        return;
    }
    wr32(obj + 0x18, 0xFFFF5000);
    if (bit8(obj + 0x0E, 3)) wr32(obj + 0x18, neg32(rd32(obj + 0x18)));
}

/* $05EB7A. Steps $10, $1C: the wind-up; then the next step, its frames by the difficulty's
 * step ($5EB98). */
void Boss58CStep10(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + 0x48, rom16(0x05EB98 + sx(rd16(AGGRESSION))));
}

/* $05EBA8. Step $14: the punch, its arm's box on for $20 frames (facing the player again from
 * step 8 of the difficulty). */
void Boss58CStep14(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    look(obj, 0x0E325C);
    look(B58C_TOP, 0x0E42A4);
    wr16(obj + 0x48, 0x20);
    wr16(B58C_ARM + O_STATE, 4);
    QueueSound(0xDB);
    if (!below(AGGRESSION, 8)) Boss58CFace(obj);
}

/* $05EBEE. Step $18: then its arm's box off, step 8 for $10 frames. */
void Boss58CStep18(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 8);
    wr16(obj + 0x48, 0x10);
    wr16(B58C_ARM + O_STATE, 0);
}

/* $05EC08. Step $20: the other punch (facing the player again from step $A). */
void Boss58CStep20(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    look(obj, 0x0E36D6);
    look(B58C_TOP, 0x0E44D8);
    QueueSound(0xDB);
    wr16(obj + 0x48, 0x20);
    wr16(B58C_ARM + O_STATE, 4);
    if (!below(AGGRESSION, 0xA)) Boss58CFace(obj);
}

/* $05EC4E. Step $24: then step 8 for $18 frames. */
void Boss58CStep24(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 8);
    wr16(obj + 0x48, 0x18);
    wr16(B58C_ARM + O_STATE, 0);
}

/* $05EC68. Step $28: then down low (its flags $CC00, the upper body's $C000). */
void Boss58CStep28(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xCC00);
    wr16(B58C_TOP + O_FLAGS, 0xC000);
    wr32(obj + 0x08, 0x0E3ABA);
    wr32(B58C_TOP + 0x08, 0x0E46A6);
}

/* $05EC92. Step $2C: down until the nearer player is within $C0: then up, step 8. */
void Boss58CStep2C(uint16_t obj)
{
    uint16_t d0 = Boss58CNearest(obj);
    if ((int16_t)(d0 - 0xC0) >= 0) return;
    wr16(obj + O_STATE, 8);
    wr8(obj + 0x21, 0x20);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(B58C_TOP + O_FLAGS, 0xE000);
    wr16(B58C_ARM + O_STATE, 0);
    wr16(obj + 0x48, 0x20);
    look(obj, 0x0E3B08);
    look(B58C_TOP, 0x0E46C4);
}

/* $05ECDC. Step $30: the crouch, then the leap. */
void Boss58CStep30(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr32(obj + 0x18, rd32(obj + 0x4A));
    wr32(obj + 0x1C, rd32(obj + 0x4E));
    look(obj, 0x0E3C0C);
    look(B58C_TOP, 0x0E474A);
}

/* $05ED0E. Step $34: in the air; landing at y $100: still, the sprites shaking, step 8. */
void Boss58CStep34(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x5000)) return;
    if (below(obj + O_Y, 0x100)) return;
    wr16(obj + O_STATE, 8);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x48, 0x20);
    wr16(obj + O_Y, 0x100);
    wr16(v_shakeSprites, 0xC);
    look(obj, 0x0E3CFA);
    look(B58C_TOP, 0x0E47B4);
    QueueSound(0xD1);
}

/* $05ED66. Step $38: the pause; then low (its flags $C000) and the step in +$4A. */
void Boss58CStep38(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_FLAGS, 0xC000);
    wr16(B58C_TOP + O_FLAGS, 0xC000);
    wr32(obj + 0x08, 0x0E3F28);
    wr16(obj + O_STATE, rd16(obj + 0x4A));
    wr32(B58C_TOP + 0x08, 0x0E48B6);
}

/* $05ED92. Step $3C: $80 frames of hands (step $40), facing the nearer player. */
void Boss58CStep3C(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x48, 0x80);
    Boss58CFace(obj);
}

/* loc_05F0E8: every 16th frame a hand (type $59C) from its side, thrown out level */
static void b58c_hand(uint16_t obj)
{
    uint16_t s, t;
    if (rd16(v_gameFrames) & 0xF) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x59C);
    wr32(s + 0x1C, (uint32_t)(rd16(v_randomSeed) & 3) << 15);
    wr16(s + O_Y, rd16(obj + O_Y));
    add16(s + O_Y, 4);
    wr16(s + O_X, rd16(obj + O_X));
    if (!bit8(obj + 0x0E, 3)) {
        add16(s + O_X, 0x26);
        wr32(s + 0x18, 0x17000);
    } else {
        add16(s + O_X, (uint16_t)-0x26);
        wr32(s + 0x18, 0xFFFE9000);
    }
}

/* $05EDA0. Step $40: hands until the frames are out; then up, step 8, its frames by the
 * difficulty's step ($5EDDE). */
void Boss58CStep40(uint16_t obj)
{
    if (!dec(obj + 0x48)) {
        b58c_hand(obj);
        return;
    }
    wr16(obj + O_STATE, 8);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(B58C_TOP + O_FLAGS, 0xE000);
    look(obj, 0x0E3F70);
    look(B58C_TOP, 0x0E48D4);
    wr16(obj + 0x48, rom16(0x05EDDE + sx(rd16(AGGRESSION))));
}

/* ---- types $590/$594, $598, $5A0: parts placed at a point of another object's frame ---- */

/* $05F388. The entry of the frame list of the object at a0 (+8; with its flags' bit 13, a
 * word offset from there first) whose first word is d1: six-byte entries, no end mark (the
 * search reads on through the ROM until one matches). */
uint32_t SpritePointFind(uint16_t at, uint16_t key)
{
    uint32_t a1 = rd32(at + 0x08);
    if (bit8(at + O_FLAGS, 5)) a1 = sx(bus_rd16(a1)) + rd32(at + 0x08);
    while (bus_rd16(a1) != key) a1 += 6;
    return a1;
}

/* $05F362. That entry's point (the bytes at +4, +5: dy, dx, dx mirrored when the object at a0
 * faces left) from the object: x returned, y in d1. */
uint16_t SpritePoint(uint16_t at, uint16_t *key)
{
    uint32_t a1 = SpritePointFind(at, *key) + 4;
    *key = (uint16_t)((int16_t)(int8_t)bus_rd8(a1) + rd16(at + O_Y));
    uint16_t x = (uint16_t)(int16_t)(int8_t)bus_rd8(a1 + 1);
    if (bit8(at + 0x0E, 3)) x = (uint16_t)-x;
    return (uint16_t)(x + rd16(at + O_X));
}

/* $05EF16. Types $590, $594: the boss's arm's hit box, at point $4800 of $DEE0's frame; step 0
 * hidden and harmless; hit (+$22 bit 4), the sprites shake. */
void Hitbox590(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0);
        wr8(obj + 0x21, 0);
        wr16(obj + 0x26, 0x14);
        wr8(obj + 0x23, 0x20);
        return;
    }
    int hit = bit8(obj + 0x22, 4);
    clr8(obj + 0x22, 4);
    if (hit) wr16(v_shakeSprites, 0xA);
    wr8(obj + 0x21, 0x30);
    uint16_t a0 = B58C_TOP, d1 = 0x4800;
    uint16_t d0 = SpritePoint(a0, &d1);
    wr16(obj + O_X, d0);
    wr16(obj + O_Y, d1);
    wr32(obj + 0x2C, bit8(a0 + 0x0E, 3) ? 0x0820CCFC : 0x08200434);
    wr32(obj + 0x28, 0xF010F010);
}

/* $05EF8C. Type $598: a part at point $483E of $CB60's frame, moving as it does (its y speed
 * less 1), its boxes and x speed by $CB60's facing. */
void Part598(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0);
        wr8(obj + 0x21, 0x24);
        wr8(obj + 0x46, 0x8F);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
    }
    uint16_t a0 = 0xCB60, d1 = 0x483E;
    uint16_t d0 = SpritePoint(a0, &d1);
    wr16(obj + O_X, d0);
    wr16(obj + O_Y, d1);
    wr32(obj + 0x1C, rd32(a0 + 0x1C) - 0x10000);
    if (!bit8(a0 + 0x0E, 3)) {
        wr32(obj + 0x2C, 0x08200434);
        wr32(obj + 0x28, 0x08200434);
        wr16(obj + 0x18, 1);
    } else {
        wr32(obj + 0x2C, 0x0820CCFC);
        wr32(obj + 0x28, 0x0820CCFC);
        wr16(obj + 0x18, 0xFFFF);
    }
}

/* $05F094. Its animation: frames $E497C + the words at $5F0C2 (offset, frames; a negative
 * offset starts the list again). */
void Part5A0Anim(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    for (;;) {
        uint16_t d7 = rd16(obj + 0x4A);
        add16(obj + 0x4A, 4);
        uint16_t d6 = rom16(0x05F0C2 + sx(d7));
        if (d6 & 0x8000) {
            wr16(obj + 0x4A, 0);
            continue;
        }
        wr32(obj + 0x08, sx(d6) + 0x0E497C);
        wr16(obj + 0x48, rom16(0x05F0C2 + sx((uint16_t)(d7 + 2))));
        return;
    }
}

/* $05F00C. Type $5A0: the boss's pilot, at point $483E of $CB60's frame (6 higher, $20 to the
 * side it faces), animated ($E49BA while $CB60's +$58 is 2 or more). */
void Part5A0(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0xC000);
        wr16(obj + 0x0E, 0x356);
        wr8(obj + 0x20, 8);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x48, 0);
        wr16(obj + 0x4A, 0);
    }
    uint16_t a0 = 0xCB60, d1 = 0x483E;
    uint16_t d0 = SpritePoint(a0, &d1);
    Part5A0Anim(obj);
    if (!below(a0 + 0x58, 2)) wr32(obj + 0x08, 0x0E49BA);
    d1 = (uint16_t)(d1 - 6);
    wr16(obj + O_X, d0);
    wr16(obj + O_Y, d1);
    if (bit8(a0 + 0x0E, 3)) {
        set8(obj + 0x0E, 3);
        add16(obj + O_X, (uint16_t)-0x20);
    } else {
        clr8(obj + 0x0E, 3);
        add16(obj + O_X, 0x20);
    }
}

/* ---- type $59C: a hand the boss $58C throws; it catches a player (+$40) and carries him up
 * until he struggles free (his buttons, +$48 times) or it leaves the room ---- */

/* loc_05F2D6: a burst ($5663A, type $100) */
static void hand_burst(uint16_t obj)
{
    QueueSound(0xCE);
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x05663A);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
}

/* loc_05F1F2: the player it holds at its place, still, facing by his +$69 */
static void hand_hold(uint16_t obj)
{
    uint16_t a0 = rd16(obj + 0x40);
    set8(a0 + 0x23, 1);
    clr8(a0 + 0x23, 4);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
    wr32(a0 + 0x18, 0);
    wr32(a0 + 0x1C, 0);
    if (bit8(a0 + 0x69, 2)) {
        clr8(a0 + 0x0E, 3);
        return;
    }
    if (bit8(a0 + 0x69, 3)) set8(a0 + 0x0E, 3);
}

/* $05F14E. Type $59C: destroyed (vitality below 0): 8 points and a burst; else its step (a
 * table of longs at $5F160). */
void Hand59C(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 8);
        hand_burst(obj);
        return;
    }
    call_object(rom32(0x05F160 + sx(rd16(obj + O_STATE))), obj);
}

/* $05F33C. Its sprite word, +8 and +$A from the three words at a0. */
void Hand59CLook(uint16_t obj, uint32_t at)
{
    wr16(obj + 0x0E, bus_rd16(at));
    wr16(obj + 0x08, bus_rd16(at + 2));
    wr16(obj + 0x0A, bus_rd16(at + 4));
}

/* $05F312. Opening: every $B frames the next of four frames ($5F34A). */
void Hand59CAnim(uint16_t obj)
{
    if (!below(obj + 0x4C, 0x18)) return;
    if (!dec(obj + 0x4A)) return;
    uint32_t a0 = sx(rd16(obj + 0x4C)) + 0x05F34A;
    wr16(obj + 0x4A, 0xA);
    add16(obj + 0x4C, 6);
    Hand59CLook(obj, a0);
}

/* $05F170. Step 0: harmful, vitality $50. */
void Hand59CStep00(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x8E80);
    wr8(obj + 0x21, 0x21);
    wr32(obj + 0x2C, 0xF60AF60A);
    wr32(obj + 0x28, 0xF40CF40C);
    wr16(obj + O_VITALITY, 0x50);
    wr16(obj + 0x48, 4);
    wr16(obj + 0x4A, 0);
    wr16(obj + 0x4C, 0);
    Hand59CStep04(obj);
}

/* $05F1A4. Flying, rising faster; when it has caught a player (+$22 bit 0, +$40): closed
 * ($5F35C), his place held, rising at half its x speed. */
void Hand59CStep04(uint16_t obj)
{
    Hand59CAnim(obj);
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x800);
    if (!bit8(obj + 0x22, 0)) return;
    if (!rd16(obj + 0x40)) return;
    Hand59CLook(obj, 0x05F35C);
    QueueSound(0xAC);
    next(obj, 4);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x1C, 0x10000);
    wr32(obj + 0x18, asr32(rd32(obj + 0x18), 1));
    hand_hold(obj);
}

/* $05F23A. Carrying him up (faster by the difficulty's step, $5F300); out of the room (y $A0,
 * x $88-$1B8) it lets him fall and bursts; his buttons count +$48 down: then it lets him jump
 * free and falls away. */
void Hand59CStep08(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) - sx(rom16(0x05F300 + sx(rd16(AGGRESSION)))));
    if (below(obj + O_Y, 0xA0) || below(obj + O_X, 0x88) || !below(obj + O_X, 0x1B8)) {
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0x8000);
        uint16_t a0 = rd16(obj + 0x40);
        set8(a0 + 0x23, 4);
        clr8(a0 + 0x23, 1);
        wr32(a0 + 0x1C, 0x24000);
        wr16(obj + 0x40, 0);
        hand_burst(obj);
        return;
    }
    uint16_t a0 = rd16(obj + 0x40);
    if (!(rd8(a0 + 0x6A) & 0x7C) || !dec(obj + 0x48)) {
        hand_hold(obj);
        return;
    }
    next(obj, 4);
    wr32(obj + 0x1C, 0x18000);
    wr32(a0 + 0x1C, 0xFFFE4000);
    QueueSound(0xAC);
}

/* $05F2F6. Then it falls away, rising ever less. */
void Hand59CStep0C(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x2000);
}

/* ---- type $5B4: a boss whose vitality is the word at $CA04 (its four orbs, $5D4, add to it):
 * its step is an offset table at $5F546 from $5F5B6 (56 steps by 2); +$48 the step's frames,
 * +$4A, +$4E a leap's speeds (or a counter, a slot), +$58 its flash, +$5C its flags (bit 0
 * flashes, bit 1 can be grabbed, bit 2 can be beaten), +$5D bits 0-1 at the room's right or
 * left edge, bit 7 the floor ripples; $8080 the orbs' orders (bits 0-3 to circle, 4-7 to
 * attack). Steps $3E-$54 its entrance, 2-$3C its fight, $54-$6E its end. ---- */

#define B5B4_HP 0xCA04           /* word: its vitality */
#define B5B4_ORDERS 0x8080       /* byte: the orbs' orders */

/* $060704. It faces the nearer player. */
void Boss5B4Face(uint16_t obj)
{
    FaceNearest(obj);
}

/* $06070A. The nearer player: its distance in x, dy in d5 (NearestPlayer). */
uint16_t Boss5B4Nearest(uint16_t obj, uint16_t *dy)
{
    uint16_t dx, p;
    return NearestPlayer(obj, &dx, dy, &p);
}

/* $06061C. Shown ($EC00), its animation from the start, its priority $70. */
void Boss5B4Show(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x20, 0x70);
}

/* $060614. Still, then shown. */
void Boss5B4Stop(uint16_t obj)
{
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    Boss5B4Show(obj);
}

/* loc_06059C: landed beaten: lying ($E5430) $60 frames at y $140 */
static void b5b4_landed(uint16_t obj)
{
    next(obj, 2);
    wr16(obj + 0x48, 0x60);
    wr16(obj + O_Y, 0x140);
    wr16(obj + O_FLAGS, 0xCC00);
    wr32(obj + 0x08, 0x0E5430);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xA5);
}

/* $060564. Beaten: the orbs called back ($8080 $F01), step $54, its vitality 0; in the air it
 * falls ($E521A), on the floor it lies down. */
void Boss5B4Beaten(uint16_t obj)
{
    wr16(B5B4_ORDERS, 0xF01);
    wr16(obj + O_STATE, 0x54);
    wr8(obj + 0x21, 0);
    wr16(B5B4_HP, 0);
    wr8(obj + 0x5C, 0);
    if (below(obj + O_Y, 0x140)) {
        wr32(obj + 0x1C, 0xFFFE0000);
        wr32(obj + 0x08, 0x0E521A);
        Boss5B4Show(obj);
        return;
    }
    b5b4_landed(obj);
}

/* loc_060406: step 4, walking ($E5008) */
static void b5b4_walk(uint16_t obj)
{
    if (rd16(obj + O_STATE) == 4) return;
    wr16(obj + O_STATE, 4);
    wr8(obj + 0x21, 0xA2);
    wr8(obj + 0x5C, 7);
    wr32(obj + 0x08, 0x0E5008);
    Boss5B4Show(obj);
}

/* loc_06042E: step $1A, standing (its look by the vitality of the object at $C9E0) */
static void b5b4_stand(uint16_t obj)
{
    if (rd16(obj + O_STATE) == 0x1A) return;
    wr16(obj + O_STATE, 0x1A);
    wr8(obj + 0x21, 0xA2);
    wr8(obj + 0x5C, 7);
    uint16_t a4 = 0xC9E0;
    if (!below(a4 + O_VITALITY, 0x24B8)) wr32(obj + 0x08, 0x0E4EBA);
    else if (!below(a4 + O_VITALITY, 0x1A90)) wr32(obj + 0x08, 0x0E4C74);
    else if (!below(a4 + O_VITALITY, 0xFA0)) wr32(obj + 0x08, 0x0E4D9A);
    else wr32(obj + 0x08, 0x0E4E1E);
    Boss5B4Stop(obj);
}

/* loc_06049C: step 6, ready to grab */
static void b5b4_grab(uint16_t obj)
{
    wr16(obj + O_STATE, 6);
    wr8(obj + 0x21, 0xA3);
    wr32(obj + 0x2C, 0xE010F010);
    wr16(obj + 0x48, 6);
}

/* loc_0604B8: step $2E, a slide ($E5420) */
static void b5b4_slide(uint16_t obj)
{
    QueueSound(0xDB);
    wr16(obj + O_STATE, 0x2E);
    wr16(obj + 0x48, 8);
    wr8(obj + 0x5C, 7);
    wr8(obj + 0x21, 0x30);
    wr16(obj + 0x26, 9);
    wr32(obj + 0x08, 0x0E5420);
    Boss5B4Stop(obj);
    if (!below(AGGRESSION, 6)) Boss5B4Face(obj);
    wr16(obj + 0x18, 2);
    wr32(obj + 0x2C, 0xE0F2FF30);
    if (bit8(obj + 0x0E, 3)) wr16(obj + 0x18, (uint16_t)-rd16(obj + 0x18));
}

/* loc_060518: step $14, before a leap ($E4FB8) */
static void b5b4_leap(uint16_t obj)
{
    wr16(obj + O_STATE, 0x14);
    wr16(obj + 0x48, 0x10);
    wr8(obj + 0x21, 0xA0);
    wr8(obj + 0x5C, 5);
    wr32(obj + 0x08, 0x0E4FB8);
    Boss5B4Stop(obj);
}

/* loc_06053C: step $1C, the fire */
static void b5b4_fire(uint16_t obj)
{
    Boss5B4Face(obj);
    wr16(obj + O_STATE, 0x1C);
    wr16(obj + 0x48, 0x20);
    wr8(obj + 0x21, 0x20);
    wr8(obj + 0x5C, 5);
    wr32(obj + 0x08, 0x0E4FB8);
    Boss5B4Stop(obj);
}

/* loc_0605CC: step $24, a throw ($E5352) */
static void b5b4_throw(uint16_t obj)
{
    wr16(obj + O_STATE, 0x24);
    wr16(obj + 0x48, 0x38);
    wr8(obj + 0x21, 0x20);
    wr8(obj + 0x5C, 5);
    wr32(obj + 0x08, 0x0E5352);
    Boss5B4Stop(obj);
}

/* loc_0605F0: step $26, the wave ($E52D4) */
static void b5b4_wave(uint16_t obj)
{
    wr16(obj + O_STATE, 0x26);
    wr16(obj + 0x48, 0x10);
    wr8(obj + 0x21, 0x20);
    wr8(obj + 0x5C, 5);
    wr32(obj + 0x08, 0x0E52D4);
    Boss5B4Stop(obj);
}

/* loc_06062E: step $28, a jump up ($E4FB8) */
static void b5b4_jump(uint16_t obj)
{
    wr16(obj + O_STATE, 0x28);
    wr16(obj + 0x48, 8);
    wr8(obj + 0x21, 0x20);
    wr8(obj + 0x5C, 5);
    wr32(obj + 0x08, 0x0E4FB8);
    Boss5B4Stop(obj);
}

/* loc_060652: step $2C, a run ($E518C) at the speed of the difficulty ($6069A) */
static void b5b4_run(uint16_t obj)
{
    wr16(obj + O_STATE, 0x2C);
    wr16(obj + 0x48, 0x28);
    wr8(obj + 0x21, 0x20);
    wr8(obj + 0x5C, 5);
    wr32(obj + 0x08, 0x0E518C);
    Boss5B4Stop(obj);
    Boss5B4Face(obj);
    wr16(obj + 0x18, rom8(0x06069A + sx((uint16_t)((int16_t)rd16(v_difficulty) >> 1))));
    if (bit8(obj + 0x0E, 3)) wr32(obj + 0x18, neg32(rd32(obj + 0x18)));
}

/* loc_06069E: step $30, gone (hidden, beams left behind) for the difficulty's frames ($606DA) */
static void b5b4_vanish(uint16_t obj)
{
    QueueSound(0xD4);
    Boss5B4Beams(obj);
    wr16(obj + O_STATE, 0x30);
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0);
    wr8(obj + 0x5C, 4);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x48, rom8(0x0606DA + sx((uint16_t)((int16_t)rd16(AGGRESSION) >> 1))));
}

/* loc_0606E2: step $34, harmless ($E4C1E) $40 frames */
static void b5b4_rest(uint16_t obj)
{
    wr16(obj + O_STATE, 0x34);
    wr8(obj + 0x21, 0);
    wr8(obj + 0x5C, 4);
    wr16(obj + 0x48, 0x40);
    wr32(obj + 0x08, 0x0E4C1E);
    Boss5B4Stop(obj);
}

/* $05FCB0. With no orb attacking ($8080 bits 4-7), one at random ($5FCD6) is sent. */
void Boss5B4Pick(void)
{
    if (rd8(B5B4_ORDERS) & 0xF0) return;
    uint8_t d7 = rd8(B5B4_ORDERS) & 0xF;
    d7 |= rom8(0x05FCD6 + (rd8(v_randomSeed) & 3));
    wr8(B5B4_ORDERS, d7);
}

/* loc_05FC28: its choice by the nearer player's distance (d0), its vitality ($CA04) and the
 * difficulty's step; the orbs circle or attack by the vitality */
static void b5b4_choose(uint16_t obj)
{
    uint16_t dy;
    uint16_t d0 = Boss5B4Nearest(obj, &dy);
    if (!below(B5B4_HP, 0x24B8)) {
        wr8(B5B4_ORDERS, 0xF);
        if (!lt(d0, 0x80)) b5b4_walk(obj);
        else if (lt(d0, 0x18)) b5b4_grab(obj);
        else b5b4_stand(obj);
        return;
    }
    if (!below(B5B4_HP, 0x1E14)) {
        wr8(B5B4_ORDERS, 0xF);
        if (lt(d0, 0x30)) {
            b5b4_grab(obj);
        } else if (lt(d0, 0x80)) {
            if (!below(AGGRESSION, 2) && !(rd16(v_gameFrames) & 0xFF)) b5b4_run(obj);
            else b5b4_stand(obj);
        } else if (frame_bit(6)) {
            b5b4_wave(obj);
        } else if (rd16(v_gameFrames) & 0x1F) {
            b5b4_walk(obj);
        } else if (!below(AGGRESSION, 6)) {
            b5b4_fire(obj);
        } else {
            b5b4_walk(obj);
        }
        return;
    }
    if (!below(B5B4_HP, 0x1964)) {
        wr8(B5B4_ORDERS, 0);
        if (lt(d0, 0x20)) {
            b5b4_grab(obj);
        } else if (lt(d0, 0x80)) {
            if (rd16(AGGRESSION) && lt(d0, 0x50)) b5b4_slide(obj);
            else b5b4_stand(obj);
        } else if (frame_bit(5)) {
            b5b4_wave(obj);
        } else if (!(rd16(v_randomSeed) & 3)) {
            b5b4_vanish(obj);
        } else if (!(rd16(v_gameFrames) & 0x1F)) {
            b5b4_run(obj);
        } else {
            b5b4_walk(obj);
        }
        return;
    }
    if (!below(B5B4_HP, 0x1068)) {
        wr8(B5B4_ORDERS, 0);
        if (lt(d0, 0x38)) {
            b5b4_grab(obj);
        } else if (lt(d0, 0x80)) {
            if (below(AGGRESSION, 6)) b5b4_walk(obj);
            else if (lt(d0, 0x50)) b5b4_slide(obj);
            else b5b4_walk(obj);
        } else if (frame_bit(4)) {
            b5b4_wave(obj);
        } else if (bit8(0xFF09, 1)) {
            b5b4_run(obj);
        } else if (!below(AGGRESSION, 4)) {
            b5b4_vanish(obj);
        } else {
            b5b4_run(obj);
        }
        return;
    }
    wr8(B5B4_ORDERS, rd8(B5B4_ORDERS) | 0xF);
    Boss5B4Pick();
    if (below(AGGRESSION, 6)) {
        b5b4_rest(obj);
        return;
    }
    if (lt(d0, 0x18)) {
        b5b4_grab(obj);
        return;
    }
    if (!bit8(v_gameFrames, 0)) {                /* the frame count's bit 8 */
        b5b4_stand(obj);
        return;
    }
    if (!below(AGGRESSION, 8) && !(rd16(v_gameFrames) & 0x7F) && !lt(d0, 0xA0)) {
        b5b4_vanish(obj);
        return;
    }
    if (frame_bit(6) && lt(d0, 0x50)) b5b4_slide(obj);
    else b5b4_walk(obj);
}

/* $05F424. Its clock (+$52, from $1800); beaten (vitality below 0, +$5C bit 2): points and
 * Boss5B4Beaten; else kept within x $A0-$1A0 (+$5D bits 0, 1), hurled when a player hits it
 * while it can be grabbed (step $E), grabbed by a player (step $A, his facing in +$48); then
 * its step. */
void Boss5B4Think(uint16_t obj)
{
    if (dec(obj + 0x52)) wr16(obj + 0x52, 0x1800);
    if ((rd16(B5B4_HP) & 0x8000) && bit8(obj + 0x5C, 2)) {
        AddScore(obj, 0x3714126);
        Boss5B4Beaten(obj);
    } else {
        wr8(obj + 0x5D, rd8(obj + 0x5D) & 0xFC);
        if (!below(obj + O_X, 0x1A0)) {
            wr16(obj + O_X, 0x19E);
            set8(obj + 0x5D, 0);
        } else if (below(obj + O_X, 0xA0)) {
            wr16(obj + O_X, 0xA2);
            set8(obj + 0x5D, 1);
        }
        int caught = 0;
        if (bit8(obj + 0x5C, 1) && bit8(obj + 0x22, 5)) {
            caught = bit8(obj + 0x23, 4);
            clr8(obj + 0x23, 4);
        }
        if (caught) {
            wr16(obj + O_STATE, 0xE);
            wr32(obj + 0x1C, 0xFFFE8000);
            wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0x20000 : 0xFFFE0000);
        } else {
            int held = bit8(obj + 0x23, 1);
            clr8(obj + 0x23, 1);
            uint16_t by = rd16(obj + 0x42);
            if (held && (by == 0xA400 || by == 0xA600)) {
                wr16(obj + 0x48, rd16(by + 0x0E) & 0x800);
                wr16(obj + O_STATE, 0xA);
                wr32(obj + 0x18, 0);
                wr32(obj + 0x1C, 0);
            } else {
                held = 0;
            }
            caught = held;
        }
        if (caught) {
            wr16(obj + 0x58, 0);
            wr8(obj + 0x5C, 4);
            wr16(obj + O_FLAGS, 0xEC00);
            look(obj, 0x0E5538);
            wr8(obj + 0x21, 0);
        }
    }
    call_object(0x05F5B6 + sx(rom16(0x05F546 + sx(rd16(obj + O_STATE)))), obj);
}

/* $05F3B4. Type $5B4: its step, the floor's waves, ripples and colour cycle, the planes shaking
 * with the sprites; colours $5F418 on line 0 ($E316, $E396), the flash $5F41E for 4 frames when
 * hit (+$5E bit 5) while +$5C bit 0 is set. */
void Boss5B4(uint16_t obj)
{
    Boss5B4Think(obj);
    Boss5B4Waves();
    Boss5B4Ripple(obj);
    Boss5B4Cycle();
    wr16(v_shakePlane, rd16(v_shakeSprites));
    uint32_t a0 = 0x05F418;
    if (bit8(obj + 0x5C, 0)) {
        if (!(rd16(obj + 0x58) & 0x8000)) {
            add16(obj + 0x58, 0xFFFF);
        } else {
            int hit = bit8(obj + 0x5E, 5);
            clr8(obj + 0x5E, 5);
            if (hit) {
                wr16(obj + 0x58, 4);
                a0 = 0x05F41E;
            }
        }
    }
    for (int k = 0; k < 3; k++) {
        wr16((uint16_t)(0xE316 + 2 * k), rom16(a0 + 2 * (uint32_t)k));
        wr16((uint16_t)(0xE396 + 2 * k), rom16(a0 + 2 * (uint32_t)k));
    }
}

/* $05F5B6. Step 0: every object from $C9E0 vanishes, the H-interrupt effect $1C, the players'
 * +$C0 bit 7 cleared; standing ($E4C1E) at ($120, $DE) for $120 frames; plane A's flag, the
 * colours $5F662 on line 0 ($E302), the orbs' orders $F00, the line table cleared; step $3E. */
void Boss5B4Step00(uint16_t obj)
{
    ObjectsVanishFrom(0xC9E0, 0, 0);
    wr16(v_hintEffect, 0x1C);
    wr16(v_hintState, 0);
    wr16(obj + 0x4A, 0);
    clr8(0xA4C0, 7);
    clr8(0xA6C0, 7);
    wr16(obj + O_STATE, 0x3E);
    wr16(0x808A, 0x8000);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(obj + 0x0E, 0x4280);
    look(obj, 0x0E4C1E);
    wr8(obj + 0x20, 0xBC);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x28, 0xDE10EC14);
    wr8(obj + 0x23, 0x20);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0xDE);
    wr16(obj + 0x48, 0x120);
    wr8(obj + 0x5C, 0);
    set8(v_planeFlagsA, 7);
    wr8(0xF7E7, 3);
    for (int k = 0; k < 4; k++) {
        wr16((uint16_t)(0xE302 + 2 * k), rom16(0x05F662 + 2 * (uint32_t)k));
        wr16((uint16_t)(0xE382 + 2 * k), rom16(0x05F662 + 2 * (uint32_t)k));
    }
    wr16(B5B4_ORDERS, 0xF00);
    Boss5B4ClearLines();
}

/* $05F66A. Step $3E: the posts at $DF40, $DFA0, the orbs, four objects from $C800 (types from
 * $5F6EC, $30 apart from x $D8, rising); the line table. */
void Boss5B4Step3E(uint16_t obj)
{
    next(obj, 2);
    Boss5B4Posts();
    Boss5B4Orbs(obj);
    uint16_t a0 = 0xC800, d0 = 0xD8;
    uint32_t a1 = 0x05F6EC;
    for (int k = 0; k < 4; k++) {
        wr16(a0 + O_TYPE, rom16(a1));
        a1 += 2;
        wr16(a0 + O_X, d0);
        wr16(a0 + O_Y, 0x120);
        wr32(a0 + 0x1C, 0xFFFFFFFE);
        wr16(a0 + O_FLAGS, 0x8400);
        wr16(a0 + 6, 0);
        d0 = (uint16_t)(d0 + 0x30);
        a0 = (uint16_t)(a0 + 0x60);
    }
    wr16(0xC970, 0x14);
    Boss5B4Fill(obj);
}

/* $05F6BA. Step $40: when the frames are out, the four from $C800 removed; $40 frames. */
void Boss5B4Step40(uint16_t obj)
{
    Boss5B4Fill(obj);
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x40);
    uint16_t a0 = 0xC800;
    for (int k = 0; k < 4; k++, a0 = (uint16_t)(a0 + 0x60))
        if (rd16(a0 + O_TYPE)) wr16(a0 + O_FLAGS, 0x1000);
}

/* $05F6F4. Step $42: then the status bar's message ($34, text $39B54). */
void Boss5B4Step42(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        next(obj, 2);
        wr16(0x80C2, 0x34);
        wr32(0x80C8, 0x039B54);
    }
    Boss5B4Fill(obj);
}

/* $05F712. Step $44: once the message is done, $60 frames. */
void Boss5B4Step44(uint16_t obj)
{
    if (!rd16(0x80C2)) {
        next(obj, 2);
        wr16(obj + 0x48, 0x60);
    }
    Boss5B4Fill(obj);
}

/* $05F728. Step $46: then a quake (the sprites shaking), +$4A 4. */
void Boss5B4Step46(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        next(obj, 2);
        wr16(obj + 0x48, 0x40);
        wr16(v_shakeSprites, 0xA);
        QueueSound(0xD1);
        wr16(obj + 0x4A, 4);
    }
    Boss5B4Fill(obj);
}

/* $05F754. Step $48: $40 frames, a sound. */
void Boss5B4Step48(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        next(obj, 2);
        QueueSound(0xB5);
    }
    Boss5B4Fill(obj);
}

/* $05F76E. Step $4A: the floor opening (+$4A up to $90); then its tiles marked, plane B $200
 * lower ($A928) and sinking ($A938), its sprite in front. */
void Boss5B4Step4A(uint16_t obj)
{
    add16(obj + 0x4A, 1);
    if (!below(obj + 0x4A, 0x90)) {
        next(obj, 2);
        Boss5B4Mark();
        add16(0xA928, (uint16_t)-0x200);
        wr16(0xA938, 8);
        set8(obj + 0x0E, 7);
    }
    Boss5B4Fill(obj);
}

/* $05F79A. Step $4C: smoke until plane B is down to $800; then plane A moving. */
void Boss5B4Step4C(uint16_t obj)
{
    Boss5B4Smoke(obj);
    if (!below(0xA928, 0x800)) {
        next(obj, 2);
        wr16(0xA938, 0);
        wr16(v_planeSpeeds, 8);
    }
    Boss5B4Fill(obj);
}

/* $05F7BA. Step $4E: smoke until plane A is at $1C80; then still, the floor rippling (+$5D bit
 * 7), the H-interrupt effect off, $40 frames. */
void Boss5B4Step4E(uint16_t obj)
{
    Boss5B4Smoke(obj);
    if (below(v_planes, 0x1C80)) {
        Boss5B4Fill(obj);
        return;
    }
    next(obj, 2);
    wr16(obj + 0x48, 0x40);
    set8(obj + 0x5D, 7);
    wr16(v_planeSpeeds, 0);
    wr16(v_hintEffect, 0);
    wr16(v_hintState, 0);
}

/* $060856. Four DMAs of the chunk the word at a0 names (times $20 from $FF3000) to the plane at
 * d1, 8 apart. */
void Boss5B4Chunk(uint16_t d1, uint32_t *at)
{
    uint32_t d0 = (uint32_t)(uint16_t)(bus_rd16(*at) << 5) + 0xFFFF3000u;
    *at += 2;
    for (int k = 0; k < 4; k++) {
        ChunkDma(d0, d1);
        d1 = (uint16_t)(d1 + 8);
    }
}

/* $05F7E6. Step $50: smoke, the floor's three rows ($E018, $E218, $E418) changing by +$48
 * ($5F810); then $40 frames. */
void Boss5B4Step50(uint16_t obj)
{
    Boss5B4Smoke(obj);
    if (dec(obj + 0x48)) {
        next(obj, 2);
        wr16(obj + 0x48, 0x40);
        return;
    }
    uint32_t a0 = rom32(0x05F810 + (uint16_t)((rd16(obj + 0x48) << 2) & 0xC));
    Boss5B4Chunk(0xE018, &a0);
    Boss5B4Chunk(0xE218, &a0);
    Boss5B4Chunk(0xE418, &a0);
}

/* $05F820. Step $52: smoke; then the object at $C9E0 (type $10, vitality $270E) the status
 * bar's boss, it leaps left ($34000, $FFFE0000 kept for step $14). */
void Boss5B4Step52(uint16_t obj)
{
    Boss5B4Smoke(obj);
    if (!dec(obj + 0x48)) return;
    uint16_t a0 = 0xC9E0;
    wr16(a0 + O_TYPE, 0x10);
    wr16(a0 + O_FLAGS, 0);
    wr8(a0 + 0x21, 0);
    wr16(a0 + O_VITALITY, 0x270E);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, a0);
    set8(obj + 0x0E, 3);
    wr32(obj + 0x4A, 0x34000);
    wr32(obj + 0x4E, 0xFFFE0000);
    b5b4_leap(obj);
}

/* $05F866. Step $54, beaten in the air: falling; landing at y $140, the sprites shake. */
void Boss5B4Step54(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x5000)) return;
    if (below(obj + O_Y, 0x140)) return;
    wr16(v_shakeSprites, 0x12);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    b5b4_landed(obj);
}

/* loc_05F908: its priority by the frame, the sprites shaking */
static void b5b4_shudder(uint16_t obj)
{
    wr8(obj + 0x20, 0x70);
    if (!frame_bit(0)) wr8(obj + 0x20, 0xBC);
    wr16(v_shakeSprites, 4);
}

/* $05F88E. Step $56: lying, dust; then the status bar's event $2C, $A0 frames. */
void Boss5B4Step56(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        next(obj, 2);
        wr16(0x80C2, 0x2C);
        wr16(obj + 0x48, 0xA0);
        wr8(obj + 0x20, 0x70);
        wr16(v_shakeSprites, 4);
        return;
    }
    Boss5B4Dust(obj);
    b5b4_shudder(obj);
}

/* $05F8B0. Step $58: rumbling; then it jumps up and back ($E51D6), +$4A the slot $CA40, +$4E 4
 * pieces to drop. */
void Boss5B4Step58(uint16_t obj)
{
    if (!dec(obj + 0x48)) {
        Boss5B4Rumble(obj);
        b5b4_shudder(obj);
        return;
    }
    next(obj, 2);
    wr16(obj + 0x48, 0x70);
    wr16(obj + 0x4A, 0xCA40);
    wr16(obj + 0x4E, 4);
    wr16(obj + 0x18, 0xFFFE);
    wr16(obj + 0x1C, 0xFFFB);
    wr16(obj + O_FLAGS, 0xEC00);
    if (bit8(obj + 0x0E, 3)) wr32(obj + 0x18, neg32(rd32(obj + 0x18)));
    look(obj, 0x0E51D6);
}

/* $05F926. Step $5A: falling; landing at y $140 it bounces ($E521A). */
void Boss5B4Step5A(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x5000)) return;
    if (below(obj + O_Y, 0x140)) return;
    next(obj, 2);
    wr16(v_shakeSprites, 0xC);
    wr16(obj + 0x1C, 0xFFFD);
    wr32(obj + 0x18, asr32(rd32(obj + 0x18), 1));
    look(obj, 0x0E521A);
}

/* $05F964. Step $5C: falling again; landing: still ($E5538) $80 frames. */
void Boss5B4Step5C(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x4000)) return;
    if (below(obj + O_Y, 0x140)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x80);
    wr16(v_shakeSprites, 6);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    look(obj, 0x0E5538);
}

/* $05F9A0. Step $5E: every $61 frames a piece falls (type $5FC in the slot +$4A, $60 on); after
 * the last ($E55B0), $10 frames. */
void Boss5B4Step5E(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    if (dec(obj + 0x4E)) {
        next(obj, 2);
        wr16(obj + 0x48, 0x10);
        wr32(obj + 0x08, 0x0E55B0);
        Boss5B4Show(obj);
        return;
    }
    uint16_t a0 = rd16(obj + 0x4A);
    wr16(a0 + O_TYPE, 0x5FC);
    wr16(a0 + O_STATE, 0);
    wr16(obj + 0x48, 0x60);
    add16(obj + 0x4A, 0x60);
}

/* $05F9E0. Step $60: then it rises ($E56C6) $C0 frames, facing the room's middle. */
void Boss5B4Step60(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    QueueSound(0x9B);
    next(obj, 2);
    wr16(obj + 0x48, 0xC0);
    wr32(obj + 0x08, 0x0E56C6);
    Boss5B4Show(obj);
    set8(obj + 0x0E, 3);
    if (below(obj + O_X, 0x120)) clr8(obj + 0x0E, 3);
}

/* $05FA20. Step $62: smoke; then it flies up ($E513C) toward the middle. */
void Boss5B4Step62(uint16_t obj)
{
    Boss5B4Smoke(obj);
    if (!dec(obj + 0x48)) return;
    QueueSound(0xA5);
    wr16(v_shakeSprites, 0x10);
    next(obj, 2);
    wr32(obj + 0x1C, 0xFFF88000);
    wr32(obj + 0x08, 0x0E513C);
    Boss5B4Show(obj);
    if (below(obj + O_X, 0x120)) {
        wr16(obj + 0x18, 2);
        clr8(obj + 0x0E, 3);
    } else {
        wr16(obj + 0x18, 0xFFFE);
        set8(obj + 0x0E, 3);
    }
}

/* $05FA7A. Step $64: sparks until above y $20; then hidden, the palettes to white. */
void Boss5B4Step64(uint16_t obj)
{
    Boss5B4Sparks(obj);
    if (!below(obj + O_Y, 0x20)) return;
    next(obj, 2);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + 0x48, 0xA0);
    wr16(0x8002, 0xF000);
    wr16(0x8014, 0);
    wr16(0x8016, 0);
    wr16(0x8018, 0);
    wr16(0x801A, 0);
    wr32(obj + 0x08, 0x0E56C6);
    Boss5B4Show(obj);
}

/* $05FB00. Its picture's tiles: the commands at $5FB36, then the picture at $9C00 shrunk (by 1)
 * to the VRAM at $6000 (ScaleShrink, list $9E00). */
void Boss5B4Shrink(void)
{
    RunCommands(0x05FB36);
    wr16(0x8046, 1);
    ScaleShrink(0x9E00, 0x9C00, 0x6000);
}

/* $05FAB8. Step $66: whiter every other frame up to 8; then +$5C $80, the shrunk pictures made,
 * one notch back. */
void Boss5B4Step66(uint16_t obj)
{
    if (!frame_bit(0)) {
        add16(0x8014, 1);
        add16(0x8016, 1);
        add16(0x8018, 1);
        add16(0x801A, 1);
        if (!below(0x8014, 8)) {
            next(obj, 2);
            wr8(obj + 0x5C, 0x80);
            Boss5B4Shrink();
            add16(0x8014, 0xFFFF);
            add16(0x8016, 0xFFFF);
            add16(0x8018, 0xFFFF);
            add16(0x801A, 0xFFFF);
        }
    }
    PalettesFull();
}

/* loc_061B1C: its size d0 (0-$1F): the sprite's size (+$A from bits 1-4 of d0 and the ROM word
 * at $F0; +8 by d0's eighths) and its tile (d1 + the byte at $61B7A) */
static void b5b4_size(uint16_t obj, uint16_t d0, uint16_t d1)
{
    uint16_t d2 = (uint16_t)(((d0 & 0xFF) >> 1) & 0xF);
    d2 |= rom16(0x0000F0);
    d2 |= (uint16_t)(d2 << 8);
    wr16(obj + 0x0A, d2);
    if ((int16_t)(d0 - 0x18) >= 0) wr16(obj + 0x08, 0);
    else if ((int16_t)(d0 - 0x10) >= 0) wr16(obj + 0x08, 0x500);
    else if ((int16_t)(d0 - 8) >= 0) wr16(obj + 0x08, 0xA00);
    else wr16(obj + 0x08, 0xF00);
    d0 = (uint16_t)((d0 & 0xFF00) | rom8(0x061B7A + sx(d0)));
    wr16(obj + 0x0E, (uint16_t)(d0 + d1));
}

/* $05FC0C. Its picture at the size +$48 (0 below 0), tiles from $6300. */
void Boss5B4Glow(uint16_t obj)
{
    uint16_t d0 = rd16(obj + 0x48);
    if (d0 & 0x8000) d0 = 0;
    b5b4_size(obj, d0, 0x6300);
}

/* $05FB40. Step $68: back from white every other frame; at 0 shown at ($108, $AC), its picture
 * at size $1E. */
void Boss5B4Step68(uint16_t obj)
{
    if (!frame_bit(0)) {
        add16(0x8014, 0xFFFF);
        add16(0x8016, 0xFFFF);
        add16(0x8018, 0xFFFF);
        add16(0x801A, 0xFFFF);
        if (!rd16(0x801A)) {
            next(obj, 2);
            wr16(obj + O_FLAGS, 0x8C00);
            wr32(obj + 0x18, 0);
            wr32(obj + 0x1C, 0);
            wr16(obj + O_X, 0x108);
            wr16(obj + O_Y, 0xAC);
            wr8(obj + 0x20, 0xBC);
            wr16(obj + 0x48, 0x1E);
            Boss5B4Glow(obj);
        }
    }
    PalettesFull();
}

/* $05FB92. Step $6A: drifting right every other frame to x $138; then away ($80 frames). */
void Boss5B4Step6A(uint16_t obj)
{
    if (frame_bit(0)) return;
    add16(obj + O_X, 1);
    if (below(obj + O_X, 0x138)) return;
    QueueSound(0xC8);
    next(obj, 2);
    wr16(obj + 0x4A, 0x80);
    wr16(obj + 0x4C, 0x20);
    wr32(obj + 0x18, 0x10000);
    wr32(obj + 0x1C, 0x10000);
}

/* $05FBD6. Step $6C: flying off up and left, shrinking every other frame; then hidden, the
 * stage cleared. */
void Boss5B4Step6C(uint16_t obj)
{
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x1C00);
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x800);
    if (dec(obj + 0x4A)) {
        next(obj, 2);
        wr16(obj + O_FLAGS, 0);
        set8(v_stageClear, 7);
        return;
    }
    if (!frame_bit(0)) add16(obj + 0x48, 0xFFFF);
    Boss5B4Glow(obj);
}

/* $05FC1E. Step $6E: nothing. */
void Boss5B4Step6E(uint16_t obj)
{
    (void)obj;
}

/* $05FC20. Step 2: standing; when the frames are out it chooses. */
void Boss5B4Step02(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    b5b4_choose(obj);
}

/* $05FDE6. Step $1A: smoke, facing the nearer player, choosing every frame. */
void Boss5B4Step1A(uint16_t obj)
{
    Boss5B4Smoke(obj);
    Boss5B4Face(obj);
    b5b4_choose(obj);
}

/* $05FDF2. Step 4: walking ($E000 the way it faces), choosing every frame. */
void Boss5B4Step04(uint16_t obj)
{
    Boss5B4Smoke(obj);
    Boss5B4Face(obj);
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFF2000 : 0xE000);
    b5b4_choose(obj);
}

/* $05FE1C. Step 6: a player touching it (+$22 bit 0, +$40) is grabbed: held $16 above it, $20
 * to the side, step 8; else when the frames are out a slide, a throw (the player below), a jump
 * (him well above) or at random vanishing. */
void Boss5B4Step06(uint16_t obj)
{
    int touched = bit8(obj + 0x22, 0);
    clr8(obj + 0x22, 0);
    uint16_t d0 = rd16(obj + 0x40);
    if (touched && (d0 == 0xA400 || d0 == 0xA600)) {
        next(obj, 2);
        wr16(obj + O_FLAGS, 0xEC00);
        wr32(obj + 0x08, 0x0E541C);
        wr16(obj + 0x48, 0xC);
        wr16(obj + 0x0C, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        uint16_t a0 = d0;
        set8(a0 + 0x23, 1);
        wr16(a0 + O_X, rd16(obj + O_X));
        wr16(a0 + O_Y, rd16(obj + O_Y));
        add16(a0 + O_Y, (uint16_t)-0x16);
        if (below(obj + O_X, 0x120)) {
            clr8(obj + 0x0E, 3);
            add16(a0 + O_X, 0x20);
        } else {
            set8(obj + 0x0E, 3);
            add16(a0 + O_X, (uint16_t)-0x20);
        }
        return;
    }
    if (!dec(obj + 0x48)) return;
    if (below(B5B4_HP, 0x1068)) {
        b5b4_slide(obj);
        return;
    }
    uint16_t dy;
    Boss5B4Nearest(obj, &dy);
    if (!(dy & 0x8000)) b5b4_throw(obj);
    else if ((int16_t)(dy - 0xFFD0) < 0) b5b4_jump(obj);
    else if (!(rd16(v_randomSeed) & 3)) b5b4_vanish(obj);
    else b5b4_slide(obj);
}

/* $05FED6. Step 8: holding the player (+$40), his facing its own; then he is thrown away
 * (step 2). */
void Boss5B4Step08(uint16_t obj)
{
    uint16_t a0 = rd16(obj + 0x40);
    set8(a0 + 0x23, 1);
    wr32(a0 + 0x18, 0);
    wr32(a0 + 0x1C, 0);
    uint16_t d0 = rd16(obj + 0x0E) & 0x800;
    wr16(a0 + 0x0E, rd16(a0 + 0x0E) & 0xF7FF);
    wr16(a0 + 0x0E, rd16(a0 + 0x0E) | d0);
    if (!dec(obj + 0x48)) return;
    wr16(v_shakeSprites, 0x10);
    wr16(obj + O_STATE, 2);
    wr16(obj + 0x48, 0x20);
    set8(a0 + 0x23, 4);
    clr8(a0 + 0x23, 1);
    wr32(a0 + 0x1C, 0xFFFC8000);
    wr16(obj + 0x40, 0);
    wr32(a0 + 0x18, bit8(obj + 0x0E, 3) ? 0xFFF9C000 : 0x64000);
}

/* $05FF4A. Step $A, held by a player: until he lets go (+$23 bit 1). */
void Boss5B4Step0A(uint16_t obj)
{
    int held = bit8(obj + 0x23, 1);
    clr8(obj + 0x23, 1);
    if (held) return;
    clr8(obj + 0x23, 4);
    next(obj, 2);
}

/* $05FF60. Step $C: thrown the way he faced (+$48). */
void Boss5B4Step0C(uint16_t obj)
{
    next(obj, 2);
    wr32(obj + 0x1C, 0x20000);
    wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | (uint16_t)(rd16(obj + 0x48) ^ 0x800));
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0x88000 : 0xFFF78000);
    Boss5B4Step0E(obj);
}

/* $05FF9C. Step $E: flying; at the floor (y $140) a bounce. */
void Boss5B4Step0E(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x5000)) return;
    if (below(obj + O_Y, 0x140)) return;
    next(obj, 2);
    wr32(obj + 0x18, asr32(rd32(obj + 0x18), 1));
    wr32(obj + 0x1C, 0xFFFEC000);
    wr16(v_shakeSprites, 6);
    QueueSound(0xAC);
}

/* $05FFD8. Step $10: then on the floor, sliding. */
void Boss5B4Step10(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x5000)) return;
    if (below(obj + O_Y, 0x140)) return;
    next(obj, 2);
    wr16(obj + O_Y, 0x140);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x18, asr32(rd32(obj + 0x18), 1));
    wr16(v_shakeSprites, 2);
    QueueSound(0xAC);
}

/* $060024. Step $12: sliding to a stop ($1000 a frame); then (after a slide) step 2, or step $3C
 * for the difficulty's frames ($6008E). */
void Boss5B4Step12(uint16_t obj)
{
    if (rd16(obj + 0x18) & 0x8000) {
        add32(obj + 0x18, 0x1000);
        if ((int32_t)(rd32(obj + 0x18) - 0xFFFF0000u) < 0) return;
    } else {
        wr32(obj + 0x18, rd32(obj + 0x18) - 0x1000);
        if ((int32_t)(rd32(obj + 0x18) - 0x10000u) >= 0) return;
    }
    wr32(obj + 0x18, 0);
    if (rd16(obj + O_STATE) == 0x2E) {
        wr16(obj + O_STATE, 2);
        wr16(obj + 0x48, 0x10);
        wr32(obj + 0x08, 0x0E4CCA);
        Boss5B4Stop(obj);
        return;
    }
    wr16(obj + O_STATE, 0x3C);
    wr16(obj + 0x48, rom16(0x06008E + sx(rd16(v_difficulty))));
}

/* $060016. Step $2E, the slide: harmful again after +$48 frames; else as step $12. */
void Boss5B4Step2E(uint16_t obj)
{
    if (dec(obj + 0x48)) wr8(obj + 0x21, 0xA2);
    Boss5B4Step12(obj);
}

/* $060096. Step $3C: then step 2 ($E55B0), $18 frames. */
void Boss5B4Step3C(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 2);
    wr16(obj + 0x48, 0x18);
    wr32(obj + 0x08, 0x0E55B0);
    Boss5B4Stop(obj);
}

/* $0600B6. Step $14: then the leap at (+$4A, +$4E) ($E51D6). */
void Boss5B4Step14(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr32(obj + 0x18, rd32(obj + 0x4A));
    wr32(obj + 0x1C, rd32(obj + 0x4E));
    wr32(obj + 0x08, 0x0E51D6);
    Boss5B4Show(obj);
}

/* $0600DE. Step $16: rising; at the top step $18, falling ($E521A) at half its x speed. */
void Boss5B4Step16(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x5000)) return;
    wr16(obj + O_STATE, 0x18);
    wr8(obj + 0x21, 0xA2);
    wr32(obj + 0x18, asr32(rd32(obj + 0x18), 1));
    wr32(obj + 0x08, 0x0E521A);
    Boss5B4Show(obj);
}

/* $0600DA. Step $2A: a trail, then as step $16. */
void Boss5B4Step2A(uint16_t obj)
{
    Boss5B4Trail1(obj);
    Boss5B4Step16(obj);
}

/* $06010C. Step $18: falling to y $140: landed, step 2 ($E4FB8). */
void Boss5B4Step18(uint16_t obj)
{
    add32(obj + 0x1C, 0x5000);
    if (below(obj + O_Y, 0x140)) return;
    wr16(obj + O_STATE, 2);
    wr16(v_shakeSprites, 8);
    wr16(obj + O_Y, 0x140);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x48, 0x20);
    wr8(obj + 0x5C, 7);
    wr32(obj + 0x08, 0x0E4FB8);
    Boss5B4Show(obj);
}

/* $060150. Step $1C: fire forward, the sprites shaking; then it leaps at the nearer player at
 * the difficulty's speed ($601B6). */
void Boss5B4Step1C(uint16_t obj)
{
    Boss5B4Fire(obj);
    wr16(v_shakeSprites, 3);
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0xC);
    wr16(v_shakeSprites, 0x10);
    wr32(obj + 0x08, 0x0E51D6);
    Boss5B4Show(obj);
    FaceNearest(obj);
    QueueSound(0xCC);
    wr16(obj + 0x1C, 0xFFFC);
    uint32_t v = rom32(0x0601B6 + sx((uint16_t)(rd16(v_difficulty) << 1)));
    if (bit8(obj + 0x0E, 3)) v = neg32(v);
    wr32(obj + 0x18, v);
}

/* $0601C6. Step $1E: firing in the air; then harmful ($E518C). */
void Boss5B4Step1E(uint16_t obj)
{
    Boss5B4Fire(obj);
    add32(obj + 0x1C, 0x5000);
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr8(obj + 0x21, 0x30);
    wr32(obj + 0x2C, 0xD8FCF010);
    wr16(obj + 0x26, 0xA);
    wr32(obj + 0x08, 0x0E518C);
    Boss5B4Show(obj);
}

/* $0601FE. Step $20: falling to y $140: a bounce ($E521A). */
void Boss5B4Step20(uint16_t obj)
{
    add32(obj + 0x1C, 0x5000);
    if (below(obj + O_Y, 0x140)) return;
    next(obj, 2);
    wr16(v_shakeSprites, 0xA);
    wr32(obj + 0x1C, 0xFFFD8000);
    wr32(obj + 0x18, asr32(rd32(obj + 0x18), 2));
    wr32(obj + 0x08, 0x0E521A);
    Boss5B4Show(obj);
}

/* $060238. Step $22: landing at y $140: sliding (step $12). */
void Boss5B4Step22(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x5000)) return;
    if (below(obj + O_Y, 0x140)) return;
    wr16(obj + O_STATE, 0x12);
    wr16(obj + O_Y, 0x140);
    wr16(v_shakeSprites, 8);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x08, 0x0E4FB8);
    Boss5B4Show(obj);
}

/* $060276. Steps $24, $26: then the shots (or the wave), facing the nearer player; step 2. */
void Boss5B4Step24(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    Boss5B4Face(obj);
    Boss5B4Shots(obj);
    wr16(obj + O_STATE, 2);
    wr16(v_shakeSprites, 8);
    wr16(obj + 0x48, 0x28);
    wr8(obj + 0x5C, 7);
}

/* $0602A0. Step $28: then a jump up ($E513C) toward the nearer player. */
void Boss5B4Step28(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    QueueSound(0xF1);
    next(obj, 2);
    wr16(v_shakeSprites, 8);
    wr32(obj + 0x1C, 0xFFF80000);
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xD808F010);
    wr16(obj + 0x26, 0xE);
    wr8(obj + 0x5C, 5);
    wr32(obj + 0x08, 0x0E513C);
    Boss5B4Show(obj);
    Boss5B4Face(obj);
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFEC000 : 0x14000);
}

/* $06030C. Step $2C, the run: a trail; when the frames are out it stands; a player within $40:
 * a jump (every 4th frame) or a slide. */
void Boss5B4Step2C(uint16_t obj)
{
    Boss5B4Trail2(obj);
    if (dec(obj + 0x48)) {
        b5b4_stand(obj);
        return;
    }
    uint16_t dy;
    uint16_t d0 = Boss5B4Nearest(obj, &dy);
    if ((int16_t)(d0 - 0x40) >= 0) return;
    if (!(rd16(v_gameFrames) & 3)) b5b4_jump(obj);
    else b5b4_slide(obj);
}

/* $060336. Step $30, vanished: then it comes back on the floor $40 to the room's middle side of
 * the weaker player, facing him, beams around. */
void Boss5B4Step30(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    uint16_t a0 = PlayerWeaker();
    uint16_t d0 = rd16(a0 + O_X);
    if ((int16_t)(d0 - 0x120) < 0) {
        set8(obj + 0x0E, 3);
        d0 = (uint16_t)(d0 + 0x40);
    } else {
        clr8(obj + 0x0E, 3);
        d0 = (uint16_t)(d0 - 0x40);
    }
    wr16(obj + O_X, d0);
    wr16(obj + O_Y, 0x140);
    next(obj, 2);
    wr16(obj + 0x48, 8);
    QueueSound(0xD4);
    uint16_t d2 = 0xFFE8;
    Boss5B4Beam(obj, 4, &d2);
    Boss5B4Beam(obj, 0xFFFC, &d2);
}

/* $06038E. Step $32: then a slide. */
void Boss5B4Step32(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    b5b4_slide(obj);
}

/* $06039A. Steps $34, $38: blinking; then a sound and $20 frames shown. */
void Boss5B4Step34(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        QueueSound(0xDC);
        next(obj, 2);
        wr16(obj + 0x48, 0x20);
        wr8(obj + 0x20, 0xC0);
        set8(obj + O_FLAGS, 7);
        return;
    }
    if (bit8(obj + 0x49, 0)) set8(obj + O_FLAGS, 7);
    else clr8(obj + O_FLAGS, 7);
}

/* $0603DA. Step $36: hidden; then back at ($120, $DE), $40 frames. */
void Boss5B4Step36(uint16_t obj)
{
    clr8(obj + O_FLAGS, 7);
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x40);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0xDE);
}

/* $060400. Step $3A: an orb sent now and then. */
void Boss5B4Step3A(uint16_t obj)
{
    (void)obj;
    Boss5B4Pick();
}

/* $060710. The palette's colour cycle: line 0's word 3 by the frame count ($E31C, $E39C). */
void Boss5B4Cycle(void)
{
    uint16_t d0 = (uint16_t)(((int16_t)rd16(v_gameFrames) >> 1) & 0xE);
    wr16(0xE31C, d0);
    wr16(0xE39C, d0);
}

/* $060724. The line table at $9600: +$4A, its 18 less +$4A/8 first lines (one at least), then
 * $58 above it by 8 to $9626, a 0 after. */
void Boss5B4Fill(uint16_t obj)
{
    uint16_t a0 = 0x9600;
    uint16_t d2 = (uint16_t)((int16_t)rd16(obj + 0x4A) >> 3);
    uint16_t d7 = (uint16_t)(17 - d2);
    if (d7 & 0x8000) d7 = 0;
    uint16_t d0 = rd16(obj + 0x4A);
    do {
        wr16(a0, d0);
        a0 = (uint16_t)(a0 + 2);
    } while (d7-- != 0);
    uint16_t d1 = (uint16_t)((d0 & 0xFFF8) + 0x58);
    do {
        wr16(a0, d1);
        a0 = (uint16_t)(a0 + 2);
        d1 = (uint16_t)(d1 - 8);
    } while ((int16_t)(a0 - 0x9626) < 0);
    wr16(a0, 0);
}

/* $06075C. The line table at $9600 cleared ($404 bytes). */
void Boss5B4ClearLines(void)
{
    for (uint16_t a0 = 0x9600, k = 0; k <= 0x100; k++, a0 = (uint16_t)(a0 + 4)) wr32(a0, 0);
}

/* $06076E. The floor's waves: the scroll words from $E680 and $E67C (8 apart, 17 each) by the
 * long at $8040 (+$8000 a frame) and the word at $8044 (+1), swapped on even frames. */
void Boss5B4Waves(void)
{
    uint16_t a0 = 0xE680, a1 = 0xE67C;
    add32(0x8040, 0x8000);
    uint16_t d0 = rd16(0x8040);
    add16(0x8044, 1);
    uint16_t d1 = rd16(0x8044);
    if (!frame_bit(0)) {
        uint16_t t = d0;
        d0 = d1;
        d1 = t;
    }
    for (int k = 0; k < 17; k++) {
        d0 = (uint16_t)(d0 - 4);
        d1 = (uint16_t)(d1 + 4);
        wr16(a0, d0);
        wr16(a1, d1);
        a0 = (uint16_t)(a0 + 8);
        a1 = (uint16_t)(a1 + 8);
    }
}

/* $0607AE. The floor rippling (+$5D bit 7): 24 scroll words from $E460 (4 apart) moved by the
 * bytes at $60808, the next 16 at $140 on even frames, the next 32 up 1 every 4th frame. */
void Boss5B4Ripple(uint16_t obj)
{
    if (!bit8(obj + 0x5D, 7)) return;
    uint16_t a0 = 0xE460, d0 = 0;
    for (int k = 0; k < 24; k++) {
        add16(a0, (uint16_t)(int16_t)(int8_t)rom8(0x060808 + d0));
        d0 = (uint16_t)((d0 + 1) & 0xF);
        a0 = (uint16_t)(a0 + 4);
    }
    for (int k = 0; k < 16; k++) {
        if (!frame_bit(0)) wr16(a0, 0x140);
        a0 = (uint16_t)(a0 + 4);
    }
    if (rd16(v_gameFrames) & 3) return;
    for (int k = 0; k < 32; k++) {
        add16(a0, 1);
        a0 = (uint16_t)(a0 + 4);
    }
}

/* $060818. Three rows of 16 tiles in the plane buffer ($FF3720, $FF33C0, $FF33E0: the words at
 * $6083C) in front (bit 15). */
void Boss5B4Mark(void)
{
    for (uint16_t d0 = 0; d0 < 6; d0 = (uint16_t)(d0 + 2)) {
        uint32_t a0 = (uint32_t)rom16(0x06083C + d0) + 0xFFFF3000u;
        for (int k = 0; k < 16; k++, a0 += 2) bus_wr16(a0, bus_rd16(a0) | 0x8000);
    }
}

/* $06088C. Every 4th frame smoke (type $57C) near its feet, rising. */
void Boss5B4Smoke(uint16_t obj)
{
    uint16_t s, t;
    if (rd16(v_gameFrames) & 3) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + 0x48, 0x20);
    wr16(s + O_TYPE, 0x57C);
    wr16(s + O_FLAGS, 0x8480);
    wr16(s + 0x0E, 0xE59C);
    wr16(s + 0x08, 0);
    wr16(s + 0x0A, 0xFCFC);
    wr8(s + 0x20, 4);
    wr32(s + 0x1C, (uint32_t)rd16(v_randomSeed) + 0xFFFF0000u);
    wr16(s + O_X, (uint16_t)((rd16(v_randomSeed) & 0x7F) - 0x40 + rd16(obj + O_X)));
    wr16(s + O_Y, rd16(obj + O_Y));
    add16(s + O_Y, 0x10);
    if (!bit8(obj + 0x0E, 3)) add16(s + O_X, 4);
    else add16(s + O_X, (uint16_t)-8);
}

/* $061806. On even frames a fireball (type $5DC, $563A4) from $10 above it, thrown forward and
 * down. */
void Boss5B4Fire(uint16_t obj)
{
    uint16_t s, t;
    if (frame_bit(0)) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x5DC);
    wr32(s + 0x48, 0x0563A4);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    add16(s + O_Y, (uint16_t)-0x10);
    wr32(s + 0x1C, 0x50000);
    wr32(s + 0x18, bit8(obj + 0x0E, 3) ? 0x50000 : 0xFFFB0000);
}

/* $06185E. Sparks (type $5DC, $618C6) falling behind it while it is below y $78, the sprites
 * shaking. */
void Boss5B4Sparks(uint16_t obj)
{
    uint16_t s, t;
    wr16(v_shakeSprites, 8);
    if (below(obj + O_Y, 0x78)) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x5DC);
    wr32(s + 0x48, 0x0618C6);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    add16(s + O_Y, (uint16_t)-0x10);
    wr16(s + 0x1C, (uint16_t)((rd8(v_randomSeed) & 1) + 2));
    wr16(s + 0x1A, 0x8000);
    wr16(s + 0x18, rd8(0xFF0A) & 3);
    if (!bit8(obj + 0x0E, 3)) wr32(s + 0x18, neg32(rd32(s + 0x18)));
}

/* $0618F0. Its shots: in step $26 one wave (type $5E4) along the floor at the difficulty's speed
 * ($619D6); else the difficulty's count ($61976) of shots (type $5E0) from its hand at the
 * angles at $6197E (mirrored facing left). */
void Boss5B4Shots(uint16_t obj)
{
    uint16_t s, t;
    if (rd16(obj + O_STATE) == 0x26) {
        if (!SlotFreeHigh(&s, &t)) return;
        wr16(s + O_TYPE, 0x5E4);
        wr16(s + O_X, rd16(obj + O_X));
        wr16(s + O_Y, rd16(obj + O_Y));
        add16(s + O_Y, 8);
        /* moveq #0; move.w; asr.w #1; move.b T(pc,d0.w),d0 (its bits 8-15 kept); swap */
        uint16_t i = (uint16_t)((int16_t)rd16(v_difficulty) >> 1);
        uint32_t d0 = (uint32_t)((i & 0xFF00) | rom8(0x0619D6 + sx(i)));
        wr32(s + 0x18, swap32(d0));
        if (!bit8(obj + 0x0E, 3)) {
            add16(s + O_X, 0x10);
        } else {
            add16(s + O_X, (uint16_t)-0x10);
            wr32(s + 0x18, neg32(rd32(s + 0x18)));
        }
        return;
    }
    uint16_t d2 = 0;
    uint16_t d7 = rom16(0x061976 + sx(rd16(v_difficulty)));
    do {
        if (!SlotFreeHigh(&s, &t)) return;
        wr16(s + O_TYPE, 0x5E0);
        wr16(s + O_X, rd16(obj + O_X));
        wr16(s + O_Y, rd16(obj + O_Y));
        add16(s + O_Y, (uint16_t)-0x1E);
        uint16_t d6 = rom16(0x06197E + sx(d2));
        if (!bit8(obj + 0x0E, 3)) {
            add16(s + O_X, 0x23);
        } else {
            add16(s + O_X, (uint16_t)-0x23);
            d6 = (uint16_t)((d6 + 0x100) & 0x1FE);
        }
        uint32_t d4 = sx(rom16(SINES + sx(d6))), d5 = sx(rom16(COSINES + sx(d6)));
        wr32(s + 0x1C, d4 << 4);
        wr32(s + 0x18, d5 << 4);
        d2 = (uint16_t)(d2 + 2);
    } while (d7-- != 0);
}

/* loc_0619EA: every 4th frame an after-image (type $250, the frames at d1) of it, drifting back
 * at an eighth of its speed */
static void b5b4_trail(uint16_t obj, uint32_t map)
{
    uint16_t s, t;
    if (rd16(v_gameFrames) & 3) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x250);
    wr16(s + O_FLAGS, 0xCC00);
    wr16(s + 0x0E, rd16(obj + 0x0E));
    wr32(s + 0x08, map);
    wr8(s + 0x20, 0xA4);
    wr16(s + 0x48, 1);
    wr16(s + 0x4A, 0xE);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    wr32(s + 0x18, asr32(neg32(rd32(obj + 0x18)), 3));
    wr32(s + 0x1C, asr32(neg32(rd32(obj + 0x1C)), 3));
}

/* $0619DA. Its after-images, running ($E5194). */
void Boss5B4Trail2(uint16_t obj)
{
    b5b4_trail(obj, 0x0E5194);
}

/* $0619E4. Its after-images, leaping ($E5144). */
void Boss5B4Trail1(uint16_t obj)
{
    b5b4_trail(obj, 0x0E5144);
}

/* $061A66. A beam (type $250, $E5144) d2 to its side, moving at d1; d2 negated for the next. */
void Boss5B4Beam(uint16_t obj, uint16_t d1, uint16_t *d2)
{
    uint16_t s, t;
    if (!SlotFreeMid(&s, &t)) return;
    wr16(s + O_TYPE, 0x250);
    wr16(s + O_FLAGS, 0xC800);
    wr16(s + 0x0E, rd16(obj + 0x0E));
    wr32(s + 0x08, 0x0E5144);
    wr8(s + 0x20, 0x80);
    wr16(s + 0x48, 1);
    wr16(s + 0x4A, 7);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    add16(s + O_X, *d2);
    wr16(s + 0x18, d1);
    *d2 = (uint16_t)-*d2;
}

/* $061A5A. Two beams at it, moving apart. */
void Boss5B4Beams(uint16_t obj)
{
    uint16_t d2 = 0;
    Boss5B4Beam(obj, 4, &d2);
    Boss5B4Beam(obj, 0xFFFC, &d2);
}

/* $061AB2. Its four orbs (type $5D4) at $CA40 on, numbered by +6 (0, 2, 4, 6), their sprite
 * words from $61B14 (kept at +$5A). */
void Boss5B4Orbs(uint16_t obj)
{
    uint16_t a0 = 0xCA40, d1 = 0;
    for (int k = 0; k < 4; k++) {
        wr16(a0 + O_TYPE, 0x5D4);
        wr16(a0 + O_FLAGS, 0x8C80);
        wr16(a0 + O_STATE, 0);
        wr16(a0 + O_VITALITY, 0);
        wr16(a0 + 6, d1);
        wr16(a0 + 0x08, 0);
        wr16(a0 + 0x0A, 0xFCFC);
        wr8(a0 + 0x20, 4);
        wr8(a0 + 0x21, 0x22);
        wr32(a0 + 0x28, 0xF808F808);
        wr16(a0 + O_X, rd16(obj + O_X));
        wr16(a0 + O_Y, rd16(obj + O_Y));
        wr16(a0 + 0x0E, rom16(0x061B14 + sx(d1)));
        wr16(a0 + 0x5A, rd16(a0 + 0x0E));
        d1 = (uint16_t)(d1 + 2);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $061BB0. On odd frames dust (type $104, $563F6) about its feet. */
void Boss5B4Dust(uint16_t obj)
{
    uint16_t s, t;
    if (!frame_bit(0)) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x104);
    wr32(s + 0x48, 0x0563F6);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    add16(s + O_X, (uint16_t)((rd16(v_gameFrames) & 0xF) - 8));
    wr16(s + 0x18, (uint16_t)((rd8(v_randomSeed) & 7) - 4));
    wr16(s + 0x1A, 0x8000);
    wr16(s + 0x1C, (uint16_t)((rd8(0xFF09) & 3) - 4));
}

/* $061B9A. Every 16th frame a rumble; dust. */
void Boss5B4Rumble(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 0xF)) QueueSound(0xBE);
    Boss5B4Dust(obj);
}

/* $061C26. A post (type $10) at a0: still, harmless but solid, y $160. */
void Boss5B4Post(uint16_t at)
{
    wr16(at + O_TYPE, 0x10);
    wr16(at + O_FLAGS, 0);
    wr8(at + 0x21, 4);
    wr8(at + 0x46, 0x8F);
    wr32(at + 0x2C, 0xF010807F);
    wr16(at + O_Y, 0x160);
}

/* $061C0E. The posts at $DF40 (x $180) and $DFA0 (x $C0). */
void Boss5B4Posts(void)
{
    wr16(0xDF40 + O_X, 0x180);
    Boss5B4Post(0xDF40);
    wr16(0xDFA0 + O_X, 0xC0);
    Boss5B4Post(0xDFA0);
}

/* ---- type $5D4: one of the boss $5B4's four orbs (+6 its number: 0, 2, 4, 6): it hovers about
 * the boss ($C980) and, on the boss's orders ($8080: bits 7-4 attack, 3-0 circle, orb 0 the
 * high bit of each), attacks. Its step is an offset table at $60A20 from $60A56. Its hits go
 * to the boss's vitality ($CA04); +$5A its sprite word, +$58 its flash. ---- */

#define ORB_BOSS 0xC980          /* a1 in its steps */

/* loc_061D1E: its own sprite word, the small frame */
static void orb_plain(uint16_t obj)
{
    wr16(obj + 0x0E, rd16(obj + 0x5A));
    wr16(obj + 0x08, 0);
    wr16(obj + 0x0A, 0xFCFC);
}

/* $061CE6. Back to step 0, its order bit in $8080 (the bit number at $60A3E by +6) cleared. */
void Orb5D4Home(uint16_t obj)
{
    wr16(obj + O_STATE, 0);
    uint16_t d0 = rom16(0x060A3E + sx(rd16(obj + 6)));
    clr8(B5B4_ORDERS, d0 & 7);
    orb_plain(obj);
}

/* $061D00. On odd frames its glowing frame ($84B9), else its own. */
void Orb5D4Blink(uint16_t obj)
{
    if (!frame_bit(0)) {
        orb_plain(obj);
        return;
    }
    wr16(obj + 0x0E, 0x84B9);
    wr16(obj + 0x08, 0xA00);
    wr16(obj + 0x0A, 0xF4F4);
}

/* the speed (+field) toward a target: up by inc below hi, down by dec above lo (the speed's
 * sign word tested first, then cmpi.l) */
static void orb_chase(uint16_t at, uint16_t target, uint16_t pos, uint32_t lo, uint32_t hi, uint32_t down, uint32_t up)
{
    if ((int16_t)(target - pos) >= 0) {
        if ((rd16(at) & 0x8000) || (int32_t)(rd32(at) - hi) < 0) wr32(at, rd32(at) + up);
    } else {
        if (!(rd16(at) & 0x8000) || (int32_t)(rd32(at) - lo) >= 0) wr32(at, rd32(at) - down);
    }
}

/* $060D86. Toward (d2, d1), at most a pixel a frame, $1000 more a frame. */
void Orb5D4Seek(uint16_t obj, uint16_t d1, uint16_t d2)
{
    orb_chase(obj + 0x18, d2, rd16(obj + O_X), 0xFFFEFFFF, 0x10000, 0x1000, 0x1000);
    orb_chase(obj + 0x1C, d1, rd16(obj + O_Y), 0xFFFEFFFF, 0x10000, 0x1000, 0x1000);
}

/* loc_060D52: circling the room's middle ($120, $C8) at radius 1, from angle d0, with the frame
 * count */
static void orb_circle(uint16_t obj, uint16_t d0)
{
    d0 = (uint16_t)((d0 + (uint16_t)(rd16(v_gameFrames) << 2)) & 0x1FE);
    uint32_t d1 = swap32(sx(rom16(SINES + d0)) << 8), d2 = swap32(sx(rom16(COSINES + d0)) << 8);
    Orb5D4Seek(obj, (uint16_t)((uint16_t)d1 + 0xC8), (uint16_t)((uint16_t)d2 + 0x120));
}

/* $0617BA. Once in 8 frames (its own) a puff (type $100, $566BC) about it. */
void Orb5D4Puff(uint16_t obj)
{
    uint16_t s, t;
    if ((rd16(v_gameFrames) & 7) != rd16(obj + 6)) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x100);
    wr32(s + 0x48, 0x0566BC);
    wr16(s + O_X, (uint16_t)((rd8(0xFF09) & 0xF) - 8 + rd16(obj + O_X)));
    wr16(s + O_Y, (uint16_t)((rd8(v_randomSeed) & 0xF) - 8 + rd16(obj + O_Y)));
}

/* $06095E. With the boss beaten (its step $54 or more) it goes home; else hit by a player's
 * grab (+$22 bit 1) it loses $50 and goes home; its hits passed on to the boss (+$5E), its
 * sprite flashing (by $60A36) when hit; its vitality added to the boss's. Then kept inside x
 * $90-$1B0, y $90-$150 (bouncing) and its step. */
void Orb5D4Think(uint16_t obj)
{
    uint16_t a1 = ORB_BOSS;
    if (!below(a1 + O_STATE, 0x54)) {
        Orb5D4Home(obj);
    } else {
        int grabbed = bit8(obj + 0x22, 1);
        clr8(obj + 0x22, 1);
        uint16_t by = rd16(obj + 0x42);
        if (grabbed && (by == 0xA400 || by == 0xA600)) {
            add16(obj + O_VITALITY, (uint16_t)-0x50);
            Orb5D4Home(obj);
        }
        wr8(a1 + 0x5E, rd8(a1 + 0x5E) | rd8(obj + 0x22));
        wr16(obj + 0x0E, rd16(obj + 0x5A));
        if (bit8(obj + 0x22, 5)) wr16(obj + 0x0E, rom16(0x060A36 + sx(rd16(obj + 6))));
        uint16_t d0 = rd16(obj + O_VITALITY);
        wr16(obj + O_VITALITY, 0);
        add16(B5B4_HP, d0);
    }
    if (!(rd16(obj + 0x1C) & 0x8000) ? !below(obj + O_Y, 0x150) : below(obj + O_Y, 0x90))
        wr32(obj + 0x1C, neg32(rd32(obj + 0x1C)));
    if (!(rd16(obj + 0x18) & 0x8000) ? !below(obj + O_X, 0x1B0) : below(obj + O_X, 0x90))
        wr32(obj + 0x18, neg32(rd32(obj + 0x18)));
    call_object(0x060A56 + sx(rom16(0x060A20 + sx(rd16(obj + O_STATE)))), obj);
}

/* $060910. Type $5D4: its step; harmless and not hittable while the orders' bit 8 ($8081 bit 0)
 * is set; else hit while hittable, $20 frames of shield (+$23 bit 2). */
void Orb5D4(uint16_t obj)
{
    Orb5D4Think(obj);
    if (bit8(0x8081, 0)) {
        wr8(obj + 0x23, 0);
        wr8(obj + 0x21, 0);
        return;
    }
    if (!(rd16(obj + 0x58) & 0x8000)) {
        add16(obj + 0x58, 0xFFFF);
        set8(obj + 0x23, 2);
        return;
    }
    if (bit8(obj + 0x22, 5) && bit8(obj + 0x23, 4)) wr16(obj + 0x58, 0x20);
    clr8(obj + 0x23, 2);
}

/* $060A56. Step 0: puffs; it hovers $40 above the boss, each orb at its own pace, until its
 * order to attack (to step 8, $C, $10 or $E-$14 by its number) or to circle comes. */
void Orb5D4Step00(uint16_t obj)
{
    uint16_t a1 = ORB_BOSS;
    Orb5D4Puff(obj);
    uint16_t d0 = rd16(a1 + O_X), d1 = (uint16_t)(rd16(a1 + O_Y) - 0x40);
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    uint16_t n = rd16(obj + 6);
    if (n == 6) {
        if (bit8(B5B4_ORDERS, 4)) {
            next(obj, 2);
            wr16(obj + 0x48, 0x40);
            return;
        }
        if (bit8(B5B4_ORDERS, 0)) {
            orb_circle(obj, 0x180);
            return;
        }
        orb_chase(obj + 0x18, d0, x, 0xFFFF0000, 0x10000, 0x2000, 0x2000);
        orb_chase(obj + 0x1C, d1, y, 0xFFFC0000, 0x40000, 0x2000, 0x2000);
    } else if (n == 4) {
        if (bit8(B5B4_ORDERS, 5)) {
            wr16(obj + O_STATE, 0xC);
            wr16(obj + 0x48, 0x80);
            wr16(obj + 0x4A, 5);
            return;
        }
        if (bit8(B5B4_ORDERS, 1)) {
            orb_circle(obj, 0x100);
            return;
        }
        orb_chase(obj + 0x18, d0, x, 0xFFFE0000, 0x20000, 0x1000, 0x1000);
        orb_chase(obj + 0x1C, d1, y, 0xFFFE0000, 0x20000, 0x1000, 0x1000);
    } else if (n == 2) {
        if (bit8(B5B4_ORDERS, 6)) {
            wr16(obj + O_STATE, 0x10);
            wr16(obj + 0x48, 0x80);
            return;
        }
        if (bit8(B5B4_ORDERS, 2)) {
            orb_circle(obj, 0x80);
            return;
        }
        orb_chase(obj + 0x18, d0, x, 0xFFFD0000, 0x30000, 0x1800, 0x1800);
        orb_chase(obj + 0x1C, d1, y, 0xFFFC0000, 0x40000, 0x2000, 0x2000);
    } else {
        if (bit8(B5B4_ORDERS, 7)) {
            wr16(obj + O_STATE, 8);
            wr16(obj + 0x48, 0xA0);
            wr16(obj + 0x4A, 4);
            return;
        }
        if (bit8(B5B4_ORDERS, 3)) {
            orb_circle(obj, 0);
            return;
        }
        orb_chase(obj + 0x18, d0, x, 0xFFFB0000, 0x50000, 0x2800, 0x2800);
        orb_chase(obj + 0x1C, d1, y, 0xFFFF0000, 0x10000, 0x800, 0x8000);
    }
}

/* loc_061C4A: the chain: the angle list at $9800 all $180, eight links (type $5E8) from $CBC0,
 * their frames from $61CB6, +6 8, $10 ... */
static void orb_chain(void)
{
    for (uint16_t a = 0x9800; a < 0x9840; a = (uint16_t)(a + 2)) wr16(a, 0x180);
    uint16_t a0 = 0xCBC0, d2 = 0;
    uint32_t a1 = 0x061CB6;
    for (int k = 0; k < 8; k++) {
        wr16(a0 + O_TYPE, 0x5E8);
        wr16(a0 + O_FLAGS, 0x8080);
        wr8(a0 + 0x21, 0x10);
        wr32(a0 + 0x2C, 0xFC04FC04);
        wr16(a0 + 0x26, 0x12);
        wr8(a0 + 0x23, 0);
        sprite3(a0, a1);
        a1 += 6;
        for (uint16_t a2 = (uint16_t)(a0 + 0x48); a2 < (uint16_t)(a0 + 0x60); a2 = (uint16_t)(a2 + 4)) wr32(a2, 0);
        d2 = (uint16_t)(d2 + 8);
        wr16(a0 + 6, d2);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $060E0E. Step 2 (orb 6): to the middle ($120, $C8); then rising, the chain made, step 4. */
void Orb5D4Step02(uint16_t obj)
{
    Orb5D4Blink(obj);
    if (dec(obj + 0x48)) {
        next(obj, 2);
        wr16(obj + 0x4A, 2);
        wr32(obj + 0x1C, 0xFFFE0000);
        orb_chain();
        return;
    }
    Orb5D4Seek(obj, 0xC8, 0x120);
}

/* loc_060E90: its angle +$48 (masked) at the head of the list at $9800 (the rest moved down a
 * place), toward (d2, $120) */
static void orb_swing(uint16_t obj, uint16_t d2)
{
    wr16(obj + 0x48, rd16(obj + 0x48) & 0x1FE);
    wr16(0x9800, rd16(obj + 0x48));
    for (uint16_t a1 = 0x9840; a1 != 0x9800; a1 = (uint16_t)(a1 - 2)) wr16(a1, rd16((uint16_t)(a1 - 2)));
    Orb5D4Seek(obj, 0x120, d2);
}

/* $060E3C. Step 4: swinging right (its angle up 6) to x $190 on the floor ($118): step 6. */
void Orb5D4Step04(uint16_t obj)
{
    Orb5D4Blink(obj);
    add16(obj + 0x48, 6);
    if (!below(obj + O_X, 0x190) && !below(obj + O_Y, 0x118)) next(obj, 2);
    orb_swing(obj, 0x1A0);
}

/* $060E64. Step 6: swinging left to x $B0; twice (+$4A), then home. */
void Orb5D4Step06(uint16_t obj)
{
    Orb5D4Blink(obj);
    add16(obj + 0x48, (uint16_t)-6);
    if (below(obj + O_X, 0xB0) && !below(obj + O_Y, 0x118)) {
        if (dec(obj + 0x4A)) {
            Orb5D4Home(obj);
            return;
        }
        back(obj, 2);
    }
    orb_swing(obj, 0xA0);
}

/* $060EB4. Step 8 (orb 0): over the weaker player at y $C8; then still, $80 frames. */
void Orb5D4Step08(uint16_t obj)
{
    Orb5D4Blink(obj);
    if (dec(obj + 0x48)) {
        next(obj, 2);
        wr16(obj + 0x48, 0x80);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        return;
    }
    uint16_t a0 = PlayerWeaker();
    Orb5D4Seek(obj, 0xC8, rd16(a0 + O_X));
}

/* $060EE6. Step $A: raining drops (type $5EC) every frame (a sound every 8th); then again
 * (step 8) $60 frames later, four times (+$4A), then home. */
void Orb5D4Step0A(uint16_t obj)
{
    uint16_t s, t;
    Orb5D4Blink(obj);
    if (dec(obj + 0x48)) {
        if (dec(obj + 0x4A)) {
            Orb5D4Home(obj);
            return;
        }
        back(obj, 2);
        wr16(obj + 0x48, 0x60);
        return;
    }
    if (!(rd16(v_gameFrames) & 7)) QueueSound(0xB6);
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x5EC);
    wr16(s + O_FLAGS, 0x8480);
    wr16(s + 0x0E, 0xC4AB);
    wr16(s + 0x08, 0x200);
    wr16(s + 0x0A, 0xFEF6);
    wr8(s + 0x21, 0x10);
    wr32(s + 0x2C, 0xF010FF01);
    wr16(s + 0x26, 0xC);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    add16(s + O_Y, 0xC);
    wr16(s + 0x1C, 0x10);
    wr8(s + 0x20, 4);
}

/* $060F76. Step $C (orb 4): opposite the weaker player (mirrored about x $120) at y $C0; then
 * a homing shot (type $5F0, vitality $32), rising, step $E. */
void Orb5D4Step0C(uint16_t obj)
{
    uint16_t s, t;
    Orb5D4Blink(obj);
    if (!dec(obj + 0x48)) {
        uint16_t a0 = PlayerWeaker();
        uint16_t d2 = (uint16_t)(-(uint16_t)(rd16(a0 + O_X) - 0x120) + 0x120);
        Orb5D4Seek(obj, 0xC0, d2);
        return;
    }
    next(obj, 2);
    wr16(obj + 0x48, 0x20);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0xFFFF8000);
    QueueSound(0xC7);
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x5F0);
    wr16(s + O_FLAGS, 0x8080);
    wr16(s + 0x0E, 0x844C);
    wr16(s + 0x08, 0xA00);
    wr16(s + 0x0A, 0xF4F4);
    wr8(s + 0x21, 0x30);
    wr8(s + 0x23, 0x20);
    wr32(s + 0x2C, 0xF808F808);
    wr32(s + 0x28, 0xFC04FC04);
    wr16(s + O_VITALITY, 0x32);
    wr16(s + 0x26, 0xB);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    wr16(s + 0x4C, 0x80);
    wr16(s + 0x4E, 0x1B);
    wr8(s + 0x20, 4);
}

/* $06102C. Step $E: then step $C again, five times (+$4A), then home. */
void Orb5D4Step0E(uint16_t obj)
{
    Orb5D4Blink(obj);
    if (!dec(obj + 0x48)) return;
    if (dec(obj + 0x4A)) {
        Orb5D4Home(obj);
        return;
    }
    back(obj, 2);
    wr16(obj + 0x48, 0x80);
}

/* $06104C. Step $10 (orb 2): to ($120, $F0); then a sweep left (step $12) or right ($14) at
 * random, $180 frames. */
void Orb5D4Step10(uint16_t obj)
{
    Orb5D4Blink(obj);
    if (dec(obj + 0x48)) {
        wr16(obj + 0x48, 0x180);
        next(obj, 2);
        if (!bit8(0xFF09, 2)) next(obj, 2);
    }
    Orb5D4Seek(obj, 0xF0, 0x120);
}

/* loc_06108C: sweeping toward (d2, $128), every 4th frame a shot (type $5F8) at d7; home when
 * the frames are out */
static void orb_sweep(uint16_t obj, uint16_t d2, uint16_t d7)
{
    uint16_t s, t;
    if (dec(obj + 0x48)) {
        Orb5D4Home(obj);
        return;
    }
    Orb5D4Seek(obj, 0x128, d2);
    if (!(rd16(v_gameFrames) & 3)) {
        QueueSound(0xC3);
        if (SlotFreeHigh(&s, &t)) {
            wr16(s + O_TYPE, 0x5F8);
            wr16(s + O_X, rd16(obj + O_X));
            wr16(s + O_Y, rd16(obj + O_Y));
            wr16(s + 0x18, d7);
            wr16(s + O_FLAGS, 0x8880);
            wr16(s + 0x0E, 0x8546);
            wr16(s + 0x08, 0x500);
            wr16(s + 0x0A, 0xF8F8);
            wr8(s + 0x21, 0x30);
            wr32(s + 0x2C, 0xFF01F808);
            wr32(s + 0x28, 0xF010E020);
            wr16(s + 0x26, 5);
        }
    }
    Orb5D4Blink(obj);
}

/* $06107C. Step $12: sweeping right to x $19E, firing left. */
void Orb5D4Step12(uint16_t obj)
{
    orb_sweep(obj, 0x19E, 0xFFF6);
}

/* $061086. Step $14: sweeping left to x $A2, firing right. */
void Orb5D4Step14(uint16_t obj)
{
    orb_sweep(obj, 0xA2, 0xA);
}

/* ---- the orbs' shots and the chain ---- */

/* $061108. Type $5E8: a link of orb 6's chain (+6 its place in the angle list at $9800), while
 * the orders' bit 4 holds (else removed): its place from the history of the link before it
 * (+$48-$5E, 2 frames apart), kept in the room (a bounce off its edges: a sound, a spark,
 * hidden that frame), its next place that link's plus the angle's sine and cosine times $20. */
void Chain5E8(uint16_t obj)
{
    Random();
    if (!bit8(B5B4_ORDERS, 4)) {
        set8(obj + O_FLAGS, 4);
        return;
    }
    uint16_t a4 = (uint16_t)(obj - 0x60);
    set8(obj + 0x0E, 3);
    if (!frame_bit(1)) clr8(obj + 0x0E, 3);
    for (int k = 0; k < 10; k++) wr16((uint16_t)(obj + 0x5E - 2 * k), rd16((uint16_t)(obj + 0x5A - 2 * k)));
    wr16(obj + O_Y, rd16(obj + 0x5C));
    wr16(obj + O_X, rd16(obj + 0x5E));
    uint16_t d2 = 0;
    if (!below(obj + O_Y, 0x148)) {
        d2 += 1;
        wr16(obj + O_Y, 0x150);
    }
    if (below(obj + O_X, 0x90)) {
        d2 += 2;
        wr16(obj + O_X, 0x90);
    }
    if (!below(obj + O_X, 0x1B0)) {
        d2 += 4;
        wr16(obj + O_X, 0x1B0);
    }
    set8(obj + O_FLAGS, 7);
    if (d2) {
        clr8(obj + O_FLAGS, 7);
        if (!frame_bit(0)) {
            uint16_t s, t;
            QueueSound(0xCD);
            if (SlotFreeHigh(&s, &t)) {
                wr16(s + O_TYPE, 0x104);
                wr32(s + 0x48, 0x0563AC);
                wr16(s + O_X, rd16(obj + O_X));
                wr16(s + O_Y, rd16(obj + O_Y));
                wr32(s + 0x1C, (((uint32_t)rd16(v_randomSeed) << 2) & 0x3FFFF) + 0xFFFB0000u);
                if (d2 & 4) wr16(s + 0x18, 0xFFFE);
                else if (d2 & 2) wr16(s + 0x18, 2);
                else wr16(s + 0x18, (uint16_t)((rd8(v_randomSeed) & 3) - 2));
            }
        }
    }
    uint16_t d0 = rd16((uint16_t)(0x9800 + rd16(obj + 6)));
    uint32_t d1 = (sx(rom16(SINES + sx(d0))) << 5) + rd32(a4 + O_Y);
    uint32_t dd = (sx(rom16(COSINES + sx(d0))) << 5) + rd32(a4 + O_X);
    wr16(obj + 0x48, (uint16_t)(d1 >> 16));
    wr16(obj + 0x4A, (uint16_t)(dd >> 16));
}

/* $061262. Type $5EC: a drop falling; at the floor (y $140) it bursts into a bouncing spark
 * ($56652, type $104). */
void Drop5EC(uint16_t obj)
{
    if (below(obj + O_Y, 0x140)) return;
    wr16(obj + O_TYPE, 0x104);
    wr32(obj + 0x48, 0x056652);
    wr16(obj + O_Y, 0x140);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x1C, (((uint32_t)rd16(v_randomSeed) << 3) & 0x7FFFF) + 0xFFFA0000u);
    wr16(obj + 0x18, (uint16_t)((rd8(v_randomSeed) & 3) - 2));
}

/* $0612B2. Type $5F0: a homing shot: every 4th frame a trail (type $5F4); above y $80 removed;
 * destroyed or out of the room (y $150, x $90-$1B0) it becomes type $354; else it moves along
 * its angle (+$4C) and for +$4E frames turns 8 toward the weaker player. */
void Homer5F0(uint16_t obj)
{
    uint16_t s, t;
    if (!(rd16(v_gameFrames) & 3) && SlotFreeHigh(&s, &t)) {
        wr16(s + O_TYPE, 0x5F4);
        wr8(s + 0x20, 8);
        wr16(s + 0x4A, 0x1E);
        wr16(s + O_X, rd16(obj + O_X));
        wr16(s + O_Y, rd16(obj + O_Y));
    }
    if (below(obj + O_Y, 0x80)) {
        set8(obj + O_FLAGS, 4);
        return;
    }
    if ((rd16(obj + O_VITALITY) & 0x8000) || bit8(obj + 0x22, 4) || !below(obj + O_Y, 0x150)
        || below(obj + O_X, 0x90) || !below(obj + O_X, 0x1B0)) {
        wr16(obj + O_TYPE, 0x354);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        return;
    }
    uint16_t d2 = rd16(obj + 0x4C);
    add32(obj + O_Y, sx(rom16(SINES + sx(d2))) << 4);
    add32(obj + O_X, sx(rom16(COSINES + sx(d2))) << 4);
    if (dec(obj + 0x4E)) return;
    uint16_t a0 = PlayerWeaker();
    int16_t dx = (int16_t)(rd16(a0 + O_X) - rd16(obj + O_X));
    uint32_t d1 = (uint16_t)(rd16(a0 + O_Y) + 0x10 - rd16(obj + O_Y));   /* only its low word counts */
    d2 = (uint16_t)(((int16_t)Atan2(dx, &d1) >> 7) & 0x1FE);
    d2 = (uint16_t)(d2 - rd16(obj + 0x4C));
    int up;
    if (d2 & 0x8000) up = lt(d2, 0xFF00);
    else up = lt(d2, 0x100);
    if (up) wr16(obj + 0x4C, (uint16_t)((rd16(obj + 0x4C) + 8) & 0x1FE));
    else wr16(obj + 0x4C, (uint16_t)((rd16(obj + 0x4C) - 8) & 0x1FE));
}

/* $0613C2. Type $5F4: the homing shot's trail, smaller every 8 frames ($61540), then removed. */
void Trail5F4(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        add16(obj + 0x4A, (uint16_t)-6);
        if (rd16(obj + 0x4A) & 0x8000) {
            set8(obj + O_FLAGS, 4);
            return;
        }
        wr16(obj + 0x48, 7);
    }
    wr16(obj + O_FLAGS, 0x8080);
    uint16_t d0 = rd16(obj + 0x4A);
    wr16(obj + 0x0E, rom16(0x061540 + sx(d0)));
    wr16(obj + 0x08, rom16(0x061540 + sx((uint16_t)(d0 + 2))));
    wr16(obj + 0x0A, rom16(0x061540 + sx((uint16_t)(d0 + 4))));
}

/* $061408. Type $5F8: a shot; destroyed, hit or out of x $98-$1A8 it bounces back as a spark
 * ($563AC, type $104). */
void Shot5F8(uint16_t obj)
{
    if (!(rd16(obj + O_VITALITY) & 0x8000) && !bit8(obj + 0x22, 4) && below(obj + O_X, 0x1A8)
        && !below(obj + O_X, 0x98)) return;
    wr32(obj + 0x18, neg32(asr32(rd32(obj + 0x18), 2)));
    wr16(obj + O_TYPE, 0x104);
    wr32(obj + 0x48, 0x0563AC);
    wr16(obj + 0x1C, (uint16_t)((rd16(v_randomSeed) & 3) - 2));
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
}

/* $06145E. Type $5DC: an animation list bouncing on the floor (y $148, a random rebound) and
 * stopping at the room's sides (x $9C, $1A4). */
void Bounce5DC(uint16_t obj)
{
    AnimList(obj);
    if (!(rd16(obj + 0x1C) & 0x8000) && !below(obj + O_Y, 0x148)) {
        wr32(obj + 0x1C, neg32(rd32(obj + 0x1C)));
        add32(obj + 0x1C, ((uint32_t)rd16(v_randomSeed) << 3) & 0x7FFFF);
    }
    if (rd16(obj + 0x18) & 0x8000) {
        if (below(obj + O_X, 0x9C)) wr32(obj + 0x18, 0);
    } else if (!below(obj + O_X, 0x1A4)) {
        wr32(obj + 0x18, 0);
    }
}

/* $0614AE. Type $5E0: a shot growing every 5 frames ($61540, five sizes); then a burst
 * ($5639C, type $100), the sprites shaking. */
void Burst5E0(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0x8C00);
        wr8(obj + 0x21, 0x10);
        wr8(obj + 0x23, 0x20);
        wr32(obj + 0x2C, 0xFC04FC04);
        wr16(obj + 0x26, 0x10);
        wr16(obj + 0x48, 0);
        wr16(obj + 0x4A, 0);
    }
    if (!dec(obj + 0x48)) return;
    wr16(obj + 0x48, 4);
    sprite3(obj, 0x061540 + sx(rd16(obj + 0x4A)));
    add16(obj + 0x4A, 6);
    if (below(obj + 0x4A, 0x1E)) return;
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x05639C);
    wr16(obj + O_STATE, 0);
    wr16(v_shakeSprites, 0xA);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr8(obj + 0x21, 0);
    QueueSound(0xC5);
}

/* $06155E. Type $5E4: a wave along the floor, the sprites shaking; its step an offset table at
 * $61574 from $6157A. */
void Wave5E4(uint16_t obj)
{
    wr16(v_shakeSprites, 4);
    call_object(0x06157A + sx(rom16(0x061574 + sx(rd16(obj + O_STATE)))), obj);
}

/* $06157A. Step 0: hidden but harmful. */
void Wave5E4Step00(uint16_t obj)
{
    next(obj, 2);
    wr16(obj + O_FLAGS, 0x800);
    wr8(obj + 0x21, 0x10);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x2C, 0xFC04F808);
    wr16(obj + 0x26, 6);
    Wave5E4Step02(obj);
}

/* $06159E. Step 2: blasts, sparks thrown up and back on even frames; at the room's side
 * (x $98-$1A8) it stops, $20 frames. */
void Wave5E4Step02(uint16_t obj)
{
    uint16_t s, t;
    BlastSound8();
    if (!below(obj + O_X, 0x1A8) || below(obj + O_X, 0x98)) {
        next(obj, 2);
        wr32(obj + 0x18, 0);
        wr16(obj + 0x48, 0x20);
        wr16(v_shakeSprites, 0x10);
        return;
    }
    if (frame_bit(0)) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x104);
    wr32(s + 0x48, 0x056652);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    wr32(s + 0x1C, (((uint32_t)rd16(v_randomSeed) << 2) & 0x3FFFF) + 0xFFFB0000u);
    wr32(s + 0x18, asr32(neg32(rd32(obj + 0x18)), 2));
}

/* $061620. Step 4: sparks thrown toward the room's middle on even frames; then removed. */
void Wave5E4Step04(uint16_t obj)
{
    uint16_t s, t;
    if (dec(obj + 0x48)) {
        set8(obj + O_FLAGS, 4);
        return;
    }
    if (frame_bit(0)) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x104);
    wr32(s + 0x48, 0x05639C);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    wr32(s + 0x1C, (((uint32_t)rd16(v_randomSeed) << 3) & 0x7FFFF) + 0xFFF80000u);
    wr32(s + 0x18, 0x24000);
    if (!below(obj + O_X, 0x120)) wr32(s + 0x18, neg32(rd32(s + 0x18)));
}

/* $06168C. Type $5FC: a piece of the boss $5B4 falling ($4000 a frame); its step an offset
 * table at $616A4 from $616AA. */
void Fall5FC(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    call_object(0x0616AA + sx(rom16(0x0616A4 + sx(rd16(obj + O_STATE)))), obj);
}

/* $0616AA. Step 0: shown in front (its sprite word +$5A), drifting left. */
void Fall5FCStep00(uint16_t obj)
{
    next(obj, 2);
    wr16(obj + O_FLAGS, 0x8480);
    wr16(obj + 0x0E, rd16(obj + 0x5A));
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x8000);
    wr16(obj + 0x08, 0);
    wr16(obj + 0x0A, 0xFCFC);
    wr8(obj + 0x20, 0);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x18, 0xFFFF0000);
    Fall5FCStep02(obj);
}

/* $0616E0. Step 2: at the floor (y $150) it bounces up. */
void Fall5FCStep02(uint16_t obj)
{
    if (below(obj + O_Y, 0x150)) return;
    next(obj, 2);
    wr16(obj + O_Y, 0x150);
    wr32(obj + 0x1C, 0xFFFC0000);
    QueueSound(0xDC);
}

/* $06174C. d7+1 fragments (type $600) from $C980 on, around it from angle d6 (d5 apart) at speed
 * d4 (slowing by 1/128 a frame); a1, a2 the sine and cosine tables (its caller's). Its loop
 * branches back to its own entry. */
void Fall5FCBurst(uint16_t obj, uint16_t d4, uint16_t d5, uint16_t d6, uint16_t d7)
{
    do {
        uint16_t s, t;
        if (!SlotFreeFromC980(&s, &t)) return;
        wr16(s + 0x48, 0x2C);
        wr16(s + O_TYPE, 0x600);
        wr16(s + O_FLAGS, 0x8C80);
        wr16(s + 0x0E, 0xE59C);
        wr16(s + 0x08, 0);
        wr16(s + 0x0A, 0xF9F8);
        wr8(s + 0x20, 4);
        wr16(s + O_X, rd16(obj + O_X));
        wr16(s + O_Y, rd16(obj + O_Y));
        uint32_t d0 = (uint32_t)((int32_t)(int16_t)rom16(SINES + sx(d6)) * (int16_t)d4);
        uint32_t d1 = (uint32_t)((int32_t)(int16_t)rom16(COSINES + sx(d6)) * (int16_t)d4);
        wr32(s + 0x1C, d0);
        wr32(s + 0x18, d1);
        wr32(s + 0x50, asr32(neg32(d0), 7));
        wr32(s + 0x4C, asr32(neg32(d1), 7));
        d6 = (uint16_t)((d6 + d5) & 0x1FE);
    } while (d7-- != 0);
}

/* $061706. Step 4: rising off the floor past y $13C: removed in three rings of fragments (4 at
 * speed 2, 8 at 4, 16 at 6). */
void Fall5FCStep04(uint16_t obj)
{
    if (!below(obj + O_Y, 0x13C)) return;
    set8(obj + O_FLAGS, 4);
    Fall5FCBurst(obj, 2, 0x80, 0, 3);
    Fall5FCBurst(obj, 4, 0x40, 0, 7);
    Fall5FCBurst(obj, 6, 0x20, 0, 15);
}

/* ---- type $614: the ending's director: the figure at $DE80 (a3) and the one at $A800 (a4,
 * both type $544), the camera held ($A958), the status bar's messages. Its step is an offset
 * table at $61D4A from $61D7A; +$48 the step's frames. ---- */

#define SCENE_A3 0xDE80
#define SCENE_A4 0xA800

/* loc_061E5E: every object from $C9E0 vanishes but type $544 */
static void scene_clear(void)
{
    ObjectsVanishFrom(0xC9E0, 0x544, 0);
}

/* $061D32. Type $614, by its step. */
void Scene614(uint16_t obj)
{
    call_object(0x061D7A + sx(rom16(0x061D4A + sx(rd16(obj + O_STATE)))), obj);
}

/* $061D7A. Step 0: the camera held; itself at ($340, $140) ($E5FD8), the figures at $DE80
 * ($35C, $151) and $A800 ($384, $13C); the tiles' jobs ($61E6E), line 1's colours ($61E88) and
 * line 2's; every other object gone. */
void Scene614Step00(uint16_t obj)
{
    next(obj, 2);
    set8(v_cameraHold, 0);
    wr16(obj + O_FLAGS, 0xED00);
    wr16(obj + 0x0E, 0x8B00);
    wr32(obj + 0x08, 0x0E5FD8);
    wr8(obj + 0x20, 8);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_X, 0x340);
    wr16(obj + O_Y, 0x140);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr8(obj + 0x21, 0);
    uint16_t a0 = SCENE_A3;
    wr16(a0 + O_TYPE, 0x544);
    wr16(a0 + O_FLAGS, 0xE100);
    wr16(a0 + 0x0E, 0x8C80);
    look(a0, 0x0E5EDE);
    wr8(a0 + 0x20, 8);
    wr16(a0 + O_X, 0x35C);
    wr16(a0 + O_Y, 0x151);
    wr32(a0 + 0x18, 0);
    wr32(a0 + 0x1C, 0);
    wr8(a0 + 0x21, 0);
    a0 = SCENE_A4;
    wr16(a0 + O_TYPE, 0x544);
    wr16(a0 + O_FLAGS, 0xED00);
    wr16(a0 + 0x0E, 0x8801);
    wr32(a0 + 0x08, 0x0CFDFC);
    wr8(a0 + 0x20, 0x10);
    wr16(a0 + 0xDA, 0x20);
    wr16(a0 + O_X, 0x384);
    wr16(a0 + O_Y, 0x13C);
    uint32_t list = 0x061E6E;
    NextJob(&list);
    for (int k = 0; k < 3; k++) {
        wr16((uint16_t)(0xE372 + 2 * k), rom16(0x061E88 + 2 * (uint32_t)k));
        wr16((uint16_t)(0xE3F2 + 2 * k), rom16(0x061E88 + 2 * (uint32_t)k));
    }
    StageColoursFirst();
    scene_clear();
}

/* $061E8E. Step 2: once the camera is there (bit 7 of $A958): the message $2C, $E0 frames. */
void Scene614Step02(uint16_t obj)
{
    if (!bit8(v_cameraHold, 7)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0xE0);
    wr16(0x80C2, 0x2C);
}

/* $061EAA. Step 4: then the figure at $A800 acts ($D0380), $100 frames. */
void Scene614Step04(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    QueueSound(0x8F);
    next(obj, 2);
    wr16(obj + 0x48, 0x100);
    look(SCENE_A4, 0x0D0380);
    QueueSound(0xDF);
}

/* loc_0622B8: every 4th frame smoke (type $600) from the player in front (player 1 with both
 * playing, else the one further right), 7 to his front, $10 up */
static void scene_smoke(void)
{
    uint16_t d5 = 7, d6 = 0xFFF0, a1, s, t;
    if (rd8(v_players) != 3) a1 = (int16_t)(rd16(0xA410) - rd16(0xA610)) >= 0 ? v_player1 : v_player2;
    else a1 = bit8(v_players, 0) ? v_player1 : v_player2;
    if (rd16(v_gameFrames) & 3) return;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x600);
    wr16(s + O_FLAGS, 0x8C80);
    wr16(s + 0x0E, 0xE59C);
    wr16(s + 0x08, 0);
    wr16(s + 0x0A, 0xF9F8);
    wr8(s + 0x20, 0x80);
    wr16(s + 0x48, 0xC);
    wr16(s + 0x52, 0x4000);
    d6 = (uint16_t)(d6 + rd16(a1 + O_Y));
    wr16(s + O_Y, d6);
    wr16(s + 0x1C, (uint16_t)((rd8(0xFF09) & 1) - 2));
    wr16(s + 0x18, (uint16_t)((rd8(v_randomSeed) & 3) - 2));
    wr16(s + 0x1A, 0x8000);
    if (bit8(a1 + 0x0E, 3)) wr16(s + O_X, (uint16_t)(rd16(a1 + O_X) - d5));
    else wr16(s + O_X, (uint16_t)(d5 + rd16(a1 + O_X)));
}

/* $061EDC. Step 6: at frame $F0 a figure (type $57C) at ($E2, $120); smoke from frame $CF on;
 * then the scene's signal (bit 1 of $A028), $10 frames. */
void Scene614Step06(uint16_t obj)
{
    if (rd16(obj + 0x48) == 0xF0) {
        uint16_t a0 = 0xDFA0;
        wr16(a0 + O_TYPE, 0x57C);
        wr16(a0 + O_STATE, 0);
        wr16(a0 + O_FLAGS, 0x8000);
        wr16(a0 + 0x0E, 0xC6A4);
        wr16(a0 + 0x08, 0x500);
        wr16(a0 + 0x0A, 0xF8F8);
        wr16(a0 + O_X, 0xE2);
        wr16(a0 + O_Y, 0x120);
        wr16(a0 + 0x48, 0x20);
    }
    if (dec(obj + 0x48)) {
        next(obj, 2);
        wr16(obj + 0x48, 0x10);
        set8(v_sceneSignal, 1);
        return;
    }
    if (below(obj + 0x48, 0xD0)) scene_smoke();
}

/* $061F42. Step 8: four sparks (type $600, sprite words $61B14) from ($DC, $130) at the speeds
 * at $61FBC; $3E frames. */
void Scene614Step08(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    QueueSound(0xC3);
    next(obj, 2);
    wr16(obj + 0x48, 0x3E);
    uint16_t a0 = 0xCB00;
    uint32_t a1 = 0x061FBC, a2 = 0x061B14;
    for (int k = 0; k < 4; k++) {
        wr16(a0 + O_TYPE, 0x600);
        wr16(a0 + O_FLAGS, 0x8C80);
        wr16(a0 + 0x0E, rom16(a2));
        a2 += 2;
        wr16(a0 + 0x08, 0);
        wr16(a0 + 0x0A, 0xFCFC);
        wr8(a0 + 0x20, 0x80);
        wr16(a0 + 0x48, 0x3C);
        wr32(a0 + 0x50, 0x2000);
        wr16(a0 + O_X, 0xDC);
        wr16(a0 + O_Y, 0x130);
        wr32(a0 + 0x18, rom32(a1));
        wr32(a0 + 0x1C, rom32(a1 + 4));
        a1 += 8;
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $061FDC. Step $A: then type $608 at $DEE0, $B0 frames. */
void Scene614Step0A(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0xB0);
    wr16(0xDEE0 + O_TYPE, 0x608);
    wr16(0xDEE0 + O_STATE, 0);
    QueueSound(0xDC);
}

/* $062004. Step $C: then its look $E6072, $C frames. */
void Scene614Step0C(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0xC);
    look(obj, 0x0E6072);
}

/* $062024. Step $E: then it jumps up ($E60A4). */
void Scene614Step0E(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(v_shakeSprites, 4);
    look(obj, 0x0E60A4);
    wr16(obj + 0x1C, 0xFFF8);
    wr16(obj + 0x18, 1);
    QueueSound(0xB0);
}

/* $062058. Step $10: above the top it comes down at ($164, $80) ($E613C); the others vanish. */
void Scene614Step10(uint16_t obj)
{
    if (!(rd16(obj + O_Y) & 0x8000)) return;
    next(obj, 2);
    wr16(obj + O_Y, 0x80);
    wr16(obj + O_X, 0x164);
    wr16(obj + 0x1C, 1);
    wr32(obj + 0x18, 0x8000);
    look(obj, 0x0E613C);
    scene_clear();
}

/* $06208E. Step $12: landing at y $C0 ($E616C), $40 frames. */
void Scene614Step12(uint16_t obj)
{
    if (below(obj + O_Y, 0xC0)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x40);
    look(obj, 0x0E616C);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xB1);
}

/* $0620C0. Step $14: $40 frames more. */
void Scene614Step14(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x40);
}

/* $0620D4. Step $16: then the figure at $A800 walks ($CFE68). */
void Scene614Step16(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    look(SCENE_A4, 0x0CFE68);
}

/* $0620EE. Step $18: walking left to x $140 ($CFF98), $60 frames. */
void Scene614Step18(uint16_t obj)
{
    add16(SCENE_A4 + O_X, 0xFFFF);
    if (!below(SCENE_A4 + O_X, 0x140)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x60);
    look(SCENE_A4, 0x0CFF98);
    QueueSound(0xDF);
}

/* $06211C. Step $1A: then type $618 at $DF40 (its first step 4). */
void Scene614Step1A(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(0xDF40 + O_TYPE, 0x618);
    wr16(0xDF40 + O_STATE, 0);
    wr16(0xDF40 + 6, 4);
}

/* $06213C. Step $1C: waiting (the next step comes from type $618). */
void Scene614Step1C(uint16_t obj)
{
    (void)obj;
}

/* $06213E. Step $1E: the figure at $A800 faces right ($D0004), $30 frames. */
void Scene614Step1E(uint16_t obj)
{
    next(obj, 2);
    wr16(obj + 0x48, 0x30);
    clr8(SCENE_A4 + 0x0E, 3);
    look(SCENE_A4, 0x0D0004);
}

/* $06215C. Step $20: then $CFF2C, $40 frames. */
void Scene614Step20(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x40);
    look(SCENE_A4, 0x0CFF2C);
}

/* $06217C. Step $22: then walking ($CFE68). */
void Scene614Step22(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    look(SCENE_A4, 0x0CFE68);
}

/* $062196. Step $24: walking right to x $1E0; the figure at $DE80 then $E5F8C, $40 frames. */
void Scene614Step24(uint16_t obj)
{
    add16(SCENE_A4 + O_X, 1);
    if (below(SCENE_A4 + O_X, 0x1E0)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x40);
    look(SCENE_A3, 0x0E5F8C);
}

/* $0621BC. Step $26: the sprites shaking; then the figure at $A800 runs off left, 16 of type
 * $618 (step $1C) at ($200, $14D). */
void Scene614Step26(uint16_t obj)
{
    wr16(v_shakeSprites, 4);
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr32(SCENE_A4 + 0x18, 0xFFFEE000);
    set8(SCENE_A4 + 0x0E, 3);
    QueueSound(0xA3);
    uint16_t d7 = 15;
    do {
        uint16_t s, t;
        if (!SlotFreeHigh(&s, &t)) return;
        wr16(s + O_TYPE, 0x618);
        wr16(s + 6, 0x1C);
        wr16(s + O_X, 0x200);
        wr16(s + O_Y, 0x14D);
    } while (d7-- != 0);
}

/* $06220E. Step $28: the screen shaking; the figure at $A800 hopping left (the one at $DE80
 * turning as it passes x $120); past x $70 step $2C, $40 frames; landing (y $13C) it hops again
 * ($D009C). */
void Scene614Step28(uint16_t obj)
{
    wr16(v_shakeSprites, 6);
    wr16(v_shakePlane, 6);
    if (below(SCENE_A4 + O_X, 0x120)) set8(SCENE_A3 + 0x0E, 3);
    if (below(SCENE_A4 + O_X, 0x70)) {
        next(obj, 4);
        wr16(obj + 0x48, 0x40);
        return;
    }
    add32(SCENE_A4 + 0x1C, 0x4000);
    if (below(SCENE_A4 + O_Y, 0x13C)) return;
    next(obj, 2);
    look(SCENE_A4, 0x0D009C);
    wr16(SCENE_A4 + 0x1C, 0xFFFC);
    QueueSound(0xB0);
}

/* $062272. Step $2A: rising; at the top back to step $28 ($D00CC). */
void Scene614Step2A(uint16_t obj)
{
    wr16(v_shakeSprites, 6);
    wr16(v_shakePlane, 6);
    if (add32_neg(SCENE_A4 + 0x1C, 0x4000)) return;
    back(obj, 2);
    look(SCENE_A4, 0x0D00CC);
}

/* $06229C. Step $2C: then the camera let go, the figure at $DE80 shown ($E000). */
void Scene614Step2C(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    clr8(v_cameraHold, 7);
    wr16(SCENE_A3 + O_FLAGS, 0xE000);
}

/* $0622B6. Step $2E: the end. */
void Scene614Step2E(uint16_t obj)
{
    (void)obj;
}

/* ---- type $618: the ending's soldiers (a1 the figure at $DE80, a2 the one at $A800): their
 * first step in +6; a table of longs at $62396 ---- */

#define ACTOR_A1 0xDE80
#define ACTOR_A2 0xA800

/* loc_062564: facing right unless moving left */
static void actor_face(uint16_t obj)
{
    if (!(rd16(obj + 0x18) & 0x8000)) set8(obj + 0x0E, 3);
}

/* loc_062580: the jump's frame */
static void actor_jump(uint16_t obj)
{
    wr16(obj + 0x0E, 0xC6DA);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    actor_face(obj);
}

/* $062540. Its walking frames ($62574, by bit 3 of the frame count), facing its way. */
void Actor618Look(uint16_t obj)
{
    sprite3(obj, 0x062574 + (frame_bit(3) ? 0u : 6u));
    actor_face(obj);
}

/* $06237E. Type $618 (the random number stepped first), by its step. */
void Actor618(uint16_t obj)
{
    Random();
    call_object(rom32(0x062396 + sx(rd16(obj + O_STATE))), obj);
}

/* $0623BA. Step 0: its step from +6; shown. */
void Actor618Step00(uint16_t obj)
{
    wr16(obj + O_STATE, rd16(obj + 6));
    wr16(obj + O_FLAGS, 0x8C00);
    wr8(obj + 0x20, 0x80);
    Actor618Look(obj);
}

/* $0623D0. Step 4: from ($1E0, $14D) walking left. */
void Actor618Step04(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_X, 0x1E0);
    wr16(obj + O_Y, 0x14D);
    wr16(obj + 0x18, 0xFFFE);
}

/* $0623E8. Step 8: to x $166; then still $140 frames. */
void Actor618Step08(uint16_t obj)
{
    Actor618Look(obj);
    if (!below(obj + O_X, 0x166)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0x140);
    wr32(obj + 0x18, 0);
}

/* $062406. Step $C: facing left, the figure at $DE80 bobbing with its count; then the director
 * on ($C984), the figure's look $E5F1E, and it jumps away right. */
void Actor618Step0C(uint16_t obj)
{
    Actor618Look(obj);
    clr8(obj + 0x0E, 3);
    if (dec(obj + 0x48)) {
        next(obj, 4);
        add16(0xC984, 2);
        look(ACTOR_A1, 0x0E5F1E);
        clr8(ACTOR_A1 + 0x0E, 3);
        wr16(obj + 0x1C, 0xFFFE);
        wr16(obj + 0x18, 2);
        actor_jump(obj);
        return;
    }
    if (bit8(obj + 0x49, 6)) return;
    wr16(ACTOR_A1 + O_Y, bit8(obj + 0x49, 3) ? 0x152 : 0x151);
}

/* loc_062596: standing */
static void actor_stand(uint16_t obj)
{
    wr16(obj + 0x0E, 0xC6D6);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

/* $062466. Step $10: falling to y $14D: standing, facing right, $60 frames. */
void Actor618Step10(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x4000)) return;
    if (below(obj + O_Y, 0x14D)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0x60);
    set8(obj + 0x0E, 3);
    wr16(obj + O_Y, 0x14D);
    actor_stand(obj);
}

/* $062496. Step $14: then walking right. */
void Actor618Step14(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + 0x18, 2);
}

/* $0624AA. Step $18: walking off ($8A00). */
void Actor618Step18(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8A00);
    Actor618Look(obj);
}

/* $0624B4. Step $1C: walking about (toward the figure at $A800 every 32nd frame, a new speed's
 * fraction); then a random hop. */
void Actor618Step1C(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        next(obj, 4);
        wr32(obj + 0x1C, (((uint32_t)rd16(v_randomSeed) << 2) & 0x3FFFF) - 0x50000);
        actor_jump(obj);
        return;
    }
    Actor618Look(obj);
    if (rd16(v_gameFrames) & 0x1F) return;
    wr16(obj + 0x1A, rd16(v_randomSeed));
    if ((int16_t)(rd16(ACTOR_A2 + O_X) - rd16(obj + O_X)) >= 0) wr16(obj + 0x18, 2);
    else wr16(obj + 0x18, 0xFFFE);
}

/* $0624EC. Step $20: the hop; landing at y $14D, back to step $1C for a random time. */
void Actor618Step20(uint16_t obj)
{
    if (add32_neg(obj + 0x1C, 0x4000)) return;
    if (below(obj + O_Y, 0x14D)) return;
    back(obj, 4);
    wr16(obj + 0x48, rd16(v_randomSeed) & 0x7F);
    wr16(obj + O_Y, 0x14D);
    wr32(obj + 0x1C, 0);
}

/* ---- shared helpers ---- */

/* $05D0D8. It faces the nearer player (+$E bit 3 set when he is to the left). */
void FaceNearest(uint16_t obj)
{
    uint16_t dx, dy, p;
    NearestPlayer(obj, &dx, &dy, &p);
    if (dx & 0x8000) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
}

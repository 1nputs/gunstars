#include "game.h"

/* ---- type $380: a boss on foot (the slot at $A800): it stands, walks, leaps, flies, dashes,
 * fires, lifts what it catches (+$40) and throws it; a player can grab it and throw it. Its step
 * is the word at $9C00 (an offset into the table at $2FA30), its counter the word at $9C02.
 * +$56 its flags (byte $56: bit 7 and bit 5 how it ends, bit 3 done, bits 2-3 of the word its
 * attack; byte $57: bit 0 not to be hit, bit 1 no screen limits, bits 2-3 at the screen's right
 * or left edge, bit 4 no defeat, bit 5 flying), +$58 the hit's frames, +$5A its blinking, +$5C
 * the hits it takes before it is knocked down. Steps $74-$A4 are its entrance, $A8-$E4 its
 * defeats. ---- */

#define W380_STEP    0x9C00      /* word: the step (offset into the table at $2FA30) */
#define W380_TIMER   0x9C02      /* word: the step's counter (byte $9C03 its low byte) */
#define W380_CLOCK   0x9C08      /* word: counts the frames (from a random start) */
#define W380_DRIFT   0x9C14      /* word: bit 1 flying down (byte $9C15) */
#define W380_THROWER 0x9C16      /* word: the player that threw it */
#define W380_SWOOP   0x9C19      /* byte: the swoop's bits (0 turning, 1 rising, 2 and 3 done) */
#define W380_SPEED   0x9C1A      /* long: its x speed kept while it crouches */
#define W380_HUD     0x80C2      /* word: the status bar's event ($10 the boss's bar, $34 text) */
#define W380_HUD_TEXT 0x80C8     /* long: the event's text */
#define W380_HUD_OBJ 0x80D6      /* word: the boss whose vitality the bar shows */
#define W380_PROP    0xDFA0      /* the object it picks up in its entrance */
#define W380_TABLE   0x02FA30u

static void w380_step(uint16_t s) { wr16(W380_STEP, s); }
static void w380_next(void) { wr16(W380_STEP, (uint16_t)(rd16(W380_STEP) + 4)); }
static void w380_timer(uint16_t t) { wr16(W380_TIMER, t); }
/* subq.w #1,($9C02): 1 when it is now below 0 */
static int w380_tick(void)
{
    wr16(W380_TIMER, (uint16_t)(rd16(W380_TIMER) - 1));
    return (rd16(W380_TIMER) & 0x8000) != 0;
}
static void w380_look(uint16_t obj, uint32_t map) { wr32(obj + 0x08, map); wr16(obj + 0x0C, 0); }
static uint16_t w380_seed(void) { return rd16(v_randomSeed); }
static uint16_t w380_frames(void) { return rd16(v_gameFrames); }
/* addi.l #n,$1C(a5): 1 when the y speed is now negative (still rising) */
static int w380_fall(uint16_t obj, uint32_t g)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + g);
    return (rd32(obj + 0x1C) & 0x80000000u) != 0;
}
static void w380_face_left(uint16_t obj) { set8(obj + 0x0E, 3); }
static void w380_face_right(uint16_t obj) { clr8(obj + 0x0E, 3); }
/* the long at +field toward the limit by step: lowered unless already below a negative limit */
static void w380_lower(uint16_t obj, uint16_t field, uint32_t limit, uint32_t step)
{
    if (bit8(obj + field, 7) && (int32_t)(rd32(obj + field) - limit) < 0) return;
    wr32(obj + field, rd32(obj + field) - step);
}
/* raised unless already at or above a positive limit */
static void w380_raise(uint16_t obj, uint16_t field, uint32_t limit, uint32_t step)
{
    if (!bit8(obj + field, 7) && (int32_t)(rd32(obj + field) - limit) >= 0) return;
    wr32(obj + field, rd32(obj + field) + step);
}

/* $02A2AA. A chance: C set when the frame count's (halved) low 5 bits plus the aggression ($80C0)
 * are below the random's low byte. */
int FrameChance(void)
{
    uint16_t d6 = (uint16_t)((((int16_t)w380_frames() >> 1) & 0x1F) + rd16(0x80C0));
    uint16_t d7 = rd8(0xFF09);
    return d6 < d7;
}

/* $02A1C2. Line 2's three colours from the first stage's list. */
void StageColoursFirst(void)
{
    StageColoursCopy2(rom32(0x02A1F4));
}

/* $03074A. Its y: $10 above the camera's line ($348 less $A90C). */
void Boss380RideY(uint16_t obj)
{
    SetYAboveCamera348(obj, 0x10);
}

/* $030B76. In front on frames with bit 1 set, behind on the others. */
void Boss380Priority(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 1)) set8(obj + 0x0E, 4);
    else clr8(obj + 0x0E, 4);
}

/* $030AF0. Every 4th frame a spark (type $2CC) about its middle, thrown back over its shoulder
 * at a random angle at half speed; then its priority by the frame. */
void Boss380Sparks(uint16_t obj)
{
    uint16_t slot, t;
    if (!(w380_frames() & 3) && SlotFreeHigh(&slot, &t)) {
        wr16(slot + O_TYPE, 0x2CC);
        wr32(slot + 0x48, 0x05663A);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) - 7));
        wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + (w380_seed() & 0xF)));
        uint16_t d0 = bit8(obj + 0x0E, 3) ? 0xFFC0 : 0xC0;
        d0 = (uint16_t)((d0 + (w380_seed() & 0x78)) & 0x1FE);
        int32_t d1 = (int16_t)rom16(0x02977C + d0), d2 = (int16_t)rom16(0x0297FC + d0);
        wr32(slot + 0x5C, (uint32_t)(d1 >> 1));
        wr32(slot + 0x58, (uint32_t)(d2 >> 1));
    }
    Boss380Priority(obj);
}

/* $030A4E. The beam's 16 parts (from $C9E0) removed. */
void Boss380BeamClear(void)
{
    uint16_t a = 0xC9E0;
    for (int k = 0; k < 16; k++, a = (uint16_t)(a + 0x60))
        if (rd16(a)) wr16(a + O_FLAGS, 0x1000);
}

/* $030A6C. Down: harmless, still, its blinking for good, facing the screen's middle; $60 frames. */
void Boss380Down(uint16_t obj)
{
    wr8(obj + 0x20, 4);
    clr8(obj + 0x0E, 4);
    wr16(obj + 0x5A, 0xFFFF);
    w380_timer(0x60);
    wr8(obj + 0x21, 0);
    wr16(obj + O_VITALITY, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x08, 0x0D003C);
    wr16(obj + 0x0C, 0);
    if ((int16_t)(rd16(obj + O_X) - 0x120) < 0) w380_face_left(obj);
    else w380_face_right(obj);
}

/* $030ABC. Down in the air: knocked up and away from the middle. */
void Boss380DownFlying(uint16_t obj)
{
    Boss380Down(obj);
    wr32(obj + 0x1C, 0xFFFD8000);
    QueueSound(0xA5);
    if ((int16_t)(rd16(obj + O_X) - 0x120) < 0) wr32(obj + 0x18, 0x10000);
    else wr32(obj + 0x18, 0xFFFF0000);
}

/* $030D2A. Blinking for a time by the difficulty; 7 to 10 hits before the next knock-down. */
void Boss380Blink(uint16_t obj)
{
    wr16(obj + 0x5A, rom16(0x031480 + (uint32_t)(int16_t)rd16(v_difficulty)));
    wr16(obj + 0x5C, (uint16_t)((w380_frames() & 3) + 7));
}

/* $030C7C. Standing again ($9C00 $10): in front of the background, hurting, 7 hits. */
void Boss380LandStand(uint16_t obj)
{
    w380_step(0x10);
    clr8(obj + 0x0E, 4);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + 0x20, 4);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x23, 0x20);
    wr16(obj + 0x5C, 7);
    wr8(obj + 0x57, rd8(obj + 0x57) & 0x20);
}

/* $030CC2. The same, its +$23 left as it is. */
void Boss380StandStart(uint16_t obj)
{
    w380_step(0x10);
    clr8(obj + 0x0E, 4);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + 0x20, 4);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x21, 0x22);
    wr16(obj + 0x5C, 7);
    wr8(obj + 0x57, rd8(obj + 0x57) & 0x20);
}

/* loc_030CB4: $10 frames standing */
static void w380_stand_after(uint16_t obj)
{
    w380_timer(0x10);
    wr32(obj + 0x08, 0x0CFF98);
    Boss380StandStart(obj);
}

/* $030962. Facing the nearer player; its distance (|dx|) returned. */
uint16_t Boss380FacePlayer(uint16_t obj)
{
    uint16_t dx, dy, player;
    uint16_t d0 = NearestPlayer(obj, &dx, &dy, &player);
    if (dx & 0x8000) w380_face_left(obj);
    else w380_face_right(obj);
    return d0;
}

/* $03127C. Its sound $DF every $80 frames. */
void Boss380Grunt(void)
{
    if (w380_frames() & 0x7F) return;
    QueueSound(0xDF);
}

/* loc_03122A: every 4th frame a puff (type $100, animation in d7) at its middle, drifting
 * backward at d1; d0 is returned as the 68k leaves it (the frame's bits, the slot search's type,
 * or the random's 0-7) */
static uint8_t w380_smoke(uint16_t obj, uint32_t anim, uint16_t d1)
{
    uint16_t d0 = w380_frames() & 3;
    if (d0) return (uint8_t)d0;
    uint16_t slot;
    if (!SlotFreeHigh(&slot, &d0)) return (uint8_t)d0;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, anim);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    d0 = w380_seed() & 7;
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) - 8));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + d0));
    if (!bit8(obj + 0x0E, 3)) d1 = (uint16_t)-d1;
    wr16(slot + 0x18, d1);
    return (uint8_t)d0;
}

/* $031216. Puffs behind it, drifting at 1. */
void Boss380SmokeSlow(uint16_t obj)
{
    w380_smoke(obj, 0x0563F6, 1);
}

/* $031222. Puffs drifting at 4; d0 (its bit 0 picks the bob's way) as the 68k leaves it. */
uint8_t Boss380Smoke(uint16_t obj)
{
    return w380_smoke(obj, 0x0563F6, 4);
}

/* $031108. Flying across: turning at the screen's sides ($B0, $190), sometimes (every $40th
 * frame by the random) between; its x speed toward 4.25 its way by 3/16. */
void Boss380FlyX(uint16_t obj)
{
    int left;
    uint16_t x = rd16(obj + O_X);
    if ((int16_t)(x - 0xB0) < 0) left = 0;
    else if ((int16_t)(x - 0x190) >= 0) left = 1;
    else if (!(w380_frames() & 0x3F)) left = !bit8(0xFF09, 2);
    else left = bit8(obj + 0x0E, 3);
    if (left) {
        w380_face_left(obj);
        w380_lower(obj, 0x18, 0xFFFBC000, 0x3000);
    } else {
        w380_face_right(obj);
        w380_raise(obj, 0x18, 0x44000, 0x3000);
    }
}

/* $03107A. Flying across and bobbing: every $20 frames down above y $C8, up below $F0, else by
 * the random; its y speed toward 1.125 that way by 3/32. */
void Boss380FlyY(uint16_t obj)
{
    Boss380FlyX(obj);
    int down;
    if (!(w380_frames() & 0x1F)) {
        if ((int16_t)(rd16(obj + O_Y) - 0xC8) < 0) {
            wr16(W380_DRIFT, rd16(W380_DRIFT) | 2);
            down = 1;
        } else if ((int16_t)(rd16(obj + O_Y) - 0xF0) >= 0) {
            wr16(W380_DRIFT, rd16(W380_DRIFT) & 0xFFFD);
            down = 0;
        } else {
            wr16(W380_DRIFT, rd16(W380_DRIFT) | (w380_seed() & 2));
            down = bit8(W380_DRIFT + 1, 1);
        }
    } else {
        down = bit8(W380_DRIFT + 1, 1);
    }
    if (down) w380_raise(obj, 0x1C, 0x12000, 0x1800);
    else w380_lower(obj, 0x1C, 0xFFFEE000, 0x1800);
}

/* $03118C. Hovering: its x speed toward 0 by 1/2; up (by 3/8) below y $150 and while rising
 * above $90, down otherwise (and when y is negative). */
void Boss380Hover(uint16_t obj)
{
    uint32_t vx = rd32(obj + 0x18);
    if (!(vx & 0x80000000u)) {
        if ((int32_t)(vx - 0x8000) < 0) wr32(obj + 0x18, 0);
        else wr32(obj + 0x18, vx - 0x8000);
    } else {
        if ((int32_t)(vx - 0xFFFF8000u) >= 0) wr32(obj + 0x18, 0);
        else wr32(obj + 0x18, vx + 0x8000);
    }
    int up;
    uint16_t y = rd16(obj + O_Y);
    if (bit8(obj + O_Y, 7)) up = 0;
    else if ((int16_t)(y - 0x150) >= 0) up = 1;
    else if ((int16_t)(y - 0x90) < 0) up = 0;
    else up = !bit8(obj + 0x1C, 7);
    if (up) wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x6000);
    else wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x6000);
}

/* $030D4A. Thrown: pulled toward the player that threw it (twice the angle's sine and cosine on
 * its speeds); its y kept between $90 and $140. */
void Boss380Home(uint16_t obj)
{
    uint16_t p = rd16(W380_THROWER);
    int16_t dx = (int16_t)(rd16(p + O_X) - rd16(obj + O_X));
    uint32_t d1 = (uint16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
    uint16_t d2 = (uint16_t)(((int16_t)Atan2(dx, &d1) >> 7) & 0x1FE);
    uint32_t sy = (uint32_t)(int32_t)(int16_t)rom16(0x02977C + d2) << 1;
    uint32_t sx = (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + d2) << 1;
    wr32(obj + 0x18, rd32(obj + 0x18) + sx);
    uint16_t y = rd16(obj + O_Y);
    if ((int16_t)(y - 0x140) >= 0) wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x6000);
    else if ((int16_t)(y - 0x90) < 0) wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x6000);
    else wr32(obj + 0x1C, rd32(obj + 0x1C) + sy);
}

/* $030E46. The swoop's x: facing left it slows and turns back right of $190 (going on to $1A0),
 * facing right left of $B0 (on to $A0); its x speed within 2 by 1/16. */
void Boss380SwoopX(uint16_t obj)
{
    uint16_t x = rd16(obj + O_X);
    int up;                                 /* the speed raised (rightward) */
    if (bit8(obj + 0x0E, 3)) {
        if (!bit8(W380_SWOOP, 0)) {
            if ((int16_t)(x - 0x190) < 0) { set8(W380_SWOOP, 0); up = 1; }
            else up = 0;
        } else if ((int16_t)(x - 0x1A0) < 0) {
            up = 1;
        } else {
            clr8(W380_SWOOP, 0);
            set8(W380_SWOOP, 2);
            up = 0;
        }
    } else {
        if (!bit8(W380_SWOOP, 0)) {
            if ((int16_t)(x - 0xB0) >= 0) { set8(W380_SWOOP, 0); up = 0; }
            else up = 1;
        } else if ((int16_t)(x - 0xA0) >= 0) {
            up = 0;
        } else {
            clr8(W380_SWOOP, 0);
            set8(W380_SWOOP, 2);
            up = 1;
        }
    }
    if (up) w380_raise(obj, 0x18, 0x20000, 0x1000);
    else w380_lower(obj, 0x18, 0xFFFE0000, 0x1000);
}

/* $030DB8. The swoop: across (above), then down to $C above its player's y and back up past 4
 * below it; its y speed within 1 by 1/16. */
void Boss380SwoopY(uint16_t obj)
{
    Boss380SwoopX(obj);
    uint16_t p = PlayerTarget();
    uint16_t d0 = rd16(p + O_Y);
    if (!bit8(W380_SWOOP, 1)) {
        d0 = (uint16_t)(d0 - 0xC);
        if ((int16_t)(d0 - rd16(obj + O_Y)) >= 0) {
            set8(W380_SWOOP, 1);
            set8(W380_SWOOP, 3);
            wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x1000);
            return;
        }
        w380_lower(obj, 0x1C, 0xFFFF0000, 0x1000);
        return;
    }
    d0 = (uint16_t)(d0 - 4);
    if ((int16_t)(d0 - rd16(obj + O_Y)) < 0) {
        clr8(W380_SWOOP, 1);
        set8(W380_SWOOP, 3);
        wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x1000);
        return;
    }
    w380_raise(obj, 0x1C, 0x10000, 0x1000);
}

/* $030330. A shot (type d2) at x d3, 8 above it, at speed d1, its +$48 d4. */
void Boss380Shot(uint16_t obj, uint32_t vx, uint16_t type, uint16_t x, uint16_t arg)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, type);
    wr16(slot + 0x48, arg);
    wr16(slot + O_X, x);
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) - 8));
    wr32(slot + 0x18, vx);
}

/* loc_03006A: it reaches for the player ($9C00 $50): its grab box */
static void w380_reach(uint16_t obj)
{
    w380_step(0x50);
    wr16(obj + 0x56, 0x6001);
    wr8(obj + 0x21, 0x23);
    wr16(obj + 0x40, 0);
    wr32(obj + 0x18, 0);
}

/* loc_031032: a leap ($9C00 $64) when the chance allows */
static void w380_leap(uint16_t obj)
{
    if (FrameChance()) return;
    w380_step(0x64);
    wr32(obj + 0x1C, 0xFFFA8000);
    wr32(obj + 0x08, 0x0CFF04);
    set8(obj + 0x57, 0);
}

/* loc_03105A: a jump ($9C00 $14) */
static void w380_jump(uint16_t obj)
{
    w380_step(0x14);
    wr32(obj + 0x1C, 0xFFFCC000);
    wr32(obj + 0x08, 0x0CFF04);
    set8(obj + 0x57, 0);
}

/* loc_030FF8: firing ($9C00 $70): still, $30 frames */
static void w380_fire(uint16_t obj)
{
    w380_step(0x70);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x08, 0x0D0380);
    wr16(obj + 0x0C, 0);
    set8(obj + 0x57, 0);
    w380_timer(0x30);
}

/* loc_030FDE: firing every $40 frames */
static void w380_fire_40(uint16_t obj)
{
    if (!(w380_frames() & 0x3F)) w380_fire(obj);
}

/* loc_030FEC: every $80 */
static void w380_fire_80(uint16_t obj)
{
    if (!(w380_frames() & 0x7F)) w380_fire(obj);
}

/* the farthest distance it acts at, by the level ($A20C): d0 below it does nothing */
static int w380_too_far(uint16_t d0)
{
    return (int16_t)(d0 - rom16(0x031022 + (uint32_t)(int16_t)rd16(0xA20C))) < 0;
}

/* loc_030F40: walking back, the player d0 away: its attack by +$56 (fire, more often while its
 * vitality is high), else a grab close, a leap, or fire / a jump by the frame count */
static void w380_after_retreat(uint16_t obj, uint16_t d0)
{
    if (rd16(obj + 0x56) & 0xC) {
        if ((int16_t)(rd16(obj + O_VITALITY) - 0x800) < 0) w380_fire(obj);
        else w380_fire_40(obj);
        return;
    }
    if ((int16_t)(d0 - 0x1A) < 0) { w380_reach(obj); return; }
    if ((int16_t)(d0 - 0x40) < 0) return;
    if ((int16_t)(d0 - 0x50) < 0) { w380_leap(obj); return; }
    if (w380_too_far(d0)) return;
    uint16_t d1 = w380_frames() & 0x7FF;
    if ((int16_t)(d1 - 0x400) >= 0) { w380_fire_80(obj); return; }
    if ((int16_t)(d1 - 0x180) < 0) { w380_jump(obj); return; }
    w380_fire_40(obj);
}

/* loc_030FA0: walking toward the player d0 away: a grab close, a leap, fire every $80 frames in
 * the frame count's second half */
static void w380_after_approach(uint16_t obj, uint16_t d0)
{
    if ((int16_t)(d0 - 0x1A) < 0) { w380_reach(obj); return; }
    if ((int16_t)(d0 - 0x58) < 0) return;
    if ((int16_t)(d0 - 0x60) < 0) { w380_leap(obj); return; }
    if (w380_too_far(d0)) return;
    uint16_t d1 = w380_frames() & 0x7FF;
    if ((int16_t)(d1 - 0x400) < 0) return;
    w380_fire_80(obj);
}

/* loc_03097E: the beam: 16 parts (type $10 from $C9E0) from beside it, each faster than the
 * last; the last one hurts */
static void w380_beam(uint16_t obj)
{
    uint16_t d0 = rd16(obj + O_X);
    uint16_t d1 = (uint16_t)(rd16(obj + O_Y) + 0x10);
    uint32_t d4 = 0x4000, d2;
    uint16_t d3;
    if (!bit8(obj + 0x0E, 3)) {
        d0 = (uint16_t)(d0 + 0x14);
        d2 = 0x4000;
        d3 = 0x481F;
    } else {
        d0 = (uint16_t)(d0 - 0x14);
        d2 = 0xFFFFBFFF;
        d3 = 0x401F;
    }
    uint16_t a = 0xC9E0;
    uint32_t d5 = 0, d6 = 0;
    for (int k = 0; k < 16; k++) {
        wr16(a + O_TYPE, 0x10);
        wr16(a + O_FLAGS, 0x8C00);
        wr8(a + 0x20, 4);
        wr16(a + 0x0E, d3);
        wr16(a + 0x0E, (uint16_t)(rd16(a + 0x0E) - 1));
        wr16(a + 0x08, 0);
        wr16(a + 0x0A, 0xFCFC);
        wr16(a + 0x0C, 0);
        wr8(a + 0x21, 0);
        wr16(a + O_X, d0);
        wr16(a + O_Y, d1);
        d5 += d2;
        wr32(a + 0x18, d5);
        d6 += d4;
        wr32(a + 0x1C, d6);
        a = (uint16_t)(a + 0x60);
    }
    a = (uint16_t)(a - 0x60);
    wr16(a + 0x0E, d3);
    wr8(a + 0x20, 0);
    wr16(a + 0x08, 0x100);
    wr16(a + 0x0A, 0xFCF3);
    wr8(a + 0x21, 0x30);
    wr8(a + 0x23, 0x20);
    wr32(a + 0x2C, 0xFC04FC04);
    wr32(a + 0x28, 0xF808F808);
    wr16(a + 0x26, 9);
}

/* loc_030B90: flying with players in the game: now and then (bit 2 of the clock's high byte, or
 * 1 in $200) a swoop ($9C00 $38); else every $40 frames, low enough, a dash's shot (type $384)
 * ahead of it ($9C00 $34) */
static void w380_fly_attack(uint16_t obj)
{
    if (!rd8(v_players)) return;
    if (!bit8(obj + 0x56, 7) && (bit8(W380_CLOCK, 2) || !(w380_seed() & 0x1FF))) {
        w380_step(0x38);
        wr8(obj + 0x21, 0x20);
        set8(W380_SWOOP, 1);
        clr8(W380_SWOOP, 2);
        clr8(W380_SWOOP, 3);
        set8(obj + 0x57, 0);
        wr16(W380_CLOCK, 0);
        if ((int16_t)(rd16(obj + O_X) - 0x120) < 0) w380_face_right(obj);
        else w380_face_left(obj);
        return;
    }
    if (rd16(W380_CLOCK) & 0x3F) return;
    if ((int16_t)(rd16(obj + O_Y) - 0x90) < 0) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    w380_next();
    w380_timer(4);
    wr32(obj + 0x08, 0x0D0424);
    wr16(obj + 0x0C, 0);
    wr16(slot + O_TYPE, 0x384);
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_X, rd16(obj + O_X));
    if (!bit8(obj + 0x0E, 3)) {
        wr32(slot + 0x18, 0xA000);
        wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + 0x18));
    } else {
        wr32(slot + 0x18, 0xFFFF6000);
        wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) - 0x18));
    }
}

/* loc_0305B6: the defeated look, by bit 2 of the counter's low byte */
static void w380_down_look(uint16_t obj)
{
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x08, bit8(W380_TIMER + 1, 2) ? 0x0D009C : 0x0D003C);
}

/* loc_030518: while the prop is there, it falls */
static void w380_prop_fall(void)
{
    if (rd16(W380_PROP)) wr32(W380_PROP + 0x1C, rd32(W380_PROP + 0x1C) + 0x4000);
}

/* ---- the steps ---- */

/* $02FB18. The set-up: its look, place ($1E8, $13C), vitality $896, boxes; the entrance next
 * ($9C00 $74); the clock from the random, the drift and the swoop's bits cleared. */
void Boss380Setup(uint16_t obj)
{
    w380_step(0x74);
    wr16(obj + 0x56, 0x4022);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(obj + 0x0E, 0x801);
    wr16(obj + 0xDA, 0x20);
    w380_look(obj, 0x0CFE68);
    wr16(obj + O_Y, 0x13C);
    wr16(obj + O_X, 0x1E8);
    wr16(obj + O_VITALITY, 0x896);
    wr8(obj + 0x20, 0x90);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x2C, 0xF808EE12);
    wr32(obj + 0x28, 0xF410F010);
    wr16(obj + 0x26, 0x12);
    wr16(obj + 0x5C, 7);
    wr16(W380_CLOCK, w380_seed());
    wr16(W380_DRIFT, 0);
    wr16(0x9C18, 0);
}

/* $02FBC0. Standing: close to the nearer player it grabs; else when the counter runs out it walks
 * toward it ($5C) or away ($60) by its vitality and the frame count. Flying (byte $57 bit 5): it
 * hovers, puffing, then flies ($30); its look bobs. */
void Boss380Stand(uint16_t obj)
{
    if (bit8(obj + 0x57, 5)) {
        Boss380Grunt();
        Boss380Hover(obj);
        if (w380_tick()) {
            w380_step(0x30);
            w380_look(obj, 0x0D03F4);
        }
        if (!bit8(v_gameFrames + 1, 1)) wr32(obj + 0x1C, 0xFFFF4000);
        else wr32(obj + 0x1C, 0xC000);
        return;
    }
    uint16_t d0 = Boss380FacePlayer(obj);
    if (!(d0 & 0x8000) && (int16_t)(d0 - 0x1A) < 0) { w380_reach(obj); return; }
    if (!w380_tick()) return;
    w380_look(obj, 0x0CFE68);
    if ((int16_t)(rd16(obj + O_VITALITY) - 0x320) >= 0
        && (!bit8(v_gameFrames, 3) || !(w380_frames() & 0x1FF))) {
        w380_step(0x5C);
        return;
    }
    w380_step(0x60);
}

/* $02FC64. Hit while flying: flying on (puffs) for +$58's frames, then hovering. */
void Boss380HitFlying(uint16_t obj)
{
    Boss380FlyY(obj);
    Boss380SmokeSlow(obj);
    if (rd16(obj + 0x58) == 2) w380_look(obj, 0x0D003C);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 1));
    if (!(rd16(obj + 0x58) & 0x8000)) return;
    w380_step(0x10);
    w380_timer(0xFFFF);
    w380_look(obj, 0x0D03F4);
    wr16(W380_CLOCK, (uint16_t)(rd16(W380_CLOCK) + 4));
}

/* $02FCAC. Hit: +$58's frames, then standing. */
void Boss380Hit(uint16_t obj)
{
    if (rd16(obj + 0x58) == 1) w380_look(obj, 0x0D0004);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 1));
    if (!(rd16(obj + 0x58) & 0x8000)) return;
    w380_step(0x10);
    w380_timer(0xFFFF);
    w380_look(obj, 0x0CFF98);
}

/* $02FCE8. Knocked up: falling; flying, it hovers again when the counter runs out; on foot it
 * lands at y $13C and stands. */
void Boss380Knocked(uint16_t obj)
{
    if (w380_fall(obj, 0x4000)) return;
    if (bit8(obj + 0x57, 5)) {
        if (!w380_tick()) return;
        w380_step(0x10);
        w380_timer(0xFFFF);
        w380_look(obj, 0x0D03F4);
        return;
    }
    if ((int16_t)(rd16(obj + O_Y) - 0x13C) < 0) return;
    w380_step(0x10);
    w380_timer(0xC);
    wr16(obj + O_Y, 0x13C);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    w380_look(obj, 0x0CFF2C);
}

/* $02FD5E. Grabbed by a player: while held nothing; let go (not thrown up), it drops at 4; then
 * thrown, facing its flight. */
void Boss380Grabbed(uint16_t obj)
{
    if (bit8(obj + 0x23, 1)) { clr8(obj + 0x23, 1); return; }
    if (bit8(obj + 0x23, 4)) clr8(obj + 0x23, 4);
    else wr32(obj + 0x1C, 0xFFFC0000);
    w380_next();
    if (bit8(obj + 0x18, 7)) w380_face_right(obj);
    else w380_face_left(obj);
}

/* $02FD98. Thrown: falling until y $13C: then bouncing up ($9C00 $14), hurting players, $17C
 * vitality lost, its x speed halved, blinking. */
void Boss380Thrown(uint16_t obj)
{
    if (w380_fall(obj, 0x4000)) return;
    if ((int16_t)(rd16(obj + O_Y) - 0x13C) < 0) return;
    w380_step(0x14);
    wr8(obj + 0x21, 0x22);
    clr8(obj + 0x57, 0);
    wr16(obj + O_Y, 0x13C);
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0x17C));
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x1C, 0xFFFCDC00);
    w380_look(obj, 0x0D009C);
    Boss380Blink(obj);
}

/* $02FDF2. Grabbed while flying: the same; once let go, $10 frames of flight back. */
void Boss380GrabbedFlying(uint16_t obj)
{
    Boss380Grabbed(obj);
    if (rd16(W380_STEP) != 0x24) w380_timer(0x10);
}

/* $02FE08. Thrown while flying: pulled toward the thrower in its last $C frames; then tumbling
 * ($2C), hurting players. */
void Boss380ThrownFlying(uint16_t obj)
{
    if ((int16_t)(rd16(W380_TIMER) - 0xC) < 0) Boss380Home(obj);
    if (!w380_tick()) return;
    w380_next();
    w380_timer(0x1A);
    wr8(obj + 0x21, 0x30);
    wr16(obj + 0x26, 0x12);
    w380_look(obj, 0x0D0424);
}

/* $02FE44. Tumbling toward the thrower, sparks behind it; then standing, turned. */
void Boss380Tumble(uint16_t obj)
{
    Boss380Grunt();
    Boss380Home(obj);
    Boss380Sparks(obj);
    if (!w380_tick()) return;
    w380_timer(0x20);
    wr32(obj + 0x08, 0x0CFF04);
    Boss380LandStand(obj);
    wr16(obj + 0x0E, rd16(obj + 0x0E) ^ 0x800);
}

/* $02FE72. Rising, until it falls. */
void Boss380Rise(uint16_t obj)
{
    if (w380_fall(obj, 0x4000)) return;
    w380_next();
}

/* $02FE84. Falling, until y $13C: standing. */
void Boss380Fall(uint16_t obj)
{
    w380_fall(obj, 0x4000);
    if ((int16_t)(rd16(obj + O_Y) - 0x13C) < 0) return;
    w380_step(0x10);
    wr16(obj + O_Y, 0x13C);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    w380_timer(4);
    clr8(obj + 0x57, 0);
    wr16(obj + 0x58, 0);
    wr8(obj + 0x21, 0x22);
    w380_look(obj, 0x0CFF98);
}

/* $02FEDA. Flying: for $3F frames of every $100 it bobs by 2 (puffing), else it puffs slowly;
 * across and up or down; then its attacks. */
void Boss380Fly(uint16_t obj)
{
    Boss380Grunt();
    if ((int16_t)((w380_frames() & 0xFF) - 0x3F) < 0) {
        if (Boss380Smoke(obj) & 1) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 2));
        else wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 2));
    } else {
        Boss380SmokeSlow(obj);
    }
    Boss380FlyY(obj);
    w380_fly_attack(obj);
}

/* $02FF16. After the dash's shot: 4 frames, then flying again ($30). */
void Boss380FlyHold(uint16_t obj)
{
    if (w380_tick()) {
        wr16(W380_STEP, (uint16_t)(rd16(W380_STEP) - 4));
        w380_look(obj, 0x0D03F4);
    }
    Boss380Grunt();
    Boss380SmokeSlow(obj);
    Boss380FlyY(obj);
}

/* $02FF3C. The swoop, puffing; when both its turns are done, a dash: hurting, for a time by the
 * level. */
void Boss380Swoop(uint16_t obj)
{
    Boss380Smoke(obj);
    Boss380Priority(obj);
    Boss380SwoopY(obj);
    if ((rd16(0x9C18) & 0xC) != 0xC) return;
    w380_next();
    wr8(obj + 0x21, 0x30);
    wr8(obj + 0x20, 0xBC);
    wr16(obj + 0x26, 0x14);
    w380_timer(rom16(0x02FF80 + (uint32_t)(int16_t)rd16(0xA20C)));
}

/* $02FF90. Swooping on, sparks behind it; then still for $C frames. */
void Boss380SwoopSparks(uint16_t obj)
{
    Boss380Sparks(obj);
    Boss380SwoopY(obj);
    if (!w380_tick()) return;
    w380_next();
    w380_timer(0xC);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x18, 0);
}

/* $02FFBC. Sparks; then the dash, with its sound. */
void Boss380Charge(uint16_t obj)
{
    Boss380Sparks(obj);
    if (!w380_tick()) return;
    w380_next();
    QueueSound(0xB9);
}

/* $02FFD6. Dashing its way at 6 until the screen's edge: then standing, turned. */
void Boss380Dash(uint16_t obj)
{
    Boss380Sparks(obj);
    if (!bit8(obj + 0x0E, 3)) {
        if (!bit8(obj + 0x57, 2)) { wr32(obj + 0x18, 0x60000); return; }
    } else {
        if (!bit8(obj + 0x57, 3)) { wr32(obj + 0x18, 0xFFFA0000); return; }
    }
    w380_timer(0x20);
    wr32(obj + 0x08, 0x0CFF04);
    Boss380LandStand(obj);
    wr16(obj + 0x0E, rd16(obj + 0x0E) ^ 0x800);
}

/* $030026. A leap: up at 4, forward at 1/2. */
void Boss380Leap(uint16_t obj)
{
    w380_next();
    w380_look(obj, 0x0CFF04);
    wr32(obj + 0x18, 0x8000);
    wr32(obj + 0x1C, 0xFFFC0000);
}

/* $03004A. Rising, until it falls: then standing ($20 frames), hurting players. */
void Boss380LeapRise(uint16_t obj)
{
    if (w380_fall(obj, 0x4000)) return;
    w380_step(0x10);
    wr8(obj + 0x21, 0x22);
    w380_timer(0x20);
}

/* $03008C. The grab: nothing caught (+$40 0), a leap by the chance; something caught: lifted
 * over its head (held by it), both still, $A frames, facing the screen's middle. */
void Boss380Lift(uint16_t obj)
{
    uint16_t held = rd16(obj + 0x40);
    if (!held) {
        wr16(obj + 0x56, 0x6000);
        wr8(obj + 0x21, 0x22);
        w380_leap(obj);
        return;
    }
    w380_next();
    wr8(obj + 0x21, 1);
    set8(held + 0x23, 1);
    wr16(held + O_X, rd16(obj + O_X));
    wr16(held + O_Y, rd16(obj + O_Y));
    wr16(held + O_Y, (uint16_t)(rd16(held + O_Y) + 8));
    wr32(held + 0x18, 0);
    wr32(held + 0x1C, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    w380_look(obj, 0x0D013C);
    w380_timer(0xA);
    wr8(obj + 0x20, 0x90);
    QueueSound(0xDF);
    if ((int16_t)(rd16(obj + O_X) - 0x120) < 0) w380_face_right(obj);
    else w380_face_left(obj);
}

/* $030116. Holding it up (rising 1 a frame); then throwing it toward the screen's middle (up at
 * 15, across at 2), and $60 frames after. */
void Boss380Throw(uint16_t obj)
{
    uint16_t held = rd16(obj + 0x40);
    set8(held + 0x23, 1);
    wr16(held + O_Y, (uint16_t)(rd16(held + O_Y) - 1));
    if (!w380_tick()) return;
    w380_next();
    wr16(obj + 0x56, 0x6000);
    wr8(obj + 0x21, 0x22);
    w380_timer(0x60);
    set8(held + 0x23, 4);
    clr8(held + 0x23, 1);
    wr32(held + 0x1C, 0xFFF10000);
    if ((int16_t)(rd16(obj + O_X) - 0x120) < 0) wr32(held + 0x18, 0x20000);
    else wr32(held + 0x18, 0xFFFE0000);
}

/* $030174. After the throw: from $40 frames left its look, facing by the counter's bit 4; then
 * standing $10 frames. */
void Boss380AfterThrow(uint16_t obj)
{
    if (w380_tick()) { w380_stand_after(obj); return; }
    if ((int16_t)(rd16(W380_TIMER) - 0x40) > 0) return;
    w380_look(obj, 0x0D027C);
    if (!bit8(W380_TIMER + 1, 4)) w380_face_left(obj);
    else w380_face_right(obj);
}

/* $0301B2. Walking toward the nearer player at 1.125; then what it does from there. */
void Boss380Approach(uint16_t obj)
{
    uint16_t dx, dy, player;
    uint16_t d0 = NearestPlayer(obj, &dx, &dy, &player);
    if (dx & 0x8000) {
        w380_face_left(obj);
        wr32(obj + 0x18, 0xFFFEE000);
    } else {
        w380_face_right(obj);
        wr32(obj + 0x18, 0x12000);
    }
    w380_after_approach(obj, d0);
}

/* $0301E2. Walking back from it at 13/32, facing it. */
void Boss380Retreat(uint16_t obj)
{
    uint16_t dx, dy, player;
    uint16_t d0 = NearestPlayer(obj, &dx, &dy, &player);
    if (!(dx & 0x8000)) {
        w380_face_right(obj);
        wr32(obj + 0x18, 0xFFFF9800);
    } else {
        w380_face_left(obj);
        wr32(obj + 0x18, 0x6800);
    }
    w380_after_retreat(obj, d0);
}

/* $030212. The leap: rising; falling, (1 in $200) on down ($18), else crouching, still, its x
 * speed kept, harmless. */
void Boss380JumpFall(uint16_t obj)
{
    if (w380_fall(obj, 0x4000)) return;
    if (!(w380_seed() & 0x1FF)) { w380_step(0x18); return; }
    w380_next();
    w380_look(obj, 0x0D045C);
    w380_timer(0x14);
    wr32(W380_SPEED, rd32(obj + 0x18));
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr8(obj + 0x21, 0);
    QueueSound(0xDF);
}

/* $030266. Crouching in the air; then facing the player, the beam. */
void Boss380Crouch(uint16_t obj)
{
    if (!w380_tick()) return;
    Boss380FacePlayer(obj);
    w380_next();
    w380_timer(0x10);
    w380_beam(obj);
}

/* $030280. Then falling on with the kept speed, the beam's parts removed. */
void Boss380BeamWait(uint16_t obj)
{
    if (!w380_tick()) return;
    w380_step(0x18);
    wr8(obj + 0x21, 0x22);
    w380_look(obj, 0x0CFF04);
    wr32(obj + 0x18, rd32(W380_SPEED));
    Boss380BeamClear();
}

/* $0302AC. Firing, facing the player: when the counter runs out, a shot (type $390 or $394 by
 * the random) ahead at 3, a second ($390's) the other side's way, then standing for a time by
 * the difficulty. */
void Boss380Fire(uint16_t obj)
{
    Boss380FacePlayer(obj);
    if (!w380_tick()) return;
    QueueSound(0xB9);
    w380_next();
    w380_timer(rom16(0x030358 + (uint32_t)(int16_t)rd16(v_difficulty)));
    w380_look(obj, 0x0D0380);
    Boss380StandStart(obj);
    uint16_t type = rom16(0x030354 + (w380_seed() & 2));
    uint16_t x = rd16(obj + O_X);
    uint32_t vx;
    if (!bit8(obj + 0x0E, 3)) {
        vx = 0x30000;
        x = (uint16_t)(x + 0x1C);
    } else {
        vx = 0xFFFD0000;
        x = (uint16_t)(x - 0x1C);
    }
    Boss380Shot(obj, vx, type, x, 8);
    if (type != 0x390) return;
    Boss380Shot(obj, vx, type, x, 0xFFF8);
}

/* $030360. The entrance: the camera held, its text ($39A70), the music stopped; a prop (type $10
 * at $DFA0) at ($220, $141). */
void Boss380Intro(uint16_t obj)
{
    (void)obj;
    w380_next();
    set8(v_cameraHold, 0);
    wr16(W380_HUD, 0x34);
    wr32(W380_HUD_TEXT, 0x039A70);
    QueueSound(0xF6);
    uint16_t a = W380_PROP;
    wr16(a + O_TYPE, 0x10);
    wr8(a + 0x21, 0x20);
    wr32(a + 0x28, 0xF010F010);
    wr16(a + O_FLAGS, 0xED00);
    wr16(a + 0x0E, 0x2B00);
    wr32(a + 0x08, 0x0E022C);
    wr16(a + O_X, 0x220);
    wr16(a + O_Y, 0x141);
    wr8(a + 0x20, 0x70);
}

/* $0303C0. Until the text is done; then $40 frames. */
void Boss380IntroText(uint16_t obj)
{
    (void)obj;
    if (rd16(W380_HUD)) return;
    w380_next();
    w380_timer(0x40);
}

/* $0303D4. Then the boss's coming flagged ($8080 bit 1). */
void Boss380IntroWait(uint16_t obj)
{
    (void)obj;
    if (!w380_tick()) return;
    w380_next();
    set8(0x8080, 1);
}

/* $0303E8. Walking left to x $168; then the prop turned, its look and box changed. */
void Boss380IntroWalk(uint16_t obj)
{
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 1));
    if ((int16_t)(rd16(obj + O_X) - 0x168) >= 0) return;
    w380_next();
    w380_timer(0x80);
    w380_look(obj, 0x0CFF98);
    wr16(W380_PROP + 0x0E, rd16(W380_PROP + 0x0E) ^ 0x800);
    wr32(W380_PROP + 0x08, 0x0DF4FC);
    wr16(W380_PROP + 0x0C, 0);
    wr8(W380_PROP + 0x21, 0);
}

/* $03042A. $80 frames; then another look for $30. */
void Boss380IntroPause(uint16_t obj)
{
    if (!w380_tick()) return;
    w380_next();
    w380_timer(0x30);
    w380_look(obj, 0x0CFF2C);
}

/* $03044C. $30 frames; then its standing look. */
void Boss380IntroLook(uint16_t obj)
{
    if (!w380_tick()) return;
    w380_next();
    w380_look(obj, 0x0CFE68);
}

/* $030468. Walking left at 2 to the prop: lifting it ($13 frames). */
void Boss380IntroReach(uint16_t obj)
{
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 2));
    if ((int16_t)(rd16(obj + O_X) - rd16(W380_PROP + O_X)) >= 0) return;
    w380_next();
    w380_timer(0x13);
    w380_look(obj, 0x0D013C);
    wr32(W380_PROP + 0x08, 0x0DF4A4);
    wr16(W380_PROP + 0x0C, 0);
    wr16(W380_PROP + O_Y, (uint16_t)(rd16(W380_PROP + O_Y) - 0x10));
    wr16(W380_PROP + O_FLAGS, rd16(W380_PROP + O_FLAGS) | 0x200);
}

/* $0304AC. Then tossing it away (left at 8, up at 4). */
void Boss380IntroToss(uint16_t obj)
{
    (void)obj;
    if (!w380_tick()) return;
    w380_next();
    w380_timer(0x20);
    wr32(W380_PROP + 0x18, 0xFFF80000);
    wr32(W380_PROP + 0x1C, 0xFFFC0000);
    QueueSound(0xA3);
}

/* $0304D8. $20 frames, the prop falling; then another look. */
void Boss380IntroWatch(uint16_t obj)
{
    if (w380_tick()) {
        w380_next();
        w380_look(obj, 0x0CFDFC);
    }
    w380_prop_fall();
}

/* $0304F6. The prop falling until it is gone: then standing $20 frames. */
void Boss380IntroGone(uint16_t obj)
{
    if (rd16(W380_PROP)) {
        wr32(W380_PROP + 0x1C, rd32(W380_PROP + 0x1C) + 0x4000);
        return;
    }
    w380_next();
    w380_timer(0x20);
    w380_look(obj, 0x0CFF98);
}

/* $03052A. Then a pose for $20. */
void Boss380IntroPose(uint16_t obj)
{
    if (!w380_tick()) return;
    w380_next();
    w380_timer(0x20);
    w380_look(obj, 0x0D0314);
}

/* $03054C. Then its second text ($39A80) and the boss's music. */
void Boss380IntroTalk(uint16_t obj)
{
    (void)obj;
    if (!w380_tick()) return;
    w380_next();
    wr16(W380_HUD, 0x34);
    wr32(W380_HUD_TEXT, 0x039A80);
    QueueSound(0x91);
}

/* $030572. Until the text is done: the fight ($9C00 $48, a leap), its vitality bar shown. */
void Boss380IntroEnd(uint16_t obj)
{
    if (rd16(W380_HUD)) return;
    w380_step(0x48);
    wr16(obj + 0x56, 0x4020);
    wr16(W380_HUD, 0x10);
    wr16(W380_HUD_OBJ, obj);
}

/* $030754. Beaten the first time: down $60 frames; then knocked up and back toward the middle. */
void Boss380Beaten(uint16_t obj)
{
    if (w380_tick()) {
        w380_next();
        wr32(obj + 0x1C, 0xFFFB8000);
        QueueSound(0xA5);
        if ((int16_t)(rd16(obj + O_X) - 0x120) < 0) wr32(obj + 0x18, 0xC000);
        else wr32(obj + 0x18, 0xFFFF4000);
    }
    w380_down_look(obj);
}

/* $030794. Falling to y $13C: lying there $8C frames. */
void Boss380BeatenLand(uint16_t obj)
{
    if (w380_fall(obj, 0x3E00)) return;
    if ((int16_t)(rd16(obj + O_Y) - 0x13C) < 0) return;
    w380_next();
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_Y, 0x13C);
    w380_timer(0x8C);
    w380_look(obj, 0x0CFDFC);
}

/* $0307DA. Getting up, facing the player: then standing, its vitality back ($EE1), its bar. */
void Boss380Recover(uint16_t obj)
{
    Boss380FacePlayer(obj);
    if (rd16(W380_TIMER) == 0x20) w380_look(obj, 0x0D027C);
    if (!w380_tick()) return;
    w380_step(0x10);
    wr16(obj + 0x56, 0x6000);
    wr8(obj + 0x21, 0x22);
    w380_timer(0xFFFF);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(obj + O_VITALITY, 0xEE1);
    wr16(obj + 0x5C, 7);
    wr16(W380_HUD, 0x10);
    wr16(W380_HUD_OBJ, obj);
}

/* $030834. Defeated: falling to y $13C; then its text ($3A48C), the players held. */
void Boss380DefeatLand(uint16_t obj)
{
    if (w380_fall(obj, 0x4000)) return;
    if ((int16_t)(rd16(obj + O_Y) - 0x13C) < 0) return;
    w380_next();
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    w380_look(obj, 0x0D027C);
    w380_timer(0xC8);
    wr32(W380_HUD_TEXT, 0x03A48C);
    wr16(W380_HUD, 0x28);
    wr16(0x80C4, 0);
    wr16(0x80C6, 0xB8);
    set8(v_player1 + 0xC0, 7);
    set8(v_player2 + 0xC0, 7);
}

/* $030890. Talking $C8 frames, turning by the counter's bit 4; then facing left. */
void Boss380DefeatTalk(uint16_t obj)
{
    if (w380_tick()) {
        w380_next();
        w380_face_left(obj);
        return;
    }
    if (!bit8(W380_TIMER + 1, 4)) w380_face_left(obj);
    else w380_face_right(obj);
}

/* $0308BA. Leaving: right of x $1E0 it is gone, the camera let go, the next boss (type $614 at
 * $C980), the scene's signal, the effects cleared; else when the counter runs out a hop right. */
void Boss380DefeatLeave(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x1E0) >= 0) {
        clr8(v_cameraHold, 7);
        wr16(obj + O_FLAGS, 0x1000);
        wr16(0xC980 + O_TYPE, 0x614);
        wr16(0xC980 + O_STATE, 0);
        QueueSound(0xF6);
        set8(v_sceneSignal, 0);
        EffectsClear();
        return;
    }
    if (!w380_tick()) return;
    w380_next();
    wr32(obj + 0x1C, 0xFFFC2000);
    wr32(obj + 0x18, 0x32000);
    wr32(obj + 0x08, 0x0D003C);
    wr16(obj + 0x0C, 0);
    QueueSound(0xDF);
}

/* $030926. The hop: falling to y $13C, then $A frames before the next. */
void Boss380DefeatHop(uint16_t obj)
{
    if (w380_fall(obj, 0x4000)) return;
    if ((int16_t)(rd16(obj + O_Y) - 0x13C) < 0) return;
    wr16(obj + O_Y, 0x13C);
    wr16(W380_STEP, (uint16_t)(rd16(W380_STEP) - 4));
    w380_timer(0xA);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x08, 0x0CFDFC);
    wr16(obj + 0x0C, 0);
}

/* $030592. Beaten the other way: down $60 frames, then knocked up behind the background. */
void Boss380Ejected(uint16_t obj)
{
    if (w380_tick()) {
        w380_next();
        set8(obj + 0x0E, 4);
        wr32(obj + 0x1C, 0xFFFE0000);
        QueueSound(0xA5);
    }
    w380_down_look(obj);
}

/* $0305DA. Falling to y $180: hidden, $80 frames, the music stopped. */
void Boss380EjectedFall(uint16_t obj)
{
    w380_fall(obj, 0x4000);
    if ((int16_t)(rd16(obj + O_Y) - 0x180) < 0) return;
    w380_next();
    wr16(obj + O_FLAGS, 0xE000);
    w380_timer(0x80);
    QueueSound(0xF6);
}

/* $030608. Once its +4 is at $1C or more, after the counter: back in another look at ($A0,
 * $140), line 2's colours reset, the boss timer 4. */
void Boss380Return(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_STATE) - 0x1C) < 0) return;
    if (!w380_tick()) return;
    StageColoursFirst();
    w380_next();
    wr32(obj + 0x08, 0x0BFB62);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_X, 0xA0);
    wr16(obj + O_Y, 0x140);
    wr16(obj + 0x0E, 0x26C0);
    wr16(v_bossTimer, 4);
    QueueSound(0x84);
}

/* $030654. Walking right (its y on the camera's line) to x $F0. */
void Boss380ReturnWalk(uint16_t obj)
{
    Boss380RideY(obj);
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + 1));
    if ((int16_t)(rd16(obj + O_X) - 0xF0) < 0) return;
    w380_next();
    wr32(obj + 0x08, 0x0C0022);
    wr16(obj + 0x0C, 0);
    w380_timer(0x20);
}

/* $03067E. $20 frames; then a jump (up at 8, left at 1/2). */
void Boss380ReturnJump(uint16_t obj)
{
    Boss380RideY(obj);
    if (!w380_tick()) return;
    w380_next();
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x1C, 0xFFF80000);
    wr32(obj + 0x18, 0xFFFF8000);
    wr32(obj + 0x08, 0x0BFC2E);
    wr16(obj + 0x0C, 0);
}

/* $0306B2. Falling to its +$48: landed, still, $C frames. */
void Boss380ReturnLand(uint16_t obj)
{
    if (w380_fall(obj, 0x4000)) return;
    if ((int16_t)(rd16(obj + 0x48) - rd16(obj + O_Y)) >= 0) return;
    w380_next();
    wr16(obj + O_FLAGS, 0xCC00);
    wr32(obj + 0x08, 0x0BFF2E);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_Y, rd16(obj + 0x48));
    w380_timer(0xC);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

/* $030700. Standing there; then a pose for $20 frames. */
void Boss380ReturnPose(uint16_t obj)
{
    wr16(obj + O_Y, rd16(obj + 0x48));
    if (!w380_tick()) return;
    w380_next();
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0C007E);
    wr16(obj + 0x0C, 0);
    w380_timer(0x20);
}

/* $03072E. Then done (byte $56 bit 3). */
void Boss380ReturnDone(uint16_t obj)
{
    wr16(obj + O_Y, rd16(obj + 0x48));
    if (!w380_tick()) return;
    w380_next();
    set8(obj + 0x56, 3);
}

/* $030748. Nothing. */
void Boss380Idle(uint16_t obj)
{
    (void)obj;
}

/* $02F80C. Its frame: the clock; defeated (vitality below 0, unless byte $57 bit 4), points and
 * one of three defeats by +$56; else kept on the screen (x $92-$1AE, y below $140) unless byte
 * $57 bit 1; hit (not while bit 0): blinking and knocked up after its hits, else a hit's frames;
 * grabbed by a player (+$42), carried; then its step. */
void Boss380Think(uint16_t obj)
{
    wr16(W380_CLOCK, (uint16_t)(rd16(W380_CLOCK) + 1));
    if (!bit8(obj + 0x57, 4) && (rd16(obj + O_VITALITY) & 0x8000)) {
        if (bit8(obj + 0x56, 7)) {
            AddScore(obj, 0x23005);
            w380_step(0xC4);
            wr16(obj + 0x56, 0x8033);
            Boss380Down(obj);
        } else if (bit8(obj + 0x56, 5)) {
            AddScore(obj, 0x99999);
            w380_step(0xB4);
            wr16(obj + 0x56, 0x6013);
            Boss380DownFlying(obj);
            Boss380BeamClear();
        } else {
            AddScore(obj, 1);
            w380_step(0xA8);
            wr16(obj + 0x56, 0x6011);
            Boss380Down(obj);
        }
        call_object(rom32(W380_TABLE + (uint32_t)(int16_t)rd16(W380_STEP)), obj);
        return;
    }
    if (!bit8(obj + 0x57, 1)) {
        clr8(obj + 0x57, 2);
        clr8(obj + 0x57, 3);
        if ((int16_t)(rd16(obj + O_X) - 0x94) < 0) {
            set8(obj + 0x57, 3);
            if ((int16_t)(rd16(obj + O_X) - 0x90) < 0) wr16(obj + O_X, 0x92);
        } else if ((int16_t)(rd16(obj + O_X) - 0x1AC) >= 0) {
            set8(obj + 0x57, 2);
            if ((int16_t)(rd16(obj + O_X) - 0x1B0) >= 0) wr16(obj + O_X, 0x1AE);
        }
        if ((int16_t)(rd16(obj + O_Y) - 0x140) >= 0) wr16(obj + O_Y, 0x13F);
    }
    if (bit8(obj + 0x22, 5) && !bit8(obj + 0x57, 0)) {
        uint16_t d0 = rd16(obj + 0x58);
        if (d0 == 0 || (d0 & 0x8000)) {
            if (d0 & 0x8000) clr8(obj + 0x0E, 4);
            int knocked = bit8(obj + 0x23, 4);
            clr8(obj + 0x23, 4);
            if (!knocked) {
                wr16(obj + 0x5C, (uint16_t)(rd16(obj + 0x5C) - 1));
                if (!(rd16(obj + 0x5C) & 0x8000)) {
                    wr16(obj + 0x0C, 0);
                    if (!bit8(obj + 0x57, 5)) {
                        w380_step(8);
                        wr16(obj + 0x58, 3);
                        wr32(obj + 0x08, 0x0D0064);
                        wr32(obj + 0x18, 0);
                        wr32(obj + 0x1C, 0);
                    } else {
                        w380_step(4);
                        wr16(obj + 0x58, 4);
                        wr32(obj + 0x08, 0x0D009C);
                    }
                    call_object(rom32(W380_TABLE + (uint32_t)(int16_t)rd16(W380_STEP)), obj);
                    return;
                }
            }
            w380_step(0xC);
            wr16(W380_DRIFT, 0);
            wr32(obj + 0x1C, 0xFFFD0000);
            w380_timer(0xA);
            Boss380Blink(obj);
            wr16(obj + 0x0C, 0);
            wr32(obj + 0x08, 0x0D00CC);
        }
    }
    if (bit8(obj + 0x23, 1)) {
        clr8(obj + 0x23, 1);
        uint16_t by = rd16(obj + 0x42);
        if (by == v_player1 || by == v_player2) {
            wr16(W380_THROWER, by);
            clr8(obj + 0x0E, 4);
            wr32(obj + 0x08, 0x0D00CC);
            wr16(obj + 0x0C, 0);
            wr32(obj + 0x18, 0);
            wr32(obj + 0x1C, 0);
            set8(obj + 0x57, 0);
            wr8(obj + 0x21, 0);
            wr16(0x80E8, 0x10);
            w380_step(bit8(obj + 0x57, 5) ? 0x24 : 0x1C);
        }
    }
    call_object(rom32(W380_TABLE + (uint32_t)(int16_t)rd16(W380_STEP)), obj);
}

/* $02F804. Type $380: its frame, then its blinking (+$5A counting down; while it runs, hidden on
 * odd counts and flagged in +$23 bit 2). */
void Boss380(uint16_t obj)
{
    Boss380Think(obj);
    clr8(obj + 0x23, 2);
    uint16_t d0 = rd16(obj + 0x5A);
    if (d0 && !(d0 & 0x8000)) {
        set8(obj + 0x23, 2);
        wr16(obj + 0x5A, (uint16_t)(d0 - 1));
        if (!bit8(obj + 0x5B, 0)) {
            wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0x7FFF);
            return;
        }
    }
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x8000);
}

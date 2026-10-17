/* The players' ships of stage 5's space shooter (types $3FC and $540, $1567C-$16ABD, run in the
 * players' slots), then the object types after the effects library ($5804A-$5E091): small types
 * (a sprite viewer, crates, a lifter, a bomber and its mines, a hive), the strider ($130) with
 * the crane before it ($170) and its pilot's escape ($210), a floating boss ($3A8) and its guns
 * and bombs, a boss on foot ($564). Helpers shared with other parts of the game (an integer
 * square root, the palette's colours flipped, an aimed shot by the difficulty) are under "shared
 * helpers" at the end. */
#include "game.h"

/* the sine and cosine tables ($2977C, $297FC: 256 words) */
#define SINES   0x02977Cu
#define COSINES 0x0297FCu
static int32_t sine(uint16_t i) { return (int16_t)rom16(SINES + (uint32_t)(int16_t)i); }
static int32_t cosine(uint16_t i) { return (int16_t)rom16(COSINES + (uint32_t)(int16_t)i); }
/* an angle (a word, $10000 a turn) as the tables' byte index: +$80, asr 7, the even ones */
static uint16_t angle_index(uint16_t a) { return (uint16_t)((int16_t)(uint16_t)(a + 0x80) >> 7) & 0x1FE; }

static void add16(uint16_t a, uint16_t v) { wr16(a, (uint16_t)(rd16(a) + v)); }
static void add32(uint16_t a, uint32_t v) { wr32(a, rd32(a) + v); }
/* subq.w #1 on a word: 1 when it is now 0 (beq) */
static int dec_zero(uint16_t a)
{
    wr16(a, (uint16_t)(rd16(a) - 1));
    return rd16(a) == 0;
}
/* subq.w #1 on a word: 1 when it is now below 0 (bmi) */
static int dec_neg(uint16_t a)
{
    wr16(a, (uint16_t)(rd16(a) - 1));
    return (rd16(a) & 0x8000) != 0;
}

static void smoke_puff(uint16_t obj, uint32_t list);

/* ---- types $3FC, $540: the players' ships in stage 5. $3FC flies on its own (the pad's eight
 * ways, B to shoot, C and a way to dash); with two players, $540 circles the other ship
 * (+$4A), at +$4C its angle from it and +$4E its distance. Both run in the players' slots ---- */

#define S_HURT     0x21          /* byte: bit 7 can be hurt */
#define S_HIT      0x22          /* byte: bit 7 hit */
#define S_STATUS   0x23          /* byte: bit 2 blinking, bit 3 touching an item, bit 4 knocked down */
#define S_TOUCHING 0x42          /* the object it touches (an item, what hit it) */
#define S_PARTNER  0x4A          /* the other player's slot */
#define S_ANGLE    0x4C          /* $540: its angle from the other ship */
#define S_DISTANCE 0x4E          /* $540: its distance from it */
#define S_ANIM     0x50          /* long: the animation set (to start it once) */
#define S_HELD     0x69
#define S_PRESSED  0x6A
#define S_BLINK    0x7C
#define S_DISPLAY  0x81
#define S_TIMER    0x92
#define S_SHOT     0x94
#define S_SHOT_B   0x96
#define S_AIM      0x9A          /* the aim (a word, the byte its direction) */
#define S_MUZZLE_X 0x9E
#define S_MUZZLE_Y 0xA0
#define S_FIRING   0xA8
#define S_CHARACTER 0xCA         /* 0 Red, else Blue */
#define S_VITALITY_SHOWN 0xBE
#define SHIP_SWAP  0xA298        /* with two players: 0 player 1 flies ship $3FC, 4 ship $540 */
#define SPACE_STEP 0xA98C        /* stage 5: the background's step (4, 8 as the ships come, $C) */

/* the blinking after a hit (loc_0F61A, PlayerFrame's end): shown on odd frames, then for good */
static void ship_blink(uint16_t obj)
{
    if (!rd16(obj + S_BLINK)) return;
    if (!dec_zero(obj + S_BLINK)) {
        set8(obj + S_STATUS, 2);
        wr16(obj + O_FLAGS, (uint16_t)((rd16(obj + O_FLAGS) & 0x7FFF) | (rd16(v_frameCount) & 1) << 15));
        return;
    }
    clr8(obj + S_STATUS, 2);
    set8(obj + O_FLAGS, 7);
}

/* the animation set by the aim's direction (eight from a table of longs), started when new */
static void ship_anim(uint16_t obj, uint32_t table)
{
    uint16_t i = (uint16_t)(((uint8_t)(rd8(obj + S_AIM) + 0x10) >> 3) & 0x1C);
    uint32_t a = rom32(table + i);
    if (a == rd32(obj + S_ANIM)) return;
    wr32(obj + 0x08, a);
    wr32(obj + S_ANIM, a);
    wr16(obj + 0x0C, 0);
}

/* the muzzle's place (+$9E, +$A0): $30 along the aim (loc_1638C) */
static void ship_muzzle(uint16_t obj)
{
    uint16_t i = (uint16_t)((rd8(obj + S_AIM) & 0xFF) << 1);
    wr16(obj + S_MUZZLE_X, (uint16_t)((uint32_t)(cosine(i) * 0x30) >> 16));
    wr16(obj + S_MUZZLE_Y, (uint16_t)((uint32_t)(sine(i) * 0x30) >> 16));
}

/* the vitality's display redrawn when it changed (or when it is 0) */
static void ship_vitality_shown(uint16_t obj, uint16_t v)
{
    if (v && v == rd16(obj + S_VITALITY_SHOWN)) return;
    wr16(obj + S_VITALITY_SHOWN, v);
    set8(obj + S_DISPLAY, 2);
}

/* the player gone from the game (and from the pause's pads) */
static void ship_out(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x1000);
    set8(obj + S_DISPLAY, 6);
    int b = obj == 0xA400 ? 0 : 1;
    clr8(v_players, b);
    clr8(v_pauseFlags, b);
}

/* $01567C. Type $3FC: a player's ship: the weapon button, nothing more while paused; the screen's
 * limits, its state, the sprite unflipped, the blinking after a hit. */
void Ship(uint16_t obj)
{
    PlayerWeaponButton(obj);
    if (rd8(v_pauseFlags) & 0x80) return;
    PlayerLimits(obj);
    ShipStates(obj);
    clr8(obj + 0x0E, 3);
    ship_blink(obj);
}

static void ship_to_hit(uint16_t obj);
static void ship_to_knocked(uint16_t obj);

/* $0156A2. The vitality's display; a hit: hurt (or knocked when it was the last of it); an item
 * touched: picked up with B; then the state's routine (the table at $156F0). */
void ShipStates(uint16_t obj)
{
    ship_vitality_shown(obj, rd16(obj + O_VITALITY));
    if (bit8(obj + S_HIT, 7)) {
        clr8(obj + S_HIT, 7);
        ShipHitFatal(obj);
        if (!bit8(obj + S_STATUS, 4)) ship_to_hit(obj);
        else ship_to_knocked(obj);
        return;
    }
    if (bit8(obj + S_STATUS, 3)) {
        clr8(obj + S_STATUS, 3);
        ShipPickUp(obj);
    }
    call_object(rom32(0x0156F0 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $01572C. Out of vitality: knocked (+$23 bit 4). */
void ShipHitFatal(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) set8(obj + S_STATUS, 4);
}

/* $015740. B on an item: vitality ($68: 20, $6C: 50), or a weapon, the one it replaces left where
 * the item was (the item turned into it; with the combined weapon, the second goes). */
void ShipPickUp(uint16_t obj)
{
    if (!bit8(obj + S_PRESSED, 4)) return;
    uint16_t item = rd16(obj + S_TOUCHING);
    uint16_t type = rd16(item + O_TYPE), old;
    if (type == 0x68) {
        QueueSound(0xA7);
        add16(obj + O_VITALITY, 0x14);
        clr8(item + S_STATUS, 3);
        return;
    }
    if (type == 0x6C) {
        add16(obj + O_VITALITY, 0x32);
        clr8(item + S_STATUS, 3);
        return;
    }
    QueueSound(0xA7);
    wr8(obj + S_SHOT_B, 0);
    wr16(obj + S_SHOT, 0);
    uint8_t w = rd8(obj + 0x72);
    if (w != 3 && (w & 2)) {
        old = rd16(obj + 0x70);
        wr16(obj + 0x70, (uint16_t)(rd16(item + O_TYPE) - 0x54));
    } else if (w != 3 && (w & 1)) {
        old = rd16(obj + 0x6E);
        wr16(obj + 0x6E, (uint16_t)(rd16(item + O_TYPE) - 0x54));
    } else {
        old = rd16(obj + 0x70);
        wr16(obj + 0x70, rd16(obj + 0x6E));
        wr16(obj + 0x6E, (uint16_t)(rd16(item + O_TYPE) - 0x54));
        if (!old) {
            clr8(item + S_STATUS, 3);
            return;
        }
    }
    /* the item's type from the words after the rts at $1580A (4, 8, $C, $10: $58-$64) */
    wr16(item + O_TYPE, rom16(0x01580A + (uint32_t)(int16_t)((int16_t)old >> 1)));
    wr32(item + O_X, rd32(obj + O_X));
    wr32(item + O_Y, rd32(obj + O_Y));
    wr32(item + 0x18, 0);
    wr32(item + 0x1C, 0);
}

/* its coming in (loc_15838): state 4 for $20 frames */
static void ship_to_appear(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0);
    wr32(obj + 0x08, 0x0DC600);
    wr16(obj + 0x0C, 0);
    wr16(obj + S_TIMER, 0x20);
    ShipAppear(obj);
}

/* $015814. State 0: still, its partner set; in the stage's start, waits for the stars (state
 * $34), else comes in. */
void ShipStart(uint16_t obj)
{
    wr8(obj + S_HIT, 0);
    wr8(obj + S_SHOT_B, 0);
    wr16(obj + S_SHOT, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr32(obj + S_ANIM, 0);
    ShipPartner(obj);
    if (!rd16(SPACE_STEP)) {
        ship_to_appear(obj);
        return;
    }
    wr16(obj + O_STATE, 0x34);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0);
    wr32(obj + 0x08, 0x0DC688);
    wr16(obj + 0x0C, 0);
    ShipArriveWait(obj);
}

/* back to flying (loc_15880): state 8 */
static void ship_to_fly(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0x80);
    wr32(obj + 0x28, 0xFA06FC08);
    wr16(obj + O_STATE, 8);
    wr16(obj + 0x48, 0);
    ShipFly(obj);
}

/* $01585A. State 4: coming in, moved by the pad; its look changed at $10, then flying. */
void ShipAppear(uint16_t obj)
{
    uint16_t t = (uint16_t)(rd16(obj + S_TIMER) - 1);
    wr16(obj + S_TIMER, t);
    if (!t) {
        ship_to_fly(obj);
        return;
    }
    if (t == 0x10) {
        wr32(obj + 0x08, 0x0DC628);
        wr16(obj + 0x0C, 0);
    }
    ShipPadSpeed(obj);
}

static void ship_to_dash(uint16_t obj);
static void ship_to_up(uint16_t obj);
static void ship_to_down(uint16_t obj);
static void ship_to_shoot(uint16_t obj);

/* C pressed with a way held: the dash */
static int dash_asked(uint16_t obj)
{
    return bit8(obj + S_PRESSED, 5) && (rd8(obj + S_HELD) & 0xF);
}

/* the aim from the pad (kept with none), the speed from it, the pull to the partner */
static void ship_steer(uint16_t obj, uint16_t aim)
{
    aim = ShipPadAngle(obj, aim);
    wr16(obj + 6, aim);
    wr16(obj + S_AIM, aim);
    ShipPadSpeed(obj);
    ShipPull(obj);
}

/* $01589E. State 8: flying, the aim turned by the pad; up and down (their own looks), B (shoot)
 * and the dash change the state. */
void ShipFly(uint16_t obj)
{
    if (dash_asked(obj)) { ship_to_dash(obj); return; }
    uint8_t held = rd8(obj + S_HELD);
    if (held & 1) { ship_to_up(obj); return; }
    if (held & 2) { ship_to_down(obj); return; }
    if (held & 0x10) { ship_to_shoot(obj); return; }
    ship_steer(obj, rd16(obj + S_AIM));
    ShipAnim(obj);
}

/* $0158EC. The speed by the pad's way (none: still). */
void ShipPadSpeed(uint16_t obj)
{
    uint16_t a = rom16(0x015920 + (uint32_t)((rd8(obj + S_HELD) & 0xF) << 1));
    if (a & 0x8000) {
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        return;
    }
    uint16_t i = (uint16_t)(((uint16_t)(a + 0x10) & 0xE0) >> 3);
    wr32(obj + 0x18, rom32(0x015940 + i));
    wr32(obj + 0x1C, rom32(0x015960 + i));
}

/* to state $C (loc_15980) */
static void ship_to_up(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0x80);
    wr32(obj + 0x28, 0xFA06FC08);
    wr16(obj + O_STATE, 0xC);
    ShipFlyUp(obj);
}

/* $01599A. State $C: flying with up held (its look tilted, the table at $16234). */
void ShipFlyUp(uint16_t obj)
{
    if (dash_asked(obj)) { ship_to_dash(obj); return; }
    uint8_t held = rd8(obj + S_HELD);
    if (!(held & 1)) { ship_to_fly(obj); return; }
    if (held & 0x10) { ship_to_shoot(obj); return; }
    ship_steer(obj, rd16(obj + 6));
    ship_anim(obj, 0x016234);
}

/* to flying down (loc_159E0). It sets state $C, up's (the next frame finds up not held and
 * comes back here through flying): kept. */
static void ship_to_down(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0x80);
    wr32(obj + 0x28, 0xFA06FC08);
    wr16(obj + O_STATE, 0xC);
    ShipFlyDown(obj);
}

/* $0159FA. State $10: flying with down held (the table at $1627C). */
void ShipFlyDown(uint16_t obj)
{
    if (dash_asked(obj)) { ship_to_dash(obj); return; }
    uint8_t held = rd8(obj + S_HELD);
    if (!(held & 2)) { ship_to_fly(obj); return; }
    if (held & 0x10) { ship_to_shoot(obj); return; }
    ship_steer(obj, rd16(obj + 6));
    ship_anim(obj, 0x01627C);
}

/* to shooting (loc_15A40): state $14 */
static void ship_to_shoot(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0x80);
    wr32(obj + 0x28, 0xFA06FC08);
    wr16(obj + S_MUZZLE_X, 0);
    wr16(obj + S_MUZZLE_Y, 0);
    wr16(obj + O_STATE, 0x14);
    ShipShotBits(obj);
    ShipShoot(obj);
}

static void ship_to_shoot_up(uint16_t obj);
static void ship_to_shoot_down(uint16_t obj);

/* $015A66. State $14: shooting (the aim kept) while the weapon fires; up or down held: their
 * shooting states. */
void ShipShoot(uint16_t obj)
{
    if (dash_asked(obj)) { ship_to_dash(obj); return; }
    uint8_t held = rd8(obj + S_HELD);
    if (held & 1) { ship_to_shoot_up(obj); return; }
    if (held & 2) { ship_to_shoot_down(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + S_SHOT)) { ship_to_fly(obj); return; }
    ShipShootSpeed(obj);
    ShipPull(obj);
    ShipShootAnim(obj);
    ship_muzzle(obj);
}

/* $015AAE. The speed by the pad (a branch to ShipPadSpeed). */
void ShipShootSpeed(uint16_t obj)
{
    ShipPadSpeed(obj);
}

/* $015ABC. The weapon's bits 1, 2 of +$96. */
void ShipShotBits(uint16_t obj)
{
    set8(obj + S_SHOT_B, 1);
    set8(obj + S_SHOT_B, 2);
}

/* to shooting up (loc_15AD2): state $18 */
static void ship_to_shoot_up(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0x80);
    wr32(obj + 0x28, 0xFA06FC08);
    wr16(obj + S_MUZZLE_X, 0);
    wr16(obj + S_MUZZLE_Y, 0xFFFC);
    wr16(obj + O_STATE, 0x18);
    ShipShotBits(obj);
    ShipShootUp(obj);
}

/* $015AF8. State $18: shooting with up held. */
void ShipShootUp(uint16_t obj)
{
    if (dash_asked(obj)) { ship_to_dash(obj); return; }
    if (!(rd8(obj + S_HELD) & 1)) { ship_to_shoot(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + S_SHOT)) { ship_to_up(obj); return; }
    ShipShootSpeed(obj);
    ShipPull(obj);
    ShipShootUpAnim(obj);
    ship_muzzle(obj);
}

/* to shooting down (loc_15B36). It sets state $18, shooting up's (as ship_to_down): kept. */
static void ship_to_shoot_down(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0x80);
    wr32(obj + 0x28, 0xFA06FC08);
    wr16(obj + S_MUZZLE_X, 0);
    wr16(obj + S_MUZZLE_Y, 4);
    wr16(obj + O_STATE, 0x18);
    ShipShotBits(obj);
    ShipShootDown(obj);
}

/* $015B5E. State $1C: shooting with down held. */
void ShipShootDown(uint16_t obj)
{
    if (dash_asked(obj)) { ship_to_dash(obj); return; }
    if (!(rd8(obj + S_HELD) & 2)) { ship_to_shoot(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + S_SHOT)) { ship_to_down(obj); return; }
    ShipShootSpeed(obj);
    ShipPull(obj);
    ShipShootDownAnim(obj);
    ship_muzzle(obj);
}

/* to the dash (loc_15B9E): state $20, its speed by the aim, two parts held at it */
static void ship_to_dash(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0xC0);
    wr32(obj + 0x28, 0xFA06FC08);
    wr32(obj + 0x2C, 0xF808F808);
    wr16(obj + 0x26, 4);
    ShipAnim(obj);
    wr16(obj + O_STATE, 0x20);
    wr8(obj + S_SHOT_B, 0);
    wr16(obj + S_SHOT, 0);
    ShipDashSpeed(obj);
    wr16(obj + S_TIMER, 0xC);
    ShipDashParts(obj);
    wr16(obj + 0x48, 1);
    ShipDash(obj);
}

/* the partner dashing the same way: this one harmful too (+$21 $40) */
static void ship_dash_together(uint16_t obj)
{
    uint16_t p = rd16(obj + S_PARTNER);
    if (rd16(p + 0x48) && rd16(p + 6) == rd16(obj + 6)) wr8(obj + S_HURT, 0x40);
}

/* $015BE6. State $20: the dash, $C frames slowing. */
void ShipDash(uint16_t obj)
{
    if (dec_zero(obj + S_TIMER)) { ship_to_fly(obj); return; }
    ShipDashSlow(obj);
    ShipPull(obj);
    ship_dash_together(obj);
}

/* to hurt (loc_15C16): state $24 for 8 frames */
static void ship_to_hit(uint16_t obj)
{
    if (rd16(obj + O_STATE) == 0x24) { ShipHit(obj); return; }
    wr16(obj + O_STATE, 0x24);
    QueueSound(0xDE);
    wr8(obj + S_SHOT_B, 0);
    wr16(obj + S_SHOT, 0);
    wr8(obj + S_HURT, 0);
    wr8(obj + S_HIT, 0);
    wr16(obj + 0x48, 0);
    wr16(obj + S_TIMER, 8);
    ShipHit(obj);
}

/* $015C4A. State $24: hurt, slowing. */
void ShipHit(uint16_t obj)
{
    if (dec_neg(obj + S_TIMER)) { ship_to_fly(obj); return; }
    ShipSlow(obj);
}

/* $015C56. Both speeds slowed (by a quarter and more). */
void ShipSlow(uint16_t obj)
{
    wr32(obj + 0x18, SlowSigned(rd32(obj + 0x18)));
    wr32(obj + 0x1C, SlowSigned(rd32(obj + 0x1C)));
}

/* $015C70. A speed (d0, the caller's move.l set the flags) slowed toward 0. */
uint32_t SlowSigned(uint32_t v)
{
    if (!v) return v;
    if (!(v & 0x80000000u)) return SlowPositive(v);
    return 0u - SlowPositive(0u - v);
}

/* $015C82. v / 2 + v / 4 (asr). */
uint32_t SlowPositive(uint32_t v)
{
    v = (uint32_t)((int32_t)v >> 1);
    return v + (uint32_t)((int32_t)v >> 1);
}

/* to knocked away (loc_15C8C): state $28 for $10 frames, away from what hit it */
static void ship_to_knocked(uint16_t obj)
{
    if (rd16(obj + O_STATE) == 0x28) { ShipKnocked(obj); return; }
    wr16(obj + O_STATE, 0x28);
    QueueSound(0xDE);
    wr8(obj + S_SHOT_B, 0);
    wr16(obj + S_SHOT, 0);
    wr8(obj + S_HURT, 0);
    wr8(obj + S_HIT, 0);
    wr16(obj + 0x48, 0);
    ShipKnockFrom(obj);
    wr16(obj + S_TIMER, 0x10);
    ShipKnocked(obj);
}

/* to falling (loc_15D76): state $2C for $80 frames */
static void ship_to_falling(uint16_t obj)
{
    if (rd16(obj + O_STATE) == 0x2C) { ShipFalling(obj); return; }
    if (rd16(obj + O_STATE) == 0x30) { ShipGone(obj); return; }
    wr16(obj + O_STATE, 0x2C);
    QueueSound(0xBC);
    wr8(obj + S_SHOT_B, 0);
    wr16(obj + S_SHOT, 0);
    wr16(obj + O_VITALITY, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr8(obj + S_HURT, 0);
    wr8(obj + S_HIT, 0);
    wr8(obj + 0x20, 0xC0);
    wr16(obj + 0x48, 0);
    wr16(obj + S_TIMER, 0x80);
    ShipFalling(obj);
}

/* $015CC4. State $28: knocked away, slowing; then flying, blinking (falling with no vitality). */
void ShipKnocked(uint16_t obj)
{
    if (!dec_neg(obj + S_TIMER)) { ShipKnockSlow(obj); return; }
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) { ship_to_falling(obj); return; }
    wr16(obj + S_BLINK, 0x40);
    ship_to_fly(obj);
}

/* $015CE6. Knocked down: turned away from what hit it (+$42), its speed $20 along that. */
void ShipKnockFrom(uint16_t obj)
{
    if (!bit8(obj + S_STATUS, 4)) return;
    uint16_t by = rd16(obj + S_TOUCHING);
    uint32_t dy = (uint16_t)(rd16(obj + O_Y) - rd16(by + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(obj + O_X) - rd16(by + O_X)), &dy);
    wr16(obj + 6, a);
    wr16(obj + S_AIM, a);
    uint16_t i = angle_index(a);
    wr32(obj + 0x18, (uint32_t)cosine(i) << 5);
    wr32(obj + 0x1C, (uint32_t)sine(i) << 5);
}

/* $015D42. The knock slowed: a step along the aim taken off. */
void ShipKnockSlow(uint16_t obj)
{
    uint16_t i = angle_index(rd16(obj + S_AIM));
    wr32(obj + 0x18, rd32(obj + 0x18) - ((uint32_t)cosine(i) << 1));
    wr32(obj + 0x1C, rd32(obj + 0x1C) - ((uint32_t)sine(i) << 1));
}

/* the smoke of a ship falling (loc_1682E): every 4th frame a puff (two kinds) */
static void ship_smoke(uint16_t obj)
{
    if (rd16(v_gameFrames) & 3) return;
    smoke_puff(obj, rd8(v_players) == 3 ? 0x056450 : 0x0563EE);
}

/* the ship out (loc_15E3A): the other flies $3FC */
static void ship_leave(uint16_t obj)
{
    ShipSwapTo(obj);
    ship_out(obj);
}

/* $015DC6. State $2C: falling in smoke for $80 frames; then out, or with two players gone (state
 * $30). */
void ShipFalling(uint16_t obj)
{
    if (!dec_neg(obj + S_TIMER)) { ship_smoke(obj); return; }
    if (rd8(v_players) != 3) { ship_leave(obj); return; }
    if (rd16(obj + O_STATE) == 0x30) { ShipGone(obj); return; }
    wr16(obj + O_STATE, 0x30);
    wr8(obj + S_SHOT_B, 0);
    wr16(obj + S_SHOT, 0);
    wr16(obj + O_VITALITY, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr8(obj + S_HURT, 0);
    wr8(obj + S_HIT, 0);
    wr16(obj + 0x48, 0);
    wr32(obj + 0x08, 0x0DC9B0);
    wr32(obj + S_ANIM, 0x0DC9B0);
    wr16(obj + 0x0C, 0);
    ShipGone(obj);
}

/* $015E22. State $30: gone; with two players it waits while the other ship circles ($540's
 * distance not 0), then the ship is out. */
void ShipGone(uint16_t obj)
{
    if (rd8(v_players) == 3 && rd16(rd16(obj + S_PARTNER) + S_DISTANCE)) return;
    ship_leave(obj);
}

/* $015E8A. State $34: waiting for the stars to speed up (the stage's step 8: the arrival's
 * glints); past it, the arrival (state $38, $80 frames). */
void ShipArriveWait(uint16_t obj)
{
    int16_t s = (int16_t)rd16(SPACE_STEP);
    if (s < 8) return;
    if (s == 8) {
        ShipArriveGlint(obj);
        return;
    }
    wr16(obj + O_STATE, 0x38);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0);
    wr16(obj + S_TIMER, 0x80);
    ShipArrive(obj);
}

/* $015EB6. State $38: at $40 frames left its look, the debris and flashes; at 0, coming in. */
void ShipArrive(uint16_t obj)
{
    uint16_t t = (uint16_t)(rd16(obj + S_TIMER) - 1);
    wr16(obj + S_TIMER, t);
    if (!t) {
        wr16(SPACE_STEP, 0);
        ship_to_appear(obj);
        return;
    }
    if (t != 0x40) return;
    wr32(obj + 0x08, 0x0DC660);
    wr16(obj + 0x0C, 0);
    ShipDebris(obj);
    ShipArriveFlash(obj);
    QueueSound(0xD2);
}

/* an effect of type $100 (an animation list) from the player's own slots */
static int ship_effect(uint16_t obj, uint32_t list, uint16_t x, uint16_t y)
{
    uint16_t e;
    if (!PlayerFindEffect(obj, &e)) return 0;
    wr16(e + O_TYPE, 0x100);
    wr32(e + 0x48, list);
    wr16(e + O_STATE, 0);
    wr16(e + O_X, x);
    wr16(e + O_Y, y);
    return 1;
}

/* $015EFA. Every 4th frame a glint (list $5663A) $20 behind, with its sound. */
void ShipArriveGlint(uint16_t obj)
{
    if (rd16(v_gameFrames) & 3) return;
    QueueSound(0xCB);
    ship_effect(obj, 0x05663A, (uint16_t)(rd16(obj + O_X) - 0x20), rd16(obj + O_Y));
}

/* $015F3C. Two flashes (list $5639C) behind it, above and below. */
void ShipArriveFlash(uint16_t obj)
{
    if (!ship_effect(obj, 0x05639C, (uint16_t)(rd16(obj + O_X) - 8), (uint16_t)(rd16(obj + O_Y) - 8))) return;
    ship_effect(obj, 0x05639C, (uint16_t)(rd16(obj + O_X) - 8), (uint16_t)(rd16(obj + O_Y) + 8));
}

/* the aim's eighth (+6's byte) as a table index of longs */
static uint16_t aim_eighth(uint16_t obj)
{
    return (uint16_t)(((uint8_t)(rd8(obj + 6) + 0x10) & 0xE0) >> 3);
}

/* $015F9E. The dash slowed by its way's step. */
void ShipDashSlow(uint16_t obj)
{
    uint16_t i = aim_eighth(obj);
    wr32(obj + 0x18, rd32(obj + 0x18) - rom32(0x015FBE + i));
    wr32(obj + 0x1C, rd32(obj + 0x1C) - rom32(0x015FDE + i));
}

/* $015FFE. The dash's speed along the aim ($10 a frame). */
void ShipDashSpeed(uint16_t obj)
{
    uint16_t i = aim_eighth(obj);
    wr32(obj + 0x18, rom32(0x01601A + i));
    wr32(obj + 0x1C, rom32(0x01603A + i));
}

/* $01605A. +$4A: the other player's slot. */
void ShipPartner(uint16_t obj)
{
    wr16(obj + S_PARTNER, obj == 0xA600 ? 0xA400 : 0xA600);
}

/* $016072. With two players, the ship's swap set so the other one flies $3FC. */
void ShipSwapTo(uint16_t obj)
{
    if (rd8(v_players) != 3) return;
    wr16(SHIP_SWAP, obj == 0xA600 ? 0 : 4);
}

/* $016092. Two pieces of debris (type $5B8) flung back from it, up and down. */
void ShipDebris(uint16_t obj)
{
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y), left, e;
    if (!ObjectFindEffect(obj, &left, &e)) return;
    wr16(e + O_TYPE, 0x5B8);
    wr16(e + O_STATE, 0);
    wr16(e + O_X, (uint16_t)(x - 16));
    wr16(e + O_Y, (uint16_t)(y - 4));
    wr32(e + 0x18, 0xFFFE0000);
    wr32(e + 0x1C, 0xFFFE0000);
    if (!ObjectFindEffect(obj, &left, &e)) return;
    wr16(e + O_TYPE, 0x5B8);
    wr16(e + O_STATE, 0);
    wr16(e + O_X, (uint16_t)(x - 16));
    wr16(e + O_Y, (uint16_t)(y + 4));
    wr32(e + 0x18, 0xFFFE0000);
    wr32(e + 0x1C, 0x20000);
}

/* two parts (type $574) held at the ship by its aim's eighth; +$4C their set */
static void ship_parts(uint16_t obj, uint16_t set)
{
    uint16_t i = (uint16_t)(((uint8_t)(rd8(obj + 6) + 0x10) >> 4) & 0xE), left, e;
    if (!ObjectFindEffect(obj, &left, &e)) return;
    wr16(e + O_TYPE, 0x574);
    wr16(e + O_STATE, 0);
    wr16(e + 0x48, rom16(0x016150 + i));
    wr16(e + 0x4A, obj);
    wr16(e + 0x4C, set);
    if (!ObjectFindEffect(obj, &left, &e)) return;
    wr16(e + O_TYPE, 0x574);
    wr16(e + O_STATE, 0);
    wr16(e + 0x48, rom16(0x016160 + i));
    wr16(e + 0x4A, obj);
    wr16(e + 0x4C, set);
}

/* $016100. The dash's two parts ($3FC's set). */
void ShipDashParts(uint16_t obj)
{
    ship_parts(obj, 0);
}

/* $016170. The same, $540's set. */
void ShipLinkedDashParts(uint16_t obj)
{
    ship_parts(obj, 4);
}

/* $0161C4. The look by the aim (flying). */
void ShipAnim(uint16_t obj)
{
    ship_anim(obj, 0x0161EC);
}

/* $01629C. The look shooting (+$A8 the weapon firing), else flying's. */
void ShipShootAnim(uint16_t obj)
{
    if (!rd16(obj + S_FIRING)) { ShipAnim(obj); return; }
    ship_anim(obj, 0x0162CC);
}

/* $0162EC. The same up. */
void ShipShootUpAnim(uint16_t obj)
{
    ship_anim(obj, rd16(obj + S_FIRING) ? 0x01631C : 0x016234);
}

/* $01633C. The same down. */
void ShipShootDownAnim(uint16_t obj)
{
    ship_anim(obj, rd16(obj + S_FIRING) ? 0x01636C : 0x01627C);
}

/* $0163BA. Type $540: the second player's ship, circling the first. As Ship. */
void ShipLinked(uint16_t obj)
{
    PlayerWeaponButton(obj);
    if (rd8(v_pauseFlags) & 0x80) return;
    PlayerLimits(obj);
    ShipLinkedStates(obj);
    clr8(obj + 0x0E, 3);
    ship_blink(obj);
}

static void linked_to_falling(uint16_t obj);
static void linked_to_rejoin(uint16_t obj);

/* $0163E0. No vitality: falling; the display; an item picked up with B; the other ship gone
 * (state $30): this one becomes $3FC; then the state's routine (the table at $16424). */
void ShipLinkedStates(uint16_t obj)
{
    uint16_t v = rd16(obj + O_VITALITY);
    if ((int16_t)v <= 0) { linked_to_falling(obj); return; }
    ship_vitality_shown(obj, v);
    if (bit8(obj + S_STATUS, 3)) {
        clr8(obj + S_STATUS, 3);
        ShipPickUp(obj);
    }
    if (rd16(rd16(obj + S_PARTNER) + O_STATE) == 0x30) { linked_to_rejoin(obj); return; }
    call_object(rom32(0x016424 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $016448. State 0: still, its partner set; once the stage's start is over, joining (state 4). */
void ShipLinkedStart(uint16_t obj)
{
    wr8(obj + S_HIT, 0);
    wr8(obj + S_SHOT_B, 0);
    wr16(obj + S_SHOT, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr32(obj + S_ANIM, 0);
    ShipLinkedPartner(obj);
    if (rd16(SPACE_STEP)) return;
    if (rd16(obj + O_STATE) == 4) { ShipLinkedJoin(obj); return; }
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0);
    wr16(obj + 0x48, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + S_ANGLE, 0);
    wr16(obj + S_DISTANCE, 0);
    ShipLinkedJoin(obj);
}

/* back to circling (loc_164CC): state 8 */
static void linked_to_fly(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0x80);
    wr32(obj + 0x28, 0xF808F808);
    wr16(obj + O_STATE, 8);
    wr16(obj + 0x48, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    ShipLinkedFly(obj);
}

/* $01649C. State 4: joining, spinning out from the other ship ($18 away, a step every 4th
 * frame). */
void ShipLinkedJoin(uint16_t obj)
{
    add16(obj + S_ANGLE, 0x400);
    if (!(rd16(v_gameFrames) & 3)) {
        uint16_t d = (uint16_t)(rd16(obj + S_DISTANCE) + 1);
        wr16(obj + S_DISTANCE, d);
        if ((int16_t)d >= 0x18) { linked_to_fly(obj); return; }
    }
    ShipLinkedPlace(obj);
    ShipLinkedDrift(obj);
    ShipLinkedAnim(obj);
}

static void linked_to_shoot(uint16_t obj);
static void linked_to_dash(uint16_t obj);

/* $0164F2. State 8: circling the other ship, turned (by $800 a frame at most) toward the pad's
 * way, back to $18 away; Red aims along it, Blue (+$CA) aims by the pad on its own. */
void ShipLinkedFly(uint16_t obj)
{
    if (dash_asked(obj)) { linked_to_dash(obj); return; }
    if (rd8(obj + S_HELD) & 0x10) { linked_to_shoot(obj); return; }
    ShipLinkedAngle(obj);
    ShipLinkedTurn(obj, ShipPadAngle(obj, rd16(obj + S_ANGLE)));
    if (!rd16(obj + S_CHARACTER)) {
        uint16_t a = rd16(obj + S_ANGLE);
        wr16(obj + 6, a);
        wr16(obj + S_AIM, a);
    } else {
        uint16_t a = ShipPadAngle(obj, rd16(obj + S_AIM));
        wr16(obj + S_AIM, a);
        wr16(obj + 6, a);
    }
    ShipLinkedDistance(obj);
    ShipLinkedReach(obj);
    ShipLinkedPlace(obj);
    ShipLinkedDrift(obj);
    ShipLinkedAnim(obj);
}

/* to shooting (loc_16580): state $C */
static void linked_to_shoot(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0x80);
    wr32(obj + 0x28, 0xF808F808);
    wr16(obj + 0x48, 0);
    wr16(obj + S_MUZZLE_X, 0);
    wr16(obj + S_MUZZLE_Y, 0);
    wr16(obj + O_STATE, 0xC);
    clr8(obj + S_SHOT_B, 2);
    set8(obj + S_SHOT_B, 1);
    ShipLinkedShoot(obj);
}

/* $0165B2. State $C: shooting while the weapon fires (Red: its place by the aim). */
void ShipLinkedShoot(uint16_t obj)
{
    if (dash_asked(obj)) { linked_to_dash(obj); return; }
    int red = !rd16(obj + S_CHARACTER);
    PlayerShoot(obj);
    if (!rd16(obj + S_SHOT)) { linked_to_fly(obj); return; }
    if (red) wr16(obj + S_ANGLE, rd16(obj + S_AIM));
    ShipLinkedPlace(obj);
    ShipLinkedDrift(obj);
    ShipLinkedShootAnim(obj);
    ship_muzzle(obj);
}

/* to the dash (loc_16612): state $10 */
static void linked_to_dash(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0x40);
    wr32(obj + 0x2C, 0xF808F808);
    wr16(obj + 0x26, 4);
    ShipLinkedAnim(obj);
    wr16(obj + O_STATE, 0x10);
    wr8(obj + S_SHOT_B, 0);
    wr16(obj + S_SHOT, 0);
    uint16_t a = ShipPadAngle(obj, rd16(obj + 6));
    wr16(obj + 6, a);
    wr16(obj + S_AIM, a);
    ShipDashSpeed(obj);
    wr16(obj + S_TIMER, 0xC);
    ShipLinkedDashParts(obj);
    wr16(obj + 0x48, 1);
    ShipLinkedDash(obj);
}

/* $016662. State $10: the dash, $C frames slowing, its angle and distance followed. */
void ShipLinkedDash(uint16_t obj)
{
    if (dec_zero(obj + S_TIMER)) { linked_to_fly(obj); return; }
    ShipDashSlow(obj);
    ShipLinkedAngle(obj);
    ShipLinkedDistance(obj);
    ShipLinkedDrift(obj);
    ship_dash_together(obj);
}

/* $0166D6. State $14 (nothing sets it): hurt, slowing. */
void ShipLinkedHit(uint16_t obj)
{
    if (dec_neg(obj + S_TIMER)) { linked_to_fly(obj); return; }
    ShipSlow(obj);
    ShipLinkedAngle(obj);
    ShipLinkedDistance(obj);
}

/* $016722. State $18 (nothing sets it): knocked away, then circling (blinking) or falling. */
void ShipLinkedKnocked(uint16_t obj)
{
    if (!dec_neg(obj + S_TIMER)) {
        ShipKnockSlow(obj);
        ShipLinkedAngle(obj);
        ShipLinkedDistance(obj);
        return;
    }
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) { linked_to_falling(obj); return; }
    wr16(obj + S_BLINK, 0x40);
    linked_to_fly(obj);
}

/* to falling (loc_1674C): state $1C */
static void linked_to_falling(uint16_t obj)
{
    if (rd16(obj + O_STATE) == 0x1C) { ShipLinkedFalling(obj); return; }
    wr16(obj + O_STATE, 0x1C);
    QueueSound(0xBC);
    wr8(obj + S_SHOT_B, 0);
    wr16(obj + S_SHOT, 0);
    wr16(obj + O_VITALITY, 0);
    wr16(obj + 0x48, 0);
    wr32(obj + 0x18, 0xFFFF8000);
    wr32(obj + 0x1C, 0x10000);
    clr8(obj + 0x86, 0);
    clr8(obj + 0x86, 1);
    wr8(obj + S_HURT, 0);
    wr8(obj + S_HIT, 0);
    wr8(obj + 0x20, 0xC0);
    ShipLinkedFalling(obj);
}

/* $0167A0. State $1C: falling in smoke below y $160, then out. */
void ShipLinkedFalling(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_Y) > 0x160) { ship_out(obj); return; }
    ShipLinkedSmoke(obj);
    ShipLinkedAngle(obj);
    ShipLinkedDistance(obj);
}

/* to rejoining (loc_167E6): state $20 */
static void linked_to_rejoin(uint16_t obj)
{
    if (rd16(obj + O_STATE) == 0x20) { ShipLinkedRejoin(obj); return; }
    wr16(obj + O_STATE, 0x20);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + S_HURT, 0);
    wr16(obj + 0x48, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    ShipLinkedRejoin(obj);
}

/* $01680C. State $20: the other ship gone, spinning in to it (blinking); at distance 0 this
 * one becomes $3FC. */
void ShipLinkedRejoin(uint16_t obj)
{
    add16(obj + S_ANGLE, 0x400);
    if (dec_zero(obj + S_DISTANCE)) {
        wr16(obj + O_TYPE, 0x3FC);
        wr16(obj + O_STATE, 0);
        return;
    }
    wr16(obj + S_BLINK, 2);
    ShipLinkedPlace(obj);
}

/* $016852. Every 4th frame a puff of smoke. */
void ShipLinkedSmoke(uint16_t obj)
{
    if (rd16(v_gameFrames) & 3) return;
    smoke_puff(obj, 0x056450);
}

/* $016866. +$4A: the other player's slot. */
void ShipLinkedPartner(uint16_t obj)
{
    wr16(obj + S_PARTNER, obj == 0xA600 ? 0xA400 : 0xA600);
}

/* $01687E. +$4C: its angle from the other ship. */
void ShipLinkedAngle(uint16_t obj)
{
    uint16_t p = rd16(obj + S_PARTNER);
    uint32_t dy = (uint16_t)(rd16(obj + O_Y) - rd16(p + O_Y));
    wr16(obj + S_ANGLE, Atan2((int16_t)(rd16(obj + O_X) - rd16(p + O_X)), &dy));
}

/* $01689E. +$4C turned toward d0, by $800 at most. */
void ShipLinkedTurn(uint16_t obj, uint16_t to)
{
    uint16_t a = rd16(obj + S_ANGLE);
    if (to == a) return;
    if ((int16_t)(uint16_t)(to - a) < 0) {
        a = (uint16_t)(a - 0x800);
        wr16(obj + S_ANGLE, (int16_t)(uint16_t)(to - a) < 0 ? a : to);
    } else {
        a = (uint16_t)(a + 0x800);
        wr16(obj + S_ANGLE, (int16_t)(uint16_t)(to - a) < 0 ? to : a);
    }
}

/* $0168CA. +$4E: its distance from the other ship. */
void ShipLinkedDistance(uint16_t obj)
{
    uint16_t p = rd16(obj + S_PARTNER);
    int16_t dx = (int16_t)(rd16(p + O_X) - rd16(obj + O_X));
    int16_t dy = (int16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
    uint32_t d = (uint32_t)((int32_t)dx * dx) + (uint32_t)((int32_t)dy * dy);
    wr16(obj + S_DISTANCE, (uint16_t)Sqrt32(d));
}

/* $0168F0. The distance brought back toward $18, by 4. */
void ShipLinkedReach(uint16_t obj)
{
    int16_t d = (int16_t)rd16(obj + S_DISTANCE);
    if (d == 0x18) return;
    if (d < 0x18) {
        d = (int16_t)(d + 4);
        wr16(obj + S_DISTANCE, d > 0x18 ? 0x18 : (uint16_t)d);
    } else {
        d = (int16_t)(d - 4);
        wr16(obj + S_DISTANCE, d >= 0x18 ? (uint16_t)d : 0x18);
    }
}

/* $016924. The angle of the pad's way (d0 kept with none). */
uint16_t ShipPadAngle(uint16_t obj, uint16_t angle)
{
    uint16_t a = rom16(0x01693E + (uint32_t)((rd8(obj + S_HELD) & 0xF) << 1));
    return (a & 1) ? angle : a;
}

/* $01695E. Its place: at +$4C, +$4E times 4 from the other ship. */
void ShipLinkedPlace(uint16_t obj)
{
    uint16_t p = rd16(obj + S_PARTNER);
    int32_t r = (int16_t)(uint16_t)(rd16(obj + S_DISTANCE) << 2);
    uint16_t i = angle_index(rd16(obj + S_ANGLE));
    uint16_t x = (uint16_t)((uint32_t)(cosine(i) * r) >> 16);
    uint16_t y = (uint16_t)((uint32_t)(sine(i) * r) >> 16);
    wr16(obj + O_X, (uint16_t)(x + rd16(p + O_X)));
    wr16(obj + O_Y, (uint16_t)(y + rd16(p + O_Y)));
}

/* $0169A4. With two players, moved along with the other ship while it dashes. */
void ShipLinkedDrift(uint16_t obj)
{
    if (rd8(v_players) != 3) return;
    uint16_t p = rd16(obj + S_PARTNER);
    if (!rd16(p + 0x48)) return;
    uint32_t vx = rd32(p + 0x18), vy = rd32(p + 0x1C);
    add32(obj + O_X, vx);
    add32(obj + O_Y, vy);
}

/* $0169CC. With two players, while the other ship dashes: pulled toward it ($10 a frame). */
void ShipPull(uint16_t obj)
{
    if (rd8(v_players) != 3) return;
    uint16_t p = rd16(obj + S_PARTNER);
    if (!rd16(p + 0x48)) return;
    uint32_t dy = (uint16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
    uint16_t i = angle_index(Atan2((int16_t)(rd16(p + O_X) - rd16(obj + O_X)), &dy));
    wr32(obj + 0x18, (uint32_t)cosine(i) << 4);
    wr32(obj + 0x1C, (uint32_t)sine(i) << 4);
}

/* $016A28. $540's look by the aim. */
void ShipLinkedAnim(uint16_t obj)
{
    ship_anim(obj, 0x016A50);
}

/* $016A70. $540's look shooting, else ShipLinkedAnim's. */
void ShipLinkedShootAnim(uint16_t obj)
{
    if (!rd16(obj + S_FIRING)) { ShipLinkedAnim(obj); return; }
    ship_anim(obj, 0x016A9E);
}

/* ---- the types after the effects library ($5804A-$592FB) ---- */

static void next(uint16_t obj, uint16_t by) { wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + by)); }
/* cmpi.w #v,(a) then bpl: 1 when the word is v or more (signed) */
static int ge(uint16_t a, uint16_t v) { return (int16_t)(uint16_t)(rd16(a) - v) >= 0; }
/* the object gone (its flags $1000) */
static void gone(uint16_t obj) { wr16(obj + O_FLAGS, 0x1000); }
/* asl.l by a register: its count taken modulo 64 */
static uint32_t asl_l(uint32_t v, uint16_t n)
{
    n &= 63;
    return n >= 32 ? 0 : v << n;
}
/* a turn toward a direction on the $200 circle: 1 to add (the difference d below $100 either
 * way round), 0 to take off */
static int turn_up(uint16_t d)
{
    if ((int16_t)d < 0) return (int16_t)(uint16_t)(d - 0xFF00) < 0;
    return (int16_t)(uint16_t)(d - 0x100) < 0;
}

/* $05804A. Type $174 (no run makes one): a sprite viewer on pad 1 at ($120, $100); Start removes
 * it. Up / down step its tile (held with A, pressed alone); with B up steps the palette and down
 * / right flip it; with C up steps the size, down / right the offsets. */
void SpriteTest(uint16_t obj)
{
    if (bit8(v_padHeld, 7)) { gone(obj); return; }
    wr16(obj + O_FLAGS, 0x8000);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0x100);
    uint16_t t = rd16(obj + 0x0E);
    if (bit8(v_padHeld, 6)) {
        if (bit8(v_padHeld, 0)) t++;
        else if (bit8(v_padHeld, 1)) t--;
    } else if (!bit8(v_padHeld, 4) && !bit8(v_padHeld, 5)) {
        if (bit8(v_padPressed, 0)) t++;
        else if (bit8(v_padPressed, 1)) t--;
    }
    wr16(obj + 0x0E, (uint16_t)((rd16(obj + 0x0E) & 0x1800) | (t & 0x7FF)));
    if (bit8(v_padHeld, 4)) {
        if (bit8(v_padPressed, 0)) add16(obj + 0x48, 0x2000);
        wr16(obj + 0x48, rd16(obj + 0x48) & 0x6000);
        wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(obj + 0x48));
        wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(0x808A));
        if (bit8(v_padPressed, 1)) wr16(obj + 0x0E, rd16(obj + 0x0E) ^ 0x1000);
        if (bit8(v_padPressed, 3)) wr16(obj + 0x0E, rd16(obj + 0x0E) ^ 0x800);
    }
    if (bit8(v_padHeld, 5)) {
        if (bit8(v_padPressed, 0)) add16(obj + 0x08, 0x100);
        wr16(obj + 0x08, rd16(obj + 0x08) & 0xF00);
        if (bit8(v_padPressed, 1)) {
            uint16_t a = rd16(obj + 0x0A);
            wr16(obj + 0x0A, (uint16_t)((a & 0xFF) | ((uint16_t)(a + 0x100) & 0xFF00)));
        }
        if (bit8(v_padPressed, 3)) {
            uint16_t a = rd16(obj + 0x0A);
            wr16(obj + 0x0A, (uint16_t)((a & 0xFF00) | ((a + 1) & 0xFF)));
        }
    }
}

/* $05822C. Type $29C (no run makes one): an invisible wanderer in the box ($E0-$160, $E0-$120),
 * its heading (+$56) turned $10 a frame toward a random one (+$4A; back in at the box's sides),
 * leaving a trail (type $1E4, list $563FE) every 4th frame. */
void Wanderer(uint16_t obj)
{
    Random();
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0xC00);
        uint16_t a = rd16(v_randomSeed) & 0x1FE;
        wr16(obj + 0x4A, a);
        wr16(obj + 0x56, a);
        return;
    }
    uint16_t a = rd16(obj + 0x56), slot, t;
    wr32(obj + 0x1C, (uint32_t)sine(a) << 5);
    wr32(obj + 0x18, (uint32_t)cosine(a) << 5);
    a = (uint16_t)((a + 0x100) & 0x1FE);
    uint32_t vy = (uint32_t)sine(a) << 4, vx = (uint32_t)cosine(a) << 3;
    if (!(rd16(v_gameFrames) & 3) && SlotFreeHigh(&slot, &t)) {
        wr16(slot + O_TYPE, 0x1E4);
        wr32(slot + 0x48, 0x0563FE);
        wr16(slot + 0x54, 0x8C00);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        wr32(slot + 0x1C, vy);
        wr32(slot + 0x18, vx);
    }
    uint16_t to;
    if (ge(obj + O_Y, 0x120)) to = 0x180;
    else if (!ge(obj + O_Y, 0xE0)) to = 0x80;
    else if (ge(obj + O_X, 0x160)) to = 0x100;
    else if (!ge(obj + O_X, 0xE0)) to = 0;
    else to = rd16(v_randomSeed) & 0x1FE;
    wr16(obj + 0x4A, to);
    uint16_t h = rd16(obj + 0x56);
    h = turn_up((uint16_t)(to - h)) ? (uint16_t)(h + 0x10) : (uint16_t)(h - 0x10);
    wr16(obj + 0x56, h & 0x1FE);
}

/* $058366. Type $2A0: a plate (the table at $58370). */
void PressPlate(uint16_t obj)
{
    call_object(rom32(0x058370 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $058378. Its look, boxes and solidity. */
void PressPlateStart(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x8300);
    wr16(obj + 0x0E, 0x46B5);
    wr16(obj + 0x08, 0xF00);
    wr16(obj + 0x0A, 0xF0E8);
    wr8(obj + 0x21, 0xA4);
    wr32(obj + 0x2C, 0xEC08E818);
    wr32(obj + 0x28, 0xEC10E41C);
    wr8(obj + 0x46, 0x8D);
    wr8(obj + 0x20, 0xA4);
}

/* $0583B8. Pressed (stood on, +$47 bit 4): its sprite 2 lower. */
void PressPlateLook(uint16_t obj)
{
    wr16(obj + 0x0A, bit8(obj + 0x47, 4) ? 0xF0EA : 0xF0E8);
}

/* $0583D2. Type $2B4: a solid box (by +6: the long at $58432, or the code after it), then by +6
 * (the table at $5842A) gone with the object at $A800, or still. */
void Blocker(uint16_t obj)
{
    if (rd16(obj + O_STATE)) {
        call_object(rom32(0x05842A + (uint32_t)(int16_t)rd16(obj + 6)), obj);
        return;
    }
    next(obj, 4);
    wr32(obj + 0x2C, rom32(0x058432 + (uint32_t)(int16_t)rd16(obj + 6)));
    wr32(obj + 0x28, rom32(0x058432 + (uint32_t)(int16_t)rd16(obj + 6)));
    wr16(obj + O_FLAGS, 0x300);
    wr8(obj + 0x21, 0xA4);
    wr8(obj + 0x46, 0x8F);
    wr16(obj + O_FLAGS, 0x8300);
    wr16(obj + 0x0E, 0x84B5);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
}

/* $058436. Gone when the object at $A800 is. */
void BlockerWatch(uint16_t obj)
{
    if (!rd16(0xA800)) gone(obj);
}

/* $058446. Nothing. */
void BlockerStill(uint16_t obj)
{
    (void)obj;
}

/* $058448. Type $2BC: from the camera's step $20, the camera's y bobbed ($210-$21F, back and
 * forth every $80 frames); gone when the colour at $E400 drops below $EB70 with the camera at
 * $220. */
void CameraBob(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        if (!ge(v_cameraStep, 0x20)) return;
        wr16(obj + 0x48, 0x80);
        next(obj, 4);
        return;
    }
    if (!ge(0xE400, 0xEB70) && rd16(v_camY) == 0x220) { gone(obj); return; }
    add16(obj + 0x48, 1);
    uint16_t d = (uint16_t)((rd16(obj + 0x48) & 0x78) >> 3);
    if (bit8(obj + 0x49, 7)) d = (uint16_t)((d ^ 0xF) + 1);
    d = (uint16_t)(d + 0x210);
    wr16(v_camY, d);
    wr16(0xA924, d);
}

/* landed (loc_58558): type $354 */
static void charge_land(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x354);
    wr16(obj + O_STATE, 0);
}

/* $0584AE. Type $56C: a charge dropped during the boss's time: falling until it lands (type
 * $354), or the time is up or it is beaten; hit by a player it becomes a bomb ($98). */
void FallingCharge(uint16_t obj)
{
    if (rd16(obj + O_STATE)) {
        if (!(rd16(v_bossTimer) & 0x8000) || (rd16(obj + O_VITALITY) & 0x8000)) { charge_land(obj); return; }
        if (bit8(obj + 0x23, 1)) {
            wr16(obj + O_TYPE, 0x98);
            wr16(obj + O_STATE, 0);
            Bomb(obj);
            return;
        }
    }
    call_object(rom32(0x0584E8 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $0584F0. State 0: gone when the boss's time is not running; its look, vitality $96, then by
 * +$48 (the table at $5852C) its start. */
void FallingChargeStart(uint16_t obj)
{
    if (!(rd16(v_bossTimer) & 0x8000)) { set8(obj + O_FLAGS, 4); return; }
    wr16(obj + O_FLAGS, 0xEF80);
    wr16(obj + 0x0E, 0);
    wr32(obj + 0x08, 0x0E0B2E);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_VITALITY, 0x96);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x28, 0xFC04FC04);
    call_object(rom32(0x05852C + (uint32_t)(int16_t)rd16(obj + 0x48)), obj);
}

/* $058538. Dropped: falling at $1.C. */
void FallingChargeDrop(uint16_t obj)
{
    next(obj, 4);
    wr32(obj + 0x1C, 0x1C000);
}

/* $058546. State 4: landed on anything solid. */
void FallingChargeFall(uint16_t obj)
{
    if (MapAttribute(rd16(obj + O_X), rd16(obj + O_Y))) charge_land(obj);
}

/* $058562. Type $3C4: once the camera is not held, a wall broken: the pieces ($8C, 9 from $40
 * at ($190, $E8)), two chunks of the plane redrawn, the sprites shaken, a message, its sound. */
void WallBreak(uint16_t obj)
{
    if (bit8(v_cameraHold, 7)) return;
    wr16(obj + O_TYPE, 0x8C);
    wr16(obj + 0x48, 0x40);
    wr16(obj + 0x4A, 8);
    wr16(obj + 0x4C, 8);
    wr16(obj + O_X, 0x190);
    wr16(obj + O_Y, 0xE8);
    ChunkDma(0xFFFF4660, 0xC430);
    ChunkDma(0xFFFF4700, 0xC630);
    wr16(v_shakeSprites, 0x10);
    wr16(0x80C2, 0x5C);
    wr16(0x80C6, 0x4000);
    wr16(0x80D2, 0x15C);
    wr16(0x80D4, 0xC8);
    QueueSound(0xBE);
}

/* $05864E. Type $568: the vitality an item gave (+$50: 15, 20, 40) shown rising for $1E
 * frames. */
void AmountPopup(uint16_t obj)
{
    if (rd16(obj + O_STATE)) {
        if (dec_neg(obj + 0x4A)) { set8(obj + O_FLAGS, 4); return; }
        add32(obj + 0x1C, 0x1000);
        return;
    }
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xC480);
    wr16(obj + 0x0E, 0x87B4);
    wr8(obj + 0x20, 0);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0xFFFE4000);
    wr16(obj + 0x4A, 0x1E);
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10));
    uint16_t n = rd16(obj + 0x50);
    wr32(obj + 0x08, n == 0x28 ? 0x0E0B76 : n == 0x14 ? 0x0E0B6A : n == 0xF ? 0x0E0B5E : 0x0E0B52);
}

/* $0586E4. Type $570: a homing shot after its target (+$48), $.4 a frame each way, its speeds
 * kept to 8 sideways and 4 down (the down limit tested by the sideways speed's sign: kept);
 * with the stage's flag ($8080 bit 0) a shot of type $54. */
void Homer(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0x8D80);
        wr16(obj + 0x0E, 0x46A0);
        wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(0x808A));
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
        wr8(obj + 0x21, 0x20);
        wr32(obj + 0x28, 0xF20EF20E);
    }
    if (bit8(0x8080, 0)) {
        wr16(obj + O_TYPE, 0x54);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        return;
    }
    uint16_t t = rd16(obj + 0x48);
    uint16_t tx = rd16(t + O_X), ty = rd16(t + O_Y);
    if ((int16_t)(uint16_t)(tx - rd16(obj + O_X)) < 0) {
        if (!(rd16(obj + 0x18) & 0x8000) || (int32_t)(rd32(obj + 0x18) - 0xFFF80000u) >= 0) add32(obj + 0x18, 0xFFFFC000);
    } else {
        if ((rd16(obj + 0x18) & 0x8000) || (int32_t)(rd32(obj + 0x18) - 0x80000u) < 0) add32(obj + 0x18, 0x4000);
    }
    if ((int16_t)(uint16_t)(ty - rd16(obj + O_Y)) < 0) {
        if (!(rd16(obj + 0x1C) & 0x8000) || (int32_t)(rd32(obj + 0x1C) - 0xFFFC0000u) >= 0) add32(obj + 0x1C, 0xFFFFC000);
        return;
    }
    if ((rd16(obj + 0x18) & 0x8000) || (int32_t)(rd32(obj + 0x1C) - 0x40000u) < 0) add32(obj + 0x1C, 0x4000);
}

/* $0587CA. Type $2F8: a spark spiralling in to (+$48, +$4A): from a random angle (+$56) and
 * distance ($8-$27), nearer by +$4E (one more every 4th frame), smaller as it comes; the axes
 * scaled by the shifts +$52 (x) and +$54 (y). */
void SpiralIn(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        add16(obj + O_STATE, 1);
        wr16(obj + O_FLAGS, 0x8100);
        wr8(obj + 0x20, 0xBC);
        wr16(obj + 0x56, rd16(v_randomSeed));
        wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
        wr16(obj + 0x4C, (uint16_t)((rd8(v_randomSeed) & 0x1F) + 8));
        return;
    }
    uint16_t r = rd16(obj + 0x4C);
    if ((int16_t)(uint16_t)(r - 0x18) >= 0) {
        wr16(obj + 0x0E, 0x7FE);
        wr16(obj + 0x08, 0x100);
        wr16(obj + 0x0A, 0xFCF8);
    } else if ((int16_t)(uint16_t)(r - 0xC) >= 0) {
        wr16(obj + 0x0E, 0x7FD);
        wr16(obj + 0x08, 0);
        wr16(obj + 0x0A, 0xFCFC);
    } else {
        wr16(obj + 0x0E, 0x58F);
        wr16(obj + 0x08, 0);
        wr16(obj + 0x0A, 0xFCFC);
    }
    if (!bit8(0xA001, 2)) wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x6000);
    r = (uint16_t)(r - rd16(obj + 0x4E));
    if (r & 0x8000) { gone(obj); return; }
    wr16(obj + 0x4C, r);
    if (!(rd16(v_gameFrames) & 3)) add16(obj + 0x4E, 1);
    uint16_t a = rd16(obj + 0x56);
    uint32_t y = asl_l((uint32_t)(sine(a) * (int16_t)r), rd16(obj + 0x54));
    uint32_t x = asl_l((uint32_t)(cosine(a) * (int16_t)r), rd16(obj + 0x52));
    wr16(obj + O_X, (uint16_t)((x >> 16) + rd16(obj + 0x48)));
    wr16(obj + O_Y, (uint16_t)((y >> 16) + rd16(obj + 0x4A)));
}

/* broken (loc_58AA4): a big burst of smoke (three rings), the sprites shaken, 3 frames of a
 * harmful blast. The 68k's second and third bursts take a1 as the first left it: the sine
 * table, unless a puff's slot was taken (all the high slots full), when RadialBurst leaves a1
 * after that slot; the C passes the sine table (RadialBurst returns no a1). */
static void crate_burst(uint16_t obj)
{
    QueueSound(0xC5);
    wr16(v_shakeSprites, 0x10);
    next(obj, 4);
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0x40);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x2C, 0x88788878);
    wr16(obj + 0x26, 0x168);
    wr16(obj + 0x48, 3);
    uint32_t a1 = SINES;            /* set once: each burst starts from the a1 the last left */
    RadialBurst(obj, &a1, COSINES, 0, 0x1E0, 0x18, 0x18, 0x0563EE, 0x1FE, 8);
    RadialBurst(obj, &a1, COSINES, 0x1F0, 0x1E0, 0x10, 0x10, 0x0563EE, 0x1FE, 7);
    RadialBurst(obj, &a1, COSINES, 0, 0x1C0, 8, 8, 0x0563EE, 0x1FE, 4);
}

/* $0588C2. Type $3DC: a solid crate (vitality $50): gone left of x $80; broken, it bursts. */
void Crate(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0xC100);
        wr16(obj + 0x0E, 0x280);
        wr32(obj + 0x08, 0x0E2AB6);
        wr8(obj + 0x21, 0xA4);
        wr32(obj + 0x2C, 0xEC08E818);
        wr32(obj + 0x28, 0xEC10E41C);
        wr8(obj + 0x46, 0x8D);
        wr8(obj + 0x20, 0xA4);
        wr16(obj + O_VITALITY, 0x50);
        return;
    }
    if (rd16(obj + O_STATE) == 8) {
        if (dec_neg(obj + 0x48)) gone(obj);
        return;
    }
    if (!ge(obj + O_X, 0x80)) { gone(obj); return; }
    if (rd16(obj + O_VITALITY) & 0x8000) crate_burst(obj);
}

/* $058934. Type $418: a lifter carrying a crate (the object at $DFA0, type $10): rolling in,
 * raising it, pushing, lowering, waiting (the table at $5895E); gone with it left of x $70. */
void Lifter(uint16_t obj)
{
    if (rd16(obj + O_STATE) && !ge(obj + O_X, 0x70)) {
        gone(obj);
        wr16(0xDFA2, 0x1000);
        return;
    }
    call_object(rom32(0x05895E + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $058976. Its look, at y $140, the crate set up 4 above it. */
void LifterStart(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xE900);
    wr16(obj + 0x0E, 0xB00);
    wr32(obj + 0x08, 0x0DFC8E);
    wr8(obj + 0x20, 0xB0);
    wr16(obj + O_Y, 0x140);
    wr16(obj + 0x4A, 0xFFFC);
    uint16_t c = 0xDFA0;
    wr16(c + O_TYPE, 0x10);
    wr16(c + O_FLAGS, 0xC000);
    wr16(c + 0x0E, 0x280);
    wr32(c + 0x08, 0x0E2AB6);
    wr8(c + 0x21, 0xA4);
    wr32(c + 0x2C, 0xEC08E818);
    wr32(c + 0x28, 0xEC10F010);
    wr8(c + 0x46, 0x8D);
    wr8(c + 0x20, 0xA4);
}

/* $0589E0. State 4: until it has rolled left of x $14E. */
void LifterRoll(uint16_t obj)
{
    LifterCarry(obj);
    if (!ge(obj + O_X, 0x14E)) next(obj, 4);
}

/* $0589F4. State 8: the crate raised to $12 above; then pushing left for $60 frames. */
void LifterRaise(uint16_t obj)
{
    LifterCarry(obj);
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (rd16(obj + 0x4A) != 0xFFEE) return;
    next(obj, 4);
    wr32(obj + 0x08, 0x0DF2D0);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0xFFFEC000);
    wr16(obj + 0x48, 0x60);
}

/* $058A28. State $C: pushing; then still for $100 frames. */
void LifterPush(uint16_t obj)
{
    LifterCarry(obj);
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x08, 0x0DFC8E);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x48, 0x100);
}

/* $058A56. State $10: the crate lowered back to 4 above. */
void LifterLower(uint16_t obj)
{
    LifterCarry(obj);
    add16(obj + 0x4A, 1);
    if (rd16(obj + 0x4A) == 0xFFFC) next(obj, 4);
}

/* $058A6E. State $14: waiting, then raising again (state 8). */
void LifterWait(uint16_t obj)
{
    LifterCarry(obj);
    if (dec_neg(obj + 0x48)) wr16(obj + O_STATE, 8);
}

/* $058A82. The crate kept on it (4 left, +$4A above), moving with it. */
void LifterCarry(uint16_t obj)
{
    uint16_t c = 0xDFA0;
    wr16(c + O_X, rd16(obj + O_X));
    wr16(c + O_X, (uint16_t)(rd16(c + O_X) - 4));
    wr16(c + O_Y, (uint16_t)(rd16(obj + O_Y) + rd16(obj + 0x4A)));
    wr32(c + 0x18, rd32(obj + 0x18));
}

/* an item giving `amount` of vitality (type $68) thrown up from it (DropItem's loop, loc_5861C,
 * with d7 as the caller set it) */
static void drop_amount(uint16_t obj, uint16_t amount)
{
    uint16_t slot;
    if (!SlotFreeLow(&slot)) return;
    wr16(slot + O_TYPE, 0x68);
    wr16(slot + 0x50, amount);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x1C, 0xFFFD0000);
    wr16(slot + O_FLAGS, 0x8F00);
    wr16(slot + 6, 0);
}

/* broken (loc_58C92): points, the sprites shaken, a puff ($2F4); with +$59 bit 0 an item of 10 */
static void stack_block_broken(uint16_t obj)
{
    AddScore(obj, 0x1993);
    wr16(v_shakeSprites, 8);
    wr8(obj + 0x21, 0);
    wr16(obj + O_TYPE, 0x2F4);
    wr16(obj + O_STATE, 0);
    add16(obj + O_Y, 0x10);
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 6));
    if (bit8(obj + 0x59, 0)) drop_amount(obj, 10);
}

/* $058B3A. Type $3E0: a block of a stack: gone left of x $80, broken when beaten; else resting
 * or falling (the table at $58B56). */
void StackBlock(uint16_t obj)
{
    if (!ge(obj + O_X, 0x80)) { gone(obj); return; }
    if (rd16(obj + O_VITALITY) & 0x8000) { stack_block_broken(obj); return; }
    call_object(rom32(0x058B56 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $058B62. Its look and box, vitality $27F (+6 bit 13: an item when broken, +$58); unless +6 bit
 * 15, the stack built on the floor at y $144: +6 (bits 0-12) more blocks above, $18 apart, a
 * step $10 left each with bit 14 (then it is $40 right); the top one marked $A000. */
void StackBlockStart(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x8500);
    wr16(obj + 0x0E, 0x2280);
    wr16(obj + 0x08, 0xF00);
    wr16(obj + 0x0A, 0xF0F0);
    wr8(obj + 0x21, 0xAC);
    wr32(obj + 0x2C, 0xF40CF010);
    wr32(obj + 0x28, 0xF40CF010);
    wr8(obj + 0x46, 0x8F);
    wr16(obj + O_VITALITY, 0x27F);
    wr16(obj + 0x58, 0);
    if (bit8(obj + 6, 5)) {
        clr8(obj + 6, 5);
        wr16(obj + 0x58, 1);
    }
    uint16_t n = rd16(obj + 6);
    if (n & 0x8000) return;
    if (n & 0x4000) {
        n &= 0xBFFF;
        add16(obj + O_X, 0x40);
    }
    wr8(obj + 0x20, 0xA4);
    uint8_t pri = 0xA4;
    wr16(obj + O_Y, 0x144);
    uint16_t y = rd16(obj + O_Y), x = rd16(obj + O_X), slot = 0, t;
    do {
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x3E0);
        if (bit8(obj + 6, 6)) x = (uint16_t)(x - 0x10);
        wr16(slot + O_X, x);
        y = (uint16_t)(y - 0x18);
        wr16(slot + O_Y, y);
        pri = (uint8_t)(pri - 4);
        wr8(slot + 0x20, pri);
        wr16(slot + 6, 0x8000);
    } while (n-- != 0);
    wr16(slot + 6, 0xA000);
}

/* $058C24. State 4: resting (on the floor at $144, or on a block, +$47 bit 5); else falling. */
void StackBlockRest(uint16_t obj)
{
    if (ge(obj + O_Y, 0x144)) {
        wr16(obj + O_Y, 0x144);
        wr8(obj + 0x20, 0xA4);
        return;
    }
    if (!bit8(obj + 0x47, 5)) next(obj, 4);
}

/* $058C4C. State 8: falling (by $.3C00 a frame) onto a block or the floor: a thud, the sprites
 * shaken. */
void StackBlockFall(uint16_t obj)
{
    add32(obj + 0x1C, 0x3C00);
    if (!bit8(obj + 0x47, 5)) {
        if (!ge(obj + O_Y, 0x144)) return;
        wr16(obj + O_Y, 0x144);
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
    wr32(obj + 0x1C, 0);
    wr16(v_shakeSprites, 8);
    QueueSound(0xD0);
}

/* $058CCC. Type $3E4: a solid thing that, knocked down or hit, gives 48 points and flies off
 * blinking. */
void Kickaway(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0xC300);
        wr16(obj + 0x0E, 0xA80);
        wr32(obj + 0x08, 0x0E2AC2);
        wr8(obj + 0x21, 0xA4);
        wr32(obj + 0x2C, 0xF602FC12);
        wr32(obj + 0x28, 0xEC10FC12);
        wr8(obj + 0x46, 0x8D);
        wr8(obj + 0x20, 0xA4);
        return;
    }
    if (rd16(obj + O_STATE) == 8) {
        add32(obj + 0x1C, 0x4000);
        if (bit8(0xA001, 0)) clr8(obj + O_FLAGS, 7);
        else set8(obj + O_FLAGS, 7);
        return;
    }
    if (!bit8(obj + 0x23, 4) && !bit8(obj + 0x22, 5)) return;
    AddScore(obj, 48);
    next(obj, 4);
    wr32(obj + 0x1C, 0xFFFE0000);
    wr32(obj + 0x18, 0x38000);
    wr8(obj + 0x21, 0);
    wr16(obj + O_FLAGS, 0xCF00);
}

/* $058D7A. Type $414: the bomber's top (at $C9E0), kept while the bomber marks it (its +$48 bit
 * 8, cleared each frame). */
void BomberTop(uint16_t obj)
{
    int kept = bit8(obj + 0x48, 0);
    clr8(obj + 0x48, 0);
    if (!kept) gone(obj);
}

/* $058D8C. Type $40C: the bomber (in the boss's slot, its top at $C9E0): flying, dropping mines,
 * falling when beaten (points), or leaving with the stage's flag ($8080 bit 0). The states'
 * table at $58E16. */
void Bomber(uint16_t obj)
{
    set8(0xCA28, 0);
    if (bit8(0x8080, 0)) {
        wr16(obj + O_STATE, 0x10);
        wr32(obj + 0x1C, 0xFFF80000);
        wr8(obj + 0x21, 0);
    } else {
        int hit = bit8(obj + 0x22, 1);
        clr8(obj + 0x22, 1);
        if (hit || (rd16(obj + O_VITALITY) & 0x8000)) {
            AddScore(obj, 0x8350);
            wr16(obj + O_STATE, 0xC);
            wr16(obj + O_VITALITY, 0);
            wr8(obj + 0x21, 0);
            add32(obj + 0x1C, 0xFFFF0000);
            wr32(obj + 0x08, 0x0E2AA4);
            wr32(0xC9E8, 0x0DFF4E);
            wr16(0xC9EC, 0);
            QueueSound(0xEF);
        }
    }
    call_object(rom32(0x058E16 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $058E2A. State 0: its look and box, vitality $37D, at ($160, $80) heading $80; its top. */
void BomberStart(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xCF00);
    wr16(obj + 0x0E, 0x280);
    wr32(obj + 0x08, 0x0E2A8C);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x2C, 0xF010E818);
    wr32(obj + 0x28, 0xF010E818);
    wr8(obj + 0x46, 0x8F);
    wr8(obj + 0x20, 0x70);
    wr16(obj + O_VITALITY, 0x37D);
    wr16(obj + 0x48, 0x120);
    wr16(obj + O_X, 0x160);
    wr16(obj + O_Y, 0x80);
    wr16(obj + 0x50, 0x80);
    uint16_t top = 0xC9E0;
    wr16(top + O_TYPE, 0x414);
    wr16(top + 0x0E, 0x300);
    wr16(top + O_FLAGS, 0xE000);
    wr32(top + 0x08, 0x0DF81A);
    wr16(top + 0x0C, 0);
    wr8(top + 0x20, 0x74);
}

/* $058EAC. State 4: flying about the box ($100-$160, $B8-$E0) for +$48 frames, its heading
 * (+$50) wandering by +$4E (a random step every 8th frame) or turned back in by 6; then still
 * for a time by the difficulty (the words at $58EDE). */
void BomberFly(uint16_t obj)
{
    BomberTopFollow(obj);
    if (dec_neg(obj + 0x48)) {
        next(obj, 4);
        wr16(obj + 0x48, rom16(0x058EDE + (uint32_t)(int16_t)rd16(v_difficulty)));
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        return;
    }
    uint16_t a = rd16(obj + 0x50), to;
    int edge = 1;
    if (!ge(obj + O_X, 0x100)) to = 0;
    else if (ge(obj + O_X, 0x160)) to = 0x100;
    else if (!ge(obj + O_Y, 0xB8)) to = 0x80;
    else if (ge(obj + O_Y, 0xE0)) to = 0x180;
    else edge = 0, to = 0;
    if (edge) {
        a = turn_up((uint16_t)(to - rd16(obj + 0x50))) ? (uint16_t)(a + 6) : (uint16_t)(a - 6);
    } else {
        a = (uint16_t)(a + rd16(obj + 0x4E));
        if (!(rd16(v_gameFrames) & 7)) {
            uint16_t d = (uint16_t)((rd16(v_randomSeed) & 0xE) + 2);
            if (bit8(0xFF09, 7)) d = (uint16_t)((d ^ 0xFFFF) + 1);
            wr16(obj + 0x4E, d);
        }
    }
    a &= 0x1FE;
    wr16(obj + 0x50, a);
    wr32(obj + 0x1C, (uint32_t)sine(a) << 2);
    wr32(obj + 0x18, (uint32_t)cosine(a) << 4);
}

/* $058FBA. State 8: still, a mine (type $410) dropped at $10 and 0 frames left (thrown aside
 * by the difficulty); then flying again for $100 frames. */
void BomberDrop(uint16_t obj)
{
    BomberTopFollow(obj);
    uint16_t t = (uint16_t)(rd16(obj + 0x48) - 1), slot, ty;
    wr16(obj + 0x48, t);
    if (t & 0x8000) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
        wr16(obj + 0x48, 0x100);
        return;
    }
    if ((int16_t)(uint16_t)(t - 0x11) >= 0 || (t & 0xF)) return;
    if (!SlotFreeMid(&slot, &ty)) return;
    wr16(slot + O_TYPE, 0x410);
    wr16(slot + 0x48, 0x20);
    wr32(slot + 0x1C, 0xFFFC8000);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) - 8));
    if (!rd16(v_difficulty)) return;
    wr32(slot + 0x18, bit8(obj + 0x49, 4) ? 0x14000 : 0xFFFEC000);
}

/* $059038. State $C: beaten, falling in smoke to the floor ($140): there a harmful blast ($2F4),
 * the sprites shaken; flipped by the frame. */
void BomberFall(uint16_t obj)
{
    BomberTopFollow(obj);
    uint16_t slot, t;
    if (!(rd16(v_gameFrames) & 3) && SlotFreeHigh(&slot, &t)) {
        wr16(slot + O_TYPE, 0x100);
        wr32(slot + 0x48, 0x0563EE);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        wr32(slot + 0x1C, 0xFFFF4000);
    }
    add32(obj + 0x1C, 0x4000);
    if (ge(obj + O_Y, 0x140)) {
        wr16(obj + 0x26, 0x8C);
        wr8(obj + 0x21, 0x40);
        wr8(obj + 0x23, 0x20);
        wr32(obj + 0x2C, 0xC040C040);
        wr16(obj + O_TYPE, 0x2F4);
        wr16(obj + O_STATE, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, 0x140);
        wr16(v_shakeSprites, 0xA);
    }
    if (bit8(0xA001, 2)) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
}

/* $0590DE. State $10: leaving (a branch to BomberTopFollow). */
void BomberGone(uint16_t obj)
{
    BomberTopFollow(obj);
}

/* $0590E2. Its top kept $10 above it, every $20 frames facing the way it flies. */
void BomberTopFollow(uint16_t obj)
{
    uint16_t top = 0xC9E0;
    wr16(top + O_X, rd16(obj + O_X));
    wr16(top + O_Y, rd16(obj + O_Y));
    wr16(top + O_Y, (uint16_t)(rd16(top + O_Y) - 0x10));
    if (rd16(v_gameFrames) & 0x1F) return;
    if (bit8(obj + 0x18, 7)) clr8(top + 0x0E, 3);
    else set8(top + 0x0E, 3);
}

/* $05911E. Type $3E8: a message (its script by +6, from the longs at $59148), then gone. */
void MessageTrigger(uint16_t obj)
{
    gone(obj);
    wr32(0x80C8, rom32(0x059148 + (uint32_t)(int16_t)rd16(obj + 6)));
    wr16(0x80C2, 0x28);
    wr16(0x80C6, 0x60);
    wr16(0x80C4, 8);
}

/* $05914C. Type $410: a mine the bomber drops (the table at $59156). */
void DroppedMine(uint16_t obj)
{
    call_object(rom32(0x059156 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $059166. Its look and box. */
void DroppedMineStart(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x0E, 0x66A0);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr16(obj + O_FLAGS, 0x8F00);
    wr16(obj + O_VITALITY, 0);
    wr32(obj + 0x2C, 0xFC04FC04);
    wr32(obj + 0x28, 0xF808F808);
    wr16(obj + 0x26, 5);
}

/* blown up (loc_592D8): pieces (type $8C, 9 from 0, $10 apart) */
static void mine_burst(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x8C);
    wr16(obj + O_STATE, 0);
    wr16(obj + 0x48, 0);
    wr16(obj + 0x4A, 8);
    wr16(obj + 0x4C, 0x10);
    wr8(obj + 0x21, 0);
}

/* $0591A0. State 4: falling for +$48 frames, then still for $10. */
void DroppedMineFall(uint16_t obj)
{
    if (DroppedMineHit(obj)) { mine_burst(obj); return; }
    add32(obj + 0x1C, 0x4000);
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0x10);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

/* $0591D4. State 8: then harmful, flung off across the way to a player (speed $10) and pulled
 * toward it (+$50, +$54 added each frame). */
void DroppedMineAim(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr8(obj + 0x21, 0x30);
    wr8(obj + 0x23, 0x20);
    uint16_t p = PlayerTarget();
    uint32_t dy = (uint16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(p + O_X) - rd16(obj + O_X)), &dy);
    a = (uint16_t)((int16_t)a >> 7) & 0x1FE;
    wr32(obj + 0x50, (uint32_t)sine(a));
    wr32(obj + 0x54, (uint32_t)cosine(a));
    a = (uint16_t)((a + 0x100) & 0x1FE);
    wr32(obj + 0x1C, (uint32_t)sine(a) << 4);
    wr32(obj + 0x18, (uint32_t)cosine(a) << 4);
    QueueSound(0xD4);
}

/* $05925A. State $C: flying, its trail, its pull added. */
void DroppedMineFly(uint16_t obj)
{
    DroppedMineTrail(obj);
    if (DroppedMineHit(obj)) { mine_burst(obj); return; }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + rd32(obj + 0x50));
    wr32(obj + 0x18, rd32(obj + 0x18) + rd32(obj + 0x54));
}

/* $059280. Whether it blows up (!Z): hurt, touched (+$22 bit 4), or in a wall (attribute bit
 * 3). */
int DroppedMineHit(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) || bit8(obj + 0x22, 4)) return 1;
    return (MapAttribute(rd16(obj + O_X), rd16(obj + O_Y)) & 8) != 0;
}

/* $0592AA. On even frames a glint (type $100, list $56580) left behind. */
void DroppedMineTrail(uint16_t obj)
{
    uint16_t slot, t;
    if (bit8(0xA001, 0) || !SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x056580);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* ---- type $130: the strider, a boss on two legs built of the objects from $C980 (it is in the
 * boss's slot): its body, the legs' segments ($D0A0, $D2E0 the feet; +$54, +$56 the legs'
 * angles, +$58 the foot it stands on), a hand on a chain (the slots from $C9E0, its trail in the
 * lists at $9000 and $9400), its gun ($DEE0), the pilot ($DE80, risen by $DEC8), its face drawn
 * in plane A. $CA8C: the side it faces (0 right, 2 left), $CC14 its body's bob ---- */

#define STR_SIDE     0xCA8C      /* 0 facing right, 2 left */
#define STR_SIDE_TO  0xCC0C      /* the side to turn to */
#define STR_HURT     0xCC10      /* frames of the hurt face; negative none */
#define STR_FLASH    0xCC18      /* frames to the next flash of the hurt colours */
#define STR_STEPS    0xCC12      /* steps taken (bit 0 picks a face) */
#define STR_BOB      0xCC14      /* the body's bob (-7 to 1) */
#define STR_ATTACK   0xCBB6      /* the arm's attack by its place: 0, 4 whip, 8 grab, $C shoot */
#define STR_SPACING  0xCBAC      /* the chain's spacing in its trail (bytes) */
#define STR_TIME     0xCBAE      /* the arm's timer */
#define STR_TURN     0xCBB0      /* the hand's turn each frame */
#define STR_TARGET   0xCBB2      /* the hand's target (a player), or a time */
#define STR_GUN_TIME 0xCBB4      /* frames to the gun's next shot */
#define STR_SCORE    0xCC0E      /* frames to the hand's next point */
#define STR_PILOT    0xDEC8      /* the pilot's rise (0 down, -$24 up) */
#define HAND         0xC9E0      /* the hand, the chain's first object */
#define HAND_DIR     0xCA8E      /* its direction (a table index) */
#define HAND_X       0xCA90      /* longs: its place along the trail */
#define HAND_Y       0xCA94
#define TRAIL_X      0x9000      /* the trail's x and y (words, newest first; $7650 empty) */
#define TRAIL_Y      0x9400
#define TRAIL_EMPTY  0x7650
#define RANK         0xA20C      /* the difficulty's step (0-$E, by 2) */

static void next6(uint16_t obj) { add16(obj + 6, 4); }

/* the parts follow (loc_597CC): the plane, the legs placed, all moved with the body */
static void strider_follow_self(uint16_t obj)
{
    StriderScroll(obj);
    StriderLegsPlace(obj);
    StriderShiftBy(obj, 0xC980, 0x1E);
}

/* the feet's looks by their height (loc_5A296): raised (above y $140) or down */
static void strider_feet_look(void)
{
    wr32(0xD0A8, (int16_t)(rd16(0xD0B4) - 0x140) < 0 ? 0x0E165A : 0x0E164E);
    wr32(0xD2E8, (int16_t)(rd16(0xD2F4) - 0x140) < 0 ? 0x0E165A : 0x0E164E);
}

/* loc_59A68: the arm too, and all moved by the body's x and the right foot's y */
static void strider_follow_feet(uint16_t obj)
{
    StriderScroll(obj);
    StriderArm(obj);
    StriderLegsPlace(obj);
    StriderShiftXY(obj, 0xC980, 0xD2E0, 0x1E);
    strider_feet_look();
}

/* loc_59A88: the arm too, all moved with the body */
static void strider_follow_arm(uint16_t obj)
{
    StriderScroll(obj);
    StriderArm(obj);
    StriderLegsPlace(obj);
    StriderShiftBy(obj, 0xC980, 0x1E);
    strider_feet_look();
}

/* $0592FC. Type $130: the strider. Beaten (its vitality below 0, not the start's $8573): points,
 * the boss's time stopped, falling apart (state $34); hit, the hurt face and its colours
 * flashing; then its state (the table at $593E4). */
void Strider(uint16_t obj)
{
    if (rd16(obj + O_STATE)) {
        uint16_t v = rd16(obj + O_VITALITY);
        if ((v & 0x8000) && v != 0x8573) {
            wr8(obj + 0x21, 0);
            wr16(obj + O_VITALITY, 0);
            wr16(obj + O_STATE, 0x34);
            wr16(obj + O_FLAGS, 0);
            wr16(v_bossTimer, 2);
            wr16(obj + 0x50, HAND);
            wr16(obj + 0x52, 0);
            wr16(obj + 6, 0x180);
            wr16(obj + 0x5A, rd16(STR_SIDE));
            if (bit8(0xDEE2, 7)) {
                wr16(0xDEE0, 0x20C);
                wr16(0xDEE2, 0xCC00);
                wr16(0xDF28, 0x40);
                wr16(0xDF2A, 0x180);
            }
            AddScore(obj, 0x68000);
            QueueSound(0xF6);
            StriderFaceHurt();
        } else {
            if (!(rd16(STR_FLASH) & 0x8000)) wr16(STR_FLASH, (uint16_t)(rd16(STR_FLASH) - 1));
            uint32_t colours = 0x059434;
            if (bit8(obj + 0x22, 5)) {
                if (rd16(STR_HURT) & 0x8000) wr16(STR_HURT, 8);
                if (rd16(STR_FLASH) & 0x8000) {
                    wr16(STR_FLASH, 3);
                    colours = 0x05943C;
                }
            }
            for (uint16_t i = 0; i < 8; i += 2) {
                wr16((uint16_t)(0xE302 + i), rom16(colours + i));
                wr16((uint16_t)(0xE382 + i), rom16(colours + i));
            }
        }
    }
    call_object(rom32(0x0593E4 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* the face turned left (loc_5AB18; the shared rows after, loc_5AAE6) */
static void plane_rows(void)
{
    PlaneAChunk(0xFFFF3CC0, 0xE618);
    PlaneAChunk(0xFFFF3CE0, 0xE620);
    PlaneAChunk(0xFFFF3D00, 0xE628);
}
static void plane_facing_right(void)
{
    PlaneAChunk(0xFFFF3D20, 0xE218);
    PlaneAChunk(0xFFFF3D80, 0xE418);
    PlaneAChunk(0xFFFF3DA0, 0xE420);
    PlaneAChunk(0xFFFF3DC0, 0xE428);
    plane_rows();
}

/* $059444. State 0: the message, the planes' scroll, the fight's fields; its parts set up (all
 * hidden, their priorities, the legs' and feet's looks), the pilot ($DE80) and its smoke
 * ($DFA0), the priority bits, the face's tiles (the job list at $59608), its face. */
void StriderStart(uint16_t obj)
{
    next(obj, 4);
    wr16(0x80C2, 0x34);
    wr32(0x80C8, 0x039918);
    wr16(0xA908, 0x100);
    wr16(0xA90C, 0);
    wr32(0xA918, 0);
    wr32(0xA91C, 0);
    wr16(STR_ATTACK, 0);
    wr16(STR_SIDE, 0);
    wr16(STR_SIDE_TO, 0);
    wr16(STR_HURT, 0xFFFF);
    wr16(STR_STEPS, 0);
    wr16(STR_BOB, 0);
    wr16(obj + O_FLAGS, 0xD00);
    wr32(obj + 0x28, 0xE837D828);
    wr16(obj + O_VITALITY, 0x8573);
    wr16(obj + 0x4C, 0x28);
    wr16(obj + 0x54, 0xC0);
    wr16(obj + 0x56, 0x40);
    wr16(obj + 0x58, 0xD0A0);
    for (uint16_t p = HAND, k = 0; k < 0x1E; k++, p = (uint16_t)(p + 0x60)) StriderPartHidden(p, 0);
    wr8(0xD0C0, 0xB0);
    wr8(0xD300, 0xB0);
    static const uint16_t low[6] = { 0xD180, 0xD1E0, 0xD240, 0xD3C0, 0xD420, 0xD480 };
    for (int k = 0; k < 6; k++) wr8(low[k], 0xAC);
    static const uint16_t shown[6] = { 0xD162, 0xD1C2, 0xD222, 0xD3A2, 0xD402, 0xD462 };
    for (int k = 0; k < 6; k++) wr16(shown[k], rd16(shown[k]) | 0x8000);
    StriderPartShown(0xD280, 4);
    wr16(0xD28E, rd16(0xD28E) | 0x800);
    StriderPartShown(0xD4C0, 4);
    StriderPartShown(0xD100, 6);
    wr16(0xD10E, rd16(0xD10E) | 0x800);
    StriderPartShown(0xD340, 6);
    StriderFootSetup(0xD0A0);
    StriderFootSetup(0xD2E0);
    wr16(0xD2EE, rd16(0xD2EE) | 0x800);
    uint16_t p = 0xDE80;
    wr16(p + O_TYPE, 0x10);
    wr16(p + 0x0E, 0x399);
    wr16(p + O_FLAGS, 0x4000);
    wr32(p + 0x08, 0x0E17CE);
    wr8(p + 0x20, 0xC0);
    wr16(STR_PILOT, 0);
    p = 0xDFA0;
    wr16(p + O_TYPE, 0x10);
    wr16(p + O_FLAGS, 0x8000);
    wr16(p + 0x0E, 0x6687);
    wr8(p + 0x20, 0xBC);
    StriderPriority();
    uint32_t list = 0x059608;
    NextJob(&list);
    StriderFaceIdle();
    plane_facing_right();
}

/* $0595CC. State 4: once the message and the tiles' job are done, the colours at $E352. */
void StriderWaitJob(uint16_t obj)
{
    if (rd16(0x80C2) || rd16(v_jobKind)) return;
    next(obj, 4);
    wr16(0xE352, 0x28);
    wr16(0xE3D2, 0x28);
    wr16(0xE354, 0xCA4);
    wr16(0xE3D4, 0xCA4);
    wr16(0xE356, 0x620);
    wr16(0xE3D6, 0x620);
}

/* $059622. State 8: a hop up. */
void StriderHop(uint16_t obj)
{
    wr32(obj + 0x1C, 0xFFFD0000);
    wr32(obj + 0x18, 0x1000);
    next(obj, 4);
}

/* $059638. State $C: rising, slowed; at 2.8 down a big leap. */
void StriderRise(uint16_t obj)
{
    add32(obj + 0x1C, 0x1000);
    if (!(rd32(obj + 0x1C) & 0x80000000u) && (int32_t)(rd32(obj + 0x1C) - 0x28000) >= 0) {
        wr32(obj + 0x1C, 0xFFFA0000);
        wr32(obj + 0x18, 0x30000);
        next(obj, 4);
    }
    strider_follow_self(obj);
    strider_feet_look();
}

/* $059668. State $10: leaping, the legs folding; past the top it is harmful and solid. */
void StriderDrop(uint16_t obj)
{
    StriderBobUp();
    StriderLegsIn(obj);
    add32(obj + 0x1C, 0x2000);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) {
        next(obj, 4);
        wr8(obj + 0x21, 0x24);
        wr8(0xD0C1, 0x24);
        wr8(0xD301, 0x24);
        wr8(obj + 0x46, 0x8F);
        wr8(0xD0E6, 0x8F);
        wr8(0xD326, 0x8F);
        wr32(obj + 0x2C, 0xEC2CD828);
    }
    strider_follow_self(obj);
    strider_feet_look();
}

/* $0596B0. State $14: falling to the floor: there a thud, everything shaken, the feet down. */
void StriderLand(uint16_t obj)
{
    StriderBobDown();
    StriderPilot(obj);
    StriderSmoke();
    StriderLegsIn(obj);
    add32(obj + 0x1C, 0x2000);
    if (!(rd32(obj + 0x1C) & 0x80000000u) && (int32_t)(rd32(obj + 0x1C) - 0x6E000) >= 0) {
        QueueSound(0xA0);
        next(obj, 4);
        wr16(v_shakeSprites, 0x10);
        wr16(v_shakePlane, 0x10);
        wr32(obj + 0x1C, 0);
        wr16(0xD0B4, 0x144);
        wr16(0xD2F4, 0x144);
    }
    strider_follow_self(obj);
    strider_feet_look();
}

/* $05970A. State $18: standing up, sliding to a stop, the legs spread back to $C0 / $40. */
void StriderStand(uint16_t obj)
{
    StriderBobRest();
    StriderPilot(obj);
    StriderSmoke();
    wr16(0xD0B4, 0x144);
    wr16(0xD2F4, 0x144);
    StriderDust();
    if (rd32(obj + 0x18)) add32(obj + 0x18, 0xFFFFF000);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 2) & 0x1FE);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 2) & 0x1FE);
    if (rd16(obj + 0x54) == 0xC0) {
        next(obj, 4);
        wr16(obj + 0x50, 0x40);
    }
    strider_follow_feet(obj);
}

/* $059766. State $1C: $40 frames, then the fight: the smoke gone, its vitality $C80, a message,
 * the boss's bar; the plane's covering of it cleared. */
void StriderReady(uint16_t obj)
{
    StriderBobRest();
    StriderPilot(obj);
    StriderSmoke();
    if (dec_neg(obj + 0x50)) {
        wr16(0xDFA2, 0x1000);
        next(obj, 4);
        wr16(obj + O_FLAGS, 0);
        wr16(obj + 0x4E, 0);
        wr16(obj + 0x5A, 0x400);
        wr16(obj + 0x54, 0xC0);
        wr16(obj + 0x56, 0x40);
        wr16(obj + O_VITALITY, 0xC80);
        wr16(0x80C2, 0x10);
        wr16(0x80D6, 0xC980);
        StriderPlaneClear();
    }
    StriderScroll(obj);
    StriderLegsPlace(obj);
    StriderShiftAnchor(obj, 0x1E);
    strider_feet_look();
}

/* $0597E4. The priority bits cleared in 11 rows of the plane's buffer (the longs at $598B4),
 * those rows redrawn. */
void StriderPlaneClear(void)
{
    for (uint16_t k = 0; k < 11; k++) {
        uint16_t row = (uint16_t)rom32(0x0598B4 + 4u * k);
        for (uint16_t i = 0; i < 0x20; i += 2) wr16((uint16_t)(row + i), rd16((uint16_t)(row + i)) & 0x7FFF);
    }
    PlaneAChunk(0xFFFF5DE0, 0xC030);
    PlaneAChunk(0xFFFF5E00, 0xC038);
    PlaneAChunk(0xFFFF5E20, 0xC230);
    PlaneAChunk(0xFFFF5E40, 0xC238);
    PlaneAChunk(0xFFFF5E60, 0xC240);
    PlaneAChunk(0xFFFF5F20, 0xC430);
    PlaneAChunk(0xFFFF5F40, 0xC438);
    PlaneAChunk(0xFFFF5F60, 0xC440);
    PlaneAChunk(0xFFFF40E0, 0xC630);
    PlaneAChunk(0xFFFF4100, 0xC638);
    PlaneAChunk(0xFFFF4120, 0xC640);
}

/* $0598E0. State $20: the fight: the pilot, the turn, the face, a walk's step (+$4C), the
 * arm's attack. */
void StriderFight(uint16_t obj)
{
    StriderPilotRise(obj);
    StriderTurnPlane();
    StriderFacePlane();
    StriderWalk(obj);
    StriderScroll(obj);
    StriderArm(obj);
    StriderLegsPlace(obj);
    StriderShiftAnchor(obj, 0x1E);
    strider_feet_look();
}

/* $059908. State $24 (no run reaches it): crouching (the legs to $160), then a leap across. */
void StriderCrouch(uint16_t obj)
{
    StriderBobDown();
    StriderPilot(obj);
    StriderFacePlane();
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 2) & 0x1FE);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 2) & 0x1FE);
    if (rd16(obj + 0x54) != 0x160) { strider_follow_feet(obj); return; }
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xC00);
    wr32(obj + 0x1C, 0xFFFB8000);
    wr32(obj + 0x18, ge(obj + O_X, 0x120) ? 0xFFFDE000 : 0x22000);
}

/* $059962. State $28: the leap, the legs spreading on the way up, folding on the way down,
 * then a thud on the floor. */
void StriderLeap(uint16_t obj)
{
    StriderPilot(obj);
    StriderFacePlane();
    add32(obj + 0x1C, 0x2000);
    if (rd32(obj + 0x1C) & 0x80000000u) {
        StriderBobUp();
        if (rd16(obj + 0x54) != 0xC0) {
            wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 4) & 0x1FE);
            wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 4) & 0x1FE);
        }
        strider_follow_arm(obj);
        return;
    }
    StriderBobDown();
    StriderLegsIn(obj);
    if (!ge(0xD2F4, 0x140)) { strider_follow_arm(obj); return; }
    QueueSound(0xA0);
    wr16(0xD0B4, 0x144);
    wr16(0xD2F4, 0x144);
    next(obj, 4);
    wr16(v_shakeSprites, 0x10);
    wr16(v_shakePlane, 0x10);
    wr32(obj + 0x1C, 0);
    strider_follow_feet(obj);
}

/* $0599E0. State $2C: landed, sliding to a stop, the legs back to $C0 / $40. */
void StriderLanded(uint16_t obj)
{
    StriderBobRest();
    StriderPilot(obj);
    StriderFacePlane();
    StriderDust();
    if (rd32(obj + 0x18)) {
        if (rd32(obj + 0x18) & 0x80000000u) add32(obj + 0x18, 0x1000);
        else add32(obj + 0x18, 0xFFFFF000);
    }
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 2) & 0x1FE);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 2) & 0x1FE);
    if (rd16(obj + 0x54) == 0xC0) {
        next(obj, 4);
        wr16(obj + 0x50, 0x40);
    }
    strider_follow_feet(obj);
}

/* $059A40. State $30: $40 frames, then fighting again (its walk's step $28). */
void StriderSettle(uint16_t obj)
{
    StriderBobRest();
    StriderPilot(obj);
    StriderFacePlane();
    if (!dec_neg(obj + 0x50)) return;
    wr16(obj + O_STATE, 0x20);
    wr16(obj + 0x5A, 0xFFFF);
    wr16(obj + 0x4C, 0x28);
}

/* $059AA4. State $34: beaten: flashing, every 5th frame the next part of the chain (from +$50)
 * flung off; then the parts from $D0A0 turned to debris. */
void StriderBeaten(uint16_t obj)
{
    StriderFlash();
    StriderPilotHide(obj);
    if (!dec_neg(obj + 0x52)) return;
    wr16(obj + 0x52, 4);
    StriderPartFly(obj);
    add16(obj + 0x50, 0x60);
    if (!ge(obj + 0x50, 0xD0A0)) return;
    wr16(0xDE82, 0);
    next(obj, 4);
    wr16(obj + 0x50, 0xD0A0);
    wr16(obj + 0x52, 8);
    wr16(obj + 0x54, 0);
}

/* $059AEC. State $38: every $11th frame the next part turned to debris (types by the words at
 * $5A326); then falling. */
void StriderFallApart(uint16_t obj)
{
    StriderFlash();
    if (!dec_neg(obj + 0x52)) return;
    wr16(obj + 0x52, 0x10);
    StriderPartOff(obj);
    add16(obj + 0x50, 0x60);
    if (!ge(obj + 0x50, 0xD520)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x400);
    wr16(obj + 0x1C, 0xFFFF);
    StriderFlashOff();
}

/* the wreck in plane A by the side it faced (loc_5AC56) */
static void plane_wreck(uint16_t obj)
{
    if (rd16(obj + 0x5A)) {
        PlaneAChunk(0xFFFF3C00, 0xE218);
        PlaneAChunk(0xFFFF3C20, 0xE220);
        return;
    }
    PlaneAChunk(0xFFFF3D40, 0xE220);
    PlaneAChunk(0xFFFF3D60, 0xE228);
}

/* $059B28. State $3C: the body falling to y $110: a crash, everything shaken, the gun ($DEE0)
 * turned to an explosion at it; the wreck drawn. */
void StriderWreck(uint16_t obj)
{
    StriderScroll(obj);
    add32(obj + 0x1C, 0x2000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (!ge(obj + O_Y, 0x110)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + O_Y, 0x110);
    wr16(obj + 0x48, 0x20);
    wr16(v_shakeSprites, 0x10);
    wr16(v_shakePlane, 0x10);
    QueueSound(0xC5);
    wr16(0xDEE0, 0x210);
    wr16(0xDEF0, rd16(obj + O_X));
    plane_wreck(obj);
}

/* $059B80. State $40: nothing. */
void StriderDone(uint16_t obj)
{
    (void)obj;
}

/* $059B82. States $44-$4C: the stage cleared. */
void StriderClear(uint16_t obj)
{
    (void)obj;
    set8(v_stageClear, 7);
}

/* $059B8A. The pilot's smoke ($DFA0) by it, bobbing every $10 frames; sparks with the pilot
 * risen. */
void StriderSmoke(void)
{
    if (!ge(STR_PILOT, 0xFFDC)) SparksAt(rd16(0xDFA0 + O_X), rd16(0xDFA0 + O_Y));
    uint16_t s = 0xDFA0, p = 0xDE80;
    if (!(rd16(v_gameFrames) & 0x10)) {
        wr16(s + O_X, (uint16_t)(rd16(p + O_X) + 0x10));
        wr16(s + O_Y, (uint16_t)(rd16(p + O_Y) - 0x10));
        return;
    }
    wr16(s + O_X, (uint16_t)(rd16(p + O_X) + 0xC));
    wr16(s + O_Y, (uint16_t)(rd16(p + O_Y) - 0x18));
}

/* $059BEE. A foot (a0): shown, its look, its boxes, at 0. */
void StriderFootSetup(uint16_t part)
{
    wr16(part + O_FLAGS, 0xC000);
    wr16(part + 0x0E, 0x330);
    wr32(part + 0x08, 0x0E164E);
    wr32(part + 0x2C, 0xD802F010);
    wr32(part + 0x28, 0xC002F40C);
    wr16(part + O_X, 0);
    wr16(part + O_Y, 0);
}

/* $059C3A. The legs folding, by 4 to $160 / $A0. */
void StriderLegsIn(uint16_t obj)
{
    if (rd16(obj + 0x54) == 0x160) return;
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 4) & 0x1FE);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 4) & 0x1FE);
}

/* $059C5A. Every 4th frame dust (type $100, list $5652E) at a foot's x, on the floor, drifting
 * back. */
void StriderDust(void)
{
    uint16_t f = rd16(v_gameFrames), slot, t;
    if (f & 3) return;
    uint16_t x = (f & 4) ? rd16(0xD2F0) : rd16(0xD0B0);
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x05652E);
    wr16(slot + O_X, x);
    wr16(slot + O_Y, 0x150);
    wr32(slot + 0x18, 0xFFFF4000);
    wr16(slot + 0x1C, (uint16_t)((rd16(v_randomSeed) & 1) - 2));
}

/* the four palette lines, each colour's bits flipped by $8014+ (loc_0370E: no routine of its
 * own) */
static void palettes_flip(void)
{
    uint16_t m = rd16(0x8002);
    if (m & 0x8000) ColourXorLine(v_paletteTarget, v_palette, rd16(0x8014));
    if (m & 0x4000) ColourXorLine(0xE3A0, 0xE320, rd16(0x8016));
    if (m & 0x2000) ColourXorLine(0xE3C0, 0xE340, rd16(0x8018));
    if (m & 0x1000) ColourXorLine(0xE3E0, 0xE360, rd16(0x801A));
}

static void strider_flash_by(uint16_t flip)
{
    wr16(0x8002, 0xF000);
    wr16(0x8014, flip);
    wr16(0x8016, flip);
    wr16(0x8018, flip);
    wr16(0x801A, flip);
    palettes_flip();
}

/* $059CCE. The colours inverted on a random fourth frame, else as they are. */
void StriderFlash(void)
{
    if ((rd16(v_gameFrames) & 3) || bit8(v_randomSeed, 0)) { StriderFlashOff(); return; }
    strider_flash_by(7);
}

/* $059CEC. The colours as they are. */
void StriderFlashOff(void)
{
    strider_flash_by(0);
}

/* its look by d0 (the tables at $59D5C-$59D74), at 0 */
static void part_look(uint16_t part, uint16_t k)
{
    wr16(part + O_TYPE, 0x10);
    wr16(part + 0x0E, rom16(0x059D5C + (uint32_t)(int16_t)k));
    wr8(part + 0x20, rom8(0x059D64 + (uint32_t)(int16_t)k));
    wr16(part + 0x08, rom16(0x059D6C + (uint32_t)(int16_t)k));
    wr16(part + 0x0A, rom16(0x059D74 + (uint32_t)(int16_t)k));
    wr16(part + O_X, 0);
    wr16(part + O_Y, 0);
}

/* $059D0A. A part (a0) of look d0, hidden. */
void StriderPartHidden(uint16_t part, uint16_t k)
{
    wr16(part + O_FLAGS, 0);
    part_look(part, k);
}

/* $059D14. The same shown. */
void StriderPartShown(uint16_t part, uint16_t k)
{
    wr16(part + O_FLAGS, 0x8000);
    part_look(part, k);
}

/* $059D7C. The walk's step (+$4C, the table at $59D86). */
void StriderWalk(uint16_t obj)
{
    call_object(rom32(0x059D86 + (uint32_t)(int16_t)rd16(obj + 0x4C)), obj);
}

/* a walk's step: +$4E on by the difficulty, the legs at (a + +$4E / da, b + +$4E / db), the
 * foot it stands on; at `end` the next step (`to`, or +4 with `to` 0) */
static int walk_step(uint16_t obj, int16_t dside, uint16_t foot, uint16_t a, uint16_t b, uint16_t da, uint16_t db, uint16_t end)
{
    wr16(obj + 0x5C, (uint16_t)(rd16(obj + 0x5C) + dside));
    wr16(obj + 0x58, foot);
    StriderLegsStep(obj, a, b, da, db);
    if (rd16(obj + 0x4E) != end) return 0;
    wr16(obj + 0x4E, 0);
    return 1;
}

/* a foot put down: a thud */
static void walk_thud(uint16_t foot_y, uint16_t shake)
{
    wr16(foot_y, 0x144);
    wr16(v_shakeSprites, shake);
    wr16(v_shakePlane, shake);
    QueueSound(0xA0);
}

/* $059DB2. Walk step 0 (no run reaches the walk's steps 0-$24): left, the right foot raised. */
void StriderWalk00(uint16_t obj)
{
    StriderBobUp();
    StriderAttackRight(obj);
    if (walk_step(obj, -1, 0xD0A0, 0xC0, 0x40, 0xFFFB, 0xFFFF, 0xA0)) {
        add16(obj + 0x4C, 4);
        add16(STR_STEPS, 1);
    }
}

/* $059DEE. Walk step 4: the right foot put down. */
void StriderWalk04(uint16_t obj)
{
    StriderBobDown();
    StriderAttackRight(obj);
    if (walk_step(obj, -1, 0xD0A0, 0xA0, 0x1A0, 1, 2, 0x80)) {
        add16(obj + 0x4C, 4);
        walk_thud(0xD0B4, 0x10);
    }
}

/* $059E42. Walk step 8: the left foot raised. */
void StriderWalk08(uint16_t obj)
{
    StriderBobUp();
    StriderAttackRight(obj);
    if (walk_step(obj, 1, 0xD2E0, 0x120, 0x1E0, 2, 1, 0x80)) {
        add16(obj + 0x4C, 4);
        add16(STR_STEPS, 1);
    }
}

/* $059E7E. Walk step $C: the left foot put down; then step $24. */
void StriderWalk0C(uint16_t obj)
{
    StriderBobDown();
    if (walk_step(obj, 1, 0xD2E0, 0x160, 0x60, 0xFFFF, 0xFFFB, 0xA0)) {
        wr16(obj + 0x4C, 0x24);
        walk_thud(0xD2F4, 8);
    }
}

/* $059ED0. Walk step $10: right, the left foot raised. */
void StriderWalk10(uint16_t obj)
{
    StriderBobUp();
    StriderAttackLeft(obj);
    if (walk_step(obj, -1, 0xD2E0, 0xC0, 0x40, 1, 5, 0xA0)) {
        add16(obj + 0x4C, 4);
        add16(STR_STEPS, 1);
    }
}

/* $059F0C. Walk step $14: the left foot put down. */
void StriderWalk14(uint16_t obj)
{
    StriderBobDown();
    StriderAttackLeft(obj);
    if (walk_step(obj, -1, 0xD2E0, 0x160, 0x60, 0xFFFE, 0xFFFF, 0x80)) {
        add16(obj + 0x4C, 4);
        walk_thud(0xD2F4, 0x10);
    }
}

/* $059F60. Walk step $18: the right foot raised. */
void StriderWalk18(uint16_t obj)
{
    StriderBobUp();
    StriderAttackLeft(obj);
    if (walk_step(obj, 1, 0xD0A0, 0x120, 0x1E0, 0xFFFF, 0xFFFE, 0x80)) {
        add16(obj + 0x4C, 4);
        add16(STR_STEPS, 1);
    }
}

/* $059F9C. Walk step $1C: the right foot put down; then step $20. */
void StriderWalk1C(uint16_t obj)
{
    StriderBobDown();
    if (walk_step(obj, 1, 0xD0A0, 0xA0, 0x1A0, 5, 1, 0xA0)) {
        wr16(obj + 0x4C, 0x20);
        walk_thud(0xD0B4, 8);
    }
}

/* $059FEE. Walk step $20: on right from x $CA, else a rest ($28) of $400 frames. */
void StriderWalk20(uint16_t obj)
{
    if (ge(obj + O_X, 0xCA)) { wr16(obj + 0x4C, 0x10); return; }
    wr16(obj + 0x4C, 0x28);
    wr16(obj + 0x5A, 0x400);
}

/* $05A00E. Walk step $24: a rest from x $176, else on left. */
void StriderWalk24(uint16_t obj)
{
    if (ge(obj + O_X, 0x176)) {
        wr16(obj + 0x4C, 0x28);
        wr16(obj + 0x5A, 0x400);
        return;
    }
    wr16(obj + 0x4C, 0);
}

/* $05A02E. Walk step $28: resting (the arm grabs) for +$5A frames; then a leap (state $24, on a
 * random half above the easiest difficulty) or walking away from its side. */
void StriderWalk28(uint16_t obj)
{
    StriderBobRest();
    StriderPilotHide(obj);
    wr16(STR_ATTACK, 8);
    if (!dec_neg(obj + 0x5A)) return;
    wr16(STR_ATTACK, 0);
    if (!bit8(0xFF09, 1) && rd16(v_difficulty)) {
        wr16(obj + O_STATE, 0x24);
        return;
    }
    wr16(obj + 0x4C, ge(obj + O_X, 0x130) ? 0x10 : 0);
}

/* divs.w: the quotient (low word) and the remainder (high); on overflow the dividend kept */
static uint32_t divs_w(uint32_t n, uint16_t d)
{
    int32_t q = (int32_t)n / (int16_t)d, r = (int32_t)n % (int16_t)d;
    if (q < -32768 || q > 32767) return n;
    return (uint32_t)(uint16_t)r << 16 | (uint16_t)q;
}

/* $05A07E. +$4E on by the difficulty's step (the words at $5A0B4); the legs' angles from it. */
void StriderLegsStep(uint16_t obj, uint16_t a, uint16_t b, uint16_t da, uint16_t db)
{
    add16(obj + 0x4E, rom16(0x05A0B4 + (uint32_t)(int16_t)rd16(v_difficulty)));
    uint32_t t = rd16(obj + 0x4E);
    wr16(obj + 0x54, (uint16_t)((uint16_t)divs_w(t, da) + a) & 0x1FE);
    wr16(obj + 0x56, (uint16_t)((uint16_t)divs_w(t, db) + b) & 0x1FE);
}

/* $05A0BC. Plane B's scroll by the body's place. */
void StriderScroll(uint16_t obj)
{
    wr16(0xA908, (uint16_t)((rd16(obj + O_X) ^ 0xFFFF) - 0xF3));
    wr16(0xA90C, (uint16_t)((rd16(obj + O_Y) ^ 0xFFFF) + 0xB4));
}

/* a leg's segment at r (times the angle's sine and cosine, /16384) from the hip */
static uint16_t seg(int32_t t, int r) { return (uint16_t)((uint32_t)(t * r) << 2 >> 16); }

/* one leg: the hip (+$48 of the part `hip`, from (x, y)), its segments' bases along `angle` */
static void leg_place(uint16_t angle, uint16_t hipy, uint16_t hipx, int16_t dx,
                      uint16_t knee, uint16_t mid, uint16_t low, uint16_t foot)
{
    uint16_t y = (uint16_t)(rd16(hipy) + 8), x = (uint16_t)(rd16(hipx) + dx);
    wr16((uint16_t)(knee + 0x4A), y);
    wr16((uint16_t)(knee + 0x48), x);
    wr16((uint16_t)(mid + 0x4A), (uint16_t)(seg(sine(angle), 0xC) + rd16((uint16_t)(knee + 0x4A))));
    wr16((uint16_t)(mid + 0x48), (uint16_t)(seg(cosine(angle), 0xC) + rd16((uint16_t)(knee + 0x48))));
    uint16_t ly = (uint16_t)(seg(sine(angle), 0x18) + rd16((uint16_t)(knee + 0x4A)));
    uint16_t lx = (uint16_t)(seg(cosine(angle), 0x18) + rd16((uint16_t)(knee + 0x48)));
    wr16((uint16_t)(low + 0x4A), ly);
    wr16((uint16_t)(low + 0x48), lx);
    ly = (uint16_t)(ly + 0x10);
    wr16((uint16_t)(foot + 0x4A), ly);
    wr16((uint16_t)(foot + 0x48), lx);
}

/* $05A0EE. The legs' parts' places (+$48, +$4A) from the body and the legs' angles: the hips
 * ($D280, $D4C0), the knees, the shins, the ankles and the feet ($D0A0 and $D2E0, lowered by
 * the bob). */
void StriderLegsPlace(uint16_t obj)
{
    uint16_t x = (uint16_t)(rd16(obj + O_X) - 0x22);
    wr16(0xD2C8, x);
    wr16(0xD508, (uint16_t)(x + 0x44));
    uint16_t y = (uint16_t)(rd16(obj + O_Y) + 0x2E);
    wr16(0xD2CA, y);
    wr16(0xD50A, y);
    leg_place(rd16(obj + 0x54), 0xD2CA, 0xD2C8, -8, 0xD220, 0xD1C0, 0xD160, 0xD100);
    wr16(0xD0EA, (uint16_t)(rd16(0xD14A) + 0x18 + rd16(STR_BOB)));
    wr16(0xD0E8, rd16(0xD148));
    leg_place(rd16(obj + 0x56), 0xD50A, 0xD508, 8, 0xD460, 0xD400, 0xD3A0, 0xD340);
    wr16(0xD32A, (uint16_t)(rd16(0xD38A) + 0x18 + rd16(STR_BOB)));
    wr16(0xD328, rd16(0xD388));
}

/* all the parts from $C980 (count + 1) moved by (dx, dy), their places from their bases
 * (loc_5A274) */
static void shift_parts(uint16_t dx, uint16_t dy, uint16_t count)
{
    uint16_t p = 0xC980;
    do {
        add16(p + 0x48, dx);
        wr16(p + O_X, rd16(p + 0x48));
        add16(p + 0x4A, dy);
        wr16(p + O_Y, rd16(p + 0x4A));
        p = (uint16_t)(p + 0x60);
    } while (count-- != 0);
}

/* $05A234. The body's base set from its place; every part moved by how far a0's x and a1's y
 * are from their bases. */
void StriderShiftXY(uint16_t obj, uint16_t px, uint16_t py, uint16_t count)
{
    wr16(obj + 0x48, rd16(obj + O_X));
    wr16(obj + 0x4A, rd16(obj + O_Y));
    shift_parts((uint16_t)(rd16(px + O_X) - rd16(px + 0x48)), (uint16_t)(rd16(py + O_Y) - rd16(py + 0x4A)), count);
}

/* $05A254. The same by the foot it stands on (+$58). */
void StriderShiftAnchor(uint16_t obj, uint16_t count)
{
    StriderShiftBy(obj, rd16(obj + 0x58), count);
}

/* $05A258. The same by one part (a0). */
void StriderShiftBy(uint16_t obj, uint16_t part, uint16_t count)
{
    wr16(obj + 0x48, rd16(obj + O_X));
    wr16(obj + 0x4A, rd16(obj + O_Y));
    shift_parts((uint16_t)(rd16(part + O_X) - rd16(part + 0x48)), (uint16_t)(rd16(part + O_Y) - rd16(part + 0x4A)), count);
}

/* $05A2D4. The priority bit set in 15 of its parts' sprites (+$E from $CA40 on) and in the
 * plane buffer's $1B0 words from $3C00. */
void StriderPriority(void)
{
    uint16_t a = 0xCA4E;
    for (int k = 0; k < 15; k++, a = (uint16_t)(a + 0x60)) wr16(a, rd16(a) | 0x8000);
    for (uint16_t i = 0; i < 0x360; i += 2) wr16((uint16_t)(0x3C00 + i), rd16((uint16_t)(0x3C00 + i)) | 0x8000);
}

/* $05A2FC. The part at +$50 turned to debris (its type the next of the words at $5A326). */
void StriderPartOff(uint16_t obj)
{
    uint16_t p = rd16(obj + 0x50), i = rd16(obj + 0x54);
    add16(obj + 0x54, 2);
    wr16(p + O_TYPE, rom16(0x05A326 + (uint32_t)(int16_t)i));
    wr16(p + O_STATE, 0);
    wr16(p + O_FLAGS, 0);
    wr8(p + 0x21, 0);
}

/* $05A33E. The arm's attack by x (walking from the left): shoot ($C) from $140, none from $100,
 * a whip (4) left of it. */
void StriderAttackLeft(uint16_t obj)
{
    if (ge(obj + O_X, 0x140)) { wr16(STR_ATTACK, 0xC); return; }
    if (ge(obj + O_X, 0x100)) { wr16(STR_ATTACK, 0); return; }
    wr16(STR_ATTACK, 4);
}

/* $05A35A. The same walking from the right: shoot left of $100, none to $140, a whip from
 * there. */
void StriderAttackRight(uint16_t obj)
{
    if (!ge(obj + O_X, 0x100)) { wr16(STR_ATTACK, 0xC); return; }
    if (!ge(obj + O_X, 0x140)) { wr16(STR_ATTACK, 0); return; }
    wr16(STR_ATTACK, 4);
}

/* $05A386. The arm's step (+6, the table at $5A390). */
void StriderArm(uint16_t obj)
{
    call_object(rom32(0x05A390 + (uint32_t)(int16_t)rd16(obj + 6)), obj);
}

/* $05A3BC. Arm step 0: the side to turn to (left of $110 left, from $130 right), the trail
 * cleared, the attack (the table at $5A3EE by STR_ATTACK). */
void StriderArm00(uint16_t obj)
{
    if (ge(obj + O_X, 0x130)) wr16(STR_SIDE_TO, 0);
    else if (!ge(obj + O_X, 0x110)) wr16(STR_SIDE_TO, 2);
    StriderTrailClear();
    call_object(rom32(0x05A3EE + (uint32_t)(int16_t)rd16(STR_ATTACK)), obj);
}

/* $05A3FE. The whip (no run reaches it): spacing $C, step 8. */
void StriderArmWhip(uint16_t obj)
{
    wr16(STR_SPACING, 0xC);
    wr16(obj + 6, 8);
    StriderArmNone(obj);
}

/* $05A40A. No attack. */
void StriderArmNone(uint16_t obj)
{
    (void)obj;
}

/* $05A40C. The grab: spacing 6, step $14. */
void StriderArmGrab(uint16_t obj)
{
    wr16(STR_SPACING, 6);
    wr16(obj + 6, 0x14);
}

/* $05A41A. Shooting (no run reaches it): step $28. */
void StriderArmShoot(uint16_t obj)
{
    wr16(obj + 6, 0x28);
}

/* $05A422. Arm step 4: the chain pulled in until its trail is empty; then the hand hidden and
 * harmless, step 0. */
void StriderArm04(uint16_t obj)
{
    if (rd16(TRAIL_X) != TRAIL_EMPTY) { StriderChainPull(obj); return; }
    wr16(obj + 6, 0);
    wr16(HAND + O_FLAGS, rd16(HAND + O_FLAGS) & 0x7FFF);
    wr8(HAND + 0x21, 0);
}

/* the trail's oldest entry dropped: each list moved up a word (loc_5A962) */
static void trail_pull(void)
{
    for (uint16_t a = TRAIL_X; a != 0x91FE; a = (uint16_t)(a + 2)) wr16(a, rd16((uint16_t)(a + 2)));
    for (uint16_t a = TRAIL_Y; a != 0x95FE; a = (uint16_t)(a + 2)) wr16(a, rd16((uint16_t)(a + 2)));
}

/* $05A440. The chain ($C9E0-$D03F) placed on its trail, the trail pulled in by one. */
void StriderChainPull(uint16_t obj)
{
    StriderChainPlace(obj, HAND, 0xD040);
    trail_pull();
}

/* the hand's start (loc_5A478): $3C frames, its direction by the side (the words at $5A4C6),
 * at the shoulder */
static void hand_start(uint16_t obj)
{
    wr16(STR_TIME, 0x3C);
    wr16(HAND_DIR, rom16(0x05A4C6 + (uint32_t)(int16_t)rd16(STR_SIDE)));
    wr32(HAND_X, 0);
    wr32(HAND_Y, 0);
    next6(obj);
    uint16_t x = rd16(obj + O_X);
    x = rd16(STR_SIDE) ? (uint16_t)(x + 0x24) : (uint16_t)(x - 0x24);
    wr16(HAND + 0x48, x);
    wr16(HAND + 0x4A, (uint16_t)(rd16(obj + O_Y) - 4));
}

/* $05A450. Arm step 8 (no run reaches it): the whip: spacing $C, its target a player, the hand
 * shown. */
void StriderArm08(uint16_t obj)
{
    wr16(STR_SPACING, 0xC);
    wr16(STR_TARGET, PlayerTarget());
    wr16(HAND + O_FLAGS, 0x8000);
    wr16(HAND + 0x0E, 0xE300);
    wr16(HAND + 0x0A, 0xF0F0);
    wr16(HAND + 0x08, 0xF00);
    hand_start(obj);
}

/* $05A4CA. Arm step $C: then the hand harmful for a time by the difficulty (the words at
 * $5A50A). */
void StriderArm0C(uint16_t obj)
{
    if (!dec_neg(STR_TIME)) return;
    next6(obj);
    wr16(STR_TIME, rom16(0x05A50A + (uint32_t)(int16_t)rd16(RANK)));
    wr16(HAND + 0x26, 0xC);
    wr8(HAND + 0x21, 0x30);
    wr8(HAND + 0x23, 0x20);
    wr32(HAND + 0x2C, 0xFC04FC04);
    wr32(HAND + 0x28, 0xF010F010);
}

/* the hand's place on its trail's head moved along its direction by (sine << sy, cosine << sx) */
static void hand_advance(int sx)
{
    StriderTrailPush();
    uint16_t d = rd16(HAND_DIR);
    add32(HAND_Y, (uint32_t)sine(d) << 3);
    add32(HAND_X, (uint32_t)cosine(d) << sx);
    wr16(TRAIL_X, rd16(HAND_X));
    wr16(TRAIL_Y, rd16(HAND_Y));
}

/* the hand's place back from the trail's head, or 0 with it empty */
static void hand_from_trail(void)
{
    if (rd16(TRAIL_X) != TRAIL_EMPTY) {
        wr16(HAND_X, rd16(TRAIL_X));
        wr16(HAND_Y, rd16(TRAIL_Y));
        return;
    }
    wr16(HAND_X, 0);
    wr16(HAND_Y, 0);
}

/* $05A51A. Arm step $10: the whip's hand flying (its direction turned $C a frame toward its
 * target) until it hits something, the time runs out or the chain is out (then pulled in). */
void StriderArm10(uint16_t obj)
{
    if (bit8(HAND + 0x22, 5)) {
        StriderChainPull(obj);
        hand_from_trail();
        return;
    }
    if (!StriderChainPlace(obj, HAND, 0xD040) || dec_neg(STR_TIME)) {
        wr16(obj + 6, 4);
        return;
    }
    hand_advance(3);
    uint16_t t = rd16(STR_TARGET);
    uint32_t dy = (uint16_t)(rd16(t + O_Y) - rd16(HAND + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(t + O_X) - rd16(HAND + O_X)), &dy);
    a = (uint16_t)(((uint16_t)((int16_t)a >> 7) & 0x1FE) - rd16(HAND_DIR));
    uint16_t d = rd16(HAND_DIR);
    d = turn_up(a) ? (uint16_t)(d + 0xC) : (uint16_t)(d - 0xC);
    wr16(HAND_DIR, d & 0x1FE);
}

/* $05A5FA. Arm step $14: the grab: spacing 8, the gun ($DEE0) and the hand shown (the hand
 * flipped facing left), then as the whip's start. */
void StriderArm14(uint16_t obj)
{
    wr16(STR_SPACING, 8);
    wr16(0xDEE0, 0x10);
    wr16(0xDEE2, 0xC000);
    wr32(0xDEE8, 0x0E187E);
    wr16(0xDEEE, 0x8399);
    wr16(HAND + 0x0E, 0x86BD);
    wr16(HAND + O_FLAGS, 0xC000);
    wr32(HAND + 0x08, 0x0E1648);
    if (ge(obj + O_X, 0x120)) wr16(HAND + 0x0E, rd16(HAND + 0x0E) | 0x800);
    wr16(STR_SCORE, 0);
    StriderGunPlace();
    hand_start(obj);
}

/* $05A64C. Arm step $18: waiting (a step every 8th frame), then the hand out with a random
 * turn (the words at $5A696). */
void StriderArm18(uint16_t obj)
{
    StriderGunPlace();
    if (!(rd16(v_gameFrames) & 4)) add16(STR_STEPS, 1);
    if (!dec_neg(STR_TIME)) return;
    next6(obj);
    wr16(STR_TURN, rom16(0x05A696 + (uint32_t)(rd8(v_randomSeed) & 6)));
    wr16(STR_GUN_TIME, 0);
    wr8(HAND + 0x21, 0x20);
    wr32(HAND + 0x28, 0xF010F010);
}

/* $05A69E. Arm step $1C: the hand curling out (the gun firing from the difficulty 4); the
 * chain out: held (step $20), or on a random quarter pulled back (step $24). */
void StriderArm1C(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 8)) add16(STR_STEPS, 1);
    StriderHandFlip();
    StriderGunPlace();
    if (ge(v_difficulty, 4)) StriderGunFire();
    StriderHandScore();
    if (!StriderChainPlace(obj, HAND, 0xCDA0)) {
        if (rd16(v_randomSeed) & 0x3000) {
            next6(obj);
            wr16(STR_TARGET, 0x40);
            return;
        }
        add16(obj + 6, 8);
        wr16(STR_TIME, (uint16_t)(((int16_t)rd16(RANK) >> 1) + 1));
        return;
    }
    hand_advance(4);
    uint16_t d = rd16(HAND_DIR);
    d = rd16(STR_SIDE) ? (uint16_t)(d - rd16(STR_TURN)) : (uint16_t)(d + rd16(STR_TURN));
    wr16(HAND_DIR, d & 0x1FE);
}

/* $05A75E. Arm step $20: the hand held out $40 frames, the gun firing. */
void StriderArm20(uint16_t obj)
{
    StriderHandFlip();
    StriderGunPlace();
    StriderHandScore();
    StriderChainPlace(obj, HAND, 0xCDA0);
    if (dec_neg(STR_TARGET)) next6(obj);
    StriderGunFire();
}

/* $05A786. Arm step $24: the chain pulled in (the gun firing from the difficulty 2); in, step 0
 * with the hand and gun hidden, or still grabbing (STR_ATTACK 8) out again with a new turn and
 * direction (the words at $5A836 by the side). */
void StriderArm24(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 4)) add16(STR_STEPS, 1);
    StriderHandFlip();
    StriderGunPlace();
    if (ge(v_difficulty, 2)) StriderGunFire();
    StriderHandScore();
    StriderChainPull(obj);
    if (rd16(TRAIL_X) != TRAIL_EMPTY) {
        wr16(HAND_X, rd16(TRAIL_X));
        wr16(HAND_Y, rd16(TRAIL_Y));
        return;
    }
    wr16(HAND_X, 0);
    wr16(HAND_Y, 0);
    if (rd16(STR_ATTACK) != 8) {
        wr16(obj + 6, 0);
        wr16(HAND + O_FLAGS, rd16(HAND + O_FLAGS) & 0x7FFF);
        wr8(HAND + 0x21, 0);
        wr16(0xDEE2, rd16(0xDEE2) & 0x7FFF);
        return;
    }
    wr16(obj + 6, 0x1C);
    wr16(STR_TURN, rom16(0x05A696 + (uint32_t)(rd8(v_randomSeed) & 6)));
    wr16(STR_GUN_TIME, 0);
    uint32_t t = 0x05A836 + (uint32_t)(int16_t)rd16(STR_SIDE);
    wr16(HAND_DIR, rom16(t + (rd16(v_randomSeed) & 0xC)));
}

/* $05A846. Arm step $28 (no run reaches it): while shooting is its attack, every $20 frames a
 * shot (type $138) from the shoulder; else step 0. */
void StriderArm28(uint16_t obj)
{
    if (rd16(STR_ATTACK) != 0xC) { wr16(obj + 6, 0); return; }
    uint16_t slot, t;
    if ((rd16(v_gameFrames) & 0x1F) || !SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x138);
    wr16(slot + 0x48, 0x180);
    wr16(slot + 0x4A, 0x38);
    uint16_t x = rd16(obj + O_X);
    if (rd16(STR_SIDE)) {
        x = (uint16_t)(x + 0x24);
        wr16(slot + 0x18, 2);
    } else {
        x = (uint16_t)(x - 0x24);
        wr16(slot + 0x18, 0xFFFE);
    }
    wr16(slot + O_X, x);
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) - 4));
    wr16(slot + 0x1C, 0xFFFE);
}

/* $05A8BA. The trail emptied (256 words of each list). */
void StriderTrailClear(void)
{
    for (uint16_t i = 0; i < 0x200; i += 2) {
        wr16((uint16_t)(TRAIL_X + i), TRAIL_EMPTY);
        wr16((uint16_t)(TRAIL_Y + i), TRAIL_EMPTY);
    }
}

/* $05A8D4. The chain's objects from a0 to a1 placed (their bases) every STR_SPACING bytes along
 * the trail from the shoulder, hidden where it is empty: Z when the last one's entry is. */
int StriderChainPlace(uint16_t obj, uint16_t first, uint16_t end)
{
    uint16_t step = rd16(STR_SPACING), ax = TRAIL_X, ay = TRAIL_Y;
    uint16_t x = rd16(obj + O_X);
    x = rd16(STR_SIDE) ? (uint16_t)(x + 0x24) : (uint16_t)(x - 0x24);
    uint16_t y = (uint16_t)(rd16(obj + O_Y) - 4);
    uint16_t p = first;
    do {
        uint16_t d = rd16(ax);
        if (d == TRAIL_EMPTY) {
            wr16(p + O_FLAGS, rd16(p + O_FLAGS) & 0x7FFF);
            d = 0;
        } else {
            wr16(p + O_FLAGS, rd16(p + O_FLAGS) | 0x8000);
        }
        wr16(p + 0x48, (uint16_t)(d + x));
        wr16(p + 0x4A, (uint16_t)(rd16(ay) + y));
        ax = (uint16_t)(ax + step);
        ay = (uint16_t)(ay + step);
        p = (uint16_t)(p + 0x60);
    } while ((int16_t)(uint16_t)(p - end) < 0);
    return rd16((uint16_t)(ax - step)) == TRAIL_EMPTY;
}

/* $05A940. The trail moved down a word (its head free; the last entries dropped). */
void StriderTrailPush(void)
{
    for (uint16_t a = 0x91FE; a != TRAIL_X; a = (uint16_t)(a - 2)) wr16(a, rd16((uint16_t)(a - 2)));
    for (uint16_t a = 0x95FE; a != TRAIL_Y; a = (uint16_t)(a - 2)) wr16(a, rd16((uint16_t)(a - 2)));
}

/* $05A984. Every $F (the harder ranks) or $13 frames, the gun ($DEE0) fires a shot (type $90)
 * at a player. */
void StriderGunFire(void)
{
    if (!dec_neg(STR_GUN_TIME)) return;
    wr16(STR_GUN_TIME, 0xE);
    if (!ge(RANK, 6)) wr16(STR_GUN_TIME, 0x12);
    AimedSpawnRanked(0xDEE0, PlayerTarget(), 0x02ADB6, 0x02ADD6, 0x90);
}

/* $05A9C8. While the hand touches a player, a point every 5th frame. */
void StriderHandScore(void)
{
    if (!(rd16(STR_SCORE) & 0x8000)) {
        wr16(STR_SCORE, (uint16_t)(rd16(STR_SCORE) - 1));
        return;
    }
    if (!bit8(HAND + 0x22, 5)) return;
    wr16(STR_SCORE, 4);
    AddScore(HAND, 1);
}

/* $05A9F8. The chain's part at +$50 flung off with a puff (type $100, list $5639C) along +6
 * (which turns $50 each time); one hidden starts at the shoulder. */
void StriderPartFly(uint16_t obj)
{
    QueueSound(0xBD);
    uint16_t p = rd16(obj + 0x50);
    if (!bit8(p + O_FLAGS, 7)) {
        uint16_t x = rd16(obj + O_X);
        x = rd16(obj + 0x5A) ? (uint16_t)(x + 0x24) : (uint16_t)(x - 0x24);
        wr16(p + O_X, x);
        wr16(p + O_Y, (uint16_t)(rd16(obj + O_Y) - 4));
    }
    wr16(p + O_STATE, 1);
    wr16(p + O_TYPE, 0x100);
    wr32(p + 0x48, 0x05639C);
    wr8(p + 0x20, 0x70);
    uint16_t a = rd16(obj + 6);
    wr16(obj + 6, (uint16_t)(rd16(obj + 6) + 0x50) & 0x1FE);
    wr32(p + 0x1C, (uint32_t)sine(a) << 3);
    wr32(p + 0x18, (uint32_t)cosine(a) << 3);
    AnimListStart(p);
}

/* $05AA92. Turned to STR_SIDE_TO: the face redrawn (rows of plane A) facing its new side. */
void StriderTurnPlane(void)
{
    uint16_t s = rd16(STR_SIDE_TO);
    if (s == rd16(STR_SIDE)) return;
    wr16(STR_SIDE, s);
    if (!s) { plane_facing_right(); return; }
    PlaneAChunk(0xFFFF3C40, 0xE228);
    PlaneAChunk(0xFFFF3C60, 0xE418);
    PlaneAChunk(0xFFFF3C80, 0xE420);
    PlaneAChunk(0xFFFF3CA0, 0xE428);
    plane_rows();
}

/* $05AB5C. The face: hurt while STR_HURT runs; else one of two by STR_STEPS bit 0. */
void StriderFacePlane(void)
{
    if (!(rd16(STR_HURT) & 0x8000)) {
        wr16(STR_HURT, (uint16_t)(rd16(STR_HURT) - 1));
        StriderFaceHurt();
        return;
    }
    if (bit8(STR_STEPS + 1, 0)) {
        if (rd16(STR_SIDE)) {
            PlaneAChunk(0xFFFF3E20, 0xE218);
            PlaneAChunk(0xFFFF3E40, 0xE220);
            return;
        }
        PlaneAChunk(0xFFFF3F00, 0xE21E);
        PlaneAChunk(0xFFFF3EE0, 0xE226);
        return;
    }
    if (!rd16(STR_SIDE)) { StriderFaceIdle(); return; }
    PlaneAChunk(0xFFFF3DE0, 0xE218);
    PlaneAChunk(0xFFFF3E00, 0xE220);
}

/* $05AB68. The hurt face. */
void StriderFaceHurt(void)
{
    if (rd16(STR_SIDE)) {
        PlaneAChunk(0xFFFF3E60, 0xE218);
        PlaneAChunk(0xFFFF3E80, 0xE220);
        return;
    }
    PlaneAChunk(0xFFFF3F40, 0xE21E);
    PlaneAChunk(0xFFFF3F20, 0xE226);
}

/* $05ABE8. The face at rest (facing right). */
void StriderFaceIdle(void)
{
    PlaneAChunk(0xFFFF3EC0, 0xE21E);
    PlaneAChunk(0xFFFF3EA0, 0xE226);
}

/* $05ACA2. The hand flipped to face its second part ($CA40): turned left or right, upside down
 * when 8 or more above it. */
void StriderHandFlip(void)
{
    uint16_t h = HAND, n = 0xCA40;
    if ((int16_t)(uint16_t)(rd16(n + O_X) - rd16(h + O_X)) < 0) wr16(h + 0x0E, rd16(h + 0x0E) & 0xF7FF);
    else wr16(h + 0x0E, rd16(h + 0x0E) | 0x800);
    uint16_t dy = (uint16_t)(rd16(n + O_Y) - rd16(h + O_Y));
    if (!(dy & 0x8000) || (int16_t)(uint16_t)(dy - 0xFFF8) >= 0) {
        wr16(h + 0x0E, rd16(h + 0x0E) & 0xEFFF);
        return;
    }
    wr16(h + 0x0E, rd16(h + 0x0E) | 0x1000);
}

/* $05ACEA. The gun ($DEE0) held at the hand: 4 behind it, $18 up. */
void StriderGunPlace(void)
{
    uint16_t g = 0xDEE0, h = HAND;
    uint16_t x = rd16(h + O_X);
    x = bit8(h + 0x0E, 3) ? (uint16_t)(x - 4) : (uint16_t)(x + 4);
    wr16(g + O_X, x);
    wr16(g + O_Y, (uint16_t)(rd16(h + O_Y) - 0x18));
}

/* $05AD2C. The bob up to 1. */
void StriderBobUp(void)
{
    if (!ge(STR_BOB, 1)) add16(STR_BOB, 1);
}

/* $05AD3C. The bob down to -7. */
void StriderBobDown(void)
{
    if (ge(STR_BOB, 0xFFF9)) add16(STR_BOB, 0xFFFF);
}

/* $05AD4C. The bob back to -1. */
void StriderBobRest(void)
{
    uint16_t b = rd16(STR_BOB);
    if (b == 0xFFFF) return;
    if (!(b & 0x8000)) add16(STR_BOB, 0xFFFF);
    else add16(STR_BOB, 1);
}

/* the pilot shown at the body, risen by STR_PILOT, waving every $10 frames, facing the side
 * (loc_5ADB8) */
static void pilot_show(uint16_t obj)
{
    uint16_t p = 0xDE80;
    wr16(p + O_FLAGS, rd16(p + O_FLAGS) | 0x8000);
    wr32(p + 0x08, (rd16(v_gameFrames) & 0x10) ? 0x0E17EC : 0x0E17CE);
    wr16(p + O_X, rd16(obj + O_X));
    wr16(p + O_Y, (uint16_t)(rd16(obj + O_Y) + 0x14 + rd16(STR_PILOT)));
    if (rd16(STR_SIDE)) set8(p + 0x0E, 3);
    else clr8(p + 0x0E, 3);
}

/* the pilot sinking by 2; hidden once down (loc_5ADB0) */
static void pilot_sink(uint16_t obj)
{
    add16(STR_PILOT, 2);
    if (!(rd16(STR_PILOT) & 0x8000)) {
        wr16(0xDE82, rd16(0xDE82) & 0x7FFF);
        return;
    }
    pilot_show(obj);
}

/* one frame in two the pilot up by 1 (loc_5AD8A) */
static void pilot_up(uint16_t obj)
{
    if (!bit8(0xA001, 0)) add16(STR_PILOT, 0xFFFF);
    pilot_show(obj);
}

/* $05AD60. The pilot shown, rising to -$24. */
void StriderPilot(uint16_t obj)
{
    if (!ge(STR_PILOT, 0xFFDC)) { pilot_show(obj); return; }
    pilot_up(obj);
}

/* $05AD72. The pilot rising to -$1C (sinking from above -$1E). */
void StriderPilotRise(uint16_t obj)
{
    if (!ge(STR_PILOT, 0xFFE2)) { pilot_sink(obj); return; }
    if (!ge(STR_PILOT, 0xFFE4)) { pilot_show(obj); return; }
    pilot_up(obj);
}

/* $05AD9C. The pilot sinking out of sight. */
void StriderPilotHide(uint16_t obj)
{
    if (rd16(STR_PILOT) & 0x8000) { pilot_sink(obj); return; }
    wr16(0xDE82, rd16(0xDE82) & 0x7FFF);
}

/* ---- type $20C: a puff homing on the boss's slot ($C980) for +$48 frames, its heading (+$4A)
 * turned $20 a frame, its speed added each frame ---- */

/* $05AE0C */
void BossHomer(uint16_t obj)
{
    if (dec_neg(obj + 0x48)) { gone(obj); return; }
    wr8(obj + 0x20, 0xC0);
    wr16(obj + 0x0E, rd16(obj + 0x0E) & 0x7FFF);
    uint16_t a = rd16(obj + 0x4A);
    add32(obj + 0x1C, (uint32_t)sine(a));
    add32(obj + 0x18, (uint32_t)cosine(a));
    uint16_t b = 0xC980;
    uint32_t dy = (uint16_t)(rd16(b + O_Y) - rd16(obj + O_Y));
    uint16_t to = Atan2((int16_t)(rd16(b + O_X) - rd16(obj + O_X)), &dy);
    to = (uint16_t)(((uint16_t)((int16_t)to >> 7) & 0x1FE) - rd16(obj + 0x4A));
    uint16_t h = rd16(obj + 0x4A);
    h = turn_up(to) ? (uint16_t)(h + 0x20) : (uint16_t)(h - 0x20);
    wr16(obj + 0x4A, h & 0x1FE);
}

/* ---- type $210: the strider's pilot escaping (in the gun's slot $DEE0, two halves of its pod
 * at $DE80 and $DF40, a3 and a4 as Type $210's routine sets them): rising out of the wreck,
 * blasted up (bursts of smoke, debris $4E0), falling apart into the distance; then the stage
 * cleared (the boss's state $4C) ---- */

#define POD_A 0xDE80
#define POD_B 0xDF40

/* $05AEA2. Its state (the table at $5AEB4). */
void StriderEscape(uint16_t obj)
{
    call_object(rom32(0x05AEB4 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $05AED8. Hidden at y $130 for $40 frames, the pod's halves set up. */
void EscapeStart(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x0E, 0xB99);
    wr16(obj + O_FLAGS, 0);
    wr32(obj + 0x08, 0x0E18E8);
    wr8(obj + 0x20, 0xC0);
    wr16(obj + O_Y, 0x130);
    wr16(obj + 0x48, 0x40);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(POD_B + O_TYPE, 0x10);
    wr16(POD_B + 0x0E, 0xB99);
    wr16(POD_B + O_FLAGS, 0xCC00);
    wr32(POD_B + 0x08, 0x0E191E);
    wr8(POD_B + 0x20, 0xC4);
    wr16(POD_B + O_X, 0);
    wr16(POD_A + O_TYPE, 0x10);
    wr16(POD_A + O_FLAGS, 0xEC00);
    wr32(POD_A + 0x08, 0x0E1786);
    wr8(POD_A + 0x20, 0xB0);
    wr16(POD_A + O_X, 0);
    wr16(POD_A + 0x0E, rd16(POD_A + 0x0E) | 0x800);
}

/* $05AF5C. State 4: waiting, then shown. */
void EscapeWait(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xCC00);
}

/* $05AF70. State 8: rising to y $F6; there a message, the players held (+$C0 bit 7). */
void EscapeRise(uint16_t obj)
{
    EscapePod(obj);
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 2));
    if (ge(obj + O_Y, 0xF6)) return;
    next(obj, 4);
    wr16(0x80C2, 0x2C);
    set8(0xA4C0, 7);
    set8(0xA6C0, 7);
}

/* $05AF9A. State $C: a blast (smoke bursts, debris at $DFA0) throwing it up and across toward
 * the middle. */
void EscapeBlast(uint16_t obj)
{
    EscapePod(obj);
    QueueSound(0xC5);
    EscapeBursts();
    uint16_t d = 0xDFA0;
    wr16(d + O_TYPE, 0x4E0);
    wr16(d + O_X, rd16(obj + O_X));
    wr16(d + O_Y, rd16(obj + O_Y));
    next(obj, 4);
    wr32(obj + 0x1C, 0xFFFB4000);
    if (!ge(obj + O_X, 0x100)) { wr32(obj + 0x18, 0x14000); return; }
    if (!ge(obj + O_X, 0x140)) return;
    wr32(obj + 0x18, 0xFFFEC000);
}

/* $05AFF4. State $10: flying up above y $80; then $80 frames gone from sight. */
void EscapeFly(uint16_t obj)
{
    EscapePod(obj);
    if (ge(obj + O_Y, 0x80)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0x80);
    QueueSound(0xD8);
}

/* $05B016. State $14: then the three falling back far off, small. */
void EscapeFarAway(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xCC00);
    wr16(POD_A + O_FLAGS, 0xCC00);
    wr16(POD_B + O_FLAGS, 0xCC00);
    wr32(obj + 0x08, 0x0E1902);
    wr32(POD_A + 0x08, 0x0E1812);
    wr32(POD_B + 0x08, 0x0E1932);
    wr16(obj + O_X, 0x118);
    wr16(POD_A + O_X, 0x110);
    wr16(POD_B + O_X, 0x108);
    wr16(obj + O_Y, 0x88);
    wr16(POD_A + O_Y, 0x90);
    wr16(POD_B + O_Y, 0x86);
    wr32(obj + 0x18, 0x1C00);
    wr32(POD_A + 0x18, 0x2000);
    wr32(POD_B + 0x18, 0x2400);
    wr32(obj + 0x1C, 0x6200);
    wr32(POD_A + 0x1C, 0x6000);
    wr32(POD_B + 0x1C, 0x6200);
}

/* $05B0A2. State $18: below y $B0, smaller still. */
void EscapeFalling(uint16_t obj)
{
    if (!ge(obj + O_Y, 0xB0)) return;
    next(obj, 4);
    wr32(obj + 0x08, 0x0E1910);
    wr32(POD_A + 0x08, 0x0E1820);
    wr32(POD_B + 0x08, 0x0E1940);
}

/* $05B0CA. State $1C: below y $C0, a small blast where the pod fell. */
void EscapeCrash(uint16_t obj)
{
    if (!ge(obj + O_Y, 0xC0)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xE000);
    wr32(obj + 0x08, 0x0E1946);
    wr16(obj + 0x0C, 0);
    wr16(POD_A + O_FLAGS, 0x1000);
    wr16(POD_B + O_FLAGS, 0x1000);
    wr16(obj + 0x48, 0x1D);
    wr16(obj + O_X, rd16(POD_A + O_X));
    wr16(obj + O_Y, rd16(POD_A + O_Y));
    QueueSound(0xC5);
}

/* $05B114. State $20: then gone, the boss's state $4C (the stage cleared). */
void EscapeEnd(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    gone(obj);
    wr16(0xC984, 0x4C);
}

/* $05B12A. The pod's halves at it. */
void EscapePod(uint16_t obj)
{
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    wr16(POD_A + O_X, (uint16_t)(x + 0xC));
    wr16(POD_B + O_X, (uint16_t)(x - 0xC));
    wr16(POD_A + O_Y, (uint16_t)(y - 0x12));
    wr16(POD_B + O_Y, (uint16_t)(y - 0x14));
}

/* $05B158. Six radial bursts of smoke (type $214, list $5639C) from the slots at $C9E0, at the
 * boss's x on the floor; the sprites shaken. */
void EscapeBursts(void)
{
    wr16(v_shakeSprites, 0x10);
    wr16(0xA908, 0x100);
    uint16_t a = 0xC9E0, rx = 4, ry = 12;
    for (int k = 5; k >= 0; k--) {
        wr16(a + O_TYPE, 0x214);
        wr16(a + O_X, rd16(0xC990));
        wr16(a + O_Y, 0x140);
        wr16(a + 0x4A, 0xFFC0);
        wr32(a + 0x50, 0x05639C);
        if (k & 1) {
            wr16(a + 0x48, 0x1E0);
            wr16(a + 0x4C, 3);
        } else {
            wr16(a + 0x48, 0);
            wr16(a + 0x4C, 4);
        }
        wr16(a + 0x54, rx);
        wr16(a + 0x56, ry);
        rx = (uint16_t)(rx + 8);
        ry = (uint16_t)(ry + 6);
        a = (uint16_t)(a + 0x60);
    }
}

/* ---- type $4E0: a burning piece thrown from the blast, sparking, bouncing on the floor ($14C)
 * until it settles: then the scene's signal ---- */

/* $05B1C8 */
void EscapeDebris(uint16_t obj)
{
    SparksAt(rd16(obj + O_X), rd16(obj + O_Y));
    call_object(rom32(0x05B1E0 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $05B1EC. Its look, thrown up and toward the middle. */
void EscapeDebrisStart(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x8C00);
    wr16(obj + 0x0E, 0x6687);
    wr16(obj + 0x08, 0);
    wr16(obj + 0x0A, 0xFCFC);
    wr32(obj + 0x1C, 0xFFFE0000);
    wr32(obj + 0x18, ge(obj + O_X, 0x120) ? 0xFFFF2000 : 0xE000);
}

/* $05B22E. Falling; on the floor bouncing at half the speed, a thud, until slower than $.4. */
void EscapeDebrisFall(uint16_t obj)
{
    add32(obj + 0x1C, 0x3000);
    if ((rd32(obj + 0x1C) & 0x80000000u) || !ge(obj + O_Y, 0x14C)) return;
    wr16(obj + O_Y, 0x14C);
    uint32_t v = rd32(obj + 0x1C);
    if ((int32_t)(v - 0x4000) < 0) {
        next(obj, 4);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        set8(v_sceneSignal, 0);
        return;
    }
    wr32(obj + 0x1C, (uint32_t)((int32_t)(0u - v) >> 1));
    QueueSound(0xDC);
}

/* $05B280. Still. */
void EscapeDebrisStill(uint16_t obj)
{
    (void)obj;
}

/* ---- type $168: a hive (vitality $C0) sending out bugs ($16C) at a time by the difficulty,
 * shaking when hit; beaten, a puff and points ---- */

/* $05B282 */
void Hive(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        wr16(obj + O_STATE, 0);
        wr16(obj + O_TYPE, 0x104);
        wr32(obj + 0x48, 0x0563EE);
        wr8(obj + 0x21, 0);
        AddScore(obj, 0x6890);
        return;
    }
    call_object(rom32(0x05B2B8 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $05B2C4. Its look and box. */
void HiveStart(uint16_t obj)
{
    next(obj, 4);
    wr8(obj + 0x20, 0xB0);
    wr16(obj + 0x0E, 0xC6F7);
    wr16(obj + O_FLAGS, 0x8F00);
    wr16(obj + 0x08, 0xE00);
    wr16(obj + 0x0A, 0xF0F4);
    wr16(obj + O_VITALITY, 0xC0);
    wr16(obj + 0x56, 0x30);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xF40BF40C);
}

/* a bug (type $16C) from one of the 12 slots from $DA00 */
static void hive_bug(uint16_t obj)
{
    uint16_t slot = 0xDA00, t;
    if (!SlotFree12(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x16C);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* $05B302. State 4: a bug every so often (the words at $5B340 by the difficulty, and up to $1F
 * more); hit, shaking for $10 frames. */
void HiveWait(uint16_t obj)
{
    if (!dec_neg(obj + 0x56)) {
        if (bit8(obj + 0x22, 5)) {
            next(obj, 4);
            wr16(obj + 0x48, 0x10);
        }
        return;
    }
    wr16(obj + 0x56, (uint16_t)(rom16(0x05B340 + (uint32_t)(int16_t)rd16(RANK)) + (rd16(v_randomSeed) & 0x1F)));
    hive_bug(obj);
}

/* $05B350. State 8: shaking (up 1, 1, 1 then down 3), a bug each frame. */
void HiveShake(uint16_t obj)
{
    if (dec_neg(obj + 0x48)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
        return;
    }
    if (rd16(obj + 0x48) & 3) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 1));
    else add16(obj + O_Y, 3);
    hive_bug(obj);
}

/* $05B396. Type $16C: a bug, bobbing, flying at its player (faster on harder difficulties);
 * gone in a puff with a point when the boss's time is not running or it is hit. */
void Bug(uint16_t obj)
{
    if (!(rd16(v_bossTimer) & 0x8000) || bit8(obj + 0x22, 5)) {
        wr16(obj + 0x1C, (uint16_t)(rd16(obj + 0x1C) - 2));
        wr16(obj + O_STATE, 0);
        wr16(obj + O_TYPE, 0x104);
        wr32(obj + 0x48, 0x056450);
        wr8(obj + 0x21, 0);
        AddScore(obj, 1);
        return;
    }
    add16(obj + 0x48, 1);
    if (bit8(obj + 0x49, 2)) {
        wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 1));
        wr16(obj + 0x0E, 0xC6F6);
    } else {
        add16(obj + O_Y, 1);
        wr16(obj + 0x0E, 0xC6F5);
    }
    if (!rd16(obj + O_STATE)) {
        add16(obj + O_STATE, 1);
        wr8(obj + 0x20, 0x70);
        wr16(obj + O_FLAGS, 0x8F00);
        wr16(obj + 0x08, 0);
        wr16(obj + 0x0A, 0xFCFC);
        wr16(obj + 0x26, 1);
        wr8(obj + 0x21, 0x30);
        wr32(obj + 0x2C, 0xFEFFFF00);
        wr32(obj + 0x28, 0xFA05FA06);
        wr8(obj + 0x23, 0x20);
        return;
    }
    Random();
    uint16_t p = PlayerBySlot(obj);
    uint32_t ax = (uint32_t)(rd8(v_randomSeed) << 8) & 0x3F00, ay = (uint32_t)(rd8(0xFF09) << 8) & 0x3F00;
    if ((int16_t)(uint16_t)(rd16(p + O_X) - rd16(obj + O_X)) < 0) {
        wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
        if ((int32_t)(rd32(obj + 0x18) - 0xFFFEC000u) >= 0) wr32(obj + 0x18, rd32(obj + 0x18) - ax);
    } else {
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
        if ((int32_t)(rd32(obj + 0x18) - 0x14000u) < 0) add32(obj + 0x18, ax);
    }
    uint16_t ty = rd16(p + O_Y);
    if (!rd16(v_difficulty) || bit8(v_gameFrames, 1)) ty = (uint16_t)(ty - 0x20);
    if ((int16_t)(uint16_t)(ty - rd16(obj + O_Y)) < 0) {
        if ((int32_t)(rd32(obj + 0x1C) - 0xFFFF2000u) >= 0) wr32(obj + 0x1C, rd32(obj + 0x1C) - ay);
        return;
    }
    if ((int32_t)(rd32(obj + 0x1C) - 0xC000u) < 0) add32(obj + 0x1C, ay);
}

/* ---- type $170: the crane before the strider's fight: an arm of 8 segments (from $CB00) and a
 * claw ($CDA0) reaching from its base at (x + +$48, y + +$4A), +$4E its length, +$50 its
 * angle; at the end it becomes the strider (type $130) ---- */

#define CLAW 0xCDA0

/* $05B4DC. Its state (the table at $5B4E6). */
void Crane(uint16_t obj)
{
    call_object(rom32(0x05B4E6 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $05B51E. The palette bit, the arm's segments at it (hidden, priority from $70 down), the claw's
 * look; VRAM $E000 cleared. */
void CraneStart(uint16_t obj)
{
    next(obj, 4);
    wr16(0x808A, 0x8000);
    wr16(obj + O_FLAGS, 0x100);
    wr16(obj + 0x4C, 0);
    uint16_t a = 0xCB00;
    uint8_t pri = 0x70;
    for (int k = 0; k < 8; k++, a = (uint16_t)(a + 0x60)) {
        wr16(a + O_TYPE, 0x10);
        wr16(a + O_FLAGS, 0x100);
        wr16(a + 0x0E, 0xC6B4);
        wr16(a + 0x08, 0xA00);
        wr16(a + 0x0A, 0xF4F4);
        wr8(a + 0x20, pri);
        wr16(a + O_X, rd16(obj + O_X));
        wr16(a + O_Y, rd16(obj + O_Y));
        pri = (uint8_t)(pri - 4);
    }
    wr16(0xCB0E, 0x46B4);
    wr16(CLAW + 0x0E, 0x86BD);
    wr16(CLAW + O_FLAGS, 0x4100);
    wr32(CLAW + 0x08, 0x0E1636);
    ClearVramE000();
}

/* $05B596. The next base (the tables at $5BA58, $5BA62, $5BA6C: its offsets and the state to
 * go on with) on the screen (x from $E0), then $20 frames. Each step to the next entry branches
 * back to this routine's start. */
void CraneReach(uint16_t obj)
{
    for (;;) {
        uint32_t i = (uint32_t)(int16_t)rd16(obj + 0x4C);
        wr16(obj + 0x48, rom16(0x05BA58 + i));
        wr16(obj + 0x4A, rom16(0x05BA62 + i));
        wr16(obj + O_STATE, rom16(0x05BA6C + i));
        if ((int16_t)(uint16_t)(rd16(obj + O_X) + rd16(obj + 0x48) - 0xE0) >= 0) {
            wr16(obj + 0x4E, 0x20);
            return;
        }
        add16(obj + 0x4C, 2);
    }
}

/* $05B5DE. States 8, $1C: sparks for +$4E frames. */
void CraneWait(uint16_t obj)
{
    if (dec_neg(obj + 0x4E)) next(obj, 4);
    CraneSparks(obj);
}

/* the arm's segments shown (d0 $8000) or hidden (loc_5B920) */
static void crane_show(uint16_t on)
{
    for (uint16_t a = 0xCB00, k = 0; k < 8; k++, a = (uint16_t)(a + 0x60)) wr16(a + O_FLAGS, (uint16_t)((rd16(a + O_FLAGS) & 0x7FFF) | on));
}

/* $05B5EE. State $C: the arm out at a player, harmful, shown. */
void CraneLunge(uint16_t obj)
{
    QueueSound(0xB7);
    next(obj, 4);
    wr16(obj + 0x4E, 0);
    CraneChain(obj);
    CraneAim(obj);
    CraneHurtful();
    crane_show(0x8000);
}

/* $05B616. State $10: growing by the difficulty's step (the words at $5B652) to $70; then held
 * $20 frames. */
void CraneExtend(uint16_t obj)
{
    wr32(CLAW + 0x08, 0x0E1636);
    CraneHandFlip();
    add16(obj + 0x4E, rom16(0x05B652 + (uint32_t)(int16_t)rd16(RANK)));
    if (ge(obj + 0x4E, 0x70)) {
        wr16(obj + 0x4E, 0x70);
        next(obj, 4);
        wr16(obj + 0x54, 0x20);
    }
    CraneChain(obj);
}

/* $05B662. State $14: held out, the claw snapping. */
void CraneHold(uint16_t obj)
{
    wr32(CLAW + 0x08, 0x0E1636);
    CraneClaw();
    if (dec_neg(obj + 0x54)) next(obj, 4);
    CraneChain(obj);
}

/* $05B67E. State $18: pulled back by 8 a frame; in, hidden and harmless, the next base (state
 * 4). */
void CraneRetract(uint16_t obj)
{
    wr32(CLAW + 0x08, 0x0E163C);
    CraneHandFlip();
    CraneSparks(obj);
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 8));
    if (!(rd16(obj + 0x4E) & 0x8000)) { CraneChain(obj); return; }
    wr16(obj + O_STATE, 4);
    CraneHarmless();
    crane_show(0);
}

/* $05B6AC. State $20: the arm out again pointing down-left ($C0), harmful, shown. */
void CraneSwing(uint16_t obj)
{
    QueueSound(0xB7);
    next(obj, 4);
    wr16(obj + 0x4E, 0);
    wr16(obj + 0x50, 0xC0);
    CraneChain(obj);
    CraneHurtful();
    crane_show(0x8000);
}

/* $05B6D6. State $24: growing as CraneExtend, the claw's breath reset. */
void CraneSwingExtend(uint16_t obj)
{
    CraneExtend(obj);
    wr16(obj + 0x54, 0);
}

/* the arm swinging round (loc_5B788): the claw snapping, its length breathing, sparks */
static void crane_swinging(uint16_t obj)
{
    wr32(CLAW + 0x08, 0x0E1642);
    CraneClaw();
    CraneBreathe(obj);
    CraneSparks(obj);
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) + 6) & 0x1FE);
    CraneChain(obj);
}

/* $05B6E2. State $28: swinging; once the camera holds, three helpers (type $48 in the slots from
 * $CE60) set out across the top, the stage's flag cleared. */
void CraneCall(uint16_t obj)
{
    if (bit8(v_cameraHold, 7)) {
        next(obj, 4);
        wr16(0x8080, 0);
        static const uint16_t slot[3] = { 0xCE60, 0xCEC0, 0xCF20 }, x[3] = { 0xA0, 0x120, 0x1A0 };
        for (int k = 0; k < 3; k++) wr16(slot[k] + O_TYPE, 0x48);
        for (int k = 0; k < 3; k++) wr16(slot[k] + O_STATE, 0);
        for (int k = 0; k < 3; k++) wr16(slot[k] + O_FLAGS, 0);
        for (int k = 0; k < 3; k++) wr16(slot[k] + O_Y, 0x90);
        for (int k = 0; k < 3; k++) wr16(slot[k] + O_X, x[k]);
        for (int k = 0; k < 3; k++) wr16(slot[k] + 6, 0x20);
    }
    crane_swinging(obj);
}

/* $05B766. State $2C: swinging until all three helpers have changed type. */
void CraneCircle(uint16_t obj)
{
    if (rd16(0xCE60) != 0x48 && rd16(0xCEC0) != 0x48 && rd16(0xCF20) != 0x48) next(obj, 4);
    crane_swinging(obj);
}

/* $05B7AA. State $30: folding in; in, a blast's sound, the arm gone, $80 frames. */
void CraneFold(uint16_t obj)
{
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 8) & 0x1FE);
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 8));
    if (!(rd16(obj + 0x4E) & 0x8000)) { CraneChain(obj); return; }
    next(obj, 4);
    wr16(obj + 0x4E, 0x80);
    QueueSound(0xF6);
    for (uint16_t a = 0xCB00, k = 0; k < 8; k++, a = (uint16_t)(a + 0x60)) wr16(a + O_FLAGS, 0x1000);
}

/* $05B7D4. State $34: then the strider (type $130, $380 right, $40 down), the music, the
 * camera let go, the boss's time. */
void CraneLeave(uint16_t obj)
{
    if (!dec_neg(obj + 0x4E)) return;
    QueueSound(0x83);
    wr16(obj + O_STATE, 0);
    wr16(obj + O_TYPE, 0x130);
    wr8(obj + 0x21, 0);
    wr16(obj + O_VITALITY, 0x7FFF);
    add16(obj + O_X, 0x380);
    add16(obj + O_Y, 0x40);
    clr8(v_cameraHold, 7);
    set8(v_cameraHold, 0);
    wr16(v_bossTimer, 2);
}

/* $05B81C. The arm placed: its first segment at the base, the next eight along their delayed
 * angles (each segment's own history of +$50), +$4E/8 apart. The eighth is the slot after the
 * claw ($CE00): kept. */
void CraneChain(uint16_t obj)
{
    uint16_t x = (uint16_t)(rd16(obj + 0x48) + rd16(obj + O_X));
    uint16_t y = (uint16_t)(rd16(obj + 0x4A) + rd16(obj + O_Y));
    wr16(0xCB10, x);
    wr16(0xCB14, y);
    uint16_t step = (uint16_t)divs_w((uint32_t)rd16(obj + 0x4E) << 8, 8), r = step, at = 0x58, seg = 0xCB60;
    for (int k = 0; k < 8; k++) {
        CraneTrail(obj, seg);
        uint16_t a = rd16((uint16_t)(seg + at));
        int32_t s = sine(a), c = cosine(a);
        int16_t len = (int16_t)((int16_t)r >> 8);
        r = (uint16_t)(r + step);
        wr16(seg + O_Y, (uint16_t)(((uint32_t)(s * len) << 2 >> 16) + rd16(0xCB14)));
        wr16(seg + O_X, (uint16_t)(((uint32_t)(c * len) << 2 >> 16) + rd16(0xCB10)));
        at = (uint16_t)(at - 2);
        seg = (uint16_t)(seg + 0x60);
    }
}

/* $05B8A2. A segment's (a0) history of the angle: +$50 in at +$48, the eight words up to +$58
 * moved up one. */
void CraneTrail(uint16_t obj, uint16_t seg)
{
    wr16(seg + 0x48, rd16(obj + 0x50));
    for (uint16_t a = 0x58; a != 0x48; a = (uint16_t)(a - 2)) wr16((uint16_t)(seg + a), rd16((uint16_t)(seg + a - 2)));
}

/* $05B8C0. Aimed at a player (+$52) from the claw: +$50 and every segment's history. */
void CraneAim(uint16_t obj)
{
    uint16_t p = PlayerTarget();
    wr16(obj + 0x52, p);
    uint32_t dy = (uint16_t)(rd16(p + O_Y) - rd16(CLAW + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(p + O_X) - rd16(CLAW + O_X)), &dy);
    a = (uint16_t)((int16_t)a >> 7) & 0x1FE;
    wr16(obj + 0x50, a);
    for (uint16_t s = 0xCB00, k = 0; k < 8; k++, s = (uint16_t)(s + 0x60))
        for (uint16_t i = 0x48; i <= 0x58; i = (uint16_t)(i + 2)) wr16((uint16_t)(s + i), a);
}

/* $05B954. Every 4th frame a spark (type $B8, one of two looks, randomly flipped) at the base,
 * rising, drifting a random way. */
void CraneSparks(uint16_t obj)
{
    uint16_t slot, t;
    if ((rd16(v_gameFrames) & 3) || !SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0xB8);
    wr16(slot + O_FLAGS, 0x8D00);
    uint16_t i = rd16(v_randomSeed) & 2;
    wr16(slot + 0x0E, rom16(0x05B9EC + i));
    wr16(slot + 0x08, rom16(0x05B9EC + i + 4));
    wr16(slot + 0x0A, rom16(0x05B9EC + i + 8));
    wr16(slot + 0x0E, rd16(slot + 0x0E) | (rd16(v_randomSeed) & 0x1800));
    wr16(slot + 0x48, 0x20);
    wr16(slot + 0x4A, 1);
    wr16(slot + 0x1C, 0xFFFE);
    uint16_t d = rd8(v_randomSeed) & 3;
    if (bit8(v_randomSeed, 5)) d = (uint16_t)((d ^ 0xFFFF) + 1);
    wr16(slot + 0x18, d);
    wr16(slot + O_X, (uint16_t)(rd16(obj + 0x48) + rd16(obj + O_X)));
    wr16(slot + O_Y, (uint16_t)(rd16(obj + 0x4A) + rd16(obj + O_Y)));
}

/* $05B9F8. The claw harmful. */
void CraneHurtful(void)
{
    wr32(CLAW + 0x2C, 0xF808F808);
    wr32(CLAW + 0x28, 0xE020E020);
    wr8(CLAW + 0x21, 0x30);
    wr8(CLAW + 0x23, 0x20);
    wr16(CLAW + 0x26, 0xA);
}

/* $05BA1C. The claw harmless. */
void CraneHarmless(void)
{
    wr8(CLAW + 0x21, 0);
}

/* $05BA24. The arm's length breathing between $20 and $7E (+$54 2: growing). */
void CraneBreathe(uint16_t obj)
{
    if (!rd16(obj + 0x54)) {
        wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
        if (!ge(obj + 0x4E, 0x20)) wr16(obj + 0x54, 2);
        return;
    }
    add16(obj + 0x4E, 1);
    if (ge(obj + 0x4E, 0x7E)) wr16(obj + 0x54, 0);
}

/* $05BA76. The claw snapping on alternate 8 frames (its frame on 6, a sound every 8th), facing
 * out. */
void CraneClaw(void)
{
    if (rd16(v_gameFrames) & 8) {
        if (!(rd16(v_gameFrames) & 7)) QueueSound(0xB8);
        add32(CLAW + 0x08, 6);
    }
    CraneHandFlip();
}

/* $05BA9C. The claw flipped facing away from the segment before it ($CD40). */
void CraneHandFlip(void)
{
    uint16_t c = CLAW, s = 0xCD40;
    if ((int16_t)(uint16_t)(rd16(s + O_X) - rd16(c + O_X)) < 0) wr16(c + 0x0E, rd16(c + 0x0E) & 0xF7FF);
    else wr16(c + 0x0E, rd16(c + 0x0E) | 0x800);
    if ((int16_t)(uint16_t)(rd16(s + O_Y) - rd16(c + O_Y)) < 0) wr16(c + 0x0E, rd16(c + 0x0E) & 0xEFFF);
    else wr16(c + 0x0E, rd16(c + 0x0E) | 0x1000);
}

/* ---- type $3A8: a floating boss (stage 6), circling about (+$4C, +$50) by +$4A, its two guns
 * ($3B4 at $CBC0, $CC20) at its sides; shots in threes ($3B8), sprays and waves of bombs
 * ($3C0); +7 its flags (bit 1 beaten, 2-4 its looks, 6 and 7 the guns firing); the view
 * ($A994) moved down while it comes and goes ---- */

#define VIEW 0xA994              /* long: the view's offset */

/* $05BAE0. Its state (the table at $5BB4E); beaten (unless +7 bit 1): points, the stage's other
 * objects gone (but its guns, $544), shaking at its place (state $3C). */
void Floater(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 1));
    if (rd16(obj + O_VITALITY) & 0x8000) {
        if (bit8(obj + 7, 1)) {
            wr16(obj + O_VITALITY, 1);
        } else {
            AddScore(obj, 0x61500);
            wr16(obj + O_STATE, 0x3C);
            wr16(obj + 0x4A, rd16(obj + O_X));
            wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 2));
            wr16(obj + 0x4C, rd16(obj + O_Y));
            wr16(obj + O_VITALITY, 0);
            wr8(obj + 0x21, 0);
            wr16(obj + 0x48, 0xA0);
            wr8(obj + 7, 3);
            Stage6Clear(0x3B4, 0x544);
        }
    }
    call_object(rom32(0x05BB4E + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $05BB92. State 0: once the message is done: its look and boxes at ($120, $1E0), its guns,
 * three colours, a sound. */
void FloaterStart(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    next(obj, 4);
    wr8(obj + 7, 0x13);
    wr16(obj + 0x5E, 0xFFFF);
    wr16(obj + O_FLAGS, 0xC080);
    wr16(obj + 0x0E, 0x360);
    wr32(obj + 0x08, 0x0E2B3A);
    wr8(obj + 0x20, 8);
    wr8(obj + 0x46, 0x8F);
    wr32(obj + 0x2C, 0xF812C43C);
    wr32(obj + 0x28, 0xE818C040);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0x1E0);
    FloaterGunsStart();
    for (uint16_t i = 0; i < 6; i += 2) {
        wr16((uint16_t)(0xE352 + i), rom16(0x05BC0E + i));
        wr16((uint16_t)(0xE3D2 + i), rom16(0x05BC0E + i));
    }
    QueueSound(0xF6);
}

/* $05BC14. State 4: rising by 6 to y $70; then $40 frames. */
void FloaterRise(uint16_t obj)
{
    FloaterViewDown();
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 6));
    if (!ge(obj + O_Y, 0x70)) {
        next(obj, 4);
        wr16(obj + O_Y, 0x70);
        wr16(obj + 0x48, 0x40);
    }
    FloaterGunsPlace(obj);
}

/* the circle's centre at its place, its angle 0 */
static void floater_centre(uint16_t obj)
{
    wr16(obj + 0x4A, 0);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0x70);
    wr16(obj + 0x4C, rd16(obj + O_X));
    wr16(obj + 0x4E, 0);
    wr16(obj + 0x50, rd16(obj + O_Y));
    wr16(obj + 0x52, 0);
}

/* $05BC3A. State 8: then dropping (2.6 down), harmful. */
void FloaterWait(uint16_t obj)
{
    FloaterViewDown();
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0x40);
    floater_centre(obj);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0x26000);
    wr8(obj + 7, 0x1A);
    wr8(obj + 0x21, 0x24);
    QueueSound(0x90);
}

/* $05BCA0. State $C: slowing to a stop; then the fight: vitality $F4F, the boss's bar, its
 * guns hurtable. */
void FloaterDrop(uint16_t obj)
{
    FloaterViewDown();
    add32(obj + 0x1C, 0xFFFFF800);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) { FloaterMove(obj); return; }
    next(obj, 4);
    wr8(obj + 7, 0x18);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x48, 0x60);
    wr16(obj + O_VITALITY, 0xF4F);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, obj);
    wr8(0xCBE1, 0x20);
    wr8(0xCC41, 0x20);
    FloaterMove(obj);
}

/* $05BCE4. Moving on its circle, its guns with it, its look. */
void FloaterMove(uint16_t obj)
{
    FloaterOrbit(obj);
    FloaterGunsPlace(obj);
    if (!(rd16(obj + 0x5E) & 0x8000)) {
        wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
        if (ge(obj + 0x5E, 2)) {
            wr32(obj + 0x08, bit8(obj + 7, 2) ? 0x0E2C2A : 0x0E2BEE);
            return;
        }
    } else if (!bit8(obj + 7, 0) && bit8(obj + 0x22, 5)) {
        wr16(obj + 0x5E, 5);
        wr32(obj + 0x08, bit8(obj + 7, 2) ? 0x0E2C2A : 0x0E2BEE);
        return;
    }
    if (bit8(obj + 7, 2)) { wr32(obj + 0x08, 0x0E2C66); return; }
    if (!bit8(obj + 7, 3)) { wr32(obj + 0x08, 0x0E2B3A); return; }
    if (bit8(obj + 7, 4)) {
        clr8(obj + 7, 4);
        wr16(obj + 0x54, 0);
        wr16(obj + 0x5C, 0);
    }
    if (!dec_neg(obj + 0x54)) return;
    add16(obj + 0x5C, 6);
    if (ge(obj + 0x5C, 0x18)) wr16(obj + 0x5C, 0);
    uint32_t a = 0x05C25C + (uint32_t)(int32_t)(int16_t)rd16(obj + 0x5C);
    wr32(obj + 0x08, rom32(a));
    wr16(obj + 0x54, rom16(a + 4));
}

/* $05BCF0. The view moved down (by $.1 a frame, the word to -5). */
void FloaterViewDown(void)
{
    if (!(rd16(VIEW) & 0x8000) || ge(VIEW, 0xFFFB)) wr32(VIEW, rd32(VIEW) - 0x1000);
}

/* $05BD28. The view brought back to $1.0-$1.1. */
void FloaterViewCentre(void)
{
    if ((rd16(VIEW) & 0x8000) || (int32_t)(rd32(VIEW) - 0x10000) < 0) { add32(VIEW, 0x1000); return; }
    if ((int32_t)(rd32(VIEW) - 0x11000) >= 0) wr32(VIEW, rd32(VIEW) - 0x1000);
}

/* $05BD4A. State $10: circling $60 frames; then with $B54 or more vitality leaving (state $2C,
 * the bombs' wave); else a spin of shots (state $14, its side at random) or on a random quarter
 * from difficulty 2 a spray (state $24). */
void FloaterFloat(uint16_t obj)
{
    FloaterViewCentre();
    FloaterMove(obj);
    if (!dec_neg(obj + 0x48)) return;
    if (ge(obj + O_VITALITY, 0xB54)) {
        wr16(obj + O_STATE, 0x2C);
        wr8(obj + 7, 0);
        return;
    }
    wr16(obj + 0x48, 0x60);
    if (ge(v_difficulty, 2) && !(rd16(v_randomSeed) & 3)) {
        wr16(obj + O_STATE, 0x24);
        wr8(obj + 7, 0xC4);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x58, 0x80);
    wr8(obj + 7, bit8(0xFF09, 0) ? 0x44 : 0x84);
}

/* a spark spiralling in to it (type $2F8, loc_5C412) */
static void floater_spark(uint16_t obj)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x2F8);
    wr16(slot + 0x48, rd16(obj + O_X));
    wr16(slot + 0x4A, rd16(obj + O_Y));
    add16(slot + 0x4A, 0x10);
    wr16(slot + 0x52, 2);
    wr16(slot + 0x54, 3);
}

/* $05BDC0. States $14, $24: gathering sparks (on even frames) for +$48 frames, in front; then
 * $40 frames ($100 before the spray). */
void FloaterGather(uint16_t obj)
{
    FloaterViewCentre();
    wr8(obj + 0x20, 0xC0);
    FloaterMove(obj);
    if (dec_neg(obj + 0x48)) {
        next(obj, 4);
        wr8(obj + 0x20, 8);
        wr16(obj + 0x48, 0x40);
        if (rd16(obj + O_STATE) == 0x28) wr16(obj + 0x48, 0x100);
    }
    if (!bit8(0xA001, 0)) floater_spark(obj);
}

/* three shots (type $3B8) $AA apart from +$58 on even frames (loc_5C274); a6 left at the
 * cosines */
static void floater_shoot3(uint16_t obj)
{
    if (bit8(0xA001, 0)) return;
    reg_a6 = COSINES;
    FloaterShootAt(obj, rd16(obj + 0x58), 0xC, SINES);
    FloaterShootAt(obj, (uint16_t)(rd16(obj + 0x58) + 0xAA), 0xC, SINES);
    FloaterShootAt(obj, (uint16_t)(rd16(obj + 0x58) + 0x154), 0xC, SINES);
}

/* $05BE02. State $18: shooting $40 frames; then a spin of $600 frames (its turn by the
 * difficulty, the words at $5BE44, the other way unless +7 bit 6). */
void FloaterAim(uint16_t obj)
{
    FloaterViewCentre();
    FloaterMove(obj);
    if (dec_neg(obj + 0x48)) {
        next(obj, 4);
        wr16(obj + 0x48, 0x5FF);
        wr16(obj + 0x5A, rom16(0x05BE44 + (uint32_t)(int16_t)rd16(v_difficulty)));
        if (!bit8(obj + 7, 6)) wr16(obj + 0x5A, (uint16_t)((rd16(obj + 0x5A) ^ 0xFFFF) + 1));
    }
    floater_shoot3(obj);
}

/* $05BE4C. State $1C: spinning the shots; every $100 frames from difficulty 4 maybe turned the
 * other way (state $20); at the end circling (state $10). */
void FloaterSpin(uint16_t obj)
{
    FloaterViewCentre();
    FloaterMove(obj);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - rd16(obj + 0x5A)));
    if (dec_neg(obj + 0x48)) {
        wr16(obj + O_STATE, 0x10);
        wr8(obj + 7, 0);
        FloaterRest(obj);
        return;
    }
    if (!(rd16(obj + 0x48) & 0xFF) && ge(v_difficulty, 4)) {
        int bit = bit8(0xFF09, 0) ? 6 : 7;
        if (!bit8(obj + 7, bit)) {
            next(obj, 4);
            wr8(obj + 7, (uint8_t)(rd8(obj + 7) ^ 0xC0));
            wr16(obj + 0x5A, (uint16_t)((rd16(obj + 0x5A) ^ 0xFFFF) + 1));
        }
    }
    floater_shoot3(obj);
}

/* $05BEC8. State $20: $30 frames shooting without turning, then the spin again (state $1C; at
 * state 0 it would fall into FloaterSpray). */
void FloaterSpinBack(uint16_t obj)
{
    FloaterViewCentre();
    FloaterMove(obj);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)(uint16_t)((rd16(obj + 0x48) & 0xFF) - 0xD0) >= 0) { floater_shoot3(obj); return; }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
    if (rd16(obj + O_STATE)) { floater_shoot3(obj); return; }
    FloaterSpray(obj);
}

/* a spray (loc_5C32E): ten shots (type $3BC) along a wavering line from below it, the frame
 * count stepped for each (their looks), then put back */
static void floater_spray(uint16_t obj)
{
    SoundC6Every8();
    Random();
    uint16_t n = FloaterSprayStart(obj);
    wr16(obj + 0x5C, (uint16_t)((rd16(obj + 0x5C) ^ 0xFFFF) + 1));
    wr16(obj + 0x5A, (uint16_t)(0x100 - rd16(obj + 0x5A)));
    uint32_t x = (uint32_t)rd16(obj + O_X) << 16, y = (uint32_t)(uint16_t)(rd16(obj + O_Y) + 0x10) << 16;
    do {
        uint16_t a = (uint16_t)(rd16(obj + 0x5A) + rd16(obj + 0x5C)) & 0x1FE;
        if ((int16_t)(uint16_t)(a - 0xA0) >= 0 || (int16_t)(uint16_t)(a - 0x60) < 0)
            wr16(obj + 0x5C, (uint16_t)((rd16(obj + 0x5C) ^ 0xFFFF) + 1));
        wr16(obj + 0x5A, a);
        add16(v_gameFrames, 1);
        FloaterShotSpawn(0x3BC, 1, 0, 0, (uint16_t)(x >> 16), (uint16_t)(y >> 16));
        a = rd16(obj + 0x5A);
        y += (uint32_t)sine(a) << 6;
        x += (uint32_t)cosine(a) << 6;
    } while (n-- != 0);
    wr16(v_gameFrames, (uint16_t)(rd16(v_gameFrames) - 0xA));
}

/* $05BEEC. State $28: the spray, for +$48 frames; then circling. */
void FloaterSpray(uint16_t obj)
{
    FloaterViewCentre();
    FloaterMove(obj);
    if (dec_neg(obj + 0x48)) {
        wr16(obj + O_STATE, 0x10);
        wr8(obj + 7, 0);
        FloaterRest(obj);
        return;
    }
    floater_spray(obj);
}

/* $05BF00. State $2C: rising off the top, harmless and hidden there ($40 frames at y $20). */
void FloaterLeave(uint16_t obj)
{
    FloaterViewDown();
    FloaterMove(obj);
    add32(obj + 0x1C, 0xFFFFFC00);
    if (ge(obj + O_Y, 0x70)) return;
    next(obj, 4);
    wr8(obj + 0x21, 0);
    clr8(obj + O_FLAGS, 7);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x48, 0x40);
    wr16(obj + O_Y, 0x20);
    FloaterGunsPlace(obj);
}

/* $05BF44. State $30: away; then a wave of bombs (+$48 a random one of four lists). */
void FloaterAway(uint16_t obj)
{
    FloaterViewDown();
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + 0x48, rd16(v_randomSeed) & 0xC000);
    wr16(obj + 0x5A, 0x10);
}

/* $05BF68. State $34: the wave's bombs; at its end back in, harmful, dropping. */
void FloaterBombs(uint16_t obj)
{
    FloaterViewDown();
    if (!FloaterBombWave(obj)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0x80);
    set8(obj + O_FLAGS, 7);
    wr8(obj + 0x21, 0x24);
    floater_centre(obj);
    wr32(obj + 0x1C, 0x26000);
}

/* $05BFBE. State $38: $80 frames, then dropping back in to a stop; then circling. */
void FloaterReturn(uint16_t obj)
{
    FloaterViewDown();
    if (!dec_neg(obj + 0x48)) return;
    add32(obj + 0x1C, 0xFFFFF800);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) { FloaterMove(obj); return; }
    wr16(obj + O_STATE, 0x10);
    wr8(obj + 7, 0x18);
    wr32(obj + 0x1C, 0);
    FloaterRest(obj);
    FloaterMove(obj);
}

/* the allies' step 4 (loc_50136: the one at $A800, then the four from $C9E0 still their types,
 * +6's byte clear: their steps from $5019E, on by 8 for each one set) */
static void allies_step4(void)
{
    Stage6AllyA800(4);
    uint16_t d0 = 4, a0 = 0xC9E0;
    uint32_t a2 = 0x0501C6;
    for (int i = 0; i < 4; i++) {
        if (!rd8(a0 + 6) && rd16(a0) == rom16(a2)) {
            wr16(a0 + O_STATE, rom16(0x05019E + (uint32_t)(int16_t)d0));
            d0 = (uint16_t)(d0 + 8);
        }
        a0 = (uint16_t)(a0 + 0x60);
        a2 += 2;
    }
}

/* $05BFF2. State $3C: beaten: shaking in smoke for $A0 frames (blasts every $10); then the
 * allies' next step, state $40. */
void FloaterBeaten(uint16_t obj)
{
    FloaterViewCentre();
    FloaterPuff(obj, 0x056450);
    if (dec_neg(obj + 0x48)) {
        next(obj, 4);
        wr16(obj + 0x48, 0x80);
        QueueSound(0xA3);
        FloaterShake(obj);
        allies_step4();
        return;
    }
    if (rd16(v_gameFrames) & 0xF) { FloaterShake(obj); return; }
    QueueSound(0xC5);
    FloaterShake(obj);
}

/* $05C03C. Shaken about (+$4A, +$4C) by the frame, its look with it; its guns with it. */
void FloaterShake(uint16_t obj)
{
    uint16_t x = rd16(obj + 0x4A);
    if (!bit8(0xA001, 0)) x = (uint16_t)(x + 4);
    wr16(obj + O_X, x);
    uint16_t y = rd16(obj + 0x4C);
    if (!bit8(0xA001, 1)) {
        wr32(obj + 0x08, 0x0E2B3A);
        wr16(obj + O_Y, y);
    } else {
        wr32(obj + 0x08, 0x0E2BEE);
        wr16(obj + O_Y, (uint16_t)(y + 4));
    }
    FloaterGunsPlace(obj);
}

/* $05C080. State $40: exploding, $40 frames past the end gone with its guns. */
void FloaterExplode(uint16_t obj)
{
    FloaterViewCentre();
    FloaterPuff(obj, 0x0563EE);
    FloaterShake(obj);
    if (!dec_neg(obj + 0x48)) return;
    BlastSound8();
    if (ge(obj + 0x48, 0xFFC0)) return;
    gone(obj);
    wr16(0xCBC2, 0x1000);
    wr16(0xCC22, 0x1000);
}

/* $05C0BE. Its two guns ($CBC0 flipped back, $CC20). */
void FloaterGunsStart(void)
{
    FloaterGunSetup(0xCBC0);
    clr8(0xCBC0 + 0x0E, 3);
    FloaterGunSetup(0xCC20);
}

/* $05C0D0. A gun (a0): type $3B4, its look, box and vitality $36F. */
void FloaterGunSetup(uint16_t part)
{
    wr16(part + O_TYPE, 0x3B4);
    wr16(part + O_STATE, 0);
    wr16(part + O_FLAGS, 0x8000);
    wr16(part + 0x0E, 0x4B86);
    wr16(part + 0x08, 0x600);
    wr16(part + 0x0A, 0xF8F4);
    wr8(part + 0x21, 0);
    wr32(part + 0x28, 0xF010F010);
    wr16(part + O_VITALITY, 0x36F);
}

/* $05C104. The guns $18 to each side, $14 below. */
void FloaterGunsPlace(uint16_t obj)
{
    uint16_t y = (uint16_t)(rd16(obj + O_Y) + 0x14), x = rd16(obj + O_X);
    wr16(0xCBC0 + O_Y, y);
    wr16(0xCC20 + O_Y, y);
    wr16(0xCBC0 + O_X, (uint16_t)(x - 0x18));
    wr16(0xCC20 + O_X, (uint16_t)(x + 0x18));
}

/* $05C134. The centre moved by the speeds; the angle on 6 (back while moving left); its place
 * $40 wide and $20 tall about it. */
void FloaterOrbit(uint16_t obj)
{
    add32(obj + 0x4C, rd32(obj + 0x18));
    add32(obj + 0x50, rd32(obj + 0x1C));
    uint16_t a = rd16(obj + 0x4A);
    a = bit8(obj + 0x18, 7) ? (uint16_t)(a - 6) : (uint16_t)(a + 6);
    a &= 0x1FE;
    wr16(obj + 0x4A, a);
    wr16(obj + O_Y, (uint16_t)(((uint32_t)sine(a) << 6 >> 16) + rd16(obj + 0x50)));
    wr16(obj + O_X, (uint16_t)(((uint32_t)cosine(a) << 5 >> 16) + rd16(obj + 0x4C)));
}

/* $05C2AA. A shot (type $3B8, +$48 d2) at angle d0 from $10 below it, speed 64 times the tables
 * at a4 (sines) and a6 (cosines); its sound every 8th frame. */
void FloaterShootAt(uint16_t obj, uint16_t angle, uint16_t arg, uint32_t sines)
{
    angle &= 0x1FE;
    uint32_t vy = (uint32_t)(int32_t)(int16_t)bus_rd16(sines + (uint32_t)(int16_t)angle) << 6;
    uint32_t vx = (uint32_t)(int32_t)(int16_t)bus_rd16(reg_a6 + (uint32_t)(int16_t)angle) << 6;
    SoundC6Every8();
    FloaterShotSpawn(0x3B8, arg, vx, vy, rd16(obj + O_X), (uint16_t)(0x10 + rd16(obj + O_Y)));
}

/* $05C2D8. A shot of type d1 (+$48 d2, speeds d4 and d5) at (d6, d7): harmful, its look by the
 * frame. */
void FloaterShotSpawn(uint16_t type, uint16_t arg, uint32_t vx, uint32_t vy, uint16_t x, uint16_t y)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_FLAGS, 0x8C00);
    FloaterShotLook(slot);
    wr16(slot + 0x26, 0xC);
    wr8(slot + 0x20, 4);
    wr8(slot + 0x21, 0x10);
    wr32(slot + 0x2C, 0xFF01FF01);
    wr8(slot + 0x23, 0x20);
    wr16(slot + O_TYPE, type);
    wr16(slot + 0x48, arg);
    wr32(slot + 0x18, vx);
    wr32(slot + 0x1C, vy);
    wr16(slot + O_X, x);
    wr16(slot + O_Y, y);
}

/* $05C3DC. The spray's start: its angle (+$5A, $62 on by up to $3E) and wavering (+$5C, $20 or
 * $48 either way) from the random bytes; d3 9 (ten shots). */
uint16_t FloaterSprayStart(uint16_t obj)
{
    uint16_t a = (uint16_t)((rd8(0xFF09) & 0x3E) + 0x62);
    uint16_t w = (uint16_t)((rd8(v_randomSeed) & 0x28) + 0x20);
    if (!bit8(0xFF09, 7)) w = (uint16_t)((w ^ 0xFFFF) + 1);
    wr16(obj + 0x5A, a);
    wr16(obj + 0x5C, w);
    return 9;
}

/* $05C440. On even frames a puff (type $100, list d1) at a random place about it; it flickers
 * in front and behind. */
void FloaterPuff(uint16_t obj, uint32_t list)
{
    uint16_t slot, t;
    if (!bit8(0xA001, 0) && SlotFreeHigh(&slot, &t)) {
        wr16(slot + O_TYPE, 0x100);
        wr32(slot + 0x48, list);
        wr16(slot + O_X, (uint16_t)((rd8(0xFF09) & 0x7F) - 0x40 + rd16(obj + O_X) + 8));
        wr16(slot + O_Y, (uint16_t)((rd8(v_randomSeed) & 0x1F) - 0x10 + rd16(obj + O_Y)));
    }
    wr8(obj + 0x20, bit8(0xA001, 0) ? 0xBC : 0x10);
}

/* $05C4A0. +$48: the rest by the difficulty (the words at $5C4B2). */
void FloaterRest(uint16_t obj)
{
    wr16(obj + 0x48, rom16(0x05C4B2 + (uint32_t)(int16_t)rd16(RANK)));
}

/* $05C4C2. A wave of bombs (type $3C0) from a list (four, by +$48's top bits; four bytes an
 * entry: the wait to the next, its turn; its difficulty and angle, bit 7 the last; its two
 * counts; its x): the entries due this frame, those for the difficulty or easier. !Z at the
 * list's last. */
int FloaterBombWave(uint16_t obj)
{
    int last = 0;
    if (!dec_neg(obj + 0x5A)) return last;
    do {
        uint16_t n = rd16(obj + 0x48);
        add16(obj + 0x48, 4);
        uint32_t at = rom32(0x05C5A0 + ((n >> 12) & 0xC)) + (n & 0xFFC);
        uint16_t b = rom8(at++);
        uint16_t turn = (uint16_t)((b & 0xF) << 1);
        if (turn & 0x10) turn = (uint16_t)((turn ^ 0xFFE0) + 2);
        wr16(obj + 0x5A, b & 0xF0);
        uint16_t k = rom8(at++);
        if (k & 0x80) {
            last = 1;
            k &= 0x7F;
        }
        uint16_t level = k & 6;
        if (level == rd16(v_difficulty) || (int16_t)(uint16_t)(level - rd16(v_difficulty)) < 0) {
            uint16_t angle = (uint16_t)((k << 2) & 0x1C0);
            uint16_t c = rom8(at++);
            uint16_t c1 = (c >> 4) & 0xF, c2 = c & 0xF;
            if (c1 & 8) c1 = 0xFFFF;
            if (c2 & 8) c2 = 0xFFFF;
            uint16_t x = (uint16_t)(rom8(at++) + 0x80), slot, t;
            if (SlotFreeHigh(&slot, &t)) {
                wr16(slot + O_TYPE, 0x3C0);
                wr16(slot + 0x4A, c1);
                wr16(slot + 0x4C, c2);
                wr16(slot + O_X, x);
                wr16(slot + 0x48, angle);
                wr16(slot + 0x50, turn);
                wr16(slot + O_Y, 0x98);
                wr16(slot + 0x1C, 1);
            }
        }
    } while (!rd16(obj + 0x5A));
    return last;
}

/* $05C6B8. Type $3B4: a gun at the floater's side ($CBC0 the left one, unflipped), its priority
 * the floater's: lit while firing (+7 bit 7 left, 6 right; a sound every 8th frame); beaten
 * (not while the floater's vitality is below 0), points and a burst ($54). */
void FloaterGun(uint16_t obj)
{
    wr8(obj + 0x20, rd8(0xCB80));
    uint16_t look = 0x4B8C;
    if (!rd16(obj + O_STATE)) {
        if ((rd16(0xCB84) & 0x8000) || (rd16(obj + O_VITALITY) & 0x8000)) {
            if (!(rd16(0xCB84) & 0x8000)) AddScore(obj, 0x10000);
            next(obj, 4);
            wr8(obj + 0x21, 0);
            uint16_t slot, t;
            if (SlotFreeHigh(&slot, &t)) {
                wr16(slot + O_TYPE, 0x54);
                wr16(slot + O_X, rd16(obj + O_X));
                wr16(slot + O_Y, rd16(obj + O_Y));
                wr32(slot + 0x1C, 0xFFFFC000);
                return;
            }
        } else {
            look = 0x4B78;
            if (bit8(0xCB67, obj == 0xCBC0 ? 7 : 6)) {
                if (!(rd16(0xCBB6) & 7)) QueueSound(0xAD);
                if (!bit8(0xCBB7, 2)) look = 0x4B86;
            }
        }
    }
    wr16(obj + 0x0E, look);
    if (obj == 0xCBC0) clr8(obj + 0x0E, 3);
}

/* $05C788. Type $3C0: a bomb (vitality $64) falling to y $122, every other frame shooting
 * (+$4A, +$4C + 1 shots, $100 apart, at +$48, which turns by +$50); beaten, points and a burst. */
void FloaterBomb(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0x8400);
        wr16(obj + 0x0E, 0x4402);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
        wr16(obj + O_VITALITY, 0x64);
        wr8(obj + 0x21, 0x20);
        wr32(obj + 0x28, 0xFC04FC04);
        return;
    }
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x500);
        wr16(obj + O_TYPE, 0x54);
        wr16(obj + O_STATE, 0);
        wr16(obj + O_FLAGS, 0);
        return;
    }
    if (ge(obj + O_Y, 0x122)) { gone(obj); return; }
    if (bit8(0xA001, 0)) return;
    wr16(obj + 0x52, rd16(obj + 0x4A));
    if (!(rd16(obj + 0x52) & 0x8000)) {
        wr16(obj + 0x54, rd16(obj + 0x48));
        FloaterBombBurst(obj);
    }
    wr16(obj + 0x52, rd16(obj + 0x4C));
    if (!(rd16(obj + 0x52) & 0x8000)) {
        add16(obj + 0x54, 0x100);
        FloaterBombBurst(obj);
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) + rd16(obj + 0x50)));
}

/* $05C83A. +$52 + 1 shots (type $3BC) along +$54 from it, 64 apart. */
void FloaterBombBurst(uint16_t obj)
{
    wr16(obj + 0x54, rd16(obj + 0x54) & 0x1FE);
    uint32_t x = (uint32_t)rd16(obj + O_X) << 16, y = (uint32_t)rd16(obj + O_Y) << 16;
    do {
        uint16_t a = rd16(obj + 0x54);
        y += (uint32_t)sine(a) << 6;
        x += (uint32_t)cosine(a) << 6;
        FloaterShotSpawn(0x3BC, 1, 0, 0, (uint16_t)(x >> 16), (uint16_t)(y >> 16));
    } while (!dec_neg(obj + 0x52));
}

/* a splash in the water ($120: loc_5C8C6): gone, a splash (type $100, list $565F6) thrown up a
 * random way, its sound */
static void shot_splash(uint16_t obj)
{
    gone(obj);
    uint16_t slot, t;
    if (SlotFreeMid(&slot, &t)) {
        wr16(slot + O_TYPE, 0x100);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, 0x120);
        wr32(slot + 0x48, 0x0565F6);
        uint16_t a = (uint16_t)((rd16(v_randomSeed) & 0xFE) + 0x100);
        wr32(slot + 0x1C, (uint32_t)sine(a) << 5);
        wr32(slot + 0x18, (uint32_t)cosine(a) << 5);
    }
    QueueSound(0xC7);
}

/* $05C890. Type $3BC: a pellet for +$48 frames, splashing at the water ($120). */
void FloaterPellet(uint16_t obj)
{
    if (dec_neg(obj + 0x48)) { gone(obj); return; }
    if (ge(obj + O_Y, 0x120)) shot_splash(obj);
}

/* $05C8AC. Type $3B8: a shot for +$48 frames, its look by the frame, splashing at the water. */
void FloaterShot(uint16_t obj)
{
    if (dec_neg(obj + 0x48)) { gone(obj); return; }
    if (!ge(obj + O_Y, 0x120)) { FloaterShotLook(obj); return; }
    shot_splash(obj);
}

/* $05C928. Its look by the frame (four from $5674C). */
void FloaterShotLook(uint16_t obj)
{
    uint32_t a = 0x05674C + ((uint16_t)(rd16(v_gameFrames) << 1) & 0x18);
    wr16(obj + 0x0E, rom16(a));
    wr16(obj + 0x08, rom16(a + 2));
    wr16(obj + 0x0A, rom16(a + 4));
}

/* ---- type $564 (no run reaches it): a boss who fights on foot (stage 6): it walks, hops, leaps,
 * dashes, kicks, guards, shoots fans of shots ($588), grabs a player (+$40) and spins and throws
 * them; knocked about by hits; beaten, it falls (state $8C) and the stage's objects are cleared.
 * +$50 a long clock, +$56 the floor's y, +$5C, +$5D its flags (bits of +$5D: 0, 1 against the
 * right or left wall, 2 a look, 7 may grab) ---- */

/* the blinking after a hit (loc_30CF4's code): +$5A frames, shown on its even ones */
static void grappler_blink(uint16_t obj)
{
    clr8(obj + 0x23, 2);
    uint16_t t = rd16(obj + 0x5A);
    if (t && !(t & 0x8000)) {
        set8(obj + 0x23, 2);
        wr16(obj + 0x5A, (uint16_t)(t - 1));
        if (!bit8(obj + 0x5B, 0)) {
            wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0x7FFF);
            return;
        }
    }
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x8000);
}

/* $05C94A. Type $564: its frame, then the blinking. */
void Grappler(uint16_t obj)
{
    GrapplerFrame(obj);
    grappler_blink(obj);
}

/* $05C954. The clock (+$50, to $1600); beaten: points, thrown up off the side it is on (state
 * $8C); kept between the walls ($8E-$192, below $88); hit (unless +$58 counts): hurt (states
 * $18, $14, $54 knocked down) and thrown back; thrown by a player: state $1C; then its state
 * (the table at $5CB66). */
void GrapplerFrame(uint16_t obj)
{
    add16(obj + 0x50, 1);
    if (ge(obj + 0x50, 0x1600)) wr16(obj + 0x50, 0);
    if (!bit8(obj + 0x5C, 0) && (rd16(obj + O_VITALITY) & 0x8000)) {
        AddScore(obj, 0x77776);
        wr16(obj + O_STATE, 0x8C);
        wr16(obj + O_FLAGS, 0xEC00);
        wr32(obj + 0x08, 0x0E20E6);
        wr16(obj + 0x0C, 0);
        wr16(obj + 0x4A, rd16(obj + O_Y));
        wr16(obj + 0x5A, 0);
        wr8(obj + 0x5C, 7);
        wr8(obj + 0x21, 0);
        wr16(obj + O_VITALITY, 0);
        QueueSound(0xC5);
        wr32(obj + 0x1C, 0xFFFBC000);
        if (!ge(obj + O_X, 0x120)) {
            set8(obj + 0x0E, 3);
            wr32(obj + 0x18, 0x28000);
        } else {
            clr8(obj + 0x0E, 3);
            wr32(obj + 0x18, 0xFFFD8000);
        }
    } else {
        if (rd16(obj + O_STATE)) {
            if (!ge(obj + O_Y, 0x88)) wr16(obj + O_Y, 0x8A);
            wr8(obj + 0x5D, rd8(obj + 0x5D) & 0xFC);
            if (!bit8(obj + 0x5C, 2)) {
                if (!ge(obj + O_X, 0x8C)) {
                    wr16(obj + O_X, 0x8E);
                    set8(obj + 0x5D, 1);
                } else if (ge(obj + O_X, 0x194)) {
                    wr16(obj + O_X, 0x192);
                    set8(obj + 0x5D, 0);
                }
            }
        }
        uint16_t c = rd16(obj + 0x58);
        if (bit8(obj + 0x22, 5) && !bit8(obj + 0x5C, 1) && (!c || (c & 0x8000))) {
            wr8(obj + 0x5C, 2);
            wr8(obj + 0x5D, rd8(obj + 0x5D) & 3);
            wr16(obj + O_FLAGS, 0xEC00);
            wr16(obj + 0x0C, 0);
            int down = bit8(obj + 0x23, 4);
            clr8(obj + 0x23, 4);
            if (down) {
                wr16(obj + O_STATE, 0x54);
            } else if (!dec_neg(obj + 0x5E)) {
                wr16(obj + O_STATE, 0x18);
                wr16(obj + 0x58, 3);
                wr32(obj + 0x08, 0x0E20CC);
                wr32(obj + 0x18, 0);
                wr32(obj + 0x1C, 0);
                call_object(rom32(0x05CB66 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
                return;
            } else {
                wr16(obj + O_STATE, 0x14);
            }
            wr8(obj + 0x21, 0);
            wr16(obj + 0x58, 0);
            wr16(obj + 0x5E, 0xE);
            wr32(obj + 0x08, 0x0E20E6);
            wr32(obj + 0x1C, 0xFFFCF000);
            wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0x18000 : 0xFFFE8000);
        } else if (bit8(obj + 0x23, 1)) {
            clr8(obj + 0x23, 1);
            uint16_t by = rd16(obj + 0x42);
            if (by == 0xA400 || by == 0xA600) {
                wr16(obj + O_STATE, 0x1C);
                wr16(obj + 0x58, 0);
                wr8(obj + 0x5C, 2);
                wr8(obj + 0x5D, rd8(obj + 0x5D) & 3);
                wr16(obj + O_FLAGS, 0xEC00);
                wr32(obj + 0x08, 0x0E20E6);
                wr16(obj + 0x0C, 0);
                wr32(obj + 0x18, 0);
                wr32(obj + 0x1C, 0);
                wr8(obj + 0x21, 0);
                wr16(obj + 0x48, 0x10);
            }
        }
    }
    call_object(rom32(0x05CB66 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $05CC02. State 0: hidden behind, at x $15EB on the planes' scroll, y $120; its partners ($580
 * at $DE80, $584 at $DEE0). */
void GrapplerStart(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xE100);
    wr16(obj + 0x0E, 0xB00);
    wr8(obj + 0x20, 0x10);
    wr16(obj + 0x50, rd16(v_gameFrames) & 0x1FF);
    wr32(obj + 0x08, 0x0E26FA);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x21, 0);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x28, 0xF00AF40C);
    wr16(obj + O_X, (uint16_t)(0x15EB - rd16(v_planes)));
    wr16(obj + O_Y, 0x120);
    wr16(obj + 0x58, 0);
    wr16(obj + 0x5A, 0);
    wr8(obj + 0x5C, 7);
    wr16(obj + 0x5E, 9);
    wr16(0xDE80 + O_TYPE, 0x580);
    wr16(0xDE80 + O_STATE, 0);
    wr16(0xDEE0 + O_TYPE, 0x584);
    wr16(0xDEE0 + O_STATE, 0);
}

/* $05CC84. State 4: once the message is done and the camera holds, a pose for $60 frames. */
void GrapplerWait(uint16_t obj)
{
    if (rd16(0x80C2) || !bit8(v_cameraHold, 7)) return;
    next(obj, 4);
    wr32(obj + 0x08, 0x0E26D2);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x48, 0x60);
    QueueSound(0xF6);
    GrapplerIntro(obj);
}

/* $05CCB6. State 8: then its vitality ($11F9), the boss's bar, its music; $18 frames. */
void GrapplerIntro(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr32(obj + 0x08, 0x0E26FA);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x48, 0x18);
    wr16(obj + O_VITALITY, 0x11F9);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, obj);
    QueueSound(0x88);
}

/* ready to decide (loc_5CCF6): state $10, standing on the floor (+$56 its y) */
static void grappler_ready(uint16_t obj)
{
    wr16(obj + O_STATE, 0x10);
    uint16_t f = rd16(obj + 0x0E) & 0x800;
    wr16(obj + 0x0E, 0x336);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | f);
    wr8(obj + 0x5C, 0);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x28, 0xEE0CF010);
    GrapplerFloor(obj);
    wr16(obj + 0x56, rd16(obj + O_Y));
    wr16(obj + 0x48, 0);
    wr16(obj + 0x4A, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

/* $05CCEE. State $C: a pause of +$48 frames. */
void GrapplerPause(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    grappler_ready(obj);
}

/* the nearest player turned to (FaceNearest), and what NearestPlayer left in the registers: the
 * player (a0), its distance (d0) and dx (d1). FaceNearest declares none of them; NearestPlayer
 * only reads, so asking it again gives the 68k's values. */
static uint16_t grappler_face(uint16_t obj, uint16_t *p, uint16_t *dx)
{
    FaceNearest(obj);
    uint16_t dy;
    return NearestPlayer(obj, dx, &dy, p);
}

/* the player above or below on the other level: +1 up there, -1 down here, 0 level */
static int grappler_level(uint16_t obj, uint16_t p)
{
    if (!ge(obj + O_Y, 0x110)) return ge(p + O_Y, 0xF8) ? 1 : 0;
    return ge(p + O_Y, 0xD1) ? 0 : -1;
}

/* the hop's set-up (loc_5DC6E): its speed kept for later (+$4C), crouched */
static void grappler_hop_set(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xE800);
    wr32(obj + 0x4C, rd32(obj + 0x18));
    wr32(obj + 0x18, 0);
    wr32(obj + 0x08, 0x0E1E2E);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x5C, 2);
    wr8(obj + 0x5D, 0x80);
}

/* a hop away (loc_5DC98): state $28, 3 sideways */
static void grappler_hop_away(uint16_t obj)
{
    wr8(obj + 0x21, 0);
    wr16(obj + O_STATE, 0x28);
    wr32(obj + 0x1C, 0xFFFD0000);
    wr16(obj + 0x48, 4);
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFD0000 : 0x30000);
    grappler_hop_set(obj);
}

/* a hop up with a speed toward the way it faces (loc_5DCD2) */
static void grappler_hop_on(uint16_t obj)
{
    GrapplerHop(obj);
    wr32(obj + 0x4C, bit8(obj + 0x0E, 3) ? 0xFFFCE000 : 0x32000);
}

/* a hop back (loc_5DCF4): state $30 */
static void grappler_hop_back(uint16_t obj)
{
    GrapplerHop(obj);
    wr16(obj + O_STATE, 0x30);
    wr32(obj + 0x1C, 0xFFFD0000);
    wr32(obj + 0x4C, bit8(obj + 0x0E, 3) ? 0x30000 : 0xFFFD0000);
}

/* a step down with a push (loc_5DD64) */
static void grappler_step_push(uint16_t obj)
{
    GrapplerStep(obj);
    wr8(obj + 0x5D, 0x80);
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFE0000 : 0x20000);
}

/* a flying kick (loc_5DD8C): state $3C */
static void grappler_kick(uint16_t obj)
{
    QueueSound(0xB9);
    wr16(obj + O_STATE, 0x3C);
    wr32(obj + 0x08, 0x0E1E2E);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xFF10F41C);
    wr16(obj + 0x26, 8);
    wr8(obj + 0x5C, 2);
    wr16(obj + 0x48, 0x12);
    FaceNearest(obj);
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFF4000 : 0xC000);
}

/* guarding (loc_5DDF0): state $44 */
static void grappler_guard(uint16_t obj)
{
    wr16(obj + O_STATE, 0x44);
    wr32(obj + 0x08, 0x0E1CA8);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + 0x21, 0);
    wr8(obj + 0x5C, 0x20);
    wr16(obj + 0x48, 0x80);
    FaceNearest(obj);
}

/* standing (loc_5DE20): state $50, $80-$17F frames */
static void grappler_stand(uint16_t obj)
{
    wr16(obj + O_STATE, 0x50);
    wr32(obj + 0x18, 0);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x5C, 0);
    wr16(obj + 0x48, 0);
    wr16(obj + 0x4A, (uint16_t)((rd16(v_randomSeed) & 0xFF) + 0x80));
    FaceNearest(obj);
}

/* a low kick (loc_5DE4E): state $64 */
static void grappler_low_kick(uint16_t obj)
{
    wr16(obj + O_STATE, 0x64);
    wr32(obj + 0x08, 0x0E2058);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + 0x21, 1);
    wr32(obj + 0x2C, 0x0010EA16);
    wr8(obj + 0x5C, 2);
    wr16(obj + 0x48, 8);
    FaceNearest(obj);
}

/* the uppercut's wind-up (loc_5DE88): state $6C */
static void grappler_windup(uint16_t obj)
{
    wr16(obj + O_STATE, 0x6C);
    wr32(obj + 0x08, 0x0E219C);
    wr32(obj + 0x18, 0);
    wr16(obj + O_FLAGS, 0xCC00);
    wr8(obj + 0x21, 0);
    wr8(obj + 0x5C, 0);
    wr16(obj + 0x48, 8);
    FaceNearest(obj);
}

/* blocking (loc_5DEB4): state $74 */
static void grappler_block(uint16_t obj)
{
    wr16(obj + O_STATE, 0x74);
    wr32(obj + 0x08, 0x0E209A);
    wr32(obj + 0x18, 0);
    wr16(obj + O_FLAGS, 0xCC00);
    wr8(obj + 0x21, 2);
    wr8(obj + 0x5C, 0x10);
    wr16(obj + 0x48, 0xA);
    FaceNearest(obj);
}

/* shooting (loc_5DEE2): state $7C, 7 shots */
static void grappler_shoot(uint16_t obj)
{
    wr16(obj + O_STATE, 0x7C);
    wr32(obj + 0x08, 0x0E1F82);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + 0x5C, 2);
    wr16(obj + 0x48, 8);
    wr16(obj + 0x4A, 6);
    if (ge(RANK, 8)) {
        wr8(obj + 0x21, 3);
        wr32(obj + 0x2C, 0xFC08F40C);
        return;
    }
    wr8(obj + 0x21, 2);
}

/* a jump step (loc_5CFA2): rising and falling back to +$56, then 6 frames down, then off
 * again 2 the player's way (d1 its dx) */
static void grappler_jump(uint16_t obj, uint16_t dx)
{
    wr16(obj + O_FLAGS, 0xCC00);
    uint16_t y = rd16(obj + O_Y);
    if (rd32(obj + 0x1C) || (int16_t)(uint16_t)(y - rd16(obj + 0x56)) < 0) {
        wr32(obj + 0x08, 0x0E1DD6);
        add32(obj + 0x1C, 0x2200);
        if (rd32(obj + 0x1C) & 0x80000000u) return;
        wr32(obj + 0x08, 0x0E1DC4);
        if ((int16_t)(uint16_t)(y - rd16(obj + 0x56)) < 0) return;
        wr16(obj + 0x48, 6);
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, rd16(obj + 0x56));
        wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    }
    clr8(obj + 0x5C, 3);
    wr32(obj + 0x08, 0x0E1DB2);
    if (!dec_neg(obj + 0x48)) return;
    set8(obj + 0x5C, 3);
    wr32(obj + 0x1C, 0xFFFF2000);
    wr32(obj + 0x18, (dx & 0x8000) ? 0xFFFE0000 : 0x20000);
}

/* a walk step (loc_5D03C): the same, 7 frames, then 1.4 away from the player */
static void grappler_walk(uint16_t obj, uint16_t dx)
{
    wr16(obj + O_FLAGS, 0xCC00);
    uint16_t y = rd16(obj + O_Y);
    if (rd32(obj + 0x1C) || (int16_t)(uint16_t)(y - rd16(obj + 0x56)) < 0) {
        wr32(obj + 0x08, 0x0E1E0A);
        add32(obj + 0x1C, 0x2000);
        if (rd32(obj + 0x1C) & 0x80000000u) return;
        wr32(obj + 0x08, 0x0E1E1C);
        if ((int16_t)(uint16_t)(y - rd16(obj + 0x56)) < 0) return;
        wr16(obj + 0x48, 7);
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, rd16(obj + 0x56));
        wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    }
    clr8(obj + 0x5C, 3);
    wr32(obj + 0x08, 0x0E1DF8);
    if (!dec_neg(obj + 0x48)) return;
    set8(obj + 0x5C, 3);
    wr32(obj + 0x1C, 0xFFFF3000);
    wr32(obj + 0x18, (dx & 0x8000) ? 0x14000 : 0xFFFEC000);
}

/* the player in range and attacking from behind its guard (+$21 bit 6, +$23 bit 5) */
static int player_attacking(uint16_t p)
{
    return bit8(p + 0x21, 6) && bit8(p + 0x23, 5);
}

/* $05CD3E. State $10: its next move by the nearest player's distance (d0) and level, its
 * vitality, the clock and the difficulty: walking or jumping on (+$5C bit 3), hops, kicks,
 * guarding, standing, blocks, shots. */
void GrapplerDecide(uint16_t obj)
{
    uint16_t p, dx, d = grappler_face(obj, &p, &dx);
    int level = grappler_level(obj, p);
    if (rd8(v_players) != 3 && ge(obj + O_VITALITY, 0xF3C)) {
        if (bit8(obj + 0x5C, 3)) { grappler_walk(obj, dx); return; }
        if (level < 0) { GrapplerHop(obj); return; }
        if (level > 0) { GrapplerStep(obj); return; }
        if ((int16_t)(uint16_t)(d - 0x5F) < 0) {
            if ((int16_t)(uint16_t)(d - 0x20) < 0) { grappler_low_kick(obj); return; }
            if (ge(RANK, 6) && bit8(p + 0x69, 4)) { grappler_kick(obj); return; }
            if (ge(RANK, 8) && !bit8(obj + 0x23, 2) && (int16_t)(uint16_t)(d - 0x40) < 0 && player_attacking(p)) {
                grappler_block(obj);
                return;
            }
        }
        if (ge(obj + O_X, 0x188) || !ge(obj + O_X, 0x98)) { grappler_stand(obj); return; }
        if ((int16_t)(uint16_t)(d - 0x60) >= 0) { grappler_guard(obj); return; }
        grappler_walk(obj, dx);
        return;
    }
    int jumping = 0;
    if (!ge(obj + O_VITALITY, 0x384) || ge(obj + 0x50, 0xE00)) {
        if (level > 0) {
            if (!bit8(0xFF09, 0)) GrapplerStep(obj);
            else grappler_step_push(obj);
            return;
        }
        if (level < 0) {
            if (!(rd16(v_gameFrames) & 3)) GrapplerHop(obj);
            else grappler_hop_on(obj);
            return;
        }
        if (bit8(obj + 0x23, 2)) { grappler_shoot(obj); return; }
        if ((int16_t)(uint16_t)(d - 0x28) < 0) { grappler_low_kick(obj); return; }
        if (!(rd16(v_randomSeed) & 3)) grappler_hop_back(obj);
        else grappler_hop_away(obj);
        return;
    }
    if (ge(obj + 0x50, 0x600)) {
        if (bit8(obj + 0x5C, 3)) {
            jumping = 1;
        } else {
            if ((int16_t)(uint16_t)((rd16(obj + 0x50) & 0x1F) - 0x10) >= 0 && level) {
                if (level < 0) grappler_hop_on(obj);
                else grappler_step_push(obj);
                return;
            }
            if (bit8(obj + 0x23, 2)) { grappler_shoot(obj); return; }
            if ((int16_t)(uint16_t)(d - 0x28) < 0) { grappler_low_kick(obj); return; }
            if (ge(RANK, 4) && (int16_t)(uint16_t)(d - 0x68) < 0 && player_attacking(p)) {
                if (!(rd16(v_randomSeed) & 3)) grappler_kick(obj);
                else grappler_hop_back(obj);
                return;
            }
            if ((int16_t)(uint16_t)((rd16(obj + 0x50) & 0x7F) - 0x70) >= 0 && (int16_t)(uint16_t)(d - 0x60) >= 0) {
                grappler_shoot(obj);
                return;
            }
            if ((int16_t)(uint16_t)(d - 0x30) >= 0) { grappler_jump(obj, dx); return; }
            grappler_walk(obj, dx);
            return;
        }
    } else {
        if (bit8(obj + 0x5C, 3)) {
            jumping = 1;
        } else {
            if (level < 0) { GrapplerHop(obj); return; }
            if (level > 0) { GrapplerStep(obj); return; }
            if (bit8(obj + 0x23, 2)) { grappler_shoot(obj); return; }
            if ((int16_t)(uint16_t)(d - 0x34) < 0) {
                if (!(rd16(v_gameFrames) & 3)) grappler_hop_away(obj);
                else grappler_windup(obj);
                return;
            }
            if (ge(RANK, 6) && (int16_t)(uint16_t)(d - 0x80) < 0 && player_attacking(p)) {
                grappler_block(obj);
                return;
            }
            if ((int16_t)(uint16_t)(d - 0x100) >= 0) { grappler_hop_away(obj); return; }
            if ((int16_t)(uint16_t)(d - 0x60) >= 0) { grappler_jump(obj, dx); return; }
            grappler_walk(obj, dx);
            return;
        }
    }
    if (jumping) {
        uint32_t look = rd32(obj + 0x08);
        if (look == 0x0E1DF8 || look == 0x0E1E0A || look == 0x0E1E1C) grappler_walk(obj, dx);
        else grappler_jump(obj, dx);
    }
}

/* the next pause (loc_5DF3A): state $C for a time by the difficulty (the words at $5DF56) */
static void grappler_next_pause(uint16_t obj)
{
    wr16(obj + O_STATE, 0xC);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x48, rom16(0x05DF56 + (uint32_t)(int16_t)rd16(RANK)));
}

/* $05D0F4. State $14: hurt, falling to the floor; there blinking, a pause. */
void GrapplerLand(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (GrapplerFloor(obj)) return;
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    Boss548Blink(obj);
    wr32(obj + 0x08, 0x0E1C6C);
    grappler_next_pause(obj);
}

/* $05D122. State $18: hurt (+$58 frames), then ready. */
void GrapplerHurt(uint16_t obj)
{
    if (!dec_neg(obj + 0x58)) return;
    wr16(obj + O_STATE, 0xC);
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0E1D24);
    wr16(obj + 0x0C, 0);
    grappler_ready(obj);
}

/* $05D146. State $1C: held by a player until let go (+$23 bit 1). */
void GrapplerHeld(uint16_t obj)
{
    int held = bit8(obj + 0x23, 1);
    clr8(obj + 0x23, 1);
    if (!held) next(obj, 4);
}

/* $05D156. State $20: thrown, facing back from the way it flies. */
void GrapplerThrown(uint16_t obj)
{
    if (bit8(obj + 0x18, 7)) clr8(obj + 0x0E, 3);
    else set8(obj + 0x0E, 3);
    GrapplerBounce(obj);
}

/* $05D170. State $60: flying; against a wall (+$5D bit 0) bounced back hurt (state $60); on the
 * floor bounced up hurt (state $54). */
void GrapplerBounce(uint16_t obj)
{
    int wall = bit8(obj + 0x5D, 0);
    clr8(obj + 0x5D, 0);
    if (wall) {
        wr16(obj + O_STATE, 0x60);
        wr32(obj + 0x18, 0xFFFEC000);
        wr16(v_shakeSprites, 4);
        wr32(obj + 0x08, 0x0E2170);
        wr16(obj + 0x0C, 0);
        wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0x50));
        QueueSound(0xAC);
    }
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (GrapplerFloor(obj)) return;
    wr16(obj + 0x5E, 0xC);
    wr16(obj + O_STATE, 0x54);
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0xF0));
    wr8(obj + 0x21, 0);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 3));
    wr32(obj + 0x1C, 0xFFFCA000);
    wr32(obj + 0x08, 0x0E2170);
    wr16(obj + 0x0C, 0);
    wr16(v_shakeSprites, 4);
    QueueSound(0xAC);
}

/* $05D202. State $54: falling to the floor, then lying $40 frames. */
void GrapplerFallen(uint16_t obj)
{
    wr8(obj + 0x21, 0);
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (GrapplerFloor(obj)) return;
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    next(obj, 4);
    wr32(obj + 0x08, 0x0E2100);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x48, 0x40);
    wr16(v_shakeSprites, 2);
    QueueSound(0xAC);
}

/* $05D248. State $58: then springing up toward the nearest player. */
void GrapplerRise(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xCC00);
    wr32(obj + 0x08, 0x0E2190);
    wr16(obj + 0x56, rd16(obj + O_Y));
    wr32(obj + 0x1C, 0xFFFEE000);
    FaceNearest(obj);
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFF4000 : 0xC000);
}

/* $05D292. State $5C: landing back on its floor, blinking; a pause of $10. */
void GrapplerRiseLand(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if ((int16_t)(uint16_t)(rd16(obj + O_Y) - rd16(obj + 0x56)) < 0) return;
    wr16(obj + O_Y, rd16(obj + 0x56));
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_STATE, 0xC);
    wr16(obj + 0x48, 0x10);
    wr32(obj + 0x08, 0x0E219C);
    Boss548Blink(obj);
}

/* landed (loc_5D2F6): a pause, harmful */
static void grappler_landed(uint16_t obj)
{
    wr16(obj + O_STATE, 0xC);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x5C, 0);
    wr32(obj + 0x08, 0x0E1C6C);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xB1);
}

/* a player grabbed (loc_5D3EA): held (its +$23 bit 1) at it, state $78 */
static void grappler_grab(uint16_t obj, uint16_t p)
{
    wr16(obj + O_STATE, 0x78);
    wr32(obj + 0x08, 0x0E1E96);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_FLAGS, 0xE000);
    wr16(obj + 0x48, 0);
    wr8(obj + 0x21, 0);
    clr8(obj + 0x5D, 7);
    set8(p + 0x23, 1);
    wr16(p + O_X, rd16(obj + O_X));
    wr16(p + O_Y, rd16(obj + O_Y));
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    if (ge(obj + O_X, 0x120)) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
}

/* harmful again (loc_5D448) */
static void grappler_harmful(uint16_t obj)
{
    wr8(obj + 0x21, 0x22);
}

/* rising in a leap (loc_5D3C0): from the difficulty 8 with +$5D bit 7, its grab's box out; a
 * player caught (+$22 bit 0, +$40) grabbed */
static void grappler_reach(uint16_t obj)
{
    if (!bit8(obj + 0x5D, 7)) { grappler_harmful(obj); return; }
    wr8(obj + 0x21, 0x23);
    wr32(obj + 0x2C, 0x000CF010);
    int caught = bit8(obj + 0x22, 0);
    clr8(obj + 0x22, 0);
    if (!caught || !rd16(obj + 0x40)) return;
    grappler_grab(obj, rd16(obj + 0x40));
}

/* the same from the difficulty 8 (loc_5D3B6) */
static void grappler_reach8(uint16_t obj)
{
    if (!ge(RANK, 8)) { grappler_harmful(obj); return; }
    grappler_reach(obj);
}

/* $05D2D2. State $24: a step down to the lower floor: landing, a pause of 8; past y $100 with no
 * floor, from the difficulty $C reaching to grab. */
void GrapplerStepDown(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (!ge(obj + O_Y, 0x100)) return;
    if (GrapplerFloor(obj)) {
        if (!ge(RANK, 0xC)) { grappler_harmful(obj); return; }
        grappler_reach(obj);
        return;
    }
    wr16(obj + 0x48, 8);
    grappler_landed(obj);
}

/* $05D334. State $30: a fan of shots first (with +$48 0), then as GrapplerCrouch. */
void GrapplerCrouchShots(uint16_t obj)
{
    if (!rd16(obj + 0x48)) GrapplerShotFan(obj);
    GrapplerCrouch(obj);
}

/* $05D340. State $28: crouched +$48 frames, then off with the speed kept (+$4C). */
void GrapplerCrouch(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr32(obj + 0x18, rd32(obj + 0x4C));
    wr16(obj + O_FLAGS, 0xEC00);
    QueueSound(0xB0);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x08, rd16(obj + O_STATE) != 0x2C ? 0x0E1C46 : 0x0E1D60);
}

/* $05D37A. States $2C, $34: rising (reaching to grab from the difficulty 6); past the top,
 * falling (state $38). */
void GrapplerLeap(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) {
        if (!ge(RANK, 6)) { grappler_harmful(obj); return; }
        grappler_reach8(obj);
        return;
    }
    wr8(obj + 0x21, 0x22);
    uint16_t s = rd16(obj + O_STATE);
    wr16(obj + O_STATE, 0x38);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x08, s != 0x2C ? 0x0E1D60 : 0x0E1C46);
}

/* $05D450. State $38: falling: against a wall (+$5D bits 1, 0) turned from it and stunned
 * (state $84); on the floor landed (a player caught grabbed); else reaching. */
void GrapplerLeapFall(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    int wall = 1;
    if (bit8(obj + 0x5D, 1)) clr8(obj + 0x0E, 3);
    else if (bit8(obj + 0x5D, 0)) set8(obj + 0x0E, 3);
    else wall = 0;
    if (wall) {
        wr16(obj + O_STATE, 0x84);
        wr16(obj + O_FLAGS, 0xEC00);
        wr32(obj + 0x08, 0x0E20B2);
        wr16(obj + 0x0C, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x48, 0x14);
        wr8(obj + 0x21, 2);
        wr8(obj + 0x5C, 2);
        clr8(obj + 0x5D, 7);
        QueueSound(0xB0);
        return;
    }
    if (GrapplerFloor(obj)) { grappler_reach8(obj); return; }
    int caught = bit8(obj + 0x22, 0);
    clr8(obj + 0x22, 0);
    if (caught && rd16(obj + 0x40)) { grappler_grab(obj, rd16(obj + 0x40)); return; }
    clr8(obj + 0x5D, 7);
    wr16(obj + 0x48, 4);
    grappler_landed(obj);
}

/* $05D4E8. State $84: stunned +$48 frames; then (from the difficulty 4, from 6 or on a random
 * half) a dive kick off the wall (state $88), else a leap back (state $38). */
void GrapplerStunned(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    if (ge(RANK, 4) && (ge(RANK, 6) || !bit8(0xFF09, 1))) {
        next(obj, 4);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x08, 0x0E1FE0);
        wr32(obj + 0x18, 0x4C000);
        wr32(obj + 0x1C, 0xFFFE4000);
        wr32(obj + 0x2C, 0xFF08F010);
        wr16(obj + 0x26, 0x12);
    } else {
        wr16(obj + O_STATE, 0x38);
        wr8(obj + 0x21, 0x22);
        wr32(obj + 0x08, 0x0E1C46);
        wr32(obj + 0x18, 0x18000);
        wr32(obj + 0x1C, 0xFFFF0000);
    }
    wr8(obj + 0x5D, 0);
    clr8(obj + 0x0E, 3);
    wr16(obj + 0x0C, 0);
    if (!ge(obj + O_X, 0x120)) return;
    set8(obj + 0x0E, 3);
    wr32(obj + 0x18, 0u - rd32(obj + 0x18));
}

/* $05D58A. State $88: the dive kick, landing: a pause of $C. */
void GrapplerDive(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (GrapplerFloor(obj)) return;
    wr16(obj + 0x48, 0xC);
    grappler_landed(obj);
}

/* $05D5A8. State $3C: the kick's wind-up, then off at 8 toward the nearest player. */
void GrapplerKickStart(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    FaceNearest(obj);
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFF80000 : 0x80000);
}

/* $05D5D6. State $40: the flying kick, slowing (a trail behind); harmless again under 1.8; at a
 * stop facing the player, a pause. */
void GrapplerKick(uint16_t obj)
{
    GrapplerTrail(obj);
    if (bit8(obj + 0x0E, 3)) {
        add32(obj + 0x18, 0x4000);
        if (rd32(obj + 0x18)) {
            if ((int32_t)(rd32(obj + 0x18) - 0xFFFE8000u) >= 0) { wr8(obj + 0x21, 0x22); wr8(obj + 0x5C, 0); }
            return;
        }
    } else {
        add32(obj + 0x18, 0xFFFFC000);
        if (rd32(obj + 0x18)) {
            if ((int32_t)(rd32(obj + 0x18) - 0x18000u) < 0) { wr8(obj + 0x21, 0x22); wr8(obj + 0x5C, 0); }
            return;
        }
    }
    wr32(obj + 0x08, 0x0E1C6C);
    FaceNearest(obj);
    grappler_next_pause(obj);
}

/* the gun at $DE80 held at its hand (loc_5D710) */
static void grappler_gun(uint16_t obj)
{
    uint16_t g = 0xDE80;
    wr16(g + O_X, rd16(obj + O_X));
    wr16(g + O_Y, rd16(obj + O_Y));
    wr16(g + O_Y, (uint16_t)(rd16(g + O_Y) - 0xB));
    if (bit8(obj + 0x0E, 3)) wr16(g + O_X, (uint16_t)(rd16(g + O_X) - 0x24));
    else add16(g + O_X, 0x24);
}

/* guarding (loc_5D6A0): the player on another level, a hop or a step; near ($60), ready; far
 * with the clock at a multiple of $200, shooting; +$5C bit 6, the guard's next look ($48) */
static void grappler_guarding(uint16_t obj)
{
    uint16_t p, dx, d = grappler_face(obj, &p, &dx);
    if (!ge(obj + O_Y, 0x110)) {
        if (ge(p + O_Y, 0xF8)) { GrapplerStep(obj); return; }
    } else if (!ge(p + O_Y, 0xD1)) {
        GrapplerHop(obj);
        return;
    }
    if ((int16_t)(uint16_t)(d - 0x60) < 0) { grappler_ready(obj); return; }
    if (!(rd16(obj + 0x50) & 0x1FF) && (int16_t)(uint16_t)(d - 0x90) >= 0) { grappler_shoot(obj); return; }
    if (bit8(obj + 0x5C, 6)) {
        wr16(obj + O_STATE, 0x48);
        wr16(obj + 0x48, 8);
        wr32(obj + 0x08, 0x0E1D04);
        wr16(obj + 0x0C, 0);
    }
    grappler_gun(obj);
}

/* $05D636. State $44: guarding; after +$48, the guard raised ($40 frames). */
void GrapplerGuard(uint16_t obj)
{
    if (dec_neg(obj + 0x48)) {
        next(obj, 4);
        wr16(obj + 0x48, 0x40);
        wr32(obj + 0x08, 0x0E1CC8);
        wr16(obj + 0x0C, 0);
    }
    grappler_guarding(obj);
}

/* $05D658. State $48: the guard raised; after +$48, lowered ($60 frames). */
void GrapplerGuardUp(uint16_t obj)
{
    if (dec_neg(obj + 0x48)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
        wr16(obj + 0x48, 0x60);
        wr32(obj + 0x08, 0x0E1CA8);
        wr16(obj + 0x0C, 0);
    }
    grappler_guarding(obj);
}

/* $05D67A. State $4C: facing the player; after +$48, guarding again ($10). */
void GrapplerGuardTurn(uint16_t obj)
{
    FaceNearest(obj);
    if (dec_neg(obj + 0x48)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 8));
        wr16(obj + 0x48, 0x10);
        wr32(obj + 0x08, 0x0E1CA8);
        wr16(obj + 0x0C, 0);
    }
    grappler_gun(obj);
}

/* $05D740. State $50: standing: the player on another level, a hop or a step; far ($70), a hop
 * away; near ($38), a wind-up; the player jumping (+$69 bit 4), a kick. */
void GrapplerStand(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xCC00);
    uint16_t p, dx, d = grappler_face(obj, &p, &dx);
    if (!ge(obj + O_Y, 0x110)) {
        if (ge(p + O_Y, 0xF8)) { GrapplerStep(obj); return; }
    } else if (!ge(p + O_Y, 0xD0)) {
        GrapplerHop(obj);
        return;
    }
    if ((int16_t)(uint16_t)(d - 0x70) >= 0) { grappler_hop_away(obj); return; }
    wr32(obj + 0x18, 0);
    wr32(obj + 0x08, 0x0E209A);
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if ((int16_t)(uint16_t)(d - 0x38) < 0) { grappler_windup(obj); return; }
    if (bit8(p + 0x69, 4)) grappler_kick(obj);
}

/* a player caught in a low kick (loc_5D7C4): held, spun round (state $68, $C frames) */
static void grappler_catch(uint16_t obj)
{
    wr16(obj + O_STATE, 0x68);
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0E1EEE);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x4A, 0xC);
    wr8(obj + 0x21, 0);
    uint16_t p = rd16(obj + 0x40);
    set8(p + 0x23, 1);
    wr16(p + O_X, rd16(obj + O_X));
    wr16(p + O_Y, rd16(obj + O_Y));
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    if (ge(obj + O_X, 0x120)) {
        set8(obj + 0x0E, 3);
        wr16(obj + 0x48, 0);
        wr32(obj + 0x18, 0xFFFF4000);
        return;
    }
    clr8(obj + 0x0E, 3);
    wr16(obj + 0x48, 0x100);
    wr32(obj + 0x18, 0xC000);
}

/* a player caught (+$22 bit 0 and +$40) */
static int grappler_caught(uint16_t obj)
{
    int c = bit8(obj + 0x22, 0);
    clr8(obj + 0x22, 0);
    return c && rd16(obj + 0x40);
}

/* $05D79C. State $64: the low kick: a player caught, spun; else after +$48 a hop or a
 * wind-up. */
void GrapplerLowKick(uint16_t obj)
{
    if (grappler_caught(obj)) { grappler_catch(obj); return; }
    if (!dec_neg(obj + 0x48)) return;
    if (bit8(0xA001, 0)) grappler_hop_away(obj);
    else grappler_windup(obj);
}

/* $05D83C. State $68: the player held (+$40) swung round it on a circle ($40 by $80) for +$4A
 * frames (a trail), then a turn to $1C0 / $140 and thrown hard; then a pause. */
void GrapplerSwing(uint16_t obj)
{
    uint16_t p = rd16(obj + 0x40);
    set8(p + 0x23, 1);
    wr32(p + 0x18, 0);
    wr32(p + 0x1C, 0);
    if (!dec_neg(obj + 0x4A)) {
        GrapplerTrail(obj);
    } else {
        wr32(obj + 0x18, 0);
        int thrown = 0;
        if (!bit8(obj + 0x0E, 3)) {
            add16(obj + 0x48, 0x20);
            if (rd16(obj + 0x48) == 0x1C0) {
                wr32(p + 0x18, 0x64000);
                thrown = 1;
            }
        } else {
            wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 0x20) & 0x1FE);
            if (rd16(obj + 0x48) == 0x140) {
                wr32(p + 0x18, 0xFFF9C000);
                thrown = 1;
            }
        }
        if (thrown) {
            set8(p + 0x23, 4);
            clr8(p + 0x23, 1);
            wr32(p + 0x1C, 0xFFFC8000);
            wr16(obj + 0x40, 0);
            wr16(obj + O_STATE, 0xC);
            wr16(obj + 0x48, 0x10);
            return;
        }
    }
    uint16_t a = rd16(obj + 0x48) & 0x1FE;
    wr32(p + O_Y, ((uint32_t)sine(a) << 6) + rd32(obj + O_Y));
    wr32(p + O_X, ((uint32_t)cosine(a) << 7) + rd32(obj + O_X));
    wr16(p + O_Y, (uint16_t)(rd16(p + O_Y) - 8));
    if (bit8(obj + 0x0E, 3)) wr16(p + O_X, (uint16_t)(rd16(p + O_X) - 0x10));
    else add16(p + O_X, 0x10);
}

/* $05D9D0. State $6C: the wind-up, then an uppercut up and forward (slowing by +$48). */
void GrapplerUppercut(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) return;
    next(obj, 4);
    wr16(v_shakeSprites, 4);
    wr32(obj + 0x08, 0x0E2046);
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xF808F808);
    wr16(obj + 0x26, 0xE);
    wr32(obj + 0x1C, 0xFFFA8000);
    if (bit8(obj + 0x0E, 3)) {
        wr32(obj + 0x18, 0xFFFC0000);
        wr32(obj + 0x48, 0x4000);
        return;
    }
    wr32(obj + 0x18, 0x40000);
    wr32(obj + 0x48, 0xFFFFC000);
}

/* $05DA34. State $70: the uppercut's flight; past the top, falling (state $38). */
void GrapplerUpFall(uint16_t obj)
{
    if (rd32(obj + 0x18)) add32(obj + 0x18, rd32(obj + 0x48));
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    wr16(obj + O_STATE, 0x38);
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0E1C46);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x5C, 2);
}

/* $05DA76. State $74: blocking +$48 frames (longer while hit, +$5C bit 7; its look by +$5D bit
 * 2; pushed back, facing the push). The slowing is computed and not kept: kept. */
void GrapplerBlock(uint16_t obj)
{
    if (dec_neg(obj + 0x48)) { grappler_ready(obj); return; }
    int hit = bit8(obj + 0x5C, 7);
    clr8(obj + 0x5C, 7);
    if (!hit) return;
    add16(obj + 0x48, 1);
    wr32(obj + 0x08, 0x0E209A);
    if (bit8(obj + 0x5D, 2)) wr32(obj + 0x08, 0x0E2060);
    uint32_t v = rd32(obj + 0x18);
    if (!v) return;
    if (v & 0x80000000u) {
        clr8(obj + 0x0E, 3);
        if ((int32_t)(v - 0xFFFFD000u) >= 0) wr32(obj + 0x18, 0);
        return;
    }
    set8(obj + 0x0E, 3);
    if ((int32_t)(v - 0x3000u) < 0) wr32(obj + 0x18, 0);
}

/* $05D91E. State $78: the player held (+$40) at its hands (the words at $5D9C8, two frames of
 * 4); then thrown up and across (state $28). */
void GrapplerGrab(uint16_t obj)
{
    uint16_t p = rd16(obj + 0x40);
    set8(p + 0x23, 1);
    wr32(p + 0x18, 0);
    wr32(p + 0x1C, 0);
    if (rd16(obj + 0x0C) == 1) {
        add16(obj + 0x48, 4);
        if (ge(obj + 0x48, 8)) {
            set8(p + 0x23, 4);
            clr8(p + 0x23, 1);
            wr32(p + 0x1C, 0xFFFB8000);
            wr16(obj + 0x40, 0);
            wr16(obj + O_STATE, 0x28);
            wr16(obj + O_FLAGS, 0xEC00);
            wr32(obj + 0x08, 0x0E1D60);
            wr16(obj + 0x0C, 0);
            wr32(p + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFAC000 : 0x54000);
            return;
        }
    }
    uint16_t i = rd16(obj + 0x48);
    uint16_t dx = rom16(0x05D9C8 + (uint32_t)(int16_t)i);
    uint16_t dy = rom16(0x05D9C8 + (uint32_t)(int16_t)(uint16_t)(i + 2));
    if (bit8(obj + 0x0E, 3)) dx = (uint16_t)(0u - dx);
    wr16(p + O_X, (uint16_t)(dx + rd16(obj + O_X)));
    wr16(p + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
}

/* a shot at a random angle by the difficulty (loc_5DFC0: the masks at $5DFFC, the bases at
 * $5E004) */
static void grappler_random_shot(uint16_t obj, uint16_t dx, uint16_t dy, uint16_t arg)
{
    uint32_t i = (uint32_t)(int16_t)rd16(v_difficulty);
    uint16_t a = (uint16_t)((rd16(v_randomSeed) & rom16(0x05DFFC + i)) + rom16(0x05E004 + i));
    GrapplerShot(obj, a, dx, dy, arg);
}

/* $05DAEA. State $7C: a player caught, held; else every 8 (4 from the difficulty 4) frames a
 * shot, +$4A of them; then a pause (state $C). */
void GrapplerShoot(uint16_t obj)
{
    if (grappler_caught(obj)) { grappler_catch(obj); return; }
    if (!dec_neg(obj + 0x48)) return;
    wr16(obj + 0x48, 8);
    if (ge(v_difficulty, 4)) wr16(obj + 0x48, 4);
    if (dec_neg(obj + 0x4A)) {
        wr16(obj + O_STATE, 8);
        wr16(obj + 0x48, 8);
    }
    next(obj, 4);
    grappler_random_shot(obj, 0xFFF0, 0xFFF9, 0);
}

/* $05DB3C. State $80: between the shots (a player caught, held). */
void GrapplerShootPause(uint16_t obj)
{
    if (grappler_caught(obj)) { grappler_catch(obj); return; }
    if (!dec_neg(obj + 0x48)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
    wr16(obj + 0x48, 8);
    wr32(obj + 0x08, 0x0E1F82);
    wr16(obj + 0x0C, 0);
    FaceNearest(obj);
}

/* $05DB70. State $8C: beaten, thrown up; on the floor bounced (the players held), a stall. */
void GrapplerBeaten(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if ((rd32(obj + 0x1C) & 0x80000000u) || GrapplerFloor(obj)) { GrapplerStall(); return; }
    next(obj, 4);
    set8(0xA4C0, 7);
    set8(0xA6C0, 7);
    wr16(v_shakeSprites, 0xA);
    QueueSound(0xAC);
    wr16(obj + 0x48, rd16(obj + O_Y));
    wr32(obj + 0x08, 0x0E2170);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x1C, 0xFFFCC000);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 2));
    GrapplerStall();
}

/* $05DBC8. A stall: a busy loop of $6001 turns. */
void GrapplerStall(void)
{
    cpu_spin(0x6001);
}

/* $05DBD2. State $90: down again on the floor (+$48): lying $13 frames, a stall each. */
void GrapplerBeatenLand(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if ((rd32(obj + 0x1C) & 0x80000000u) || (int16_t)(uint16_t)(rd16(obj + 0x48) - rd16(obj + O_Y)) >= 0) {
        GrapplerStall();
        return;
    }
    next(obj, 4);
    wr16(obj + O_Y, rd16(obj + 0x48));
    wr32(obj + 0x08, 0x0E2100);
    wr16(obj + 0x0C, 0);
    QueueSound(0xAC);
    wr16(v_shakeSprites, 6);
    wr16(obj + 0x48, 0x13);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    GrapplerStall();
}

/* $05DC22. State $94: then the scene after it (type $604 in the slot before it), the stage's
 * objects cleared. */
void GrapplerEnd(uint16_t obj)
{
    if (!dec_neg(obj + 0x48)) { GrapplerStall(); return; }
    next(obj, 4);
    uint16_t s = (uint16_t)(obj - 0x60);
    wr16(s + O_TYPE, 0x604);
    wr16(s + O_STATE, 0);
    GrapplerStall();
    Stage6Clear(0, 0);
}

/* $05DC4A. State $98: nothing. */
void GrapplerDone(uint16_t obj)
{
    (void)obj;
}

/* $05DC56. A hop up (to the upper floor): state $28. */
void GrapplerHop(uint16_t obj)
{
    wr8(obj + 0x21, 0);
    wr32(obj + 0x1C, 0xFFF9E000);
    wr16(obj + O_STATE, 0x28);
    wr16(obj + 0x48, 0xC);
    grappler_hop_set(obj);
}

/* $05DD2E. A step down (to the lower floor): state $24, half its speed. */
void GrapplerStep(uint16_t obj)
{
    wr8(obj + 0x21, 0);
    wr16(obj + O_STATE, 0x24);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x1C, 0xFFFF0000);
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0E1C46);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x5C, 2);
}

/* $05DF32. On the floor ($10 below, ObjectFloor): Z when not. */
int GrapplerFloor(uint16_t obj)
{
    uint16_t d3, d4;
    return ObjectFloor(obj, 16, &d3, &d4);
}

/* $05DF66. Every 4th frame its trail (type $57C) below it, rising. */
void GrapplerTrail(uint16_t obj)
{
    uint16_t slot, t;
    if ((rd16(v_gameFrames) & 3) || !SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x57C);
    wr16(slot + 0x48, 0xC);
    wr16(slot + O_FLAGS, 0x8400);
    wr16(slot + 0x0E, 0x4565);
    wr16(slot + 0x08, 0);
    wr16(slot + 0x0A, 0xFCFC);
    wr8(slot + 0x20, 0x88);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    add16(slot + O_Y, 0x10);
    wr32(slot + 0x1C, 0xFFFF4000);
}

/* $05DFDC. A shot (type $588, +$48 d7) at angle d0, speed 16, from (d5, d6) off it (mirrored
 * unless it faces left). */
void GrapplerShot(uint16_t obj, uint16_t angle, uint16_t dx, uint16_t dy, uint16_t arg)
{
    uint32_t vy = (uint32_t)sine(angle) << 4, vx = (uint32_t)cosine(angle) << 4;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    QueueSound(0xDB);
    wr16(slot + O_TYPE, 0x588);
    if (!bit8(obj + 0x0E, 3)) {
        dx = (uint16_t)(0u - dx);
        vx = 0u - vx;
    }
    wr32(slot + 0x18, vx);
    wr32(slot + 0x1C, vy);
    wr16(slot + O_X, (uint16_t)(dx + rd16(obj + O_X)));
    wr16(slot + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
    wr16(slot + 0x48, arg);
}

/* $05E00C. From the difficulty 4 a fan of shots, $20 apart from $100 (three from $E0 from the
 * difficulty 8). */
void GrapplerShotFan(uint16_t obj)
{
    if (!ge(RANK, 4)) return;
    uint16_t n;
    if (ge(RANK, 8)) {
        wr16(0x8040, 0xE0);
        n = 2;
    } else {
        wr16(0x8040, 0x100);
        n = 0;
    }
    do {
        GrapplerShot(obj, rd16(0x8040), 0xFFF0, 0xFFF0, 0);
        add16(0x8040, 0x20);
    } while (n-- != 0);
}

/* ---- shared helpers ---- */

/* a puff of smoke (type $2CC, list $563EE or $56450) about a falling ship, from its own slots
 * (loc_2A334 / loc_2A33E's code: no routine of their own): placed by the random bytes, drifting
 * back, rising or sinking by the third. Its state is left as the slot had it. */
static void smoke_puff(uint16_t obj, uint32_t list)
{
    uint16_t e;
    if (!PlayerFindEffect(obj, &e)) return;
    wr16(e + O_TYPE, 0x2CC);
    wr32(e + 0x48, list);
    wr16(e + O_X, (uint16_t)((rd8(0xFF09) & 0x1F) - 0x10 + rd16(obj + O_X)));
    wr16(e + O_Y, (uint16_t)((rd8(v_randomSeed) & 0x1F) - 0x10 + rd16(obj + O_Y)));
    wr16(e + 0x18, 1);
    wr32(e + 0x58, 0xFFFFE000);
    uint16_t i = rd8(0xFF0A) & 0xC;
    wr32(e + 0x1C, rom32(0x02A3B4 + i));
    wr32(e + 0x5C, 0x800);
    if (i >= 8) wr32(e + 0x5C, 0xFFFFF800);
}

/* $003526. The square root of d0, rounded down: up to $271 by odd numbers taken off, up to $FFFF
 * two bits at a time (eight steps), above that sixteen steps (the last two in longs). */
uint32_t Sqrt32(uint32_t v)
{
    if (!v) return v;
    if (v < 0x10000) {
        uint16_t w = (uint16_t)v;
        if (w <= 0x271) {
            uint16_t odd = 0xFFFF;
            do {
                odd = (uint16_t)(odd + 2);
                w = (uint16_t)(w - odd);
            } while (!(w & 0x8000));
            return (uint16_t)((int16_t)odd >> 1);
        }
        uint16_t r = 0, root = 0;
        for (int k = 0; k < 8; k++) {
            r = (uint16_t)(r << 1 | w >> 15);
            w = (uint16_t)(w << 1);
            r = (uint16_t)(r << 1 | w >> 15);
            w = (uint16_t)(w << 1);
            root = (uint16_t)(root << 1);
            uint16_t t = (uint16_t)(root << 1);
            if (r > t) {
                root++;
                r = (uint16_t)(r - (uint16_t)(t + 1));
            }
        }
        return root;
    }
    uint32_t d0 = v, r32;
    uint16_t r = 0, root = 0;
    for (int k = 0; k < 14; k++) {
        r = (uint16_t)(r << 1 | d0 >> 31);
        d0 <<= 1;
        r = (uint16_t)(r << 1 | d0 >> 31);
        d0 <<= 1;
        root = (uint16_t)(root << 1);
        uint16_t t = (uint16_t)(root << 1);
        if (r > t) {
            root++;
            r = (uint16_t)(r - (uint16_t)(t + 1));
        }
    }
    r = (uint16_t)(r << 1 | d0 >> 31);          /* addx.w */
    d0 <<= 1;
    r32 = (uint32_t)r << 1 | d0 >> 31;          /* addx.l */
    d0 <<= 1;
    root = (uint16_t)(root << 1);
    uint32_t t = (uint16_t)(root << 1);
    if (r32 > t) {
        root++;
        r32 -= (uint16_t)(t + 1);
    }
    r32 = r32 << 1 | d0 >> 31;
    d0 <<= 1;
    r32 = r32 << 1 | d0 >> 31;
    root = (uint16_t)(root << 1);
    if (r32 > (uint32_t)root << 1) root++;
    return root;
}

/* $0037F6. A colour (d3) with each component's bits flipped by d4 (x2 per component's place):
 * d4 7 inverts it, 0 keeps it (its low bits cleared). */
uint16_t ColourXor(uint16_t colour, uint16_t flip)
{
    uint16_t f = (uint16_t)(flip << 1);
    uint16_t c = (uint16_t)((colour & 0xE) ^ f);
    f = (uint16_t)(f << 4);
    c |= (uint16_t)((colour & 0xE0) ^ f);
    f = (uint16_t)(f << 4);
    c |= (uint16_t)((colour & 0xE00) ^ f);
    return c;
}

/* $003834. 15 colours from a0 to a1, flipped by d4. */
void ColourXorLine(uint16_t src, uint16_t dst, uint16_t flip)
{
    for (uint16_t i = 0; i < 30; i += 2) wr16((uint16_t)(dst + i), ColourXor(rd16((uint16_t)(src + i)), flip));
}

/* the shot (in `slot`) placed at the muzzle (loc_2A758's code: objects.c has it as a static) */
static void muzzle_shot(uint16_t obj, uint16_t slot, uint32_t offsets, uint16_t angle, uint16_t dir, uint16_t type)
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

/* $02A79E. A shot of `type` from a slot of $D400 on at the target (a0), facing it (`faces` the
 * looks, `offsets` the muzzles): on the easiest difficulties straight at it (FaceToward's), at 2
 * by its sixteenth, from 4 by its 32nd of a turn. */
void AimedSpawnRanked(uint16_t obj, uint16_t target, uint32_t faces, uint32_t offsets, uint16_t type)
{
    uint16_t d = rd16(v_difficulty);
    int fine = (int16_t)(uint16_t)(d - 4) >= 0;
    if (!fine && d != 2) {
        AimedSpawnHighToward(obj, target, faces, offsets, type);
        return;
    }
    uint32_t dy = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &dy);
    uint16_t d0 = fine ? AngleIndex(&a) : AngleSixteenth(&a);
    uint16_t angle = (uint16_t)((int16_t)d0 >> 2);
    uint16_t dir = (uint16_t)((int16_t)AngleEighthOf(a) >> 4), d6, d7 = type;
    FaceAngle(obj, dir, faces, &d6, &d7);
    if (!fine) {
        AimedSpawnHigh(obj, angle, d6, d7, offsets);
        return;
    }
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    muzzle_shot(obj, slot, offsets, angle, d6, d7);
}

/* $02A292. Sound $C6 every 8th frame. */
void SoundC6Every8(void)
{
    if (rd16(v_gameFrames) & 7) return;
    QueueSound(0xC6);
}

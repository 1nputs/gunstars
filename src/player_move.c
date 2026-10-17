/* The players' movement, by state (PlayerMove, the table at $011576, after the states' routines):
 * gravity and the air's steering, the slopes' speeds, friction, carrying what was grabbed, and
 * while shooting the aim: the animation for the shot's direction and the muzzle's place (P_GUN_X,
 * P_GUN_Y) from tables by character and direction, or with the free aim from the direction's
 * sine and cosine. The dust a player raises is an effect of its own. */
#include "game.h"

#define P_SPRITE   0x0E          /* byte: bit 3 flipped (facing left), bit 4 upside down; the word's
                                    bit 11 is the same bit 3 */
#define P_VX       0x18
#define P_VY       0x1C
#define P_STATUS   0x23
#define P_HELD     0x69
#define P_FLOOR_KIND 0x76
#define P_JUMP     0x78
#define P_FLOOR    0x82
#define P_WATER    0x86          /* byte: bit 7 under water (no dust, other animations); as a
                                    word negative also: no slope speed */
#define P_TURNED   0x87
#define P_ANIM_SET 0x90
#define P_TIMER    0x92
#define P_SHOT_B   0x96          /* byte: bit 7 no aim yet, bit 4 the free aim */
#define P_AIM_DIR  0x9A          /* byte: the shot's direction (256 to the turn) */
#define P_GUN_X    0x9E          /* the muzzle's place from the player */
#define P_GUN_Y    0xA0
#define P_ALT_ANIMS 0xA8         /* not 0: the second set of shooting animations */
#define P_ATTACK_DIR 0xAC
#define P_ANIM_END 0xAB          /* byte: the jump's animation still to change at its end */
#define P_THROWN   0xAE
#define P_PUSHED   0xB0          /* longs: a push this frame (added to x, y, then cleared) */
#define P_ANIM     0xB8
#define P_CHARACTER 0xCA

#define SINES      0x02977Cu     /* 256 words, $4000 for 1 (the cosines from +$80) */

static void add_vy(uint16_t obj, uint32_t d) { wr32(obj + P_VY, rd32(obj + P_VY) + d); }

/* $011560. The movement of the player's state (after any push). */
void PlayerMove(uint16_t obj)
{
    PlayerPushed(obj);
    PlayerSplash(obj);
    uint32_t at = 0x011576 + (uint32_t)((int16_t)rd16(obj + O_STATE) >> 1);
    uint32_t fn = at + (uint32_t)(int16_t)rom16(at);
    switch (fn) {
    case 0x0115E6: MoveNone(obj); return;
    case 0x0115E8: MoveStand(obj); return;
    case 0x01164C: MoveShootStand(obj); return;
    case 0x0119A6: MoveNone2(obj); return;
    case 0x0119A8: MoveShootCrouch(obj); return;
    case 0x011A7A: MoveWalk(obj); return;
    case 0x011CCC: MoveJump(obj); return;
    case 0x011CE6: MoveFall(obj); return;
    case 0x011D8C: MoveShootAir(obj); return;
    case 0x011D14: MoveAir(obj); return;
    case 0x01164A: MoveNone3(obj); return;
    case 0x01202A: MoveGravity(obj); return;
    case 0x011E88: MoveGravity2(obj); return;
    case 0x011E92: MoveSlide(obj); return;
    case 0x011EC6: MoveHang(obj); return;
    case 0x011F2C: MoveShootHang(obj); return;
    case 0x012026: MoveNone4(obj); return;
    case 0x012028: MoveNone5(obj); return;
    case 0x012034: MoveThrow(obj); return;
    case 0x01211E: MoveNone6(obj); return;
    case 0x012120: MoveGravity3(obj); return;
    case 0x011B30: MoveShootWalk(obj); return;
    case 0x011CBE: MoveHurtAir(obj); return;
    case 0x011644: MoveStill(obj); return;
    case 0x011CBA: MoveAir2(obj); return;
    case 0x011A10: MoveTurn(obj); return;
    case 0x011A3C: MoveDash(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $0115CC */
void PlayerPushed(uint16_t obj)
{
    wr32(obj + O_X, rd32(obj + O_X) + rd32(obj + P_PUSHED));
    wr32(obj + O_Y, rd32(obj + O_Y) + rd32(obj + P_PUSHED + 4));
    wr32(obj + P_PUSHED, 0);
    wr32(obj + P_PUSHED + 4, 0);
}

/* $0115E6, $0119A6, $01164A, $012026, $012028, $01211E: nothing */
void MoveNone(uint16_t obj) { (void)obj; }
void MoveNone2(uint16_t obj) { (void)obj; }
void MoveNone3(uint16_t obj) { (void)obj; }
void MoveNone4(uint16_t obj) { (void)obj; }
void MoveNone5(uint16_t obj) { (void)obj; }
void MoveNone6(uint16_t obj) { (void)obj; }

/* $0115E8. Standing: the slope's slide, its dust, the animation. */
void MoveStand(uint16_t obj)
{
    PlayerSlopeSpeed(obj);
    PlayerSlopeDust(obj);
    PlayerStandAnim(obj);
}

/* $011616. Down a slope (the floor's kind: 0.5 a frame), none under water; y follows. */
void PlayerSlopeSpeed(uint16_t obj)
{
    wr32(obj + P_VX, rom32(0x011630 + (uint32_t)(int16_t)rd16(obj + P_FLOOR_KIND)));
    if (rd16(obj + P_WATER) & 0x8000) wr32(obj + P_VX, 0);
    PlayerSlopeVy(obj);
}

/* $011644 */
void MoveStill(uint16_t obj)
{
    PlayerSlopeSpeed(obj);
    PlayerSlopeDust(obj);
}

/* $01164C. Shooting, standing: the aim by the floor (flat, either slope). */
void MoveShootStand(uint16_t obj)
{
    PlayerSlopeSpeed(obj);
    PlayerSlopeDust(obj);
    uint32_t at = 0x011660 + (uint32_t)((int16_t)rd16(obj + P_FLOOR_KIND) >> 1);
    uint32_t fn = at + (uint32_t)(int16_t)rom16(at);
    switch (fn) {
    case 0x01166A: AimFlat(obj); return;
    case 0x011764: AimSlopeUp(obj); return;
    case 0x011864: AimSlopeDown(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* the aim's eighth of the turn, times 2 (the nearest) */
static uint16_t octant(uint16_t obj) { return (uint16_t)(((rd8(obj + P_AIM_DIR) + 0x10) & 0xE0) >> 4); }

/* the muzzle's place from the character's tables: y by the direction, x by it and the facing */
static void gun_from(uint16_t obj, uint32_t ys, uint32_t xs, uint16_t i, int by_facing)
{
    uint16_t c = rd16(obj + P_CHARACTER);
    wr16(obj + P_GUN_Y, rom16(rom32(ys + (uint32_t)(int16_t)c) + (uint32_t)(int16_t)i));
    if (by_facing) i = (uint16_t)(i + ((rd8(obj + P_SPRITE) & 8) << 1));
    wr16(obj + P_GUN_X, rom16(rom32(xs + (uint32_t)(int16_t)c) + (uint32_t)(int16_t)i));
}

/* the animation for the aim: the first set or the second */
static void aim_anim(uint16_t obj, uint32_t first, uint32_t second, uint16_t i)
{
    wr16(obj + P_ANIM_SET, rom16((rd16(obj + P_ALT_ANIMS) ? second : first) + i));
}

/* $01166A. On the flat. */
void AimFlat(uint16_t obj)
{
    if (rd8(obj + P_SHOT_B) & 0x80) { wr16(obj + P_ANIM_SET, 0x74); return; }
    uint16_t i = octant(obj);
    AimFlatAnim(obj, i);
    if (bit8(obj + P_SHOT_B, 4)) { PlayerFreeAimGun(obj); return; }
    gun_from(obj, 0x0116FC, 0x0116F4, i, 1);
}

/* $0116BC */
void AimFlatAnim(uint16_t obj, uint16_t i) { aim_anim(obj, 0x0116D4, 0x0116E4, i); }

/* on a slope the aim also sets the facing ($0117E4 / $0118E4: bit 11 of the sprite's word) */
static void face_by_aim(uint16_t obj, uint32_t table, uint16_t i)
{
    wr16(obj + P_SPRITE, (uint16_t)((rd16(obj + P_SPRITE) & ~0x0800) | rom16(table + i)));
}

/* $011764. On the slope rising to the right. */
void AimSlopeUp(uint16_t obj)
{
    if (rd8(obj + P_SHOT_B) & 0x80) {
        wr16(obj + P_ANIM_SET, bit8(obj + P_SPRITE, 3) ? 0x78 : 0x79);
        return;
    }
    uint16_t i = octant(obj);
    face_by_aim(obj, 0x0117E4, i);
    AimSlopeUpAnim(obj, i);
    if (bit8(obj + P_SHOT_B, 4)) { PlayerFreeAimGun(obj); return; }
    gun_from(obj, 0x01181C, 0x011814, i, 0);
}

/* $0117CC */
void AimSlopeUpAnim(uint16_t obj, uint16_t i) { aim_anim(obj, 0x0117F4, 0x011804, i); }

/* $011864. On the slope falling to the right. */
void AimSlopeDown(uint16_t obj)
{
    if (rd8(obj + P_SHOT_B) & 0x80) {
        wr16(obj + P_ANIM_SET, bit8(obj + P_SPRITE, 3) ? 0x79 : 0x78);
        return;
    }
    uint16_t i = octant(obj);
    face_by_aim(obj, 0x0118E4, i);
    AimSlopeDownAnim(obj, i);
    if (bit8(obj + P_SHOT_B, 4)) { PlayerFreeAimGun(obj); return; }
    gun_from(obj, 0x01191C, 0x011914, i, 0);
}

/* $0118CC */
void AimSlopeDownAnim(uint16_t obj, uint16_t i) { aim_anim(obj, 0x0118F4, 0x011904, i); }

/* $011964 (the aims' common end): the free aim's muzzle, $60 along the direction, 8 above the
 * middle (below, upside down) */
void PlayerFreeAimGun(uint16_t obj)
{
    uint16_t i = (uint16_t)((rd8(obj + P_AIM_DIR) & 0xFF) << 1);
    uint16_t x = (uint16_t)(((int32_t)(int16_t)rom16(SINES + 0x80 + i) * 0x60) >> 16);
    wr16(obj + P_GUN_X, x);
    int16_t y = (int16_t)(((int32_t)(int16_t)rom16(SINES + i) * 0x60) >> 16);
    wr16(obj + P_GUN_Y, (uint16_t)(bit8(obj + P_SPRITE, 4) ? y + 8 : y - 8));
}

/* $0119A8. Shooting, crouched: straight ahead. */
void MoveShootCrouch(uint16_t obj)
{
    if (rd8(obj + P_SHOT_B) & 0x80) { wr16(obj + P_ANIM_SET, 0x75); return; }
    wr16(obj + P_ANIM_SET, 0x14);
    wr16(obj + P_GUN_X, bit8(obj + P_SPRITE, 3) ? 0xFFD6 : 0x2A);
    wr16(obj + P_GUN_Y, 7);
}

/* $011A10. Turning: the facing flips two frames from the end. */
void MoveTurn(uint16_t obj)
{
    int right = bit8(obj + P_TURNED, 3);
    if ((int16_t)rd16(obj + P_TIMER) > 2) right = !right;
    if (right) clr8(obj + P_SPRITE, 3);
    else set8(obj + P_SPRITE, 3);
}

/* $011A3C. The dash: after its time, friction and dust; y follows the slope. */
void MoveDash(uint16_t obj)
{
    if (rd16(obj + P_TIMER) & 0x8000) {
        PlayerFriction(obj);
        PlayerSlideDust(obj);
    }
    PlayerSlopeVy(obj);
}

/* $011A50. x's speed toward 0 by 1/8. */
void PlayerFriction(uint16_t obj)
{
    int32_t v = (int32_t)rd32(obj + P_VX);
    if (!v) return;
    if (v < 0) { v += 0x2000; if (v >= 0) v = 0; }
    else { v -= 0x2000; if (v < 0) v = 0; }
    wr32(obj + P_VX, (uint32_t)v);
}

/* $011A7A. Walking: the animation, y following the slope. */
void MoveWalk(uint16_t obj)
{
    PlayerWalkAnim(obj);
    PlayerSlopeVy(obj);
}

/* $011A7E. y's speed from x's on the floor's slope (its attribute's low bits). */
void PlayerSlopeVy(uint16_t obj)
{
    uint32_t at = 0x011A90 + (uint32_t)((rd16(obj + P_FLOOR) & 7) << 1);
    uint32_t fn = at + (uint32_t)(int16_t)rom16(at);
    switch (fn) {
    case 0x011AA0: SlopeVyFlat(obj); return;
    case 0x011AA6: SlopeVyRising(obj); return;
    case 0x011AB2: SlopeVyRisingHalf(obj); return;
    case 0x011AC0: SlopeVyFalling(obj); return;
    case 0x011AC8: SlopeVyFallingHalf(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $011AA0, $011AA6, $011AB2, $011AC0, $011AC8: none, -x, -x / 2, x, x / 2 */
void SlopeVyFlat(uint16_t obj) { wr32(obj + P_VY, 0); }
void SlopeVyRising(uint16_t obj) { wr32(obj + P_VY, 0u - rd32(obj + P_VX)); }
void SlopeVyRisingHalf(uint16_t obj) { wr32(obj + P_VY, (uint32_t)((int32_t)(0u - rd32(obj + P_VX)) >> 1)); }
void SlopeVyFalling(uint16_t obj) { wr32(obj + P_VY, rd32(obj + P_VX)); }
void SlopeVyFallingHalf(uint16_t obj) { wr32(obj + P_VY, (uint32_t)((int32_t)rd32(obj + P_VX) >> 1)); }

/* $011B30. Shooting, walking. */
void MoveShootWalk(uint16_t obj)
{
    AimWalk(obj);
    PlayerWalkSpeed(obj);
    PlayerSlopeVy(obj);
}

/* $011B3C. The aim walking, by the floor, the facing, the water. */
void AimWalk(uint16_t obj)
{
    uint16_t i = (uint16_t)((rd16(obj + P_FLOOR_KIND) << 1) + ((rd8(obj + P_SPRITE) >> 1) & 4));
    if (rd8(obj + P_WATER) & 0x80) i = (uint16_t)(i + 0x20);
    uint32_t at = 0x011B64 + (uint32_t)((int16_t)i >> 1);
    uint32_t fn = at + (uint32_t)(int16_t)rom16(at);
    switch (fn) {
    case 0x011B84: AimNone(obj); return;
    case 0x011B86: AimWalkFlat(obj); return;
    case 0x011764: AimSlopeUp(obj); return;
    case 0x011864: AimSlopeDown(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $011B84 */
void AimNone(uint16_t obj) { (void)obj; }

/* $011B86 */
void AimWalkFlat(uint16_t obj)
{
    if (rd8(obj + P_SHOT_B) & 0x80) { wr16(obj + P_ANIM_SET, 7); return; }
    uint16_t i = octant(obj);
    AimWalkAnim(obj, i);
    AimWalkLegs(obj);
    if (bit8(obj + P_SHOT_B, 4)) { PlayerFreeAimGun(obj); return; }
    gun_from(obj, 0x011C1C, 0x011C14, i, 1);
}

/* $011BDC */
void AimWalkAnim(uint16_t obj, uint16_t i) { aim_anim(obj, 0x011BF4, 0x011C04, i); }

/* $011C84. Between the two walking-and-shooting animations ($8F, $90) the step goes on. */
void AimWalkLegs(uint16_t obj)
{
    uint16_t a = rd16(obj + P_ANIM_SET), now = rd16(obj + P_ANIM);
    if (a == 0x8F && now == 0x90) wr16(obj + P_ANIM, 0x8F);
    else if (a == 0x90 && now == 0x8F) wr16(obj + P_ANIM, 0x90);
}

/* $011CBA */
void MoveAir2(uint16_t obj) { MoveAir(obj); }

/* $011CBE. Hurt in the air: steering and gravity. */
void MoveHurtAir(uint16_t obj)
{
    PlayerSteer(obj);
    add_vy(obj, 0x5000);
}

/* $011CCC. Jumping: steering, gravity, the jump's animation. */
void MoveJump(uint16_t obj)
{
    MoveAir(obj);
    PlayerJumpAnim(obj);
}

/* $011CD0 */
void PlayerJumpAnim(uint16_t obj)
{
    wr16(obj + P_ANIM_SET, rom16(0x011CDE + (uint32_t)((int16_t)rd16(obj + P_JUMP) >> 1)));
}

/* $011CE6. Falling: the jump's animation changes once at its end. */
void MoveFall(uint16_t obj)
{
    MoveAir(obj);
    if (!(rd8(obj + 0x0D) & 0x80) || !rd8(obj + P_ANIM_END)) return;
    wr8(obj + P_ANIM_END, 0);
    wr16(obj + P_ANIM_SET, rom16(0x011D0C + (uint32_t)((int16_t)rd16(obj + P_JUMP) >> 1)));
}

/* $011D14. Steering, then gravity. */
void MoveAir(uint16_t obj)
{
    PlayerSteer(obj);
    PlayerGravity(obj);
}

/* $011D18. Gravity (5/16), less with Up, more with Down. */
void PlayerGravity(uint16_t obj)
{
    add_vy(obj, 0x5000);
    if (bit8(obj + P_HELD, 0)) add_vy(obj, 0xFFFFF000u);
    else if (bit8(obj + P_HELD, 1)) add_vy(obj, 0x1000);
}

/* $011D4A. The side held speeds x up by 1/8 to 1.75. */
void PlayerSteer(uint16_t obj)
{
    uint8_t held = rd8(obj + P_HELD);
    int32_t vx = (int32_t)rd32(obj + P_VX);
    if (held & 4) {
        if (vx >= -0x1C000) wr32(obj + P_VX, (uint32_t)(vx - 0x2000));
    } else if (held & 8) {
        if (vx <= 0x1C000) wr32(obj + P_VX, (uint32_t)(vx + 0x2000));
    }
}

/* $011D8C. Shooting in the air: gravity (no steering), the aim. */
void MoveShootAir(uint16_t obj)
{
    PlayerGravity(obj);
    if (rd8(obj + P_SHOT_B) & 0x80) { wr16(obj + P_ANIM_SET, 0x77); return; }
    uint16_t i = octant(obj);
    AimAirAnim(obj, i);
    if (bit8(obj + P_SHOT_B, 4)) { PlayerFreeAimGun(obj); return; }
    gun_from(obj, 0x011E18, 0x011E20, i, 1);
}

/* $011DE0 */
void AimAirAnim(uint16_t obj, uint16_t i) { aim_anim(obj, 0x011DF8, 0x011E08, i); }

/* $011E88, $01202A, $012120: gravity alone */
void MoveGravity2(uint16_t obj) { add_vy(obj, 0x5000); }
void MoveGravity(uint16_t obj) { add_vy(obj, 0x5000); }
void MoveGravity3(uint16_t obj) { add_vy(obj, 0x5000); }

/* $011E92. Sliding: friction, the slope, dust. */
void MoveSlide(uint16_t obj)
{
    PlayerSlideFriction(obj);
    PlayerSlopeVy(obj);
    PlayerSlideDust(obj);
}

/* $011EA0. x's speed toward 0 by 1/8 (by its integer part's sign). */
void PlayerSlideFriction(uint16_t obj)
{
    int32_t v = (int32_t)rd32(obj + P_VX);
    if (!(rd16(obj + P_VX) & 0x8000)) {
        v -= 0x2000;
        if (v < 0) v = 0;
    } else {
        v += 0x2000;
        if (v >= 0) v = 0;
    }
    wr32(obj + P_VX, (uint32_t)v);
}

/* $011EC6. Hanging: moving along it, 1 a frame. */
void MoveHang(uint16_t obj)
{
    HangAnim(obj);
    HangSpeed(obj);
    PlayerSlopeVy(obj);
}

/* $011ED2. Starting or stopping along the floor hung from. */
void HangAnim(uint16_t obj)
{
    int side = (rd8(obj + P_HELD) & 0xC) != 0, moving = rd32(obj + P_VX) != 0;
    if (side && !moving) wr16(obj + P_ANIM_SET, 0x17);
    else if (!side && moving) wr16(obj + P_ANIM_SET, 0x16);
}

/* $011EFE */
void HangSpeed(uint16_t obj)
{
    wr32(obj + P_VX, 0);
    uint8_t held = rd8(obj + P_HELD);
    if (held & 4) wr32(obj + P_VX, 0xFFFF0000u);
    else if (held & 8) wr32(obj + P_VX, 0x10000);
}

/* $011F2C. Shooting, hanging. */
void MoveShootHang(uint16_t obj)
{
    if (rd8(obj + P_SHOT_B) & 0x80) { wr16(obj + P_ANIM_SET, 0x76); return; }
    uint16_t i = octant(obj);
    AimHangAnim(obj, i);
    if (bit8(obj + P_SHOT_B, 4)) { PlayerFreeAimGun(obj); return; }
    gun_from(obj, 0x011FBE, 0x011FB6, i, 1);
}

/* $011F7E */
void AimHangAnim(uint16_t obj, uint16_t i) { aim_anim(obj, 0x011F96, 0x011FA6, i); }

/* $012034. Throwing: what it holds kept in the hands (still) until the throw's frame, then sent
 * the attack's way. */
void MoveThrow(uint16_t obj)
{
    uint16_t held = rd16(obj + P_THROWN);
    wr16(obj + P_TIMER, (uint16_t)(rd16(obj + P_TIMER) - 1));
    uint16_t t = rd16(obj + P_TIMER);
    if (t & 0x8000) return;
    if (t) {
        PlayerPlaceHeld(obj, held);
        wr32(held + P_VX, 0);
        wr32(held + P_VY, 0);
        set8(held + P_STATUS, 1);
        return;
    }
    set8(held + P_STATUS, 4);
    PlayerPlaceHeld(obj, held);
    uint16_t i = (uint16_t)((rd8(obj + P_ATTACK_DIR) >> 3) & 0x1C);
    wr32(held + P_VX, rom32(0x01207A + i));
    wr32(held + P_VY, rom32(0x01209A + i));
}

/* $0120BA. Where what it holds is through the throw. */
void PlayerPlaceHeld(uint16_t obj, uint16_t held)
{
    wr16(held + O_X, rd16(obj + O_X));
    wr16(held + O_Y, rd16(obj + O_Y));
    int16_t t = (int16_t)rd16(obj + P_TIMER);
    int left = bit8(obj + P_SPRITE, 3);
    if (!t) {
        wr16(held + O_Y, (uint16_t)(rd16(held + O_Y) - 8));
    } else if (t <= 7) {
        wr16(held + O_Y, (uint16_t)(rd16(held + O_Y) - 6));
        wr16(held + O_X, (uint16_t)(rd16(held + O_X) + (left ? 0x10 : -0x10)));
    } else {
        wr16(held + O_Y, (uint16_t)(rd16(held + O_Y) + 8));
        wr16(held + O_X, (uint16_t)(rd16(held + O_X) + (left ? 0x1C : -0x1C)));
    }
}

/* ---- dust ---- */

/* $0189C6. A free slot for the player's effects (eight, its own); whether there is one (Z). */
int PlayerFindEffect(uint16_t obj, uint16_t *slot)
{
    uint16_t at = obj == v_player1 ? 0xBFC0 : 0xC380, end = obj == v_player1 ? 0xC2C0 : 0xC680;
    for (; at < end; at = (uint16_t)(at + 0x60)) {
        if (!rd16(at)) { *slot = at; return 1; }
    }
    *slot = at;
    return 0;
}

/* $0129EC. Dust (type $140) at the player's feet, drifting up. */
void PlayerDust(uint16_t obj, uint16_t e)
{
    wr16(e + O_TYPE, 0x140);
    wr16(e + O_STATE, 0);
    wr16(e + O_X, rd16(obj + O_X));
    wr16(e + O_Y, rd16(obj + O_Y));
    wr32(e + P_VX, 0);
    wr32(e + P_VY, 0xFFFF8000u);
    wr16(e + O_FLAGS, 0x8F00);
    wr8(e + 0x20, rd8(obj + 0x20));
    wr16(e + P_SPRITE, rd16(obj + P_SPRITE));
    wr8(e + 0x21, 0);
}

/* $012970. Sliding's dust, every 4 frames (not under water). */
void PlayerSlideDust(uint16_t obj)
{
    uint16_t e;
    if ((rd8(obj + P_WATER) & 0x80) || (rd16(v_gameFrames) & 3) || !PlayerFindEffect(obj, &e)) return;
    PlayerDust(obj, e);
    wr16(e + O_Y, (uint16_t)(rd16(e + O_Y) + 0x10));
}

/* $01299A. On a slope, its dust every 8 frames, behind. */
void PlayerSlopeDust(uint16_t obj)
{
    int16_t kind = (int16_t)rd16(obj + P_FLOOR_KIND);
    uint16_t e;
    if (kind < 8 || kind > 0xC || (rd8(obj + P_WATER) & 0x80) || (rd16(v_gameFrames) & 7)
        || !PlayerFindEffect(obj, &e)) return;
    PlayerDust(obj, e);
    wr16(e + O_Y, (uint16_t)(rd16(e + O_Y) + 0x10));
    wr16(e + O_X, (uint16_t)(rd16(e + O_X) + (kind == 8 ? -6 : 6)));
}

/* $012A24. Under water, on a floor, every 4 frames a bubble (type $13C) rising at random. */
void PlayerSplash(uint16_t obj)
{
    uint16_t e;
    if (!(rd8(obj + P_WATER) & 0x80) || !rd16(obj + P_FLOOR) || (rd16(v_gameFrames) & 3)
        || !PlayerFindEffect(obj, &e)) return;
    wr16(e + O_TYPE, 0x13C);
    wr16(e + O_STATE, 0);
    wr16(e + O_X, rd16(obj + O_X));
    wr16(e + O_Y, rd16(obj + O_Y));
    wr32(e + P_VX, 0);
    wr16(e + O_FLAGS, 0x8F00);
    wr8(e + 0x20, 0xB0);
    wr8(e + 0x21, 0);
    wr32(e + P_VY, (((uint32_t)Random() & 0xFFFF) << 2) - 0x20000);
    wr16(e + O_Y, (uint16_t)(rd16(e + O_Y) + (int8_t)rd8(obj + 0x29)));
}

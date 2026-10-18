/* The players' shots ($18C58-$1A21B): the effects (types from $13C) each weapon fires into its
 * player's eight effect slots (weapons.c), and the spark a shot turns into when it hits.
 *
 * A shot's fields: +6 its angle (the high byte a 256th of a turn, 0 right, $40 down), +$18 / +$1C
 * its speeds, +$26 its power (by the difficulty), +$28 / +$2C its boxes, +$48 its distance from
 * the hand (the beams, the flames, the ring), +$4A which in a row it is (0 the first), +$4C its
 * player, +$4E its target (the chasers) or its row's first (the ring), +$50 its place in the
 * player's trail (+$100: eight bytes a point), +$52 the ring's turn. A player's +$96 bit 5: a beam
 * held; +$9A its aim; +$9E / +$A0 its gun from it; +$A2 its target; +$A4 its trail's next point. */
#include "game.h"

#define S_ANGLE    0x06
#define S_VX       0x18
#define S_VY       0x1C
#define S_POWER    0x26
#define S_DIST     0x48
#define S_ROW      0x4A
#define S_OWNER    0x4C
#define S_TARGET   0x4E
#define S_TRAIL    0x50
#define S_TURN     0x52
#define P_SHOT_B   0x96
#define P_AIM_DIR  0x9A
#define P_GUN_X    0x9E
#define P_GUN_Y    0xA0
#define P_TARGET   0xA2
#define P_TRAIL    0xA4
#define P_TRAIL_POINTS 0x100
#define COSINES    0x0297FCu     /* the sine table a quarter turn on */

/* loc_018CE2: to be removed */
static void gone(uint16_t obj)
{
    wr16(obj + O_FLAGS, (uint16_t)(rd16(obj + O_FLAGS) | 0x1000));
}

/* loc_01A0D8: the spark (type $18C) */
static void spark(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x18C);
    wr16(obj + O_STATE, 0);
}

static int out_of_view(uint16_t x, uint16_t y)
{
    return (uint16_t)(x - 0x80) > 0x140 || (uint16_t)(y - 0x80) > 0xE0;
}

/* out of view: removed; hit (+$22 bit 6) or on a marked map cell: the spark */
static void fly(uint16_t obj)
{
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    if (out_of_view(x, y)) {
        gone(obj);
        return;
    }
    if (bit8(obj + 0x22, 6) || MapMarked(x, y)) spark(obj);
}

/* out of view: removed */
static void fly_on(uint16_t obj)
{
    if (out_of_view(rd16(obj + O_X), rd16(obj + O_Y))) gone(obj);
}

static int beam_held(uint16_t player)
{
    return bit8(player + P_SHOT_B, 5);
}

/* the player's trail point `at` (a byte offset) */
static uint16_t trail(uint16_t player, uint16_t at)
{
    return (uint16_t)(player + P_TRAIL_POINTS + at);
}

/* ---- force ---- */

/* $018C58. Type $40: force. */
void ForceShot(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr8(obj + 9, 0);
        wr16(obj + 0x0C, 0);
        wr16(obj + O_FLAGS, 0x8C40);
        ForcePower(obj);
        wr32(obj + 0x2C, 0xFC04FC04);
        uint16_t p = EffectOwner(obj);
        wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
        EffectAtHand(obj, p);
        SpeedFromAngle(obj, 6);
        wr16(obj + O_STATE, 4);
    }
    EffectFlicker(obj);
    SelectAnimation(obj, 0x018CEA);
    EffectPriority(obj);
    fly(obj);
}

/* $018D1E. Type $154: force with force, spread at random by up to 8 either way. */
void ForceForceShot(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0x8C40);
        wr32(obj + 8, 0x0500F8F8);
        wr16(obj + 0x0E, 0x546);
        ForcePower(obj);
        wr32(obj + 0x2C, 0xF808F808);
        uint16_t p = EffectOwner(obj);
        wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
        EffectAtHand(obj, p);
        uint16_t r = (uint16_t)Random();
        wr16(obj + S_ANGLE, (uint16_t)(rd16(obj + S_ANGLE) + (r & 0xF00) - 0x800));
        SpeedFromAngle(obj, 6);
        wr16(obj + O_STATE, 4);
    }
    EffectFlicker(obj);
    EffectPriority(obj);
    fly(obj);
}

/* ---- chaser ---- */

/* $018DB4. Type $148: chaser, turning toward its target by up to $400 a frame while the target
 * lives and is not dead ahead. */
void ChaserShot(uint16_t obj)
{
    int16_t st = (int16_t)(rd16(obj + O_STATE) - 4);
    if (st < 0) {
        wr16(obj + O_FLAGS, 0x8C40);
        ForcePower(obj);
        wr32(obj + 0x2C, 0xFC04FC04);
        uint16_t p = EffectOwner(obj);
        wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
        EffectAtHand(obj, p);
        SpeedFromAngle(obj, 4);
        ChaserSprite(obj);
        p = NextTarget(obj);
        wr16(obj + S_TARGET, rd16(p + P_TARGET));
        wr16(obj + O_STATE, 4);
        st = 0;
    }
    if (st == 0) {
        EffectFlicker(obj);
        if (bit8(rd16(obj + S_TARGET) + 0x21, 5) && TurnToTarget(obj, 0x400)) {
            SpeedFromAngle(obj, 4);
            ChaserSprite(obj);
            EffectPriority(obj);
            fly(obj);
            return;
        }
        wr16(obj + O_STATE, 8);
    }
    EffectFlicker(obj);
    EffectPriority(obj);
    fly(obj);
}

/* $018EAC. Type $14C: chaser with chaser, turning by up to $1000, a new target when its own goes. */
void ChaserChaserShot(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0x8C40);
        ChaserChaserPower(obj);
        wr32(obj + 0x2C, 0xFC04FC04);
        wr32(obj + 8, 0x0500F8F8);
        wr16(obj + 0x0E, 0x54A);
        uint16_t p = EffectOwner(obj);
        wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
        EffectAtHand(obj, p);
        p = NextTarget(obj);
        wr16(obj + S_TARGET, rd16(p + P_TARGET));
        wr16(obj + O_STATE, 4);
    }
    EffectFlicker(obj);
    EffectPriority(obj);
    if (!bit8(rd16(obj + S_TARGET) + 0x21, 5)) {
        uint16_t p = NextTarget(obj);
        wr16(obj + S_TARGET, rd16(p + P_TARGET));
    }
    TurnToTarget(obj, 0x1000);
    SpeedFromAngle(obj, 5);
    fly(obj);
}

/* $018F5A. Type $150: force with chaser, turning toward the target for its first 4 frames. */
void ForceChaserShot(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0x8C40);
        ForceChaserPower(obj);
        wr32(obj + 0x2C, 0xF808F808);
        wr32(obj + 8, 0x0500F8F8);
        wr16(obj + 0x0E, 0x546);
        uint16_t p = EffectOwner(obj);
        wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
        EffectAtHand(obj, p);
        wr16(obj + S_DIST, 4);
        wr16(obj + S_TARGET, rd16(rd16(obj + S_OWNER) + P_TARGET));
        if (!bit8(rd16(obj + S_TARGET) + 0x21, 5)) {
            SpeedFromAngle(obj, 6);
            wr16(obj + S_DIST, 0);
        }
        wr16(obj + O_STATE, 4);
    }
    EffectFlicker(obj);
    EffectPriority(obj);
    if (rd16(obj + S_DIST)) {
        wr16(obj + S_DIST, (uint16_t)(rd16(obj + S_DIST) - 1));
        if (!bit8(rd16(obj + S_TARGET) + 0x21, 5)) {
            wr16(obj + S_DIST, 0);
        } else {
            TurnToTarget(obj, 0x1000);
            SpeedFromAngle(obj, 6);
        }
    }
    fly(obj);
}

/* $019030. The chaser's sprite by its angle (16 ways). */
void ChaserSprite(uint16_t obj)
{
    uint16_t i = (uint16_t)((rd8(obj + S_ANGLE) & 0xF0) >> 2);
    i = (uint16_t)(i + (i >> 1));
    wr32(obj + 8, rom32(0x01904E + i));
    wr16(obj + 0x0E, rom16(0x01904E + i + 4));
}

/* $0190AE. The player's next target: the first of the targets past its own (by address), else
 * the first; none: the player itself. */
uint16_t NextTarget(uint16_t obj)
{
    uint16_t p = rd16(obj + S_OWNER), n = rd16(v_targetCount), t;
    if (!n) {
        wr16(p + P_TARGET, p);
        return p;
    }
    t = rd16(v_targets);
    if (n != 1) {
        uint16_t cur = rd16(p + P_TARGET), a = v_targets;
        for (uint16_t k = 0; k < n; k++, a = (uint16_t)(a + 2)) {
            if (rd16(a) > cur) {
                t = rd16(a);
                break;
            }
        }
    }
    wr16(p + P_TARGET, t);
    return p;
}

/* $0190E4. The angle turned toward the target by up to step; the turn that was wanted (0 when
 * dead ahead). */
uint16_t TurnToTarget(uint16_t obj, uint16_t step)
{
    uint16_t t = rd16(obj + S_TARGET);
    int16_t dx = (int16_t)(rd16(t + O_X) - rd16(obj + O_X));
    uint32_t dy = (uint16_t)(rd16(t + O_Y) - rd16(obj + O_Y));
    uint16_t d = (uint16_t)(Atan2(dx, &dy) - rd16(obj + S_ANGLE));
    if (!d) return 0;
    uint16_t turn = d;
    if (d & 0x8000) {
        step = (uint16_t)-step;
        if (d < step) turn = step;
    } else if (d >= step) {
        turn = step;
    }
    wr16(obj + S_ANGLE, (uint16_t)(rd16(obj + S_ANGLE) + turn));
    return d;
}

/* ---- lightning ---- */

/* the lightning's beam: while held, along the aim from the gun, $40 further each frame; let go,
 * it flies on */
static void lightning(uint16_t obj)
{
    ShotHitMark(obj);
    int16_t st = (int16_t)(rd16(obj + O_STATE) - 4);
    if (st < 0) {
        LightningPower(obj);
        wr32(obj + 0x2C, 0xFC04FC04);
        EffectOwner(obj);
        wr16(obj + O_FLAGS, 0x8240);
        wr16(obj + O_STATE, 4);
        st = 0;
    }
    if (st == 0) {
        EffectFlicker(obj);
        uint16_t p = rd16(obj + S_OWNER);
        if (beam_held(p)) {
            wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
            BeamSprite(obj);
            EffectPriority(obj);
            ShotAtDistance(obj, p, rd16(obj + S_DIST));
            wr16(obj + S_DIST, (uint16_t)(rd16(obj + S_DIST) + 0x40));
            return;
        }
        wr16(obj + O_FLAGS, 0x8C40);
        SpeedFromAngle(obj, 6);
        wr16(obj + O_STATE, 8);
    }
    EffectFlicker(obj);
    EffectPriority(obj);
    fly_on(obj);
}

/* $01916C. Type $158: lightning. */
void LightningShot(uint16_t obj)
{
    lightning(obj);
}

/* $019206. Type $188: lightning with chaser: the row's first steers round its target a quarter
 * turn at a time, leaving its points in the player's trail; the others follow the trail. */
void LightningChaserShot(uint16_t obj)
{
    ShotHitMark(obj);
    int16_t st = (int16_t)(rd16(obj + O_STATE) - 4);
    if (st < 0) {
        LightningChaserPower(obj);
        wr32(obj + 0x2C, 0xFC04FC04);
        wr16(obj + S_TRAIL, 0);
        LightningChaserInit(obj);
        wr16(obj + O_STATE, 4);
        st = 0;
    }
    if (st == 0) {
        EffectFlicker(obj);
        if (beam_held(rd16(obj + S_OWNER))) {
            LightningChaserSteer(obj);
            BeamSprite(obj);
            EffectPriority(obj);
            wr16(obj + S_TRAIL, (uint16_t)(rd16(obj + S_TRAIL) + 8));
            return;
        }
        wr16(obj + O_FLAGS, 0x8C40);
        SpeedFromAngle(obj, 6);
        wr16(obj + O_STATE, 8);
    }
    EffectFlicker(obj);
    EffectPriority(obj);
    fly_on(obj);
}

/* $019292. The first shown at the gun along the aim; the others hidden. */
void LightningChaserInit(uint16_t obj)
{
    uint16_t p = EffectOwner(obj);
    if (rd16(obj + S_ROW)) {
        wr16(obj + O_FLAGS, 0x8040);
        return;
    }
    wr16(obj + O_FLAGS, 0x8C40);
    wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
    EffectAtHand(obj, p);
}

/* $0192B6. The first: its target, its turn, its point in the trail, its speeds. The others: the
 * trail's point (angle, x, y). */
void LightningChaserSteer(uint16_t obj)
{
    if (rd16(obj + S_ROW)) {
        uint16_t a = trail(rd16(obj + S_OWNER), rd16(obj + S_TRAIL) & 0xF8);
        wr16(obj + S_ANGLE, rd16(a));
        wr16(obj + O_X, rd16((uint16_t)(a + 4)));
        wr16(obj + O_Y, rd16((uint16_t)(a + 6)));
        return;
    }
    LightningChaserTarget(obj);
    SteerAround(obj);
    uint16_t a = trail(rd16(obj + S_OWNER), rd16(obj + S_TRAIL) & 0xF8);
    wr16(a, rd16(obj + S_ANGLE));
    wr16((uint16_t)(a + 4), rd16(obj + O_X));
    wr16((uint16_t)(a + 6), rd16(obj + O_Y));
    SpeedFromAngle(obj, 6);
}

/* $019312. A new target when the player's is gone; it is the shot's. */
void LightningChaserTarget(uint16_t obj)
{
    uint16_t p = rd16(obj + S_OWNER);
    if (!bit8(rd16(p + P_TARGET) + 0x21, 5)) NextTarget(obj);
    wr16(obj + S_TARGET, rd16(p + P_TARGET));
}

/* $019330. A quarter turn toward the target once past it, by the eighth of a turn it heads in. */
void SteerAround(uint16_t obj)
{
    uint16_t t = rd16(obj + S_TARGET);
    switch (rom32(0x019344 + (uint32_t)((rd8(obj + S_ANGLE) & 0xE0) >> 3))) {
    case 0x019374: SteerOctant0(obj, t); break;
    case 0x0193DC: SteerOctant1(obj, t); break;
    case 0x0193A8: SteerOctant2(obj, t); break;
    case 0x01940E: SteerOctant3(obj, t); break;
    case 0x01938E: SteerOctant4(obj, t); break;
    case 0x019440: SteerOctant5(obj, t); break;
    case 0x0193C2: SteerOctant6(obj, t); break;
    case 0x019472: SteerOctant7(obj, t); break;
    }
}

/* loc_019364, loc_01936C */
static void turn_right(uint16_t obj) { wr16(obj + S_ANGLE, (uint16_t)(rd16(obj + S_ANGLE) + 0x4000)); }
static void turn_left(uint16_t obj) { wr16(obj + S_ANGLE, (uint16_t)(rd16(obj + S_ANGLE) - 0x4000)); }

static int16_t sx(uint16_t a) { return (int16_t)rd16(a + O_X); }
static int16_t sy(uint16_t a) { return (int16_t)rd16(a + O_Y); }

/* $019374. Heading right: past the target, down to it or up. */
void SteerOctant0(uint16_t obj, uint16_t target)
{
    if (sx(obj) < sx(target)) return;
    if (sy(obj) > sy(target)) turn_left(obj);
    else turn_right(obj);
}

/* $01938E. Heading left. */
void SteerOctant4(uint16_t obj, uint16_t target)
{
    if (sx(obj) > sx(target)) return;
    if (sy(obj) > sy(target)) turn_right(obj);
    else turn_left(obj);
}

/* $0193A8. Heading down. */
void SteerOctant2(uint16_t obj, uint16_t target)
{
    if (sy(obj) < sy(target)) return;
    if (sx(obj) > sx(target)) turn_right(obj);
    else turn_left(obj);
}

/* $0193C2. Heading up. */
void SteerOctant6(uint16_t obj, uint16_t target)
{
    if (sy(obj) > sy(target)) return;
    if (sx(obj) > sx(target)) turn_left(obj);
    else turn_right(obj);
}

static uint16_t absw(uint16_t v) { return (v & 0x8000) ? (uint16_t)-v : v; }

/* the diagonals: past the target when dx + dy (each toward the heading) is negative; then the
 * turn by which of |dx|, |dy| is the larger (left when |dy| is, if left_when_dy) */
static void steer_diagonal(uint16_t obj, uint16_t dx, uint16_t dy, int left_when_dy)
{
    if (!((uint16_t)(dx + dy) & 0x8000)) return;
    int dy_more = (int16_t)absw(dy) > (int16_t)absw(dx);
    if (dy_more == left_when_dy) turn_left(obj);
    else turn_right(obj);
}

/* $0193DC. Heading down and right. */
void SteerOctant1(uint16_t obj, uint16_t target)
{
    steer_diagonal(obj, (uint16_t)(sx(target) - sx(obj)), (uint16_t)(sy(target) - sy(obj)), 1);
}

/* $01940E. Heading down and left. */
void SteerOctant3(uint16_t obj, uint16_t target)
{
    steer_diagonal(obj, (uint16_t)(sx(obj) - sx(target)), (uint16_t)(sy(target) - sy(obj)), 0);
}

/* $019440. Heading up and left. */
void SteerOctant5(uint16_t obj, uint16_t target)
{
    steer_diagonal(obj, (uint16_t)(sx(obj) - sx(target)), (uint16_t)(sy(obj) - sy(target)), 1);
}

/* $019472. Heading up and right. */
void SteerOctant7(uint16_t obj, uint16_t target)
{
    steer_diagonal(obj, (uint16_t)(sx(target) - sx(obj)), (uint16_t)(sy(obj) - sy(target)), 0);
}

/* $0194A4. Type $15C: lightning with fire: along the aim from the gun, $20 further each frame,
 * while held, to $100. */
void LightningFireShot(uint16_t obj)
{
    ShotHitMark(obj);
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0x8E40);
        LightningFirePower(obj);
        wr32(obj + 0x2C, 0xF808F808);
        wr32(obj + 0x28, 0xF60AF60A);
        EffectOwner(obj);
        wr32(obj + S_VX, 0);
        wr32(obj + S_VY, 0);
        wr16(obj + O_STATE, 4);
    }
    EffectFlickerHard(obj);
    uint16_t p = rd16(obj + S_OWNER);
    if (!beam_held(p) || (int16_t)rd16(obj + S_DIST) >= 0x100) {
        gone(obj);
        return;
    }
    wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
    ShotAtDistance(obj, p, rd16(obj + S_DIST));
    BeamSprite(obj);
    EffectPriority(obj);
    wr16(obj + S_DIST, (uint16_t)(rd16(obj + S_DIST) + 0x20));
}

/* $019516. Type $164: lightning with lightning (as lightning). */
void LightningLightningShot(uint16_t obj)
{
    lightning(obj);
}

/* $0195B0. Type $160: force with lightning: straight, its sprite by its angle. */
void ForceLightningShot(uint16_t obj)
{
    ShotHitMark(obj);
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0x8C40);
        LightningPower(obj);
        wr32(obj + 0x2C, 0xFC04FC04);
        uint16_t p = EffectOwner(obj);
        wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
        EffectAtHand(obj, p);
        SpeedFromAngle(obj, 6);
        BeamSprite(obj);
        wr16(obj + O_STATE, 4);
    }
    EffectFlicker(obj);
    EffectPriority(obj);
    fly_on(obj);
}

/* $019616. The beam's sprite by its angle (32 ways). */
void BeamSprite(uint16_t obj)
{
    uint16_t i = (uint16_t)((rd8(obj + S_ANGLE) & 0xF8) >> 1);
    i = (uint16_t)(i + (i >> 1));
    wr32(obj + 8, rom32(0x019634 + i));
    wr16(obj + 0x0E, rom16(0x019634 + i + 4));
}

/* ---- fire ---- */

/* a flame on a marked map cell: its point's speed (x or y, by the cell) stopped */
static void flame_wall(uint16_t obj, uint16_t marked)
{
    uint16_t p = rd16(obj + S_OWNER);
    uint16_t a = trail(p, (uint16_t)((rd16(p + P_TRAIL) - rd16(obj + S_TRAIL)) & 0x78));
    if (marked & 1) wr16(a, 0);
    if (marked & 2) wr16((uint16_t)(a + 4), 0);
}

/* $0196F4. Type $178: fire: the first at the gun, the others along the trail, while held. */
void FlameShot(uint16_t obj)
{
    ShotHitMark(obj);
    if (!rd16(obj + O_STATE)) {
        EffectOwner(obj);
        wr16(obj + O_FLAGS, 0x8040);
        wr8(obj + 9, 0);
        wr16(obj + 0x0C, 0);
        FlamePower(obj);
        wr16(obj + O_STATE, 4);
    }
    EffectFlicker(obj);
    if (!beam_held(rd16(obj + S_OWNER))) {
        gone(obj);
        return;
    }
    FlameAnimate(obj);
    EffectPriority(obj);
    uint16_t x, y;
    FlamePosition(obj, &x, &y);
    uint16_t m = MapMarked(x, y);
    if (m) flame_wall(obj, m);
}

/* $019772. The first: at the gun, the trail's next point's x and y (times 16) from it. The
 * others: the point S_TRAIL before it. Its x, y. */
void FlamePosition(uint16_t obj, uint16_t *x, uint16_t *y)
{
    uint16_t p = rd16(obj + S_OWNER), at = rd16(p + P_TRAIL);
    if (!rd16(obj + S_ROW)) {
        uint16_t v = (uint16_t)(rd16(p + P_GUN_X) + rd16(p + O_X));
        *x = v;
        wr16(obj + O_X, v);
        wr16((uint16_t)(trail(p, at) + 2), (uint16_t)(v << 4));
        v = (uint16_t)(rd16(p + P_GUN_Y) + rd16(p + O_Y));
        *y = v;
        wr16(obj + O_Y, v);
        wr16((uint16_t)(trail(p, at) + 6), (uint16_t)(v << 4));
        return;
    }
    uint16_t a = trail(p, (uint16_t)((at - rd16(obj + S_TRAIL)) & 0x78));
    *x = (uint16_t)((int16_t)rd16((uint16_t)(a + 2)) >> 4);
    wr16(obj + O_X, *x);
    *y = (uint16_t)((int16_t)rd16((uint16_t)(a + 6)) >> 4);
    wr16(obj + O_Y, *y);
}

/* $0197CE. The animation by its place in the row. */
void FlameAnimate(uint16_t obj)
{
    SelectAnimation(obj, rom32(0x0197DE + (uint32_t)(int16_t)(rd16(obj + S_ROW) << 2)));
}

/* $019852. Type $17C: fire with fire: as fire while held; let go, each flies off at its point's
 * speed, growing. */
void FireFireShot(uint16_t obj)
{
    ShotHitMark(obj);
    int16_t st = (int16_t)(rd16(obj + O_STATE) - 4);
    if (st < 0) {
        EffectOwner(obj);
        wr16(obj + O_FLAGS, 0x8040);
        wr8(obj + 9, 0);
        wr16(obj + 0x0C, 0);
        FireFirePower(obj);
        wr16(obj + O_STATE, 4);
        st = 0;
    }
    if (st == 0) {
        EffectFlicker(obj);
        if (beam_held(rd16(obj + S_OWNER))) {
            FlameAnimate(obj);
            EffectPriority(obj);
            uint16_t x, y;
            FlamePosition(obj, &x, &y);
            uint16_t m = MapMarked(x, y);
            if (m) flame_wall(obj, m);
            return;
        }
        wr16(obj + O_FLAGS, 0x8C40);
        uint16_t p = rd16(obj + S_OWNER);
        uint16_t a = trail(p, (uint16_t)((rd16(p + P_TRAIL) - rd16(obj + S_TRAIL)) & 0x78));
        if (!(rd16(a) | rd16((uint16_t)(a + 4)))) {
            gone(obj);
            return;
        }
        wr32(obj + S_VX, (uint32_t)((int32_t)((uint32_t)rd16(a) << 16) >> 4));
        wr32(obj + S_VY, (uint32_t)((int32_t)((uint32_t)rd16((uint16_t)(a + 4)) << 16) >> 4));
        wr16(obj + O_STATE, 8);
    }
    EffectFlicker(obj);
    wr16(obj + S_ROW, (uint16_t)(rd16(obj + S_ROW) + 1));
    if (rd16(obj + S_ROW) >= 8) wr16(obj + S_ROW, 7);
    FireFirePower(obj);
    FlameAnimate(obj);
    EffectPriority(obj);
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    if (out_of_view(x, y) || bit8(obj + 0x22, 6) || MapMarked(x, y)) gone(obj);
}

/* $01997C. Type $180: force with fire: the row's first flies as a shot; when it goes (or hits,
 * or the beam is let go) the row bursts into a ring turning about where it was, growing to $100. */
void ForceFireShot(uint16_t obj)
{
    ShotHitMark(obj);
    int16_t st = (int16_t)(rd16(obj + O_STATE) - 4);
    if (st < 0) {
        EffectOwner(obj);
        wr16(obj + O_FLAGS, 0x40);
        if (rd16(obj + S_TARGET) == obj) {
            wr16(obj + O_FLAGS, 0x8C40);
            ForcePower(obj);
            wr32(obj + 0x2C, 0xF808F808);
            wr32(obj + 8, 0x0500F8F8);
            wr16(obj + 0x0E, 0x546);
            wr16(obj + S_ROW, 0);
            uint16_t p = rd16(obj + S_OWNER);
            wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
            EffectAtHand(obj, p);
            SpeedFromAngle(obj, 5);
        }
        wr16(obj + O_STATE, 4);
        st = 0;
    }
    if (st == 0) {
        uint16_t lead = rd16(obj + S_TARGET);
        if (lead == obj) {
            EffectFlicker(obj);
            EffectPriority(obj);
        }
        if (!rd16(lead) || out_of_view(rd16(lead + O_X), rd16(lead + O_Y))) {
            gone(obj);
            return;
        }
        if (rd16(lead + O_STATE) != 8 && !bit8(lead + 0x22, 6)
            && !MapMarked(rd16(lead + O_X), rd16(lead + O_Y)) && beam_held(rd16(obj + S_OWNER)))
            return;
        wr16(obj + O_FLAGS, 0x8040);
        wr8(obj + 9, 0);
        wr16(obj + S_ANGLE, rd16(obj + S_TURN));
        uint16_t q = (uint16_t)((rd8(obj + S_TURN) & 0xC0) >> 4);
        wr16(obj + 0x0C, (uint16_t)((q >> 1) + q));
        wr16(obj + O_X, rd16(lead + O_X));
        wr16(obj + S_VX, rd16(lead + O_X));
        wr16(obj + O_Y, rd16(lead + O_Y));
        wr16(obj + S_VY, rd16(lead + O_Y));
        wr16(obj + S_DIST, 0);
        wr16(obj + S_ROW, 0);
        wr16(obj + O_STATE, 8);
    }
    wr16(obj + S_DIST, (uint16_t)(rd16(obj + S_DIST) + 8));
    if ((int16_t)rd16(obj + S_DIST) >= 0x100) {
        gone(obj);
        return;
    }
    EffectFlicker(obj);
    RingStage(obj);
    ForceFirePower(obj);
    RingAnimate(obj);
    EffectPriority(obj);
    RingPosition(obj);
    wr16(obj + S_ANGLE, (uint16_t)(rd16(obj + S_ANGLE) + 0x1000));
}

/* $019ACA. Its size (0, 2, 4) by the ring's radius. */
void RingStage(uint16_t obj)
{
    wr16(obj + S_ROW, rom16(0x019ADC + (uint32_t)(((int16_t)rd16(obj + S_DIST) >> 4) & 0xE)));
}

/* $019AEC */
void RingAnimate(uint16_t obj)
{
    SelectAnimation(obj, rom32(0x019AFC + (uint32_t)(int16_t)(rd16(obj + S_ROW) << 1)));
}

/* $019B08. On the ring: the radius along its angle from the centre (+$18, +$1C). */
void RingPosition(uint16_t obj)
{
    int16_t r = (int16_t)rd16(obj + S_DIST);
    uint32_t i = (uint32_t)(rd8(obj + S_ANGLE) << 1);
    uint32_t c = (uint32_t)((int16_t)rom16(COSINES + i) * r);
    wr16(obj + O_X, (uint16_t)((c >> 16) + rd16(obj + S_VX)));
    uint32_t s = (uint32_t)((int16_t)rom16(COSINES - 0x80 + i) * r);
    wr16(obj + O_Y, (uint16_t)((s >> 16) + rd16(obj + S_VY)));
}

/* $019B3E. Type $184: chaser with fire: the row's first chases (or goes the way the pad is held)
 * through the walls' gaps, leaving its points in the trail for the others; let go, they fly on. */
void ChaserFireShot(uint16_t obj)
{
    ShotHitMark(obj);
    int16_t st = (int16_t)(rd16(obj + O_STATE) - 4);
    if (st < 0) {
        wr16(obj + O_FLAGS, 0x8040);
        wr8(obj + 9, 0);
        wr16(obj + 0x0C, 0);
        wr16(obj + S_DIST, 0);
        wr16(obj + S_TRAIL, 0);
        wr16(obj + 0x24, 0);
        ChaserFireInit(obj);
        ChaserFirePower(obj);
        wr16(obj + O_STATE, 4);
        st = 0;
    }
    if (st == 0) {
        EffectFlickerHard(obj);
        if (beam_held(rd16(obj + S_OWNER))) {
            ChaserFireAnimate(obj);
            EffectPriority(obj);
            ChaserFireMove(obj);
            ChaserFireTrail(obj);
            ChaserFireSteer(obj);
            wr16(obj + S_TRAIL, (uint16_t)(rd16(obj + S_TRAIL) + 8));
            return;
        }
        SpeedFromAngle(obj, 5);
        wr16(obj + O_STATE, 8);
    }
    EffectFlickerHard(obj);
    ChaserFireAnimate(obj);
    EffectPriority(obj);
    ChaserFireDrift(obj);
    ChaserFireTrail(obj);
    wr16(obj + S_TRAIL, (uint16_t)(rd16(obj + S_TRAIL) + 8));
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    if (!out_of_view(x, y) && !MapMarked(x, y)) return;
    if (!rd16(obj + S_ROW)) {
        uint16_t a = trail(rd16(obj + S_OWNER), rd16(obj + S_TRAIL) & 0xF8);
        wr32(a, 0);
        wr32((uint16_t)(a + 4), 0);
    }
    gone(obj);
}

/* $019C1C. The first at the gun along the aim, the player's next target its own. */
void ChaserFireInit(uint16_t obj)
{
    uint16_t p = EffectOwner(obj);
    if (rd16(obj + S_ROW)) return;
    wr16(obj + O_X, (uint16_t)(rd16(p + O_X) + rd16(p + P_GUN_X)));
    wr16(obj + 0x12, 0);
    wr16(obj + O_Y, (uint16_t)(rd16(p + O_Y) + rd16(p + P_GUN_Y)));
    wr16(obj + 0x16, 0);
    wr16(obj + S_ANGLE, rd16(p + P_AIM_DIR));
    SpeedFromAngle(obj, 5);
    p = NextTarget(obj);
    wr16(obj + S_TARGET, rd16(p + P_TARGET));
}

/* $019C60. The first: the way the pad is held (+$69's low bits), else toward the target. */
void ChaserFireSteer(uint16_t obj)
{
    if (rd16(obj + S_ROW)) return;
    uint16_t held = rd8(rd16(obj + S_OWNER) + 0x69) & 0xF;
    if (held) {
        wr16(obj + S_ANGLE, rom16(0x019CAE + (uint32_t)(held << 1)));
        SpeedFromAngle(obj, 5);
        return;
    }
    if (!bit8(rd16(obj + S_TARGET) + 0x21, 5)) {
        uint16_t p = NextTarget(obj);
        wr16(obj + S_TARGET, rd16(p + P_TARGET));
    }
    TurnToTarget(obj, 0x400);
    SpeedFromAngle(obj, 5);
}

/* $019CCE. The first moves, unless onto a marked map cell. */
void ChaserFireMove(uint16_t obj)
{
    if (rd16(obj + S_ROW)) return;
    uint32_t x = rd32(obj + O_X), y = rd32(obj + O_Y);
    wr32(obj + O_X, rd32(obj + O_X) + rd32(obj + S_VX));
    wr32(obj + O_Y, rd32(obj + O_Y) + rd32(obj + S_VY));
    if (!MapMarked(rd16(obj + O_X), rd16(obj + O_Y))) return;
    wr32(obj + O_X, x);
    wr32(obj + O_Y, y);
}

/* $019D0A. The first moves. */
void ChaserFireDrift(uint16_t obj)
{
    if (rd16(obj + S_ROW)) return;
    wr32(obj + O_X, rd32(obj + O_X) + rd32(obj + S_VX));
    wr32(obj + O_Y, rd32(obj + O_Y) + rd32(obj + S_VY));
}

/* $019D24. The first leaves its x, y in the trail; the others take theirs from it. */
void ChaserFireTrail(uint16_t obj)
{
    uint16_t a = trail(rd16(obj + S_OWNER), rd16(obj + S_TRAIL) & 0xF8);
    if (rd16(obj + S_ROW)) {
        wr32(obj + O_X, rd32(a));
        wr32(obj + O_Y, rd32((uint16_t)(a + 4)));
        return;
    }
    wr32(a, rd32(obj + O_X));
    wr32((uint16_t)(a + 4), rd32(obj + O_Y));
}

/* $019D58 */
void ChaserFireAnimate(uint16_t obj)
{
    SelectAnimation(obj, rom32(0x019D68 + (uint32_t)(int16_t)(rd16(obj + S_ROW) << 2)));
}

/* ---- the shared parts ---- */

/* $019DC0. dist along its angle from the player's gun. */
void ShotAtDistance(uint16_t obj, uint16_t player, uint16_t dist)
{
    uint32_t i = (uint32_t)(rd8(obj + S_ANGLE) << 1);
    uint32_t c = (uint32_t)((int16_t)rom16(COSINES + i) * (int16_t)dist);
    wr16(obj + O_X, (uint16_t)((c >> 16) + rd16(player + P_GUN_X) + rd16(player + O_X)));
    uint32_t s = (uint32_t)((int16_t)rom16(COSINES - 0x80 + i) * (int16_t)dist);
    wr16(obj + O_Y, (uint16_t)((s >> 16) + rd16(player + P_GUN_Y) + rd16(player + O_Y)));
}

/* $019E56. EffectFlicker with +$21 $C0. */
void EffectFlickerHard(uint16_t obj)
{
    int show = 1;
    if (rd8(v_players) == 3) {
        int odd = bit8(v_gameFrames + 1, 0);
        show = bit8(obj + 0x23, 7) ? odd : !odd;
    }
    if (show) {
        wr8(obj + 0x21, 0xC0);
        set8(obj + O_FLAGS, 7);
    } else {
        wr8(obj + 0x21, 0);
        clr8(obj + O_FLAGS, 7);
    }
}

/* the power by the difficulty */
static void power(uint16_t obj, uint32_t table)
{
    wr16(obj + S_POWER, rom16(table + (uint32_t)(int16_t)rd16(v_difficulty)));
}

/* $019E96: 6, 5, 5, 4 */
void ForcePower(uint16_t obj) { power(obj, 0x019EA2); }
/* $019EAA: 4, 4, 3, 2 */
void ChaserChaserPower(uint16_t obj) { power(obj, 0x019EB6); }
/* $019EBE: 4, 3, 2, 1 */
void ForceChaserPower(uint16_t obj) { power(obj, 0x019ECA); }
/* $019ED2: 8, 6, 5, 4 */
void LightningFirePower(uint16_t obj) { power(obj, 0x019EDE); }
/* $019EE6: 3, 3, 2, 1 */
void LightningChaserPower(uint16_t obj) { power(obj, 0x019EF2); }
/* $019EFA: 6, 5, 3, 3 */
void LightningPower(uint16_t obj) { power(obj, 0x019F06); }

/* the boxes by the place in the row, the power by it and the difficulty */
static void row_power(uint16_t obj, uint32_t boxes, uint32_t powers)
{
    uint16_t i = (uint16_t)(rd16(obj + S_ROW) << 2);
    wr32(obj + 0x2C, rom32(boxes + (uint32_t)(int16_t)i));
    i = (uint16_t)((int16_t)i >> 1);
    i = (uint16_t)(i + (uint16_t)(rd16(v_difficulty) << 3));
    wr16(obj + S_POWER, rom16(powers + (uint32_t)(int16_t)i));
}

/* $019F0E */
void FlamePower(uint16_t obj) { row_power(obj, 0x019F2C, 0x019F4C); }
/* $019F8C */
void FireFirePower(uint16_t obj) { row_power(obj, 0x019FAA, 0x019FCA); }

/* $01A00A. By the ring's size. */
void ForceFirePower(uint16_t obj)
{
    uint16_t n = rd16(obj + S_ROW);
    wr32(obj + 0x2C, rom32(0x01A028 + (uint32_t)(int16_t)(uint16_t)(n + n)));
    uint16_t i = (uint16_t)((uint16_t)(rd16(v_difficulty) << 2) + n);
    wr16(obj + S_POWER, rom16(0x01A034 + (uint32_t)(int16_t)i));
}

/* $01A054. Both boxes too. */
void ChaserFirePower(uint16_t obj)
{
    uint16_t i = (uint16_t)(rd16(obj + S_ROW) << 2);
    wr32(obj + 0x2C, rom32(0x01A078 + (uint32_t)(int16_t)i));
    wr32(obj + 0x28, rom32(0x01A078 + (uint32_t)(int16_t)i));
    i = (uint16_t)((int16_t)i >> 1);
    i = (uint16_t)(i + (uint16_t)(rd16(v_difficulty) << 3));
    wr16(obj + S_POWER, rom16(0x01A098 + (uint32_t)(int16_t)i));
}

/* ---- the spark, the bubble, the dust ---- */

/* $01A0E2. Type $18C: a shot's spark, a step back from where it hit; gone when played. */
void ShotSpark(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr32(obj + O_X, rd32(obj + O_X) - rd32(obj + S_VX));
        wr32(obj + O_Y, rd32(obj + O_Y) - rd32(obj + S_VY));
        wr16(obj + O_FLAGS, 0x8300);
        wr8(obj + 0x21, 0);
        wr8(obj + 0x22, 0);
        wr8(obj + 9, 0);
        wr16(obj + 0x0C, 0);
        wr16(obj + O_STATE, 8);
    }
    SelectAnimation(obj, 0x01A1CC);
    EffectPriority(obj);
    if (rd8(obj + 9) == 0xFF) gone(obj);
}

/* played once, then gone */
static void play_once(uint16_t obj, uint32_t anim)
{
    if (!rd16(obj + O_STATE)) {
        wr8(obj + 9, 0);
        wr16(obj + 0x0C, 0);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    }
    SelectAnimation(obj, anim);
    if (rd8(obj + 9) == 0xFF) gone(obj);
}

/* $01A132. Type $13C: a bubble (PlayerSplash). */
void Bubble(uint16_t obj) { play_once(obj, 0x01A1CC); }

/* $01A15E. Type $140: dust (PlayerDust). */
void Dust(uint16_t obj) { play_once(obj, 0x01A1B4); }

/* $01A1EA. Hit (+$22 bit 6): a hit mark (type $430) where it is, if a slot is free. */
void ShotHitMark(uint16_t obj)
{
    if (!bit8(obj + 0x22, 6)) return;
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y), left, e;
    if (!ObjectFindEffect(obj, &left, &e)) return;
    wr16(e + O_STATE, 0);
    wr16(e + O_TYPE, 0x430);
    wr16(e + O_X, x);
    wr16(e + O_Y, y);
    wr16(e + 0x4C, obj);
}

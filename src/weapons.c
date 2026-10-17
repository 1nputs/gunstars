/* The players' weapons: each of the four (force 4, lightning 8, chaser $C, fire $10) and each
 * pair of them (P_WEAPON 3: the two held combined) has a routine of a few steps (P_SHOT, the
 * table after its routine): starting (the aim not yet free), holding B a little (P_SHOT_TIME,
 * from P_SHOT_DELAY), firing (a shot every few frames into the player's eight effect slots, with
 * the muzzle flash at $FFC320 / $FFC6E0), and after B is let go a few frames before the next.
 * The beams (lightning, fire) wait for the last one to end, and turn their aim toward the pad's
 * a step at a time. */
#include "game.h"

#define P_SPRITE   0x0E
#define P_HURT     0x21
#define P_STATUS   0x23
#define P_HELD     0x69
#define P_WEAPON_A 0x6E
#define P_WEAPON_B 0x70
#define P_WEAPON   0x72
#define P_SHOT     0x94          /* the weapon's step, times 4 */
#define P_SHOT_B   0x96          /* byte: bit 7 the aim not yet free, bit 6 straight ahead, bit 5
                                    a beam, bit 4 turning, bit 3 a shot this frame, bits 2 and 1
                                    the pad's aim only */
#define P_SHOT_TIME 0x98
#define P_AIM_DIR  0x9A
#define P_SHOT_DELAY 0x9C
#define P_GUN_X    0x9E
#define P_GUN_Y    0xA0
#define P_TARGET   0xA2          /* the chaser's target; the fire's spacing; the ring's first */
#define P_TRAIL    0xA4          /* the fire's trail: the next point (times 8) */
#define P_AIM_STEP 0xA6
#define P_ALT_ANIMS 0xA8
#define P_SOUND_TIME 0xBA
#define P_TRAIL_POINTS 0x100     /* the fire's 16 points: x speed, x, y speed, y */

#define SINES      0x02977Cu

static void lightning_again(uint16_t obj);

static int b_held(uint16_t obj) { return bit8(obj + P_HELD, 4); }

/* the shot object's priority and status from the player's */
static void shot_from(uint16_t obj, uint16_t shot)
{
    ShotPriority(obj, shot);
    wr8(shot + P_STATUS, rd8(obj + P_STATUS) & 0xC0);
}

/* $01776A. The weapon, and when it fired the shooting animation's frames and the hurt box. */
void PlayerShoot(uint16_t obj)
{
    uint16_t t = rd16(obj + P_ALT_ANIMS);
    if (t) wr16(obj + P_ALT_ANIMS, (uint16_t)(t - 1));
    WeaponFire(obj);
    if (!bit8(obj + P_SHOT_B, 3)) { clr8(obj + P_HURT, 6); return; }
    clr8(obj + P_SHOT_B, 3);
    wr16(obj + P_ALT_ANIMS, 8);
    set8(obj + P_HURT, 6);
    clr8(obj + P_STATUS, 5);
    wr16(obj + 0x26, 4);
    uint8_t d = rd8(obj + P_AIM_DIR);
    if (bit8(obj + P_SPRITE, 3)) d = (uint8_t)(0x80 - d);
    uint16_t i = (uint16_t)((((uint8_t)(d + 0x10)) & 0xE0) >> 3);
    wr32(obj + 0x2C, rom32(0x0177CC + i));
}

/* $0177EC. The weapon used: the first, the second, the pair. */
void WeaponFire(uint16_t obj)
{
    uint8_t w = rd8(obj + P_WEAPON);
    if (!w) { WeaponNone(obj); return; }
    if (!(w & 1)) { WeaponSecond(obj); return; }
    uint32_t fn = rom32(0x017806 + (uint32_t)(int16_t)rd16(obj + P_WEAPON_A));
    switch (fn) {
    case 0x01781A: WeaponSecond(obj); return;
    case 0x01784C: WeaponWithForce(obj); return;
    case 0x017874: WeaponWithLightning(obj); return;
    case 0x0178C4: WeaponWithChaser(obj); return;
    case 0x01789C: WeaponWithFire(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

static void weapon_alone(uint16_t obj, uint32_t fn)
{
    switch (fn) {
    case 0x017842: WeaponNone(obj); return;
    case 0x0178EC: WeaponForce(obj); return;
    case 0x017ED0: WeaponLightning(obj); return;
    case 0x017BFC: WeaponChaser(obj); return;
    case 0x0183C4: WeaponFlame(obj); return;
    case 0x017B22: WeaponForceForce(obj); return;
    case 0x0182EA: WeaponForceLightning(obj); return;
    case 0x017DB0: WeaponForceChaser(obj); return;
    case 0x0186C6: WeaponForceFire(obj); return;
    case 0x018204: WeaponLightningLightning(obj); return;
    case 0x017FC2: WeaponLightningChaser(obj); return;
    case 0x0180F8: WeaponLightningFire(obj); return;
    case 0x017CD6: WeaponChaserChaser(obj); return;
    case 0x018858: WeaponChaserFire(obj); return;
    case 0x018576: WeaponFireFire(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $01781A. The second alone. */
void WeaponSecond(uint16_t obj)
{
    if (!bit8(obj + P_WEAPON, 1)) { WeaponNone(obj); return; }
    weapon_alone(obj, rom32(0x01782E + (uint32_t)(int16_t)rd16(obj + P_WEAPON_B)));
}

/* $017842 */
void WeaponNone(uint16_t obj)
{
    wr8(obj + P_SHOT_B, 0);
    wr16(obj + P_SHOT, 0);
}

/* $01784C, $017874, $01789C, $0178C4: the first with the second (combined: P_WEAPON 3) */
void WeaponWithForce(uint16_t obj)
{
    if (!bit8(obj + P_WEAPON, 1)) { WeaponForce(obj); return; }
    weapon_alone(obj, rom32(0x017860 + (uint32_t)(int16_t)rd16(obj + P_WEAPON_B)));
}

void WeaponWithLightning(uint16_t obj)
{
    if (!bit8(obj + P_WEAPON, 1)) { WeaponLightning(obj); return; }
    weapon_alone(obj, rom32(0x017888 + (uint32_t)(int16_t)rd16(obj + P_WEAPON_B)));
}

void WeaponWithFire(uint16_t obj)
{
    if (!bit8(obj + P_WEAPON, 1)) { WeaponFlame(obj); return; }
    weapon_alone(obj, rom32(0x0178B0 + (uint32_t)(int16_t)rd16(obj + P_WEAPON_B)));
}

void WeaponWithChaser(uint16_t obj)
{
    if (!bit8(obj + P_WEAPON, 1)) { WeaponChaser(obj); return; }
    weapon_alone(obj, rom32(0x0178D8 + (uint32_t)(int16_t)rd16(obj + P_WEAPON_B)));
}

/* ---- the steps' common parts ---- */

/* a step from the weapon's table */
static uint32_t step_of(uint16_t obj, uint32_t table)
{
    return rom32(table + (uint32_t)(int16_t)rd16(obj + P_SHOT));
}

/* starting: the aim held, the delay */
static void start(uint16_t obj)
{
    set8(obj + P_SHOT_B, 7);
    wr16(obj + P_SHOT_TIME, rd16(obj + P_SHOT_DELAY));
    wr16(obj + P_SHOT, 4);
}

/* counting down: whether there was time left (the aim followed meanwhile) */
static int counting(uint16_t obj)
{
    uint16_t t = rd16(obj + P_SHOT_TIME);
    if (!t) return 0;
    wr16(obj + P_SHOT_TIME, (uint16_t)(t - 1));
    WeaponAim(obj);
    return 1;
}

/* B let go: the delay before the next (a beam's ended) */
static void release(uint16_t obj, uint16_t step, int beam)
{
    if (beam) clr8(obj + P_SHOT_B, 5);
    set8(obj + P_SHOT_B, 7);
    wr16(obj + P_SHOT_TIME, rd16(obj + P_SHOT_DELAY));
    wr16(obj + P_SHOT, step);
}

/* after B was let go: held again restarts; at the delay's end, no weapon step */
static int released(uint16_t obj)
{
    if (b_held(obj)) return 1;
    if (counting(obj)) return 0;
    wr16(obj + P_SHOT, 0);
    wr8(obj + P_SHOT_B, 0);
    return 0;
}

/* a shot object of `type` in the slot found */
static void shoot(uint16_t obj, uint16_t slot, uint16_t type)
{
    wr16(slot + O_TYPE, type);
    shot_from(obj, slot);
}

/* ---- force: rapid shots ---- */

/* $0178EC */
void WeaponForce(uint16_t obj)
{
    switch (step_of(obj, 0x0178F6)) {
    case 0x017906: ForceStart(obj); return;
    case 0x017918: ForceHold(obj); return;
    case 0x017942: ForceFire(obj); return;
    case 0x0179A0: ForceRelease(obj); return;
    default: call_object_missing(step_of(obj, 0x0178F6), obj); return;
    }
}

/* $017906 */
void ForceStart(uint16_t obj) { start(obj); ForceHold(obj); }

/* $017918 */
void ForceHold(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0xC, 0); ForceRelease(obj); return; }
    if (counting(obj)) return;
    clr8(obj + P_SHOT_B, 7);
    wr16(obj + P_SHOT_TIME, 0);
    wr16(obj + P_SHOT, (uint16_t)(rd16(obj + P_SHOT) + 4));
    ForceFire(obj);
}

/* $017942. A shot (type $40) every third frame. */
void ForceFire(uint16_t obj)
{
    WeaponAim(obj);
    if (rd16(obj + P_SHOT_TIME)) { wr16(obj + P_SHOT_TIME, (uint16_t)(rd16(obj + P_SHOT_TIME) - 1)); return; }
    if (!b_held(obj)) { release(obj, 0xC, 0); ForceRelease(obj); return; }
    uint16_t slot;
    if (!ShotSlot(obj, &slot)) return;
    wr16(obj + P_SHOT_TIME, 2);
    QueueSound(0xA2);
    ShotMuzzle(obj);
    shoot(obj, slot, 0x40);
}

/* $0179A0 */
void ForceRelease(uint16_t obj) { if (released(obj)) ForceStart(obj); }

/* the plain pattern: hold, then a shot of `type` every `every` + 1 frames */
static void plain_hold(uint16_t obj, void (*rel)(uint16_t), void (*fire)(uint16_t))
{
    if (!b_held(obj)) { release(obj, 0xC, 0); rel(obj); return; }
    if (counting(obj)) return;
    clr8(obj + P_SHOT_B, 7);
    wr16(obj + P_SHOT_TIME, 0);
    wr16(obj + P_SHOT, (uint16_t)(rd16(obj + P_SHOT) + 4));
    fire(obj);
}

/* the plain pattern's firing; the slot found, the shot made; whether one was */
static int plain_fire(uint16_t obj, void (*rel)(uint16_t), uint8_t sound, uint16_t every, uint16_t type, uint16_t *slot)
{
    WeaponAim(obj);
    if (rd16(obj + P_SHOT_TIME)) { wr16(obj + P_SHOT_TIME, (uint16_t)(rd16(obj + P_SHOT_TIME) - 1)); return 0; }
    if (!b_held(obj)) { release(obj, 0xC, 0); rel(obj); return 0; }
    if (!ShotSlot(obj, slot)) return 0;
    ShotMuzzle(obj);
    QueueSound(sound);
    wr16(obj + P_SHOT_TIME, every);
    shoot(obj, *slot, type);
    return 1;
}

/* ---- force and force ---- */

/* $017B22 */
void WeaponForceForce(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x017B2C);
    switch (fn) {
    case 0x017B3C: ForceForceStart(obj); return;
    case 0x017B4E: ForceForceHold(obj); return;
    case 0x017B78: ForceForceFire(obj); return;
    case 0x017BD6: ForceForceRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $017B3C, $017B4E, $017B78 (a shot, type $154, every other frame), $017BD6 */
void ForceForceStart(uint16_t obj) { start(obj); ForceForceHold(obj); }
void ForceForceHold(uint16_t obj) { plain_hold(obj, ForceForceRelease, ForceForceFire); }
void ForceForceFire(uint16_t obj) { uint16_t s; plain_fire(obj, ForceForceRelease, 0xAE, 1, 0x154, &s); }
void ForceForceRelease(uint16_t obj) { if (released(obj)) ForceForceStart(obj); }

/* ---- chaser ---- */

/* $017BFC */
void WeaponChaser(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x017C06);
    switch (fn) {
    case 0x017C16: ChaserStart(obj); return;
    case 0x017C28: ChaserHold(obj); return;
    case 0x017C52: ChaserFire(obj); return;
    case 0x017CB0: ChaserRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $017C16, $017C28, $017C52 (type $148, every third frame), $017CB0 */
void ChaserStart(uint16_t obj) { start(obj); ChaserHold(obj); }
void ChaserHold(uint16_t obj) { plain_hold(obj, ChaserRelease, ChaserFire); }
void ChaserFire(uint16_t obj) { uint16_t s; plain_fire(obj, ChaserRelease, 0xAA, 2, 0x148, &s); }
void ChaserRelease(uint16_t obj) { if (released(obj)) ChaserStart(obj); }

/* ---- chaser and chaser ---- */

/* $017CD6 */
void WeaponChaserChaser(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x017CE0);
    switch (fn) {
    case 0x017CF0: ChaserChaserStart(obj); return;
    case 0x017D02: ChaserChaserHold(obj); return;
    case 0x017D2C: ChaserChaserFire(obj); return;
    case 0x017D8A: ChaserChaserRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $017CF0, $017D02, $017D2C (type $14C, every third frame), $017D8A */
void ChaserChaserStart(uint16_t obj) { start(obj); ChaserChaserHold(obj); }
void ChaserChaserHold(uint16_t obj) { plain_hold(obj, ChaserChaserRelease, ChaserChaserFire); }
void ChaserChaserFire(uint16_t obj) { uint16_t s; plain_fire(obj, ChaserChaserRelease, 0xD5, 2, 0x14C, &s); }
void ChaserChaserRelease(uint16_t obj) { if (released(obj)) ChaserChaserStart(obj); }

/* ---- force and chaser: shots that home on a target, the next taken in turn ---- */

/* $017DB0 */
void WeaponForceChaser(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x017DBA);
    switch (fn) {
    case 0x017DCA: ForceChaserStart(obj); return;
    case 0x017DDC: ForceChaserHold(obj); return;
    case 0x017E0A: ForceChaserFire(obj); return;
    case 0x017E78: ForceChaserRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $017DCA */
void ForceChaserStart(uint16_t obj) { start(obj); ForceChaserHold(obj); }

/* $017DDC: a target taken as the firing starts */
void ForceChaserHold(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0xC, 0); ForceChaserRelease(obj); return; }
    if (counting(obj)) return;
    clr8(obj + P_SHOT_B, 7);
    ChaserTarget(obj);
    wr16(obj + P_SHOT_TIME, 0);
    wr16(obj + P_SHOT, (uint16_t)(rd16(obj + P_SHOT) + 4));
    ForceChaserFire(obj);
}

/* $017E0A. Type $150 every other frame; the next target once the one gone for can no longer be
 * hit. */
void ForceChaserFire(uint16_t obj)
{
    uint16_t s;
    if (!plain_fire(obj, ForceChaserRelease, 0xC2, 1, 0x150, &s)) return;
    if (!bit8(rd16(obj + P_TARGET) + P_HURT, 5)) ChaserTarget(obj);
}

/* $017E78 */
void ForceChaserRelease(uint16_t obj) { if (released(obj)) ForceChaserStart(obj); }

/* $017E9E. The next target after the one it has (by address), the first after the last; none:
 * itself. */
void ChaserTarget(uint16_t obj)
{
    uint16_t n = rd16(v_targetCount);
    if (!n) { wr16(obj + P_TARGET, obj); return; }
    uint16_t t = rd16(v_targets);
    if (--n) {
        uint16_t now = rd16(obj + P_TARGET);
        for (uint16_t at = v_targets;; at += 2) {
            uint16_t c = rd16(at);
            if (c > now) { t = c; break; }
            if (n-- == 0) { t = rd16(v_targets); break; }
        }
    }
    wr16(obj + P_TARGET, t);
}

/* ---- the beams' common steps ---- */

/* holding: at the delay's end, waiting for the last beam (step 8) */
static void beam_hold(uint16_t obj, uint16_t release_step, void (*rel)(uint16_t), void (*wait)(uint16_t))
{
    if (!b_held(obj)) { release(obj, release_step, 1); rel(obj); return; }
    if (counting(obj)) return;
    clr8(obj + P_SHOT_B, 7);
    wr16(obj + P_SHOT, 8);
    wait(obj);
}

/* waiting for the last beam to end: whether it has (B held all the while) */
static int beam_wait(uint16_t obj, uint16_t release_step, void (*rel)(uint16_t))
{
    if (!b_held(obj)) { release(obj, release_step, 1); rel(obj); return 0; }
    WeaponAim(obj);
    return !ShotLingering(obj);
}

/* a sound every third frame of a beam */
static void beam_sound(uint16_t obj, uint8_t sound)
{
    if (rd16(obj + P_SOUND_TIME)) {
        wr16(obj + P_SOUND_TIME, (uint16_t)(rd16(obj + P_SOUND_TIME) - 1));
        return;
    }
    wr16(obj + P_SOUND_TIME, 2);
    QueueSound(sound);
}

/* ---- lightning: a beam of up to 8 shots ---- */

/* $017ED0 */
void WeaponLightning(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x017EDA);
    switch (fn) {
    case 0x017EEE: LightningStart(obj); return;
    case 0x017F00: LightningHold(obj); return;
    case 0x017F2E: LightningWait(obj); return;
    case 0x017F50: LightningFire(obj); return;
    case 0x017F9C: LightningRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $017EEE */
void LightningStart(uint16_t obj) { start(obj); LightningHold(obj); }

/* $017F00 */
void LightningHold(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0x10, 1); LightningRelease(obj); return; }
    if (counting(obj)) return;
    lightning_again(obj);
}

/* $017F1C: waiting for the beam again */
static void lightning_again(uint16_t obj)
{
    clr8(obj + P_SHOT_B, 7);
    set8(obj + P_SHOT_B, 5);
    wr16(obj + P_SHOT, 8);
    LightningWait(obj);
}

/* $017F2E */
void LightningWait(uint16_t obj)
{
    if (!beam_wait(obj, 0x10, LightningRelease)) return;
    wr16(obj + P_SHOT_TIME, 8);
    wr16(obj + P_SHOT, (uint16_t)(rd16(obj + P_SHOT) + 4));
    LightningFire(obj);
}

/* $017F50. A shot (type $158) a frame, 8; then waiting again. */
void LightningFire(uint16_t obj)
{
    WeaponAim(obj);
    uint16_t slot;
    if (!ShotSlot(obj, &slot)) { lightning_again(obj); return; }
    wr16(obj + P_SHOT_TIME, (uint16_t)(rd16(obj + P_SHOT_TIME) - 1));
    if (rd16(obj + P_SHOT_TIME) & 0x8000) { lightning_again(obj); return; }
    ShotMuzzle(obj);
    QueueSound(0xA8);
    shoot(obj, slot, 0x158);
}

/* $017F9C */
void LightningRelease(uint16_t obj) { if (released(obj)) LightningStart(obj); }

/* ---- lightning and chaser: a beam of 8 shots that follows a target ---- */

/* $017FC2 */
void WeaponLightningChaser(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x017FCC);
    switch (fn) {
    case 0x017FE0: LightningChaserStart(obj); return;
    case 0x017FF2: LightningChaserHold(obj); return;
    case 0x018024: LightningChaserWait(obj); return;
    case 0x018048: LightningChaserFire(obj); return;
    case 0x0180B4: LightningChaserRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $017FE0 */
void LightningChaserStart(uint16_t obj) { start(obj); LightningChaserHold(obj); }

/* $017FF2 */
void LightningChaserHold(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0x10, 1); LightningChaserRelease(obj); return; }
    if (counting(obj)) return;
    clr8(obj + P_SHOT_B, 7);
    set8(obj + P_SHOT_B, 5);
    wr16(obj + P_SOUND_TIME, 0);
    wr16(obj + P_SHOT, 8);
    LightningChaserWait(obj);
}

/* $018024 */
void LightningChaserWait(uint16_t obj)
{
    if (!beam_wait(obj, 0x10, LightningChaserRelease)) return;
    ChaserTarget(obj);
    wr16(obj + P_SHOT_TIME, 0);
    wr16(obj + P_SHOT, (uint16_t)(rd16(obj + P_SHOT) + 4));
    LightningChaserFire(obj);
}

/* $018048. Shots (type $188) numbered 0-7, a frame each. */
void LightningChaserFire(uint16_t obj)
{
    WeaponAim(obj);
    LightningChaserSound(obj);
    uint16_t slot;
    if (ShotSlot(obj, &slot) && (int16_t)rd16(obj + P_SHOT_TIME) < 8) {
        ShotMuzzle(obj);
        QueueSound(0xA8);
        shoot(obj, slot, 0x188);
        wr16(slot + 0x4A, rd16(obj + P_SHOT_TIME));
        wr16(obj + P_SHOT_TIME, (uint16_t)(rd16(obj + P_SHOT_TIME) + 1));
        return;
    }
    if (!b_held(obj)) { release(obj, 0x10, 1); LightningChaserRelease(obj); }
}

/* $0180B4 */
void LightningChaserRelease(uint16_t obj) { if (released(obj)) LightningChaserStart(obj); }

/* $0180DA */
void LightningChaserSound(uint16_t obj) { beam_sound(obj, 0xD3); }

/* ---- lightning and fire: a turning beam ---- */

/* $0180F8 */
void WeaponLightningFire(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x018102);
    switch (fn) {
    case 0x018116: LightningFireStart(obj); return;
    case 0x018128: LightningFireHold(obj); return;
    case 0x018150: LightningFireWait(obj); return;
    case 0x018176: LightningFireFire(obj); return;
    case 0x0181DE: LightningFireRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $018116, $018128 */
void LightningFireStart(uint16_t obj) { start(obj); LightningFireHold(obj); }
void LightningFireHold(uint16_t obj) { beam_hold(obj, 0x10, LightningFireRelease, LightningFireWait); }

/* $018150 */
void LightningFireWait(uint16_t obj)
{
    if (!beam_wait(obj, 0x10, LightningFireRelease)) return;
    set8(obj + P_SHOT_B, 5);
    wr16(obj + P_SHOT_TIME, 0);
    wr16(obj + P_SHOT, (uint16_t)(rd16(obj + P_SHOT) + 4));
    LightningFireFire(obj);
}

/* $018176. Type $15C every other frame, the aim turning $1000 a frame. */
void LightningFireFire(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0x10, 1); LightningFireRelease(obj); return; }
    WeaponAimTurn(obj, 0x1000);
    if (rd16(obj + P_SHOT_TIME)) { wr16(obj + P_SHOT_TIME, (uint16_t)(rd16(obj + P_SHOT_TIME) - 1)); return; }
    uint16_t slot;
    if (!ShotSlot(obj, &slot)) return;
    ShotMuzzle(obj);
    QueueSound(0xB4);
    wr16(obj + P_SHOT_TIME, 1);
    shoot(obj, slot, 0x15C);
}

/* $0181DE */
void LightningFireRelease(uint16_t obj) { if (released(obj)) LightningFireStart(obj); }

/* ---- lightning and lightning ---- */

/* $018204 */
void WeaponLightningLightning(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x01820E);
    switch (fn) {
    case 0x01821E: LightningLightningStart(obj); return;
    case 0x018230: LightningLightningHold(obj); return;
    case 0x018260: LightningLightningFire(obj); return;
    case 0x0182C4: LightningLightningRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $01821E */
void LightningLightningStart(uint16_t obj) { start(obj); LightningLightningHold(obj); }

/* $018230 */
void LightningLightningHold(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0xC, 1); LightningLightningRelease(obj); return; }
    if (counting(obj)) return;
    clr8(obj + P_SHOT_B, 7);
    set8(obj + P_SHOT_B, 5);
    wr16(obj + P_SHOT_TIME, 0);
    wr16(obj + P_SHOT, (uint16_t)(rd16(obj + P_SHOT) + 4));
    LightningLightningFire(obj);
}

/* $018260. Type $164 every other frame. */
void LightningLightningFire(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0xC, 1); LightningLightningRelease(obj); return; }
    WeaponAim(obj);
    if (rd16(obj + P_SHOT_TIME)) { wr16(obj + P_SHOT_TIME, (uint16_t)(rd16(obj + P_SHOT_TIME) - 1)); return; }
    uint16_t slot;
    if (!ShotSlot(obj, &slot)) return;
    ShotMuzzle(obj);
    QueueSound(0xD7);
    wr16(obj + P_SHOT_TIME, 1);
    shoot(obj, slot, 0x164);
}

/* $0182C4 */
void LightningLightningRelease(uint16_t obj) { if (released(obj)) LightningLightningStart(obj); }

/* ---- force and lightning: a turning stream ---- */

/* $0182EA */
void WeaponForceLightning(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x0182F4);
    switch (fn) {
    case 0x018304: ForceLightningStart(obj); return;
    case 0x018316: ForceLightningHold(obj); return;
    case 0x018346: ForceLightningFire(obj); return;
    case 0x01839E: ForceLightningRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $018304 */
void ForceLightningStart(uint16_t obj) { start(obj); ForceLightningHold(obj); }

/* $018316 */
void ForceLightningHold(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0xC, 1); ForceLightningRelease(obj); return; }
    if (counting(obj)) return;
    clr8(obj + P_SHOT_B, 7);
    WeaponAim(obj);
    set8(obj + P_SHOT_B, 5);
    wr16(obj + P_SHOT, (uint16_t)(rd16(obj + P_SHOT) + 4));
    ForceLightningFire(obj);
}

/* $018346. Type $160 every frame, the aim turning $600 a frame. */
void ForceLightningFire(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0xC, 1); ForceLightningRelease(obj); return; }
    WeaponAimTurn(obj, 0x600);
    uint16_t slot;
    if (!ShotSlot(obj, &slot)) return;
    ShotMuzzle(obj);
    QueueSound(0xD6);
    shoot(obj, slot, 0x160);
}

/* $01839E */
void ForceLightningRelease(uint16_t obj) { if (released(obj)) ForceLightningStart(obj); }

/* ---- fire: a flame of up to 6 (fire and fire: 8) along a trail ---- */

/* $0183C4 */
void WeaponFlame(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x0183CE);
    switch (fn) {
    case 0x0183E2: FlameStart(obj); return;
    case 0x0183F4: FlameHold(obj); return;
    case 0x01841C: FlameWait(obj); return;
    case 0x01844A: FlameFire(obj); return;
    case 0x0184D0: FlameRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $0183E2 */
void FlameStart(uint16_t obj) { start(obj); FlameHold(obj); }

/* the flames' waiting step: on to firing, the trail started */
static int flame_wait(uint16_t obj, void (*rel)(uint16_t))
{
    if (!beam_wait(obj, 0x10, rel)) return 0;
    set8(obj + P_SHOT_B, 5);
    wr16(obj + P_SOUND_TIME, 0);
    wr16(obj + P_TRAIL, 0);
    wr16(obj + P_SHOT_TIME, 0);
    wr16(obj + P_SHOT, (uint16_t)(rd16(obj + P_SHOT) + 4));
    return 1;
}

/* the flames' firing: the trail moved, a flame (type) where its spacing has room, up to n */
static void flame_fire(uint16_t obj, void (*rel)(uint16_t), void (*sound)(uint16_t), uint16_t n, uint16_t type)
{
    if (!b_held(obj)) { release(obj, 0x10, 1); rel(obj); return; }
    WeaponAimTurn(obj, 0x800);
    FlameTrail(obj);
    ShotMuzzle(obj);
    sound(obj);
    uint16_t k = rd16(obj + P_SHOT_TIME);
    if ((int16_t)k >= (int16_t)n) return;
    uint16_t at = FlameSpacing(k);
    if ((int16_t)at >= (int16_t)rd16(obj + P_TRAIL)) return;
    wr16(obj + P_TARGET, at);
    uint16_t slot;
    if (!ShotSlot(obj, &slot)) return;
    shoot(obj, slot, type);
    wr16(slot + 0x50, rd16(obj + P_TARGET));
    wr16(slot + 0x4A, rd16(obj + P_SHOT_TIME));
    wr16(obj + P_SHOT_TIME, (uint16_t)(rd16(obj + P_SHOT_TIME) + 1));
}

/* $0183F4, $01841C, $01844A (type $178), $0184D0 */
void FlameHold(uint16_t obj) { beam_hold(obj, 0x10, FlameRelease, FlameWait); }
void FlameWait(uint16_t obj) { if (flame_wait(obj, FlameRelease)) FlameFire(obj); }
void FlameFire(uint16_t obj) { flame_fire(obj, FlameRelease, FlameSound, 6, 0x178); }
void FlameRelease(uint16_t obj) { if (released(obj)) FlameStart(obj); }

/* $0184F6 */
void FlameSound(uint16_t obj) { beam_sound(obj, 0xC0); }

/* $018514. Where on the trail the k-th flame is (0, 8, $10, $20 ...). */
uint16_t FlameSpacing(uint16_t k) { return rom16(0x01851C + (uint32_t)(int16_t)(k << 1)); }

/* $01852C. The trail's 16 points moved by their speeds; the next one starts at the muzzle, at
 * the aim's direction (its sine and cosine / 128). */
void FlameTrail(uint16_t obj)
{
    uint16_t p = (uint16_t)(obj + P_TRAIL_POINTS);
    for (uint16_t i = 0; i < 16; i++, p += 8) {
        wr16((uint16_t)(p + 2), (uint16_t)(rd16((uint16_t)(p + 2)) + rd16(p)));
        wr16((uint16_t)(p + 6), (uint16_t)(rd16((uint16_t)(p + 6)) + rd16((uint16_t)(p + 4))));
    }
    uint16_t n = (uint16_t)((rd16(obj + P_TRAIL) + 8) & 0x78);
    wr16(obj + P_TRAIL, n);
    uint16_t i = (uint16_t)((rd8(obj + P_AIM_DIR) & 0xFF) << 1);
    uint16_t at = (uint16_t)(obj + P_TRAIL_POINTS + n);
    wr16(at, (uint16_t)((int16_t)rom16(SINES + 0x80 + i) >> 7));
    wr16((uint16_t)(at + 4), (uint16_t)((int16_t)rom16(SINES + i) >> 7));
}

/* $018576 */
void WeaponFireFire(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x018580);
    switch (fn) {
    case 0x018594: FireFireStart(obj); return;
    case 0x0185A6: FireFireHold(obj); return;
    case 0x0185CE: FireFireWait(obj); return;
    case 0x0185FC: FireFireFire(obj); return;
    case 0x018682: FireFireRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $018594, $0185A6, $0185CE, $0185FC (type $17C, 8 of them), $018682 */
void FireFireStart(uint16_t obj) { start(obj); FireFireHold(obj); }
void FireFireHold(uint16_t obj) { beam_hold(obj, 0x10, FireFireRelease, FireFireWait); }
void FireFireWait(uint16_t obj) { if (flame_wait(obj, FireFireRelease)) FireFireFire(obj); }
void FireFireFire(uint16_t obj) { flame_fire(obj, FireFireRelease, FireFireSound, 8, 0x17C); }
void FireFireRelease(uint16_t obj) { if (released(obj)) FireFireStart(obj); }

/* $0186A8 */
void FireFireSound(uint16_t obj) { beam_sound(obj, 0xBF); }

/* ---- force and fire: a ring of four ---- */

/* $0186C6 */
void WeaponForceFire(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x0186D0);
    switch (fn) {
    case 0x0186E0: ForceFireStart(obj); return;
    case 0x0186F2: ForceFireHold(obj); return;
    case 0x018728: ForceFireFire(obj); return;
    case 0x018832: ForceFireRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $0186E0 */
void ForceFireStart(uint16_t obj) { start(obj); ForceFireHold(obj); }

/* $0186F2 */
void ForceFireHold(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0xC, 1); ForceFireRelease(obj); return; }
    if (counting(obj)) return;
    clr8(obj + P_SHOT_B, 7);
    wr16(obj + P_TARGET, 0);
    wr16(obj + P_SHOT_TIME, 0);
    set8(obj + P_SHOT_B, 5);
    wr16(obj + P_SHOT, 8);
    ForceFireFire(obj);
}

/* a ring shot (type $180) of the first, at a quarter turn from the one before */
static void ring_shot(uint16_t obj, uint16_t first, uint16_t dir)
{
    uint16_t slot;
    ShotSlot(obj, &slot);                                /* (no check: four were free) */
    wr16(slot + O_FLAGS, 0x40);
    wr16(slot + O_TYPE, 0x180);
    shot_from(obj, slot);
    wr16(slot + 0x4E, first);
    wr16(slot + 0x52, dir);
}

/* $018728. Four shots in a ring every 13 frames, when four slots are free. */
void ForceFireFire(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0xC, 1); ForceFireRelease(obj); return; }
    WeaponAim(obj);
    uint16_t t = rd16(obj + P_SHOT_TIME);
    if (t) { wr16(obj + P_SHOT_TIME, (uint16_t)(t - 1)); return; }
    if (!ShotSlotsFree(obj)) return;
    ShotMuzzle(obj);
    QueueSound(0xA9);
    wr16(obj + P_SHOT_TIME, 0xC);
    uint16_t first;
    ShotSlot(obj, &first);
    wr16(first + O_FLAGS, 0x40);
    wr16(obj + P_TARGET, first);
    wr16(first + O_TYPE, 0x180);
    shot_from(obj, first);
    wr16(first + 0x4E, first);
    uint16_t dir = rd16(obj + P_AIM_DIR);
    wr16(first + 0x52, dir);
    for (int k = 0; k < 3; k++) {
        dir = (uint16_t)(dir + 0x4000);
        ring_shot(obj, first, dir);
    }
}

/* $018832 */
void ForceFireRelease(uint16_t obj) { if (released(obj)) ForceFireStart(obj); }

/* ---- chaser and fire: a trail of 8 ---- */

/* $018858 */
void WeaponChaserFire(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x018862);
    switch (fn) {
    case 0x01887A: ChaserFireStart(obj); return;
    case 0x01888C: ChaserFireHold(obj); return;
    case 0x0188B4: ChaserFireWait(obj); return;
    case 0x0188E2: ChaserFireFire(obj); return;
    case 0x018958: ChaserFireHolding(obj); return;
    case 0x018982: ChaserFireRelease(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $01887A, $01888C */
void ChaserFireStart(uint16_t obj) { start(obj); ChaserFireHold(obj); }
void ChaserFireHold(uint16_t obj) { beam_hold(obj, 0x14, ChaserFireRelease, ChaserFireWait); }

/* $0188B4 */
void ChaserFireWait(uint16_t obj)
{
    if (!beam_wait(obj, 0x14, ChaserFireRelease)) return;
    set8(obj + P_SHOT_B, 5);
    wr16(obj + P_SOUND_TIME, 0);
    wr16(obj + P_SHOT_TIME, 0);
    wr16(obj + P_TRAIL, 0);
    wr16(obj + P_SHOT, (uint16_t)(rd16(obj + P_SHOT) + 4));
    ChaserFireFire(obj);
}

/* $0188E2. A shot (type $184) as the count reaches each of the table's, up to 8; then holding. */
void ChaserFireFire(uint16_t obj)
{
    uint16_t slot;
    int room = ShotSlot(obj, &slot);
    if (room) wr16(obj + P_TARGET, slot);
    if (!room || (int16_t)rd16(obj + P_SHOT_TIME) >= 8) {
        wr16(obj + P_SHOT, 0x10);
        ChaserFireHolding(obj);
        return;
    }
    wr16(obj + P_TRAIL, (uint16_t)(rd16(obj + P_TRAIL) + 8));
    uint16_t at = rom16(0x018942 + (uint32_t)(int16_t)(rd16(obj + P_SHOT_TIME) << 1));
    if (at != rd16(obj + P_TRAIL)) return;
    ShotMuzzle(obj);
    ChaserFireSound(obj);
    uint16_t s = rd16(obj + P_TARGET);
    wr16(s + O_TYPE, 0x184);
    shot_from(obj, s);
    wr16(s + 0x4A, rd16(obj + P_SHOT_TIME));
    wr16(obj + P_SHOT_TIME, (uint16_t)(rd16(obj + P_SHOT_TIME) + 1));
}

/* $018958 */
void ChaserFireHolding(uint16_t obj)
{
    if (!b_held(obj)) { release(obj, 0x14, 1); ChaserFireRelease(obj); return; }
    WeaponAim(obj);
    ChaserFireSound(obj);
}

/* $018982 */
void ChaserFireRelease(uint16_t obj) { if (released(obj)) ChaserFireStart(obj); }

/* $0189A8 */
void ChaserFireSound(uint16_t obj) { beam_sound(obj, 0xC1); }

/* ---- the aim ---- */

/* $0179C6. The aim from the pad (by P_SHOT_B: the pad's only, straight ahead, or with the
 * facing), into the shot's direction. */
void WeaponAim(uint16_t obj)
{
    uint8_t f = rd8(obj + P_SHOT_B);
    if (f & 4) { PlayerAimPad(obj); return; }
    clr8(obj + P_SHOT_B, 4);
    if (f & 2) PlayerAimPad(obj);
    else if (f & 0x40) PlayerAimStraight(obj);
    else PlayerAim(obj);
    wr16(obj + P_AIM_DIR, rd16(obj + 6));
}

/* $017A18. The same, the direction turning toward the aim by at most `step` a frame (straight
 * ahead: at once), the facing by the direction. */
void WeaponAimTurn(uint16_t obj, uint16_t step)
{
    uint8_t f = rd8(obj + P_SHOT_B);
    if (f & 4) { PlayerAimPad(obj); return; }
    clr8(obj + P_SHOT_B, 4);
    if (f & 2) PlayerAimPad(obj);
    else if (f & 0x40) {
        uint16_t d = bit8(obj + P_SPRITE, 3) ? 0x8000 : 0;
        wr16(obj + 6, d);
        wr16(obj + P_AIM_DIR, d);
        return;
    } else PlayerAim(obj);
    WeaponTurn(obj, step);
    WeaponFaceAim(obj);
}

/* $017A80. A step of the turn (P_SHOT_B bit 4 while turning). The step's sign is the 68k's
 * d7 after it (negated going down). */
void WeaponTurn(uint16_t obj, uint16_t step)
{
    uint16_t d = (uint16_t)(rd16(obj + 6) - rd16(obj + P_AIM_DIR));
    if (!d) return;
    set8(obj + P_SHOT_B, 4);
    int down;
    if (!(d & 0x8000)) down = 0;
    else if (d != 0x8000) down = 1;
    else {
        uint16_t a = (uint16_t)(rd16(obj + 6) + 0x4000);
        if (a && !(a & 0x8000)) down = 0;
        else if (a && a != 0x8000) down = 1;
        else down = bit8(obj + P_SPRITE, 3);
    }
    uint16_t by;
    if (down) {
        step = (uint16_t)-step;
        by = (int16_t)d < (int16_t)step ? step : d;
    } else {
        by = d >= step ? step : d;
    }
    wr16(obj + P_AIM_DIR, (uint16_t)(rd16(obj + P_AIM_DIR) + by));
    wr16(obj + P_AIM_STEP, by);
}

/* $017AF6. The facing by the shot's direction (straight up or down: unchanged). */
void WeaponFaceAim(uint16_t obj)
{
    uint16_t d = rd16(obj + P_AIM_DIR);
    if (d == 0x4000 || d == 0xC000) return;
    if (d < 0x4000 || d > 0xC000) clr8(obj + P_SPRITE, 3);
    else set8(obj + P_SPRITE, 3);
}

/* ---- the shots' slots, the muzzle ---- */

/* the player's eight slots */
static void pool(uint16_t obj, uint16_t *from, uint16_t *end)
{
    *from = obj == v_player1 ? 0xBFC0 : 0xC380;
    *end = obj == v_player1 ? 0xC2C0 : 0xC680;
}

/* $0189F6. A slot for a shot: a free one, else one marked to be given up (bit 6 of its flags'
 * low byte clear), cleared; whether there is one (Z). Without, the slot is the end. */
int ShotSlot(uint16_t obj, uint16_t *slot)
{
    uint16_t at, end;
    pool(obj, &at, &end);
    for (uint16_t a = at; a < end; a = (uint16_t)(a + 0x60))
        if (!rd16(a)) { *slot = a; return 1; }
    for (uint16_t a = at; a < end; a = (uint16_t)(a + 0x60)) {
        if (!bit8(a + O_FLAGS + 1, 6)) {
            for (uint16_t i = 0; i < 0x60; i += 4) wr32((uint16_t)(a + i), 0);
            *slot = a;
            return 1;
        }
    }
    *slot = end;
    return 0;
}

/* $018A70. Whether a shot that lingers (bit 6) is still there (Z clear). */
int ShotLingering(uint16_t obj)
{
    uint16_t a, end;
    pool(obj, &a, &end);
    for (; a != end; a = (uint16_t)(a + 0x60))
        if (bit8(a + O_FLAGS + 1, 6)) return 1;
    return 0;
}

/* $018ACC. Whether four slots have no lingering shot (Z). */
int ShotSlotsFree(uint16_t obj)
{
    uint16_t a, end, n = 4;
    pool(obj, &a, &end);
    for (; a != end; a = (uint16_t)(a + 0x60))
        if (!bit8(a + O_FLAGS + 1, 6) && --n == 0) return 1;
    return 0;
}

/* $018B08. The muzzle flash (type $374) for the shot this frame. */
void ShotMuzzle(uint16_t obj)
{
    set8(obj + P_SHOT_B, 3);
    uint16_t m = obj == v_player1 ? 0xC320 : 0xC6E0;
    wr16(m + O_STATE, 0);
    wr16(m + O_TYPE, 0x374);
    wr16(m + 0x4A, obj);
    MuzzlePriority(obj, m);
    wr16(m + P_SPRITE, rd16(obj + P_SPRITE));
}

/* $018B3A. The muzzle flash's routine (type $374): at the muzzle, 5 frames, flickering
 * between its two frames. */
void MuzzleFlash(uint16_t obj)
{
    uint16_t p = rd16(obj + 0x4A);
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0x8000);
        wr32(obj + 0x08, 0x0500F8F8u);
        wr16(obj + 0x48, 4);
        wr8(obj + P_STATUS, rd8(p + P_STATUS) & 0xC0);
        wr16(obj + O_X, (uint16_t)(rd16(p + O_X) + rd16(p + P_GUN_X)));
        wr16(obj + O_Y, (uint16_t)(rd16(p + O_Y) + rd16(p + P_GUN_Y)));
        wr16(obj + O_STATE, 4);
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) { wr16(obj + O_FLAGS, 0x1000); return; }
    uint16_t s = (uint16_t)(rd16(p + P_SPRITE) & 0x8000);
    wr16(obj + P_SPRITE, (uint16_t)(s | ((rd8(v_gameFrames + 1) & 1) ? 0x588 : 0x584)));
}

/* $018BC2. The muzzle 8 nearer than the player (not below 0). */
void MuzzlePriority(uint16_t obj, uint16_t m)
{
    int8_t p = (int8_t)(rd8(obj + 0x20) - 8);
    wr8(m + 0x20, (uint8_t)(p < 0 ? 0 : p));
}

/* $018BD4. A shot 4 nearer. */
void ShotPriority(uint16_t obj, uint16_t s)
{
    int8_t p = (int8_t)(rd8(obj + 0x20) - 4);
    wr8(s + 0x20, (uint8_t)(p < 0 ? 0 : p));
}

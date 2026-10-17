/* Object types from $31292 to $370C0: the stages' set pieces, their spawners and their small
 * enemies (types $18, $1C, $20, $190-$19C, $1B8-$1D4, $1E8, $218, $228, $22C, $2A4, $2C0, $2C4,
 * $37C-$394, $4EC). */
#include "game.h"

static void next(uint16_t obj, uint16_t by) { wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + by)); }
static void look(uint16_t obj, uint32_t map) { wr32(obj + 0x08, map); wr16(obj + 0x0C, 0); }
static uint16_t seed(void) { return rd16(v_randomSeed); }
/* a word counter less 1: 1 when it is now below 0 */
static int dec(uint16_t obj, uint16_t field)
{
    wr16(obj + field, (uint16_t)(rd16(obj + field) - 1));
    return (rd16(obj + field) & 0x8000) != 0;
}

/* ---- shared helpers ---- */


/* $03F768. d0 plus $348 less the camera's $A90C. */
uint16_t AboveCamera348(uint16_t d0)
{
    uint16_t d1 = (uint16_t)(0x348 - rd16(0xA90C));
    return (uint16_t)(d0 + d1);
}

/* $03F75E. The object's y: d0 plus $348 less the camera's $A90C. */
void SetYAboveCamera348(uint16_t obj, uint16_t d0)
{
    wr16(obj + O_Y, AboveCamera348(d0));
}

/* $040AC6. d1 less (the camera's y less $220). */
uint16_t BelowCamera220(uint16_t d1)
{
    uint16_t d0 = (uint16_t)(rd16(v_camY) - 0x220);
    d0 = (uint16_t)(d0 ^ 0xFFFF);
    d0 = (uint16_t)(d0 + 1);
    return (uint16_t)(d1 + d0);
}

/* ---- labels of other routines entered by jump (their code is run here) ---- */

/* loc_03CD80: a flame's flicker: its sprite word from the frame count (4 by 2 frames), touching
 * (+$20 $B0) on alternate pairs of frames */
static void flame_flicker(uint16_t obj)
{
    uint16_t d0 = rd16(v_gameFrames) & 6;
    wr16(obj + 0x0E, rom16(0x03CDAE + d0));
    if (bit8(v_gameFrames + 1, 1)) wr8(obj + 0x20, 0xB0);
    else wr8(obj + 0x20, 0);
}

/* loc_02AB78: knocked flying (type $114) */
static void become_knocked(uint16_t obj)
{
    wr16(obj + 0x58, 0);
    ObjectRelease(obj);
    wr16(obj + O_STATE, 0);
    wr16(obj + O_TYPE, 0x114);
    Knocked(obj);
}

/* loc_02CA0A: a soldier's jump (6 up less a random 0-1): its step $4C, 16 of its energy spent */
static void soldier_jump(uint16_t obj)
{
    wr16(obj + O_STATE, 0x4C);
    wr16(obj + 0x5C, (uint16_t)(rd16(obj + 0x5C) - 0x10));
    uint32_t d0 = (uint32_t)(seed() & 0xFF00);
    d0 += 0xFFFA0000u;
    wr32(obj + 0x1C, d0);
    wr16(obj + 0x48, 0);
    wr16(obj + 0x4A, 0);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF42C);
    wr8(obj + 0x5E, (uint8_t)((rd8(obj + 0x5E) & 0xF6) | 0x16));
    uint16_t d3 = 0, d4 = 0;
    ObjectCeiling(obj, 0xFFF0, &d3, &d4);
}

/* ---- type $388: type $380 put in the extra slot ($A800) ---- */

/* $031292. Type $388: makes the object at $A800 a $380 (set up there), and is removed. */
void Boss380Spawn(uint16_t obj)
{
    uint16_t a0 = 0xA800;
    wr16(a0 + O_TYPE, 0x380);
    wr16(a0 + O_STATE, 0);
    wr16(0x9C00, 0);
    wr16(obj + O_FLAGS, 0x1000);
    Boss380Setup(a0);
}

/* ---- type $384: something thrown up that falls back; low enough, a bomb (type $354) ---- */

/* $0312BC. Type $384: thrown up at a speed by difficulty (and a random fraction), falling at
 * 1/8; below $144 (on stage 8: within $10 above the object at $A800's y + $10), it becomes a
 * bomb ($354). */
void Lobbed(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0x8C00);
        wr8(obj + 0x21, 0x20);
        wr32(obj + 0x28, 0xFC04FC04);
        wr16(obj + 0x0E, 0x66A0);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
        uint16_t v = rom16(0x031308 + (uint32_t)(int16_t)rd16(v_difficulty));
        wr32(obj + 0x1C, (uint32_t)v << 16 | seed());
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (rd16(v_stage4) == 8) {
        uint16_t d0 = (uint16_t)(rd16(0xA848) + 0x10 - rd16(obj + O_Y));
        if ((int16_t)d0 >= 0) return;
        if ((int16_t)(d0 - 0xFFF0) >= 0) goto bomb;
    }
    if ((int16_t)(rd16(obj + O_Y) - 0x144) < 0) return;
bomb:
    wr16(obj + O_TYPE, 0x354);
    wr16(obj + O_STATE, 0);
}

/* ---- types $390, $394: flames that bob on a sine (or home on the player), leaving puffs ---- */

/* $031406. The flame's (or its puff's, a0) touch: +$21 $10, +$23 $20, its box, +$26 7. */
void FlameBox(uint16_t o)
{
    wr8(o + 0x21, 0x10);
    wr8(o + 0x23, 0x20);
    wr32(o + 0x2C, 0xFC04FC04);
    wr16(o + 0x26, 7);
}

/* loc_03135E: the flame's set-up (its y kept at +$4A as the sine's centre) */
static void flame_start(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x8800);
    wr16(obj + 0x4A, rd16(obj + O_Y));
    wr16(obj + 0x08, 0xA00);
    wr16(obj + 0x0A, 0xF4F4);
    wr16(obj + 0x56, 0);
    FlameBox(obj);
    flame_flicker(obj);
}

/* loc_0313C8: on even frames a puff (an animation list, type $100) where it is, touching too */
static void flame_puff(uint16_t obj)
{
    if (!bit8(v_gameFrames + 1, 0)) {
        uint16_t slot, t;
        if (SlotFreeHigh(&slot, &t)) {
            wr16(slot + O_TYPE, 0x100);
            wr32(slot + 0x48, 0x0567C2);
            wr16(slot + O_X, rd16(obj + O_X));
            wr16(slot + O_Y, rd16(obj + O_Y));
            FlameBox(slot);
        }
    }
    flame_flicker(obj);
}

/* $031356. Type $390: a flame bobbing on the sine (its phase +$56 stepped by +$48) about its
 * starting y; removed outside x $7C-$1C3. */
void FlameBob(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        flame_start(obj);
        return;
    }
    if ((int16_t)(rd16(obj + O_X) - 0x7C) < 0 || (int16_t)(rd16(obj + O_X) - 0x1C4) >= 0) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    uint16_t p = (uint16_t)((rd16(obj + 0x56) + rd16(obj + 0x48)) & 0x1FE);
    wr16(obj + 0x56, p);
    uint32_t s = (uint32_t)(int32_t)(int16_t)rom16(0x02977C + p) << 8;
    wr16(obj + O_Y, (uint16_t)((uint16_t)(s >> 16) + rd16(obj + 0x4A)));
    flame_puff(obj);
}

/* $031422. Type $394: a flame speeding up or down toward the nearer player (by difficulty). */
void FlameHoming(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        flame_start(obj);
        return;
    }
    wr16(obj + O_FLAGS, 0x8E00);
    uint16_t dx, dy, player;
    NearestPlayer(obj, &dx, &dy, &player);
    uint32_t d0 = (uint32_t)(int32_t)(int16_t)rom16(0x031466 + (uint32_t)(int16_t)rd16(v_difficulty));
    if (dy & 0x8000) wr32(obj + 0x1C, rd32(obj + 0x1C) - d0);
    else wr32(obj + 0x1C, rd32(obj + 0x1C) + d0);
    flame_puff(obj);
}

/* ---- type $37C: a soldier riding on another object (+$5C), shooting at the player (type
 * $90), that jumps off as a soldier ($48) left of x $140; or one dropped that falls and lands
 * ---- */

/* $03176A. Knocked onto the floor: step 4, still for 7 frames. */
void RiderSoldierDown(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0xCF00);
    look(obj, 0x0DFF96);
    wr16(obj + 0x48, 7);
    wr32(obj + 0x18, 0);
}

/* $031794. Thrown off: step 8, up at 3.5 and away from the way it faces at 0.75. */
void RiderSoldierFlung(uint16_t obj)
{
    wr16(obj + O_STATE, 8);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DFF6E);
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x2A);
    wr16(obj + 0x48, 0x40);
    wr16(obj + 0x4A, 0);
    wr32(obj + 0x1C, 0xFFFC8000);
    if (bit8(obj + 0x0E, 3)) wr32(obj + 0x18, 0xFFFF4000);
    else wr32(obj + 0x18, 0xC000);
}

/* $0317EE. Its aim: the shot type $90 (d7), the soldier's mappings by direction (a1) and
 * muzzles (a2), the player it goes after (returned, a0). */
uint16_t RiderSoldierAim(uint16_t obj, uint16_t *d7, uint32_t *a1, uint32_t *a2)
{
    wr16(obj + O_FLAGS, 0xCF00);
    *d7 = 0x90;
    *a1 = 0x02AD16;
    *a2 = 0x02AD56;
    return PlayerTarget();
}

/* $03180C. +$48: a wait by the table at $3182A ($A20C's entry) plus a random 0-$1F. */
void RiderSoldierWait(uint16_t obj)
{
    uint16_t d0 = rom16(0x03182A + (uint32_t)(int16_t)rd16(0xA20C));
    wr16(obj + 0x48, (uint16_t)(d0 + (seed() & 0x1F)));
}

/* $03183A. Its y: $E above its mount's (+$5C). Leaves the mount in a0. */
uint16_t RiderSoldierRide(uint16_t obj)
{
    uint16_t m = rd16(obj + 0x5C);
    wr16(obj + O_Y, (uint16_t)(rd16(m + O_Y) - 0xE));
    return m;
}

/* $031588. Steps 0, $14, $18, $1C: its kind's (+6) first step, vitality, sprite word, timer. */
void RiderSoldierStart(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEF00);
    uint16_t k = rd16(obj + 6);
    wr16(obj + O_STATE, rom16(0x0315D4 + (uint32_t)(int16_t)k));
    k = (uint16_t)(k + 2);
    wr16(obj + O_VITALITY, rom16(0x0315D4 + (uint32_t)(int16_t)k));
    k = (uint16_t)(k + 2);
    wr16(obj + 0x0E, rom16(0x0315D4 + (uint32_t)(int16_t)k));
    k = (uint16_t)(k + 2);
    wr16(obj + 0x48, rom16(0x0315D4 + (uint32_t)(int16_t)k));
    look(obj, 0x0DF4FC);
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x2A);
}

/* $0315E4. Steps 4, $10: down for +$48 frames, then its kind's first step again. */
void RiderSoldierLie(uint16_t obj)
{
    if (!dec(obj, 0x48)) return;
    wr16(obj + O_FLAGS, 0xEF00);
    wr16(obj + O_STATE, rom16(0x0315D4 + (uint32_t)(int16_t)rd16(obj + 6)));
    look(obj, 0x0DF4FC);
}

/* $0315EE. Step 8: flung, against walls and the ceiling, falling at 1/4; falling, step $C. */
void RiderSoldierFly(uint16_t obj)
{
    uint16_t d3 = 0, d4 = 0;
    ObjectWallsAny(obj, 0xFFF8, &d3, &d4);
    ObjectWallsAny8(obj, &d3, &d4);
    ObjectCeiling(obj, 0xFFF0, &d3, &d4);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    wr16(obj + 0x48, 0);
    wr16(obj + 0x4A, 0);
    wr16(obj + O_STATE, 0xC);
    wr16(obj + O_FLAGS, 0xEF00);
    look(obj, 0x0DF468);
}

/* $031616. Step $C: falling (at 1/4, to 6 at most); on the floor (after +$4A frames), step $10:
 * down for $F frames. */
void RiderSoldierFall(uint16_t obj)
{
    uint32_t d0 = rd32(obj + 0x1C);
    if ((d0 & 0x80000000u) || (int32_t)(d0 - 0x60000) < 0) wr32(obj + 0x1C, d0 + 0x4000);
    if (!dec(obj, 0x4A)) return;
    uint16_t d3 = 0, d4 = 0;
    if (ObjectFloor16(obj, &d3, &d4)) return;
    wr16(obj + O_STATE, 0x10);
    wr16(obj + 0x48, 0xF);
    look(obj, 0x0DF81A);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x28, 0x10F808);
    wr8(obj + 0x21, 0x2A);
}

/* $031676. Step $20: riding, a wait set, facing the player. */
void RiderSoldierAimStart(uint16_t obj)
{
    RiderSoldierRide(obj);
    next(obj, 4);
    RiderSoldierWait(obj);
    uint16_t d7, d0, d5, d6;
    uint32_t a1, a2;
    uint16_t target = RiderSoldierAim(obj, &d7, &a1, &a2);
    FaceToward(obj, target, a1, &d0, &d5, &d6, &d7);
}

/* $03168C. Step $24: riding, facing the player until the wait is out; then a shot (type $90)
 * and step $28 for $10 frames. */
void RiderSoldierAiming(uint16_t obj)
{
    RiderSoldierRide(obj);
    uint16_t d7, d0, d5, d6;
    uint32_t a1, a2;
    uint16_t target = RiderSoldierAim(obj, &d7, &a1, &a2);
    if (!dec(obj, 0x48)) {
        FaceToward(obj, target, a1, &d0, &d5, &d6, &d7);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x48, 0x10);
    AimedSpawn(obj, target, a1, a2, d7);
}

/* $0316B2. Step $28: riding until the $10 frames are out, then aiming again (step $20). */
void RiderSoldierShot(uint16_t obj)
{
    RiderSoldierRide(obj);
    if (!dec(obj, 0x48)) return;
    wr16(obj + O_STATE, 0x20);
}

/* $0316C6. Step $2C: not touching, its look. */
void RiderSoldierDropStart(uint16_t obj)
{
    next(obj, 4);
    wr8(obj + 0x21, 0);
    look(obj, 0x0DFC34);
}

/* $0316E0. Step $30: at y $118 (by the camera), dropping dust (sub $34A28); left of x $140 a
 * soldier ($48, kind $28) jumping. */
void RiderSoldierDrop(uint16_t obj)
{
    wr16(obj + O_Y, BelowCamera220(0x118));
    TrailDust(obj);
    if ((int16_t)(rd16(obj + O_X) - 0x140) >= 0) return;
    wr16(obj + O_TYPE, 0x48);
    wr16(obj + O_STATE, 0);
    wr16(obj + 6, 0x28);
    SoldierS00(obj);
    soldier_jump(obj);
}

/* $031488. Type $37C: its touch by its clock (+$50); beaten, points and a dying soldier (or
 * thrown if a player held it); hit by a player's ... (+$22 bit 1) by one of the players,
 * knocked flying (points from player 2's); grabbed (+$22 bit 5), down or flung; then its step
 * (the table at $31554). */
void RiderSoldier(uint16_t obj)
{
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    uint16_t d0 = (uint16_t)(((uint16_t)(rd16(obj + 0x50) << 2) & 0xC) + 0xA0);
    wr8(obj + 0x20, (uint8_t)d0);
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x180);
        if (!bit8(obj + 0x23, 4)) {
            SoldierDying(obj);
            return;
        }
        wr16(obj + O_STATE, 0);
        wr16(obj + 0x58, 0);
        wr16(obj + O_TYPE, 0x110);
        Thrown(obj);
        return;
    }
    if (bit8(obj + 0x22, 1)) {
        clr8(obj + 0x22, 1);
        d0 = rd16(obj + 0x42);
        if (d0) {
            if (d0 == 0xA400) {
                become_knocked(obj);
                return;
            }
            if (d0 == 0xA600) {
                AddScore(obj, 0x180);
                become_knocked(obj);
                return;
            }
        }
    }
    if (bit8(obj + 0x22, 5)) {
        uint16_t d3 = 0, d4 = 0;
        if (!bit8(obj + 0x23, 4) && !ObjectFloor16(obj, &d3, &d4)) {
            if (rd16(obj + O_STATE) != 4) RiderSoldierDown(obj);
        } else {
            clr8(obj + 0x23, 4);
            RiderSoldierFlung(obj);
        }
    }
    call_object(rom32(0x031554 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $034A28. Every 16th frame a puff of dust (type $1B0) behind the object, drifting right and
 * up. */
void TrailDust(uint16_t obj)
{
    if (rd16(v_gameFrames) & 0xF) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + 0x0E, 0x7D8);
    wr16(slot + O_TYPE, 0x1B0);
    wr16(slot + O_STATE, 1);
    wr16(slot + O_FLAGS, 0x8C00);
    wr16(slot + 0x08, 0);
    wr16(slot + 0x0A, 0xFCFC);
    wr16(slot + 0x48, 0x10);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x18, 0x8000);
    wr32(slot + 0x1C, 0xFFFF6000);
    wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + 8));
}

/* ---- type $38C: a figure in the background, carried along with the scenery ($E402, $A9D0),
 * that changes when the boss's timer runs and goes $100 frames later ---- */

/* $031924. Its x: from $E402 (less $ECA0) plus its kind's offset (the table at $31940). */
void OnlookerPlace(uint16_t obj)
{
    uint16_t d1 = (uint16_t)(rd16(0xE402) - 0xECA0);
    d1 = (uint16_t)(d1 + rom16(0x031940 + (uint32_t)(int16_t)rd16(obj + 6)));
    wr16(obj + O_X, d1);
}

/* $03184C. Type $38C. Its +$20 cycles with the frame count (by its slot); its y $18 by the
 * camera. */
void Onlooker(uint16_t obj)
{
    uint16_t d0 = (uint16_t)((int16_t)obj >> 3);
    uint16_t d1 = (uint16_t)(rd16(v_gameFrames) << 2);
    d0 = (uint16_t)(((uint16_t)(d0 + d1) & 0xC) + 0xB0);
    wr8(obj + 0x20, (uint8_t)d0);
    SetYAboveCamera348(obj, 0x18);
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_Y, 0);
        wr8(obj + 0x20, 0xBC);
        wr16(obj + O_FLAGS, 0xE800);
        wr32(obj + 0x08, rom32(0x0318BE + (uint32_t)(int16_t)(uint16_t)(rd16(obj + 6) << 1)));
        wr16(obj + 0x0C, 0);
        wr16(obj + 0x0E, (obj & 0x20) ? 0x300 : 0xB00);
        return;
    }
    if (rd16(0xE402)) {
        OnlookerPlace(obj);
        wr32(obj + 0x18, rd32(0xA9D0) ^ 0xFFFFFFFFu);
    }
    if (rd16(obj + O_STATE) == 8) {
        if (!dec(obj, 0x48)) return;
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    if (rd16(v_bossTimer) & 0x8000) return;
    next(obj, 4);
    look(obj, 0x0DFC4E);
    wr16(obj + 0x48, 0x100);
}

/* ---- type $194: for $630 frames a $198 at $CE00 (every $100 frames) and $19C at random; then
 * $C0 frames (while no $198 is at $CE00) and the stage's event 1, the objects from $C980
 * vanished ---- */

/* $0319C8. The spawning: a $198 at $CE00 every $100 frames (if the slot is free); a $19C every
 * +$54 frames (by the table at $31A16, $A20C's entry, plus a random 0-$1F). */
void WaveSpawn(uint16_t obj)
{
    if (dec(obj, 0x56)) {
        wr16(obj + 0x56, 0x100);
        if (!rd16(0xCE00)) wr16(0xCE00, 0x198);
    }
    if (!dec(obj, 0x54)) return;
    wr16(obj + 0x54, rom16(0x031A16 + (uint32_t)(int16_t)rd16(0xA20C)));
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + (seed() & 0x1F)));
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x19C);
    wr16(slot + O_STATE, 0);
}

/* $031994. Step 8: $8044 set; once no $198 is at $CE00 and the $C0 frames are out, the stage's
 * event 1, $808A $8000, the objects from $C980 vanished. */
void WaveEnd(uint16_t obj)
{
    wr16(0x8044, 1);
    if (rd16(0xCE00) == 0x198) return;
    if (!dec(obj, 0x4C)) return;
    wr16(v_stageEvent, 1);
    wr16(0x808A, 0x8000);
    ObjectsVanishFrom(0xC980, 0, 0);
}

/* $03197E. Step 4: spawning for $630 frames, then step 8. */
void WaveSpawning(uint16_t obj)
{
    WaveSpawn(obj);
    if (!dec(obj, 0x4A)) return;
    next(obj, 4);
    wr16(obj + 0x4C, 0xC0);
    WaveEnd(obj);
}

/* $03195E. Step 0: the stage's colours, the timers. */
void WaveStart(uint16_t obj)
{
    StageColours();
    next(obj, 4);
    wr16(obj + 0x4A, 0x630);
    wr16(obj + 0x54, 0x80);
    wr16(obj + 0x56, 0x80);
    wr16(0x8044, 0);
    WaveSpawning(obj);
}

/* $031948. Type $194. */
void Wave(uint16_t obj)
{
    call_object(rom32(0x031952 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $031A26. Type $190: nothing. */
void Idle190(uint16_t obj)
{
    (void)obj;
}

/* ---- type $198: a craft that flies up and away, comes back and swoops up and down dropping
 * $1A0s, and leaves when $8044 is set ---- */

/* loc_031E32: removed once $8044 is set */
static void dropship_check_end(uint16_t obj)
{
    if (rd16(0x8044)) wr16(obj + O_FLAGS, 0x1000);
}

/* loc_031CFC: speeding down-right (both speeds +$4C) to 1 at most, by 1/16 */
static void dropship_down(uint16_t obj)
{
    if (bit8(obj + 0x4C, 7) || (int32_t)(rd32(obj + 0x4C) - 0x10000) < 0)
        wr32(obj + 0x4C, rd32(obj + 0x4C) + 0x1000);
    wr32(obj + 0x18, rd32(obj + 0x4C));
    wr32(obj + 0x1C, rd32(obj + 0x4C));
}

/* loc_031D28: speeding up-left to -2 at most, by 1/8 */
static void dropship_up(uint16_t obj)
{
    if (!bit8(obj + 0x4C, 7) || (int32_t)(rd32(obj + 0x4C) - 0xFFFE0000u) >= 0)
        wr32(obj + 0x4C, rd32(obj + 0x4C) - 0x2000);
    wr32(obj + 0x18, rd32(obj + 0x4C));
    wr32(obj + 0x1C, rd32(obj + 0x4C));
}

/* loc_031D88: falling out of x $80-$1BF or y $80-$17F: back to step 0, not touching */
static void dropship_bounds(uint16_t obj)
{
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    if ((int16_t)(x - 0x1C0) >= 0 || (int16_t)(x - 0x80) < 0 || (int16_t)(y - 0x180) >= 0
        || (int16_t)(y - 0x80) < 0) {
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
    }
}

/* $031DBC. Every $80th frame a $1A0 dropped from below it. */
void DropshipDrop(uint16_t obj)
{
    if (rd16(v_gameFrames) & 0x7F) return;
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1A0);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + 0x48, 0x10);
    wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + 0x18));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + 8));
}

/* $031DF6. On even frames a puff of exhaust (an animation list, type $100) 4-11 below it. */
void DropshipExhaust(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x056580);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + (seed() & 7) + 4));
}

/* $031A9E. Step 0: at ($160, $186), still for $100 frames, vitality $190. */
void DropshipStart(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x0E, 0x2300);
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0DF9C6);
    wr8(obj + 0x20, 0x90);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x21, 0);
    wr16(obj + O_VITALITY, 0x190);
    wr16(obj + 0x26, 0xA);
    wr16(obj + O_X, 0x160);
    wr16(obj + O_Y, 0x186);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x48, 0x100);
    dropship_check_end(obj);
}

/* $031AEE. Step 4: the wait; then up and left at 2. */
void DropshipWait(uint16_t obj)
{
    if (dec(obj, 0x48)) {
        next(obj, 4);
        wr32(obj + 0x1C, 0xFFFE0000);
        wr32(obj + 0x18, 0xFFFE0000);
    }
    dropship_check_end(obj);
}

/* $031B0E. Step 8: rising to y $100, then its next look. */
void DropshipRise(uint16_t obj)
{
    DropshipExhaust(obj);
    if ((int16_t)(rd16(obj + O_Y) - 0x100) >= 0) return;
    next(obj, 4);
    look(obj, 0x0DFB8E);
}

/* $031B30. Step $C: on to x $80; then gone above (y 0) for $20 frames. */
void DropshipClimb(uint16_t obj)
{
    DropshipExhaust(obj);
    if ((int16_t)(rd16(obj + O_X) - 0x80) >= 0) return;
    next(obj, 4);
    look(obj, 0x0DFAD0);
    wr16(obj + 0x48, 0x20);
    wr16(obj + O_Y, 0);
}

/* $031B5E. Step $10: then back at ($80, $60), touching, swooping down at 1. */
void DropshipAway(uint16_t obj)
{
    if (dec(obj, 0x48)) {
        next(obj, 4);
        wr16(obj + O_X, 0x80);
        wr16(obj + O_Y, 0x60);
        wr32(obj + 0x4C, 0x10000);
        wr16(obj + 0x48, 0x10);
        wr32(obj + 0x28, 0xF410F808);
        wr8(obj + 0x21, 0x2A);
    }
    dropship_check_end(obj);
}

/* $031B96. Step $14: swooping down (dropping $1A0s); below y $B0 after the wait, step $18.
 * $8044 set: leaving (step $28). */
void DropshipSwoopDown(uint16_t obj)
{
    if (rd16(0x8044)) {
        wr16(obj + O_STATE, 0x28);
        return;
    }
    DropshipExhaust(obj);
    DropshipDrop(obj);
    if ((int16_t)(rd16(obj + O_Y) - 0xB0) < 0 || !dec(obj, 0x48)) {
        dropship_down(obj);
        return;
    }
    next(obj, 4);
}

/* $031BC6. Step $18: swooping up; above y $B0, down again after $10-$2F frames. */
void DropshipSwoopUp(uint16_t obj)
{
    if (rd16(0x8044)) {
        wr16(obj + O_STATE, 0x28);
        return;
    }
    DropshipExhaust(obj);
    DropshipDrop(obj);
    if ((int16_t)(rd16(obj + O_Y) - 0xB0) >= 0) {
        dropship_up(obj);
        return;
    }
    next(obj, (uint16_t)-4);
    wr16(obj + 0x48, (uint16_t)((seed() & 0x1F) + 0x10));
}

/* $031BFE. Step $1C: hit; held by a player (+$23 bit 4), thrown (step $20, hurting); else after
 * +$48 frames back to step 0. */
void DropshipHit(uint16_t obj)
{
    if (bit8(obj + 0x23, 4)) {
        next(obj, 4);
        wr32(obj + 0x2C, 0xF808F808);
        wr8(obj + 0x21, 0x40);
        wr8(obj + 0x23, 0x20);
        return;
    }
    if (dec(obj, 0x48)) wr16(obj + O_STATE, 0);
}

/* $031C32. Step $20: thrown, bouncing up at 6 off the level, slowing to -6 across; out of the
 * room, step 0. */
void DropshipThrown(uint16_t obj)
{
    if (!bit8(obj + 0x1C, 7) && MapAttribute(rd16(obj + O_X), rd16(obj + O_Y))) {
        wr32(obj + 0x1C, 0xFFFA0000);
        DropshipExhaust(obj);
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if ((int16_t)(rd16(obj + 0x18) - 0xFFFA) >= 0) wr32(obj + 0x18, rd32(obj + 0x18) - 0x1000);
    dropship_bounds(obj);
}

/* $031C78. Step $24: destroyed: puffs about it on even frames for $40 frames, then step 0. */
void DropshipBurning(uint16_t obj)
{
    if (dec(obj, 0x48)) {
        wr16(obj + O_STATE, 0);
        return;
    }
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0563EE);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + (uint16_t)((rd8(v_randomSeed) & 0x1F) - 0x10)));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + (uint16_t)((seed() & 0x1F) - 0x10)));
    wr16(slot + 0x18, 4);
    wr16(slot + 0x1C, 4);
}

/* $031CE2. Step $28: leaving up-left; above y $80, removed. */
void DropshipLeave(uint16_t obj)
{
    DropshipExhaust(obj);
    DropshipDrop(obj);
    if ((int16_t)(rd16(obj + O_Y) - 0x80) >= 0) {
        dropship_up(obj);
        return;
    }
    wr16(obj + O_FLAGS, 0x1000);
}

/* $031A28. Type $198. Hit by a player (+$22 bit 1): points, step $1C; beaten: points, a sound,
 * burning (step $24); then its step (the table at $31A72). */
void Dropship(uint16_t obj)
{
    if (bit8(obj + 0x22, 1)) {
        AddScore(obj, 0x520);
        wr16(obj + O_STATE, 0x1C);
        look(obj, 0x0DF4A4);
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        wr16(obj + 0x48, 0x10);
        return;
    }
    if (rd16(obj + O_VITALITY) & 0x8000) {
        wr16(obj + O_STATE, 0x24);
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        wr16(obj + O_FLAGS, 0);
        wr16(obj + 0x48, 0x40);
        AddScore(obj, 0x520);
        QueueSound(0xBE);
    }
    call_object(rom32(0x031A72 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $19C: a creature that hops right, bouncing off the level ---- */

/* $031E76. Step 0: at ($B0, $70), moving right at 1.5; its bounce (+$48) one of four. */
void LeaperStart(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x0E, 0x300);
    wr16(obj + O_FLAGS, 0xEC00);
    look(obj, 0x0DF4A4);
    wr8(obj + 0x20, 0x90);
    wr16(obj + O_VITALITY, 4);
    wr16(obj + 0x26, 2);
    wr16(obj + O_X, 0xB0);
    wr16(obj + O_Y, 0x70);
    wr32(obj + 0x18, 0x18000);
    wr32(obj + 0x48, rom32(0x031EE4 + (seed() & 0xC)));
    wr32(obj + 0x2C, 0xF807F808);
    wr32(obj + 0x28, 0xF40BF40C);
    wr8(obj + 0x21, 0x30);
}

/* $031EF4. Step 4: falling at 1/8, bouncing off the level; past x $1B8 removed. */
void LeaperHop(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x1B8) >= 0) {
        set8(obj + O_FLAGS, 4);
        return;
    }
    if (!bit8(obj + 0x1C, 7) && MapAttribute(rd16(obj + O_X), (uint16_t)(rd16(obj + O_Y) + 4))) {
        wr16(obj + 0x1C, rd16(obj + 0x48));
        wr16(obj + 0x1E, 0);
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2000);
}

/* $031E42. Type $19C: beaten, a $54 (3 points); else its step. */
void Leaper(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        wr16(obj + O_TYPE, 0x54);
        wr16(obj + O_STATE, 0);
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        AddScore(obj, 3);
        return;
    }
    call_object(rom32(0x031E6E + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- the hover platforms: types $1B8 (a carrier), $1BC (its escorts), $1C0 (soldiers on
 * platforms), $2C0 (their leader); each rides a platform (type $10, linked by +$58) ---- */

#define MOUNT 0x58               /* the platform it rides */

/* $032C1A. Its platform (type $10, from the high slots, linked by +$58) and its look (the
 * table at $32C78, entry d7); step 4. No slot: removed. */
void HoverMount(uint16_t obj, uint16_t which)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr16(obj + MOUNT, slot);
    wr16(slot + O_FLAGS, 0xE100);
    wr16(slot + 0x0E, 0xED6);
    wr32(slot + 0x08, 0x0E06E4);
    wr16(slot + O_TYPE, 0x10);
    wr8(slot + 0x20, 0x74);
    wr32(obj + 0x08, rom32(0x032C78 + (uint32_t)(int16_t)which));
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x0E, 0x300);
    wr16(obj + O_FLAGS, 0xCD00);
    wr32(obj + 0x28, 0xF40EF808);
    wr8(obj + 0x20, 0x70);
    next(obj, 4);
}

/* $032C96. Its platform placed at (x less d2 (mirrored as it faces), y plus d3). */
void HoverMountAt(uint16_t obj, uint16_t dx, uint16_t dy)
{
    if (bit8(obj + 0x0E, 3)) dx = (uint16_t)((dx ^ 0xFFFF) + 1);
    uint16_t m = rd16(obj + MOUNT);
    wr16(m + O_X, (uint16_t)(rd16(obj + O_X) - dx));
    wr16(m + O_Y, (uint16_t)(rd16(obj + O_Y) + dy));
}

/* $032C90. Its platform 8 behind, $10 above. */
void HoverMountPlace(uint16_t obj)
{
    HoverMountAt(obj, 8, 0xFFF0);
}

/* $032CC0. On even frames a puff (an animation list, type $100) 8 below it, drifting left or
 * right by the frame. */
void HoverPuff(uint16_t obj)
{
    uint16_t d0 = rd16(v_gameFrames);
    if (d0 & 1) return;
    uint32_t d1 = (d0 & 2) ? 0xC000 : 0xFFFF4000u;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x05652E);
    wr32(slot + 0x18, d1);
    wr16(slot + 0x1C, 0);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, (uint16_t)(8 + rd16(obj + O_Y)));
}

/* $032D1A. On even frames a spark (type $2C4) from its platform, down at a random angle. */
void HoverSparks(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x2C4);
    uint16_t m = rd16(obj + MOUNT);
    wr16(slot + O_X, rd16(m + O_X));
    wr16(slot + O_Y, rd16(m + O_Y));
    uint16_t d0 = (uint16_t)((seed() & 0x3E) + 0x64);
    uint32_t d1 = (uint32_t)(int32_t)(int16_t)rom16(0x02977C + d0);
    uint32_t d2 = (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + d0);
    wr32(slot + 0x1C, d1 << 4);
    wr32(slot + 0x18, d2 << 4);
    wr16(slot + 0x48, 0x18);
}

/* $032E12. Every 16th frame it and its platform face left right of x $120, else right. */
void HoverFace(uint16_t obj)
{
    if (rd16(v_gameFrames) & 0xF) return;
    uint16_t m = rd16(obj + MOUNT);
    if ((int16_t)(rd16(obj + O_X) - 0x120) >= 0) {
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
        wr16(m + 0x0E, rd16(m + 0x0E) | 0x800);
        return;
    }
    wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
    wr16(m + 0x0E, rd16(m + 0x0E) & 0xF7FF);
}

/* $032E48. Every 16th count of +$50 a new point to fly to (+$48, +$4A): x $A0 + random, y $C0. */
void HoverPick(uint16_t obj)
{
    if (rd16(obj + 0x50) & 0xF) return;
    Random();
    wr16(obj + 0x48, (uint16_t)(rd8(v_randomSeed) + 0xA0));
    wr16(obj + 0x4A, 0xC0);
}

/* $032E72. Flying on its heading (+$56: x at 2x, y at 1x the sine's), the heading turned by 8
 * toward the point (+$48, +$4A). */
void HoverSteer(uint16_t obj)
{
    uint16_t h = rd16(obj + 0x56);
    uint32_t d1 = (uint32_t)(int32_t)(int16_t)rom16(0x02977C + h);
    uint32_t d2 = (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + h);
    wr32(obj + 0x1C, d1 << 3);
    wr32(obj + 0x18, d2 << 4);
    uint16_t dx = (uint16_t)(rd16(obj + 0x48) - rd16(obj + O_X));
    d1 = (d1 << 3 & 0xFFFF0000u) | (uint16_t)(rd16(obj + 0x4A) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)dx, &d1);
    a = (uint16_t)(((int16_t)a >> 7) & 0x1FE);
    a = (uint16_t)(a - rd16(obj + 0x56));
    int up;
    if (a & 0x8000) up = (int16_t)(a - 0xFF00) < 0;
    else up = (int16_t)(a - 0x100) < 0;
    if (up) wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) + 8) & 0x1FE));
    else wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) - 8) & 0x1FE));
}

/* $032F32. Facing its player, keeping its flip (d7 1). */
void HoverFacePlayer(uint16_t obj)
{
    uint16_t target = PlayerBySlot(obj);
    uint16_t d0, d5, d6, d7 = 1;
    FaceToward(obj, target, 0x02AD66, &d0, &d5, &d6, &d7);
}

/* $032F4C. Drifting: across slowed by 1/8 (to 0 within 1/16); up and down, turning at -2 and
 * 2.25 (+$52 the way). */
void HoverDrift(uint16_t obj)
{
    uint32_t vx = rd32(obj + 0x18);
    if (!bit8(obj + 0x18, 7)) {
        if ((int32_t)(vx - 0x1000) < 0) wr32(obj + 0x18, 0);
        else wr32(obj + 0x18, vx - 0x2000);
    } else {
        if ((int32_t)(vx - 0xFFFFF000u) >= 0) wr32(obj + 0x18, 0);
        else wr32(obj + 0x18, vx + 0x2000);
    }
    if (!rd16(obj + 0x52)) {
        wr32(obj + 0x1C, rd32(obj + 0x1C) + 0xFFFFC000u);
        if ((int32_t)(rd32(obj + 0x1C) - 0xFFFE0000u) >= 0) return;
        wr16(obj + 0x52, 2);
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if ((int32_t)(rd32(obj + 0x1C) - 0x24000) < 0) return;
    wr16(obj + 0x52, 0);
}

/* $032FCE. Every 8th frame its engine's sound ($AF). */
void HoverHum(void)
{
    if (rd16(v_gameFrames) & 7) return;
    QueueSound(0xAF);
}

/* $032FE6. +$48: one of four waits. */
void HoverWait(uint16_t obj)
{
    wr16(obj + 0x48, rom16(0x032FFC + (seed() & 6)));
}

/* ---- type $1B8: the carrier ---- */

/* $032438. Step 0: the camera held, $8080 cleared, $100 further right; its platform; six
 * escorts ($1BC) about it (their waits $20, $30 ...), then $300 frames. */
void HaulerStart(uint16_t obj)
{
    set8(v_cameraHold, 0);
    wr16(0x8080, 0);
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + 0x100));
    HoverMount(obj, 4);
    wr16(obj + 0x0E, 0x2B00);
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x2000);
    uint16_t d4 = 0, d5 = 0x20;
    for (int k = 0; k < 6; k++) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x1BC);
        wr16(slot + 0x48, d5);
        wr16(slot + O_X, (uint16_t)(rom16(0x0324B0 + d4) + rd16(obj + O_X)));
        wr16(slot + O_Y, (uint16_t)(rom16(0x0324BC + d4) + rd16(obj + O_Y)));
        d4 = (uint16_t)(d4 + 2);
        d5 = (uint16_t)(d5 + 0x10);
    }
    wr16(obj + 0x48, 0x300);
}

/* $0324C8. Step 4: its platform under it; from camera x $CA0, rising (step 8). */
void HaulerWait(uint16_t obj)
{
    HoverMountAt(obj, 4, 0xFFF0);
    if ((int16_t)(rd16(v_camX) - 0xCA0) < 0) return;
    next(obj, 4);
    wr16(obj + 0x1A, 0x2000);
}

/* $0324E8. Step 8: rising faster to 4, puffing; then still; above y $80 its platform removed,
 * $80C2 $60, step $C. */
void CarrierRise(uint16_t obj)
{
    HoverMountAt(obj, 8, 0xFFF0);
    if ((int32_t)(rd32(obj + 0x1C) - 0xFFFC0000u) >= 0) {
        wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x2000);
        if ((int32_t)(rd32(obj + 0x1C) - 0xFFFF8000u) < 0) {
            wr16(obj + 0x0C, 0);
            wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0xDFFF);
            wr32(obj + 0x08, 0x0DF9D2);
        } else {
            HoverPuff(obj);
        }
    }
    if ((int16_t)(rd16(obj + O_Y) - 0x80) >= 0) return;
    wr16(rd16(obj + MOUNT) + O_FLAGS, 0x1000);
    next(obj, 4);
    wr16(0x80C2, 0x60);
}

/* $03254E. Step $C: until the camera's y is above $6C0. */
void CarrierClimb(uint16_t obj)
{
    if ((int16_t)(rd16(v_camY) - 0x6C0) >= 0) return;
    next(obj, 4);
}

/* $03255E. Step $10: every $40th frame a hover soldier ($1C0, a slot from $CC80); above camera
 * y $328 the boss's timer 8, step $14. */
void CarrierTroops(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 0x3F)) {
        uint16_t slot = 0xCC80, t;
        if (SlotFreeBelowCE(&slot, &t)) wr16(slot + O_TYPE, 0x1C0);
        return;
    }
    if ((int16_t)(rd16(v_camY) - 0x328) >= 0) return;
    wr16(v_bossTimer, 8);
    next(obj, 4);
}

/* $032594. Step $14: once the camera is let go, the job list at $325C6 and the boss (type $18
 * at $C980). */
void CarrierBoss(uint16_t obj)
{
    if (!bit8(v_cameraHold, 7)) return;
    next(obj, 4);
    uint32_t list = 0x0325C6;
    NextJob(&list);
    wr16(v_boss, 0x18);
}

/* $0325BE. Step $18: removed. */
void CarrierEnd(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x1000);
}

/* $032412. Type $1B8. */
void Hauler(uint16_t obj)
{
    call_object(rom32(0x03241C + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $1BC: the carrier's escorts ---- */

/* $0325FA. Step 0: not touching, its platform. */
void EscortStart(uint16_t obj)
{
    wr8(obj + 0x21, 0);
    HoverMount(obj, 0);
    wr16(obj + 0x0E, 0xB00);
}

/* $03260E. Step 4: its platform under it; from camera x $CA0, after its wait, rising. */
void EscortWait(uint16_t obj)
{
    HoverMountAt(obj, 4, 0xFFF8);
    uint16_t m = rd16(obj + MOUNT);
    wr16(m + O_X, (uint16_t)(rd16(obj + O_X) - 4));
    wr16(m + O_Y, (uint16_t)(rd16(obj + O_Y) - 8));
    if ((int16_t)(rd16(v_camX) - 0xCA0) < 0) return;
    if (!dec(obj, 0x48)) return;
    next(obj, 4);
    wr16(obj + 0x1A, 0x2000);
}

/* $032650. Step 8: rising faster to 4, puffing; above y $80 removed with its platform. */
void EscortRise(uint16_t obj)
{
    HoverMountAt(obj, 8, 0xFFF0);
    if ((int32_t)(rd32(obj + 0x1C) - 0xFFFC0000u) >= 0) {
        wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x1000);
        if ((int32_t)(rd32(obj + 0x1C) - 0xFFFF8000u) < 0) wr32(obj + 0x08, 0x0DF9D2);
        else HoverPuff(obj);
    }
    if ((int16_t)(rd16(obj + O_Y) - 0x80) >= 0) return;
    wr16(obj + O_FLAGS, 0x1000);
    wr16(rd16(obj + MOUNT) + O_FLAGS, 0x1000);
}

/* $0325D8. Type $1BC: its engine's sound (not after a checkpoint), its step. */
void Escort(uint16_t obj)
{
    if (!rd16(v_checkpoint)) HoverHum();
    call_object(rom32(0x0325EE + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $1C0: a soldier on a hover platform, flying about and shooting (type $91) ---- */

/* $032716. Step 0: its platform; at a random x, y $80; vitality $20. */
void HoverSoldierStart(uint16_t obj)
{
    HoverMount(obj, 8);
    if (rd16(obj + O_FLAGS) == 0x1000) return;
    wr8(obj + 0x21, 0x22);
    wr16(obj + O_VITALITY, 0x20);
    wr16(obj + O_X, (uint16_t)((seed() & 0x1FF) - 0x60));
    wr16(obj + O_Y, 0x80);
    wr16(obj + 0x50, 0x1E0);
    wr16(obj + 0x54, 0x80);
    wr16(obj + 0x56, 0x80);
}

/* $03275C. Step 4: flying to its points, facing its player, $80 frames; then step 8. */
void HoverSoldierFly(uint16_t obj)
{
    HoverFace(obj);
    HoverPick(obj);
    HoverSteer(obj);
    HoverMountPlace(obj);
    HoverFacePlayer(obj);
    if (!dec(obj, 0x54)) return;
    next(obj, 4);
    wr16(obj + 0x54, 0x20);
}

/* $032784. Step 8: drifting $20 frames, facing its player; then step $C. */
void HoverSoldierDrift(uint16_t obj)
{
    if (dec(obj, 0x54)) {
        next(obj, 4);
        wr16(obj + 0x54, 0x20);
    }
    HoverFace(obj);
    HoverDrift(obj);
    HoverMountPlace(obj);
    HoverFacePlayer(obj);
}

/* $0327A8. Step $C: drifting $20 frames, shooting at its player (type $91) when the frame
 * count's bits (by $A20C's entry of the table at $32F22) clear; then flying again (step 4,
 * $40 frames). Facing its player between shots, FaceToward keeps the flip as d7's bit 0 says:
 * d7 is what the previous object's routine left (the 68k sets it nowhere on this path); the C
 * takes it as 0. */
void HoverSoldierShoot(uint16_t obj)
{
    if (dec(obj, 0x54)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + 0x54, 0x40);
    }
    HoverFace(obj);
    HoverDrift(obj);
    HoverMountPlace(obj);
    uint16_t target = PlayerBySlot(obj);
    uint16_t d0 = (uint16_t)(rom16(0x032F22 + (uint32_t)(int16_t)rd16(0xA20C)) & rd16(obj + 0x54));
    if (!d0) {
        AimedSpawn(obj, target, 0x02AD66, 0x02ADA6, 0x91);
        return;
    }
    uint16_t d5, d6, d7 = 0;
    FaceToward(obj, target, 0x02AD66, &d0, &d5, &d6, &d7);
}

/* $0327CE. Step $10: leaving up; above y $80 removed with its platform. */
void HoverSoldierLeave(uint16_t obj)
{
    HoverFace(obj);
    HoverMountPlace(obj);
    if ((int16_t)(rd16(obj + O_Y) - 0x80) >= 0) return;
    wr16(obj + O_FLAGS, 0x1000);
    wr16(rd16(obj + MOUNT) + O_FLAGS, 0x1000);
}

/* loc_032D7A: beaten: points; by the frame count the soldier falls ($74; its platform a $70),
 * dies ($7C; its platform removed) or drops as a soldier ($48, kind $28; its platform a $8C) */
static void hover_soldier_beaten(uint16_t obj)
{
    AddScore(obj, 0x240);
    uint16_t m = rd16(obj + MOUNT);
    uint16_t d0 = rd16(v_gameFrames) & 3;
    if ((int16_t)(d0 - 2) < 0) {
        wr16(m + O_TYPE, 0x8C);
        wr16(m + 0x48, 0x60);
        wr16(m + 0x4A, 4);
        wr16(m + 0x4C, 0xC);
        wr16(obj + O_TYPE, 0x48);
        wr16(obj + O_STATE, 0);
        for (uint16_t f = 0x48; f < 0x60; f = (uint16_t)(f + 4)) wr32(obj + f, 0);
        wr16(obj + 6, 0x28);
        SoldierS00(obj);
        return;
    }
    if (d0 == 2) {
        wr16(m + O_FLAGS, 0x1000);
        wr16(obj + O_TYPE, 0x7C);
        wr16(obj + O_STATE, 0);
        return;
    }
    wr16(m + O_TYPE, 0x70);
    wr16(obj + O_TYPE, 0x74);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
}

/* $0326A6. Type $1C0: its engine's sound; beaten (above); hit by a player (+$22 bit 1),
 * knocked off ($114, its platform removed); shown, it leaves (step $10, up at 5) when its clock
 * (+$50) runs out or the boss's timer runs; then its step (the table at $32702). */
void HoverSoldier(uint16_t obj)
{
    HoverHum();
    if (rd16(obj + O_VITALITY) & 0x8000) {
        hover_soldier_beaten(obj);
        return;
    }
    if (bit8(obj + 0x22, 1)) {
        clr8(obj + 0x22, 1);
        wr16(rd16(obj + MOUNT) + O_FLAGS, 0x1000);
        become_knocked(obj);
        return;
    }
    if (bit8(obj + O_FLAGS, 7)) {
        if (dec(obj, 0x50) || !(rd16(v_bossTimer) & 0x8000)) {
            wr16(obj + O_STATE, 0x10);
            wr16(obj + 0x1C, 0xFFFB);
        }
    }
    call_object(rom32(0x032702 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

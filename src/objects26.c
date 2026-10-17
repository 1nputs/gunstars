/* Object types no recorded run reaches (their code all `;!` in the disassembly):
 * - type $24 ($264DE): the boss of seven forms on the mine's rails (Seven Force, it seems): its
 *   entrance and change of form ($00-$5C), its seven forms ($60-$1CC), each a skeleton of joints
 *   at $9000 (four bytes each: the angle in the high byte, its step per frame in the next word)
 *   turned toward the poses its lists name, the segments of type $28 (OrbiterSetup) hanging on
 *   it; type $2C ($271FE) the chain of form B; type $610 ($27848) form A's figure brought back;
 * - type $C0 ($38464): a hopper chasing the player; types $E4, $E8, $EC ($385C8-$387CC): a
 *   wreck falling in and burning, its parts held on the boss;
 * - type $280 ($3AC62): the stomper, a walker on two feet with legs of segments, its attacks
 *   and collapse; its pieces: types $340 (a ball round the walls), $344 (a dart), $348 (a
 *   turret) and $34C (its shot), $350 (a ram), $358 (its core's last fall).
 * Registers a routine reads as its caller or a jump table left them are reproduced (a0 as a
 * jump table's target, as ObjectFloor or SparksAt leave it, a3 as BossColours leaves it). */
#include "game.h"

#define SINES   0x02977Cu
#define COSINES 0x0297FCu

static int16_t sine(uint16_t i) { return (int16_t)rom16(SINES + (uint32_t)(int16_t)i); }
static int16_t cosine(uint16_t i) { return (int16_t)rom16(COSINES + (uint32_t)(int16_t)i); }
static void add16(uint16_t a, uint16_t v) { wr16(a, (uint16_t)(rd16(a) + v)); }
static void add32(uint16_t a, uint32_t v) { wr32(a, rd32(a) + v); }
static int neg16(uint16_t v) { return (v & 0x8000) != 0; }
static int neg32(uint32_t v) { return (v & 0x80000000u) != 0; }
/* cmpi.w #imm,(m) then bmi / bpl: the N flag of the subtraction */
static int below16(uint16_t a, uint16_t imm) { return neg16((uint16_t)(rd16(a) - imm)); }
static int below32(uint16_t a, uint32_t imm) { return neg32(rd32(a) - imm); }

/* divs.w: the quotient (low word) and the remainder (high); on overflow the dividend kept. A
 * zero divisor stops the 68000 (its vector, $458, is `stop`): no list of the game has one. */
static uint32_t divs_w(uint32_t n, uint16_t d)
{
    if (!d) return n;
    int32_t q = (int32_t)n / (int16_t)d, r = (int32_t)n % (int16_t)d;
    if (q < -32768 || q > 32767) return n;
    return (uint32_t)(uint16_t)r << 16 | (uint16_t)q;
}

/* a slot from $D580 on (SlotFreeFrom), 0 when none */
static uint16_t slot_from_d580(void)
{
    uint16_t slot = 0xD580, t;
    return SlotFreeFrom(&slot, &t) ? slot : 0;
}

/* the speed a shot gets from an angle: sine and cosine, sign-extended, << shift */
static void speed_at(uint16_t slot, uint16_t ay, uint16_t ax, int shift)
{
    wr32(slot + 0x1C, (uint32_t)(int32_t)sine(ay) << shift);
    wr32(slot + 0x18, (uint32_t)(int32_t)cosine(ax) << shift);
}

/* ======================================================================================== */
/* ---- the boss's common parts: its joints, its speeds, its flash ---- */

/* $027638. The joints' table ($9000, $44 bytes) cleared. */
void SevenJointsClear(void)
{
    for (uint16_t k = 0; k <= 0x10; k++) wr32((uint16_t)(0x9000 + 4 * k), 0);
}

/* $02764C. Faster rightward by `step` while the x speed is below `limit` (the N flag of limit -
 * speed); stopped at x $200 or more. */
void SevenSpeedRight(uint16_t obj, uint32_t limit, uint32_t step)
{
    if (!below16(obj + O_X, 0x200)) { wr32(obj + 0x18, 0); return; }
    if (neg32(limit - rd32(obj + 0x18))) return;
    add32(obj + 0x18, step);
}

/* $02766C. Faster leftward by `step` down to ~limit; stopped below x $40. */
void SevenSpeedLeft(uint16_t obj, uint32_t limit, uint32_t step)
{
    if (below16(obj + O_X, 0x40)) { wr32(obj + 0x18, 0); return; }
    if (!neg32(~limit - rd32(obj + 0x18))) return;
    wr32(obj + 0x18, rd32(obj + 0x18) - step);
}

/* $02768A. The x speed brought toward 0 by `step` (0 once it would cross). */
void SevenSlowX(uint16_t obj, uint32_t step)
{
    uint32_t v = rd32(obj + 0x18);
    if (!neg32(v)) {
        v -= step;
        wr32(obj + 0x18, v);
        if (neg32(v)) wr32(obj + 0x18, 0);
        return;
    }
    v += step;
    wr32(obj + 0x18, v);
    if (!neg32(v)) wr32(obj + 0x18, 0);
}

/* $0276A6. Faster downward by `step` while the y speed is below `limit`; stopped at y $160. */
void SevenSpeedDown(uint16_t obj, uint32_t limit, uint32_t step)
{
    if (!below16(obj + O_Y, 0x160)) { wr32(obj + 0x1C, 0); return; }
    if (neg32(limit - rd32(obj + 0x1C))) return;
    add32(obj + 0x1C, step);
}

/* $0276C6. Faster upward by `step` down to ~limit; stopped above y $90. */
void SevenSpeedUp(uint16_t obj, uint32_t limit, uint32_t step)
{
    if (below16(obj + O_Y, 0x90)) { wr32(obj + 0x1C, 0); return; }
    if (!neg32(~limit - rd32(obj + 0x1C))) return;
    wr32(obj + 0x1C, rd32(obj + 0x1C) - step);
}

/* $0276E4. The y speed brought toward 0 by `step`. */
void SevenSlowY(uint16_t obj, uint32_t step)
{
    uint32_t v = rd32(obj + 0x1C);
    if (!neg32(v)) {
        v -= step;
        wr32(obj + 0x1C, v);
        if (neg32(v)) wr32(obj + 0x1C, 0);
        return;
    }
    v += step;
    wr32(obj + 0x1C, v);
    if (!neg32(v)) wr32(obj + 0x1C, 0);
}

/* $0273D4. The joints from `joints` on (count+1 of them) set turning toward a pose (a word
 * each: the angle in the low byte, bit 15 the way round) over `frames` frames: each one's step
 * (the word after its angle) the difference * $100 / frames. */
void SevenPoseToAt(uint32_t pose, uint16_t frames, uint16_t count, uint16_t joints)
{
    uint16_t k = count;
    do {
        uint16_t want = bus_rd16(pose);
        pose += 2;
        uint16_t cur = (uint16_t)(rd16(joints) >> 8);
        joints = (uint16_t)(joints + 2);
        uint16_t d = (uint16_t)((want & 0xFF) - cur);
        if (!neg16(want)) {
            if (neg16(d)) d &= 0xFF;
        } else if (!neg16(d)) {
            d |= 0xFF00;
        }
        wr16(joints, (uint16_t)divs_w((uint32_t)(int32_t)(int16_t)d << 8, frames));
        joints = (uint16_t)(joints + 2);
    } while (k-- != 0);
}

/* $0273CE. The same from the first joint ($9000). */
void SevenPoseTo(uint32_t pose, uint16_t frames, uint16_t count)
{
    SevenPoseToAt(pose, frames, count, 0x9000);
}

/* $0273CA. The same, the frames kept in +$4A. */
void SevenPoseTimed(uint16_t obj, uint32_t pose, uint16_t frames, uint16_t count)
{
    wr16(obj + 0x4A, frames);
    SevenPoseTo(pose, frames, count);
}

/* $027434. count+1 joints turned by their steps. */
void SevenJointsTurn(uint16_t count)
{
    uint16_t k = count, j = 0x9000;
    do {
        wr16(j, (uint16_t)(rd16(j) + rd16(j + 2)));
        j = (uint16_t)(j + 4);
    } while (k-- != 0);
}

/* $027700. The pose list (+$58: a pose's address and its frames, 6 bytes each; negative frames:
 * a jump to the address) stepped when +$5E runs out: the next pose's turn set up. */
void SevenPoseNext(uint16_t obj, uint16_t count)
{
    add16(obj + 0x5E, 0xFFFF);
    if (!neg16(rd16(obj + 0x5E))) return;
    for (;;) {
        uint32_t list = rd32(obj + 0x58);
        uint32_t pose = bus_rd32(list);
        uint16_t frames = bus_rd16(list + 4);
        if (!neg16(frames)) {
            wr32(obj + 0x58, list + 6);
            wr16(obj + 0x5E, frames);
            SevenPoseTo(pose, frames, count);
            return;
        }
        wr32(obj + 0x58, pose);
    }
}

/* $02772E. The wait before the next attack ($9202) by $A20C: ((it ^ $E) + 1) * 16 + $20. */
void SevenWaitSet(void)
{
    wr16(0x9202, (uint16_t)((((rd16(0xA20C) ^ 0x0E) + 1) << 4) + 0x20));
}

/* $027744. Its hit box ($28: $E020E020) and its kind ($21: $20). */
void SevenHitBox(uint16_t obj)
{
    wr32(obj + 0x28, 0xE020E020);
    wr8(obj + 0x21, 0x20);
}

/* $027754. Every object from $C980 up to the first empty slot made harmless (+$21 cleared). */
void SevenPartsSafe(void)
{
    for (uint16_t a = 0xC980; rd16(a); a = (uint16_t)(a + 0x60)) wr8(a + 0x21, 0);
}

/* $02776E. Hit (+$22 bit 5): every third frame its colours ($E352, $E372: three each) white;
 * otherwise back to the fade's target's ($E3D2, $E3F2). */
void SevenFlash(uint16_t obj)
{
    if (bit8(obj + 0x22, 5)) {
        add16(0x9236, 0xFFFF);
        if (neg16(rd16(0x9236))) {
            wr16(0x9236, 2);
            wr16(0xE352, 0x444);
            wr16(0xE354, 0x666);
            wr16(0xE356, 0x888);
            wr16(0xE372, 0x888);
            wr16(0xE374, 0xBBB);
            wr16(0xE376, 0xEEE);
            return;
        }
    }
    for (uint16_t k = 0; k < 6; k += 2) {
        wr16((uint16_t)(0xE352 + k), rd16((uint16_t)(0xE3D2 + k)));
        wr16((uint16_t)(0xE372 + k), rd16((uint16_t)(0xE3F2 + k)));
    }
}

/* $02744E. Beaten (vitality below 0): 11111 points, its parts harmless, the change of form
 * (state $18) with the next form's start state (+$48), place (+$4C, +$4E), vitality and the
 * stage's event from the tables by the floor it fights on ($A981 bit 1) and a random order
 * ($9200 / $921C, stepped by $921E / $9220); the last form gone ($921A below 0): state $2C. */
void SevenBeaten(uint16_t obj)
{
    if (!neg16(rd16(obj + O_VITALITY))) return;
    AddScore(obj, 0x11111);
    SevenPartsSafe();
    wr16(obj + O_STATE, 0x18);
    wr16(obj + 0x4A, 0x20);
    wr16(obj + 0x20, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    add16(0x921A, 0xFFFF);
    if (neg16(rd16(0x921A))) {
        wr16(obj + O_STATE, 0x2C);
        wr16(obj + 0x48, 0x38);
        wr16(obj + 0x4C, 0x150);
        wr16(obj + 0x4E, 0xE0);
        wr16(obj + O_VITALITY, 0);
        QueueSound(0xF6);
        return;
    }
    wr16(0x80C2, 0x10);
    wr16(0x80D6, 0xC980);
    uint16_t i, step;
    uint32_t base;
    if (bit8(0xA981, 1)) {
        step = 0x921E;
        i = rom16(rom32(0x02757A + (uint32_t)(int16_t)rd16(0x9200)) + (uint32_t)(int16_t)rd16(step));
        base = 0x0275CA;
    } else {
        step = 0x9220;
        i = rom16(rom32(0x02757A + (uint32_t)(int16_t)rd16(0x921C)) + (uint32_t)(int16_t)rd16(step));
        base = 0x0275E8;
    }
    add16(step, 2);
    if (!below16(step, 6)) wr16(step, 0);
    uint32_t at = (uint32_t)(int16_t)i;
    wr16(obj + 0x48, rom16(base + 6 + at));
    wr16(obj + 0x4C, rom16(base + 12 + at));
    wr16(obj + 0x4E, rom16(base + 18 + at));
    wr16(obj + O_VITALITY, rom16(base + 24 + at));
    wr16(v_stageEvent, rom16(base + at));
}

/* $0264E6. Its state's routine (the table at $264F0). */
void SevenStep(uint16_t obj)
{
    call_object(rom32(0x0264F0 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $0264DE. Type $24: the boss of seven forms: its flash, its defeat, its state. */
void Seven(uint16_t obj)
{
    SevenFlash(obj);
    SevenBeaten(obj);
    SevenStep(obj);
}

/* $0272D6. OrbiterSetup with the segments' sprite word $200 (and their radius growing). */
uint16_t OrbiterSetupLow(uint16_t parent, uint32_t maps, uint32_t radii, uint32_t links, uint16_t count, uint16_t slot,
                         uint32_t *look)
{
    wr16(0x8040, 0x200);
    return OrbiterSetupGrow(parent, maps, radii, links, count, slot, look);
}

/* $0272DC. OrbiterSetup with the segments' radius growing from 0 ($8042 cleared). */
uint16_t OrbiterSetupGrow(uint16_t parent, uint32_t maps, uint32_t radii, uint32_t links, uint16_t count, uint16_t slot,
                          uint32_t *look)
{
    wr16(0x8042, 0);
    return OrbiterSetup(parent, maps, radii, links, count, slot, look);
}

/* ======================================================================================== */
/* ---- form A (states $60-$8C): the first form, on two legs ---- */

/* $024132. Form A's segments turned by its joints (nine of them stepped first): each joint's
 * angle (its high byte * 2) into the segments it carries (+$56). */
void SevenALimbs(void)
{
    SevenJointsTurn(8);
    uint16_t a = (uint16_t)(rd8(0x9000) << 1) & 0x1FE, b = a;
    a = (uint16_t)(a + 0x20) & 0x1FE;
    wr16(0xCA36, a);
    b = (uint16_t)(b - 0x1C) & 0x1FE;
    wr16(0xCAF6, b);
    b = (uint16_t)(b + 0x80) & 0x1FE;
    wr16(0xCB56, b);
    a = (uint16_t)(a + 0xE0) & 0x1FE;
    wr16(0xCA96, a);
    a = (uint16_t)(rd8(0x9004) << 1) & 0x1FE;
    wr16(0xCBB6, a);
    wr16(0xCC16, a);
    wr16(0xCC76, a);
    a = (uint16_t)(rd8(0x9008) << 1) & 0x1FE;
    wr16(0xCCD6, a);
    wr16(0xCD36, a);
    a = (uint16_t)(rd8(0x900C) << 1) & 0x1FE;
    wr16(0xD036, a);
    wr16(0xD096, a);
    wr16(0xD0F6, a);
    a = (uint16_t)(rd8(0x9010) << 1) & 0x1FE;
    wr16(0xD156, a);
    wr16(0xD1B6, a);
    a = (uint16_t)(rd8(0x9014) << 1) & 0x1FE;
    wr16(0xCD96, a);
    wr16(0xCDF6, a);
    wr16(0xCE56, a);
    a = (uint16_t)(rd8(0x9018) << 1) & 0x1FE;
    wr16(0xCEB6, a);
    wr16(0xCF16, a);
    a = (uint16_t)(a + 0x40) & 0x1FE;
    wr16(0xCF76, a);
    a = (uint16_t)(a - 0x80) & 0x1FE;
    wr16(0xCFD6, a);
    a = (uint16_t)(rd8(0x901C) << 1) & 0x1FE;
    wr16(0xD216, a);
    wr16(0xD276, a);
    wr16(0xD2D6, a);
    a = (uint16_t)(rd8(0x9020) << 1) & 0x1FE;
    wr16(0xD336, a);
    wr16(0xD396, a);
    a = (uint16_t)(a + 0x40) & 0x1FE;
    wr16(0xD3F6, a);
    a = (uint16_t)(a - 0x80) & 0x1FE;
    wr16(0xD456, a);
}

/* $024126. While +$4A lasts (counted down): the segments turned. Type $610's states too. */
void SevenAPoseRun(uint16_t obj)
{
    if (neg16(rd16(obj + 0x4A))) return;
    add16(obj + 0x4A, 0xFFFF);
    SevenALimbs();
}

/* $024080. Form A's pose list stepped, its segments turned. */
void SevenAPose(uint16_t obj)
{
    SevenPoseNext(obj, 8);
    SevenALimbs();
}

/* $02422A. A step's sound ($D0) when the pose list stands at $27B70 or $27B7C with 4 frames
 * of the pose left. */
void SevenStepSound(uint16_t obj)
{
    if ((rd32(obj + 0x58) == 0x027B70 && rd16(obj + 0x5E) == 4) ||
        (rd32(obj + 0x58) == 0x027B7C && rd16(obj + 0x5E) == 4))
        QueueSound(0xD0);
}

/* $023ABE. Form A's start: its colours, 17 + 12 segments from itself on, their boxes, its pose
 * list ($27B6A), vitality $1004. */
void SevenAStart(uint16_t obj)
{
    QueueSound(0xB2);
    BossColours(0x027FC2);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, 0xC980);
    add16(obj + O_STATE, 4);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    uint16_t slot = OrbiterSetupLow(obj, 0x027ADA, 0x027B1E, 0x027B30, 0x10, obj, &look);
    OrbiterSetupLow(obj, 0x027AEE, 0x027B23, 0x027B52, 0x0B, slot, &look);
    wr32(0xCCAC, 0xF808F808);
    wr16(0xCCA6, 0x0A);
    wr32(0xCEEC, 0xF40CF40C);
    wr16(0xCEE6, 4);
    wr8(0xCEE1, 0x10);
    wr32(0xD36C, 0xF40CF40C);
    wr16(0xD366, 4);
    wr8(0xD361, 0x10);
    wr32(0xD1AC, 0x027936);
    wr16(0xD1B0, 0x9A0);
    wr16(obj + O_TYPE, 0x24);
    wr16(obj + 0x50, 0x100);
    wr16(obj + 0x56, 0x100);
    wr32(obj + 0x58, 0x027B6A);
    wr16(obj + 0x5E, 0);
    wr16(obj + O_VITALITY, 0x1004);
    wr16(0x9210, 0xFFFF);
}

/* $023B8A. Unfolding (+$50 and +$56 down by 4 to 0); then its hit box, on. */
void SevenAUnfold(uint16_t obj)
{
    SevenAPose(obj);
    if (rd16(obj + 0x50)) {
        add16(obj + 0x50, (uint16_t)-4);
        add16(obj + 0x56, (uint16_t)-4);
        return;
    }
    QueueSound(0xD1);
    add16(obj + O_STATE, 4);
    wr16(obj + 0x18, 1);
    SevenHitBox(obj);
}

/* $02408C. When the wait ($9202) runs out (set again): the next attack by where the target is:
 * above y $100: 1 in 4 the aim (state $78), else the jump (state $84, $9208 0); otherwise,
 * behind: none; ahead within $60: the crouch (+4); $C0 or more ahead: the jump ($9208 1);
 * between: the aim ($920A from the arm, $9212 shots, $920C / $9208 frames by $A20C). */
void SevenAChoose(uint16_t obj, uint32_t target)
{
    add16(0x9202, 0xFFFF);
    if (!neg16(rd16(0x9202))) return;
    SevenWaitSet();
    if (below16((uint16_t)target + O_Y, 0x100)) {
        if (!(rd8(v_randomSeed) & 3)) goto aim;
        wr16(0x9208, 0);
        wr16(obj + O_STATE, 0x84);
        return;
    }
    uint16_t d = (uint16_t)(rd16((uint16_t)target + O_X) - rd16(obj + O_X));
    if (neg16(d)) return;
    if (neg16((uint16_t)(d - 0x60))) { add16(obj + O_STATE, 4); return; }
    if (!neg16((uint16_t)(d - 0xC0))) {
        wr16(0x9208, 1);
        wr16(obj + O_STATE, 0x84);
        return;
    }
aim:
    wr32(0x9204, target);
    wr16(obj + O_STATE, 0x78);
    wr32(0xCD2C, 0x027996);
    wr16(0x920A, rd16(0xCBB6));
    wr16(0x9212, rd16(v_randomSeed) & 3);
    uint16_t n = (uint16_t)(((rd16(0xA20C) ^ 0x0E) + 1) << 4);
    wr16(0x920C, n);
    wr16(0x9208, n);
}

/* $023BBA. Walking: back and forth ($9210 counts each way's frames: down from a positive
 * count, up from a negative one, a new count by chance when it runs out) between x $B0 and
 * $E0, toward the target walking left; the next attack chosen. */
void SevenAWalk(uint16_t obj)
{
    SevenStepSound(obj);
    uint16_t target = PlayerTarget();
    wr32(0x9204, 0xFFFF0000u | target);
    SevenAChoose(obj, 0xFFFF0000u | target);
    uint16_t r = rd16(v_randomSeed);
    if (neg16(rd16(0x9210))) {
        add16(0x9210, 1);
        if (!neg16(rd16(0x9210))) wr16(0x9210, (uint16_t)((r & 0x200) + 0x200));
        if (neg16((uint16_t)(rd16(obj + O_X) - rd16(target + O_X)))) SevenSpeedRight(obj, 0xC000, 0x1400);
        else SevenSpeedLeft(obj, 0xC000, 0x1400);
    } else {
        add16(0x9210, 0xFFFF);
        if (neg16(rd16(0x9210))) wr16(0x9210, (uint16_t)((r & 0x100) + 0xFE00));
        if (!below16(obj + O_X, 0xE0)) SevenSpeedLeft(obj, 0xC000, 0x1400);
        else if (below16(obj + O_X, 0xB0)) SevenSpeedRight(obj, 0xC000, 0x1400);
    }
    SevenAPose(obj);
}

/* $023C42. Crouching (+$56 to $20) while running left; at the pose $27B82: the leap (x speed
 * 6) into the pose $27BD0 over 16 frames. */
void SevenACrouch(uint16_t obj)
{
    SevenStepSound(obj);
    SevenSpeedLeft(obj, 0x1C000, 0x1400);
    SevenAPose(obj);
    if (rd16(obj + 0x56) != 0x20) {
        wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 4) & 0x1FE);
        return;
    }
    if (rd16(obj + 0x5E) || rd32(obj + 0x58) != 0x027B82) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x18, 0x60000);
    wr16(obj + 0x4A, 16);
    SevenPoseTo(0x027BD0, 16, 8);
    QueueSound(0xD1);
}

/* $023CAE. The leap: sinking to y $138, +$56 to $180 by 8, slowing ($1C00 a frame) until the x
 * speed is below -1.5. */
void SevenALeap(uint16_t obj)
{
    if (below16(obj + O_Y, 0x138)) add16(obj + O_Y, 4);
    if (rd16(obj + 0x56) != 0x180) wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 8) & 0x1F8);
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x1C00);
    if (below32(obj + 0x18, 0xFFFE8000)) add16(obj + O_STATE, 4);
    SevenAPoseRun(obj);
}

/* $023CEE. Rising back to y $F8 by 8, +$56 back to 0; then walking again (state $68, the pose
 * list $27B70). */
void SevenARise(uint16_t obj)
{
    if (!below16(obj + O_Y, 0xF8)) add16(obj + O_Y, (uint16_t)-8);
    if (rd16(obj + 0x56)) {
        wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 8) & 0x1F8);
    } else {
        wr32(obj + 0x58, 0x027B70);
        wr16(obj + 0x5E, 0);
        wr16(obj + O_STATE, 0x68);
    }
    SevenAPose(obj);
}

/* the arm's segments turned to `a` (and $920A) */
static void seven_a_arm(uint16_t a)
{
    wr16(0xCBB6, a);
    wr16(0xCC16, a);
    wr16(0xCC76, a);
    wr16(0xCCD6, a);
    wr16(0xCD36, a);
}

/* $023D2C. Aiming: the arm ($920A) turned by 4 toward the target (from the segment at
 * $CAA0); $920C frames, then firing (+$56: the arm's box $CCA1 on; drifting away from the
 * side the arm points to). */
void SevenAAim(uint16_t obj)
{
    SevenStepSound(obj);
    SevenSlowX(obj, 0x1400);
    SevenAPose(obj);
    uint16_t target = (uint16_t)rd32(0x9204);
    int16_t dx = (int16_t)(rd16(target + O_X) - rd16(0xCAA0 + O_X));
    uint32_t dy = (uint16_t)(rd16(target + O_Y) - rd16(0xCAA0 + O_Y));
    uint16_t want = (uint16_t)((int16_t)Atan2(dx, &dy) >> 7) & 0x1F8;
    uint16_t a = rd16(0x920A), d = (uint16_t)(want - a) & 0x1FC;
    if (d) {
        if (!neg16((uint16_t)(d - 0x100))) a = (uint16_t)(a - 4);
        else a = (uint16_t)(a + 4);
    }
    seven_a_arm(a);
    wr16(0x920A, a);
    wr16(0x920C, (uint16_t)(rd16(0x920C) - 1));
    if (!neg16(rd16(0x920C))) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0);
    wr8(0xCCA1, 0x10);
    if (neg16((uint16_t)(a - 0x80)) || !neg16((uint16_t)(a - 0x180))) wr32(obj + 0x18, 0xFFFE0000);
    else wr32(obj + 0x18, 0x20000);
    QueueSound(0xB9);
}

/* the arm stretched or not: its segments at $920A */
static void seven_a_arm_at(void)
{
    seven_a_arm(rd16(0x920A));
}

/* $023DE8. Firing: every other frame a shot (type $30) from the hand ($CC90) along the arm;
 * the arm stretching (+6 a frame) while the hand is within x $80-$1C0, y $A0-$150, else the
 * pull back. */
void SevenAFire(uint16_t obj)
{
    SevenStepSound(obj);
    if (!(rd16(v_gameFrames) & 1)) {
        uint16_t s = slot_from_d580();
        if (s) {
            wr16(s + O_TYPE, 0x30);
            uint16_t a = (uint16_t)(rd16(0x920A) + 0x100) & 0x1FE;
            speed_at(s, a, a, 5);
            wr16(s + O_X, rd16(0xCC90));
            wr16(s + O_Y, rd16(0xCC94));
            wr16(s + 0x48, 0x20);
        }
    }
    SevenSlowX(obj, 0x1400);
    SevenAPose(obj);
    if (below16(0xCCF4, 0xA0) || !below16(0xCCF4, 0x150) || below16(0xCCF0, 0x80) || !below16(0xCCF0, 0x1C0)) {
        add16(obj + O_STATE, 4);
        add16(0x9210, 1);
    } else {
        add16(0x9210, 1);
        add16(0xCCD4, 6);
        add16(0xCD34, 6);
    }
    seven_a_arm_at();
}

/* $023EA0. The arm pulled back ($9210 frames); then (at x $C0 on, $9212 times) the aim again
 * with $9208 frames, or the walk (state $68, the arm's box off). */
void SevenAFireEnd(uint16_t obj)
{
    SevenStepSound(obj);
    SevenSlowX(obj, 0x1400);
    SevenAPose(obj);
    add16(0xCCD4, (uint16_t)-6);
    add16(0xCD34, (uint16_t)-6);
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) {
        if (!below16(obj + O_X, 0xC0)) {
            add16(0x9212, 0xFFFF);
            if (!neg16(rd16(0x9212))) {
                wr16(0x920C, rd16(0x9208));
                wr16(obj + O_STATE, 0x78);
                seven_a_arm_at();
                return;
            }
        }
        wr16(0x9210, 0xFF80);
        wr16(obj + O_STATE, 0x68);
        wr32(0xCD2C, 0x027956);
        wr8(0xCCA1, 0);
        return;
    }
    seven_a_arm_at();
}

/* $023F16. Stopping for the jump: at the pose $27B82, the crouch into the pose $27BE2 (28
 * frames), up at 1, across at 1 toward the middle. */
void SevenAJump(uint16_t obj)
{
    SevenStepSound(obj);
    SevenSlowX(obj, 0x1400);
    SevenAPose(obj);
    if (rd16(obj + 0x5E) || rd32(obj + 0x58) != 0x027B82) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x4A, 0x1C);
    SevenPoseTo(0x027BE2, 0x1C, 8);
    wr32(obj + 0x1C, 0xFFFF0000);
    if (below16(obj + O_X, 0xE0)) wr32(obj + 0x18, 0x10000);
    else wr32(obj + 0x18, 0xFFFF0000);
}

/* $023F7C. Rising (the y speed, unless 0, slowed by $100) for +$4A frames; then the pose
 * $27BF4 over 13. */
void SevenAJumpUp(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C);
    if (v) wr32(obj + 0x1C, v + 0x100);
    SevenALimbs();
    add16(obj + 0x4A, 0xFFFF);
    if (rd16(obj + 0x4A)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x4A, 0x0D);
    SevenPoseTo(0x027BF4, 0x0D, 8);
}

/* $023FB6. Falling ($2000 a frame), firing from the foot ($D170) every other frame while +$4A
 * is 3-8 (type $34 straight down, by chance fast or slow; with $9208: type $38 thrown right);
 * landed at y $F0: walking again. */
void SevenAJumpFall(uint16_t obj)
{
    if (below16(obj + 0x4A, 9) && !below16(obj + 0x4A, 3) && !(rd16(v_gameFrames) & 1)) {
        uint16_t s = slot_from_d580();
        if (s) {
            QueueSound(0xD4);
            if (!rd16(0x9208)) {
                wr16(s + O_TYPE, 0x34);
                wr16(s + O_X, rd16(0xD170));
                wr16(s + O_Y, rd16(0xD174));
                wr32(s + 0x1C, bit8(v_randomSeed + 1, 0) ? 0xF000 : 0x5000);
            } else {
                wr16(s + O_TYPE, 0x38);
                wr32(s + 0x1C, 0xFFFE0000);
                wr32(s + 0x18, 0x60000);
                wr16(s + O_X, rd16(0xD170));
                wr16(s + O_Y, rd16(0xD174));
            }
        }
    }
    add32(obj + 0x1C, 0x2000);
    if (!below16(obj + O_Y, 0xF0)) {
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, 0xF0);
        wr16(obj + O_STATE, 0x68);
        wr32(obj + 0x58, 0x027B70);
        wr16(obj + 0x5E, 0);
    }
    SevenAPoseRun(obj);
}

/* ======================================================================================== */
/* ---- form B (states $90-$CC): a body on a chain of six segments (type $2C) ---- */

/* $0242FA. Six chain segments (type $2C) from $CD40, their looks alternately from `look1` and
 * `look2` (three words each: sprite word | $6000, mappings), each one's place in the history
 * tables (+$48: $9080 + 6, $E ...; +$50: $9100 / $9180 + $C, $1C ...), its radius $8040; the
 * history tables ($9080, $9100, $9180: $64 bytes each) cleared. */
void SevenChainSetup(uint32_t look1, uint32_t look2)
{
    uint16_t o48 = 6, o50 = 0x0C, s = 0xCD40;
    for (uint16_t k = 0; k < 3; k++) {
        for (int j = 0; j < 2; j++) {
            uint32_t look = j ? look2 : look1;
            wr16(s + 0x0E, bus_rd16(look));
            wr16(s + 0x0E, rd16(s + 0x0E) | 0x6000);
            wr16(s + 0x08, bus_rd16(look + 2));
            wr16(s + 0x0A, bus_rd16(look + 4));
            wr16(s + 0x48, o48);
            wr16(s + 0x50, o50);
            wr16(s + 0x52, rd16(0x8040));
            wr8(s + 0x20, j ? 0x14 : 0x18);
            wr16(s + O_TYPE, 0x2C);
            wr16(s + O_FLAGS, 0x8C80);
            o48 = (uint16_t)(o48 + 8);
            o50 = (uint16_t)(o50 + 0x10);
            s = (uint16_t)(s + 0x60);
        }
    }
    for (uint16_t k = 0; k <= 0x18; k++) {
        wr32((uint16_t)(0x9080 + 4 * k), 0);
        wr32((uint16_t)(0x9100 + 4 * k), 0);
        wr32((uint16_t)(0x9180 + 4 * k), 0);
    }
}

/* $02429C. Form B's set-up: its colours, 10 segments from itself and 4 from $CF80, the chain
 * of six (radius $10). */
void SevenBSetup(uint16_t obj)
{
    BossColours(0x02800A);
    add16(obj + O_STATE, 4);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    OrbiterSetupLow(obj, 0x027C06, 0x027C3E, 0x027C4C, 9, obj, &look);
    OrbiterSetupLow(obj, 0x027C2E, 0x027C48, 0x027C60, 3, 0xCF80, &look);
    wr16(0x8040, 0x10);
    SevenChainSetup(0x027ABC, 0x027AB6);
}

/* $024262. Form B's start: set up, its pose list ($27C68). */
void SevenBStart(uint16_t obj)
{
    SevenBSetup(obj);
    wr16(obj + O_TYPE, 0x24);
    wr32(0xCF4C, 0xF40CF40C);
    wr16(0xCF46, 0x0A);
    wr16(obj + 0x54, 0x100);
    wr16(obj + 0x56, 0x100);
    wr32(obj + 0x58, 0x027C68);
    wr16(obj + 0x5E, 0);
    QueueSound(0xB2);
}

/* form B's joints: the body's segments by the three joints, the chain's head angle ($9080) by
 * the arm ($920A), the tail's by $920E */
static void seven_b_limbs(void)
{
    SevenJointsTurn(2);
    uint16_t a = (uint16_t)(rd8(0x9000) << 1) & 0x1FE, b = a, c = a;
    wr16(0xCA96, a);
    wr16(0x9080, (uint16_t)(a + 0x100 + rd16(0x920A)) & 0x1FE);
    wr16(0xCD36, (uint16_t)(c + 0x80 + rd16(0x920E)) & 0x1FE);
    b = (uint16_t)(b + 0x30) & 0x1FE;
    wr16(0xCA36, b);
    b = (uint16_t)(b - 0x50) & 0x1FE;
    wr16(0xCAF6, b);
    wr16(0xCCD6, b);
    a = (uint16_t)(rd8(0x9004) << 1) & 0x1FE;
    wr16(0xCB56, a);
    wr16(0xCBB6, a);
    a = (uint16_t)(rd8(0x9008) << 1) & 0x1FE;
    wr16(0xCC16, a);
    wr16(0xCC76, a);
}

/* form B's wheel (four segments turning $10 a frame), the chain's anchor at its place, $920E
 * turning on unless 0 */
static void seven_b_wheel(uint16_t obj)
{
    uint16_t a = (uint16_t)(rd16(0xCFD6) + 0x10) & 0x1FE;
    wr16(0xCFD6, a);
    a = (uint16_t)(a + 0x80) & 0x1FE;
    wr16(0xD036, a);
    a = (uint16_t)(a + 0x80) & 0x1FE;
    wr16(0xD096, a);
    a = (uint16_t)(a + 0x80) & 0x1FE;
    wr16(0xD0F6, a);
    wr16(0x9100, rd16(obj + O_Y));
    wr16(0x9180, rd16(obj + O_X));
    wr16(0x9218, 0);
    if (rd16(0x920E)) wr16(0x920E, (uint16_t)(rd16(0x920E) + 2) & 0x1FE);
}

/* $024B40. Form B's pose list stepped, its joints and wheel turned. */
void SevenBPose(uint16_t obj)
{
    SevenPoseNext(obj, 2);
    seven_b_limbs();
    seven_b_wheel(obj);
}

/* $024B4C. While +$4A lasts (counted down) its joints turned; its wheel turned. */
void SevenBPoseRun(uint16_t obj)
{
    if (!neg16(rd16(obj + 0x4A))) {
        add16(obj + 0x4A, 0xFFFF);
        seven_b_limbs();
    }
    seven_b_wheel(obj);
}

/* $02493E. Drifting across: left from x $168, right below $130, between by chance. */
void SevenBDriftX(uint16_t obj)
{
    uint16_t r = rd16(v_randomSeed);
    if (!below16(obj + O_X, 0x168)) { SevenSpeedLeft(obj, 0x10000, 0x1000); return; }
    if (below16(obj + O_X, 0x130) || (r & 2)) { SevenSpeedRight(obj, 0x10000, 0x1000); return; }
    SevenSpeedLeft(obj, 0x10000, 0x1000);
}

/* $024B10. The chain's angle ($920A) back toward 0 by 2. */
void SevenBArmBack(void)
{
    uint16_t a = rd16(0x920A);
    if (!a) return;
    if (!neg16((uint16_t)(a - 0x100))) wr16(0x920A, (uint16_t)(a + 2) & 0x1FE);
    else wr16(0x920A, (uint16_t)(a - 2) & 0x1FE);
}

/* $024A94. The chain swinging ($920A by 2): $9210 counts down by 2 (a new count by chance when
 * it runs out); its parity is the way, turned back at the ends ($20-$100 one way, $100-$1E0
 * the other). */
void SevenBArmSwing(void)
{
    uint16_t a = rd16(0x920A), r = rd16(v_randomSeed);
    add16(0x9210, (uint16_t)-2);
    uint16_t n = rd16(0x9210);
    if (neg16(n)) wr16(0x9210, (uint16_t)((r & 0x3F) + 0x18));
    int down;
    if (n & 1) {
        down = 1;
        if (!neg16((uint16_t)(a - 0x100)) && neg16((uint16_t)(a - 0x1E0))) {
            if (!neg16(n)) { wr16(0x9210, 0x20); down = 0; }
        }
    } else {
        down = 0;
        if (neg16((uint16_t)(a - 0x100)) && !neg16((uint16_t)(a - 0x20))) {
            wr16(0x9210, 0x41);
            down = 1;
        }
    }
    wr16(0x920A, (uint16_t)(down ? a - 2 : a + 2) & 0x1FE);
}

/* $02499C. Every other frame: a shot (type $3C) from the head ($CC90; 8 lower in state $B8) at
 * the head's angle (+$56, +$54) give or take $10; $9214 counts them ($9212 $100 when out). */
void SevenBShot(uint16_t obj)
{
    if (rd16(v_gameFrames) & 1) return;
    add16(0x9214, 0xFFFF);
    if (neg16(rd16(0x9214))) wr16(0x9212, 0x100);
    uint16_t s = slot_from_d580();
    if (!s) return;
    wr16(s + O_TYPE, 0x3C);
    wr16(s + O_X, rd16(0xCC90));
    wr16(s + O_Y, rd16(0xCC94));
    if (rd16(obj + O_STATE) == 0xB8) add16(s + O_Y, 8);
    uint16_t ay = (uint16_t)((rd16(v_randomSeed) & 0x1E) + rd16(obj + 0x56) - 0x20) & 0x1FE;
    uint16_t ax = (uint16_t)(ay + rd16(obj + 0x54)) & 0x1FE;
    speed_at(s, ay, ax, 5);
    QueueSound(0xB6);
}

/* $024972. From $A20C 8 on: firing in bursts ($9212 the pause, then $9214 shots by $A20C);
 * $920E (the tail's swing) held at $18C or wound back by 4. */
void SevenBFire(uint16_t obj)
{
    uint16_t d = rd16(0xA20C);
    if (neg16((uint16_t)(d - 8))) return;
    if (rd16(0x9212)) {
        add16(0x9212, 0xFFFF);
        wr16(0x9214, rom16(0x024A50 + (uint32_t)(int16_t)(uint16_t)(d - 8)));
        return;
    }
    uint16_t t = rd16(0x920E);
    if (!t || !neg16((uint16_t)(t - 0x190))) { add16(0x920E, (uint16_t)-4); return; }
    wr16(0x920E, 0x18C);
    SevenBShot(obj);
}

/* $024A58. Its speeds from its angle (+$56 - $80; +$54 the x's offset), * 8. */
void SevenBSpeedAt(uint16_t obj)
{
    uint16_t ay = (uint16_t)(rd16(obj + 0x56) - 0x80) & 0x1FE;
    uint16_t ax = (uint16_t)(ay + rd16(obj + 0x54)) & 0x1FE;
    speed_at(obj, ay, ax, 3);
}

/* following the target up or down ($20000 / $2000), the pose stepped */
static void seven_b_follow(uint16_t obj, int up)
{
    if (up) SevenSpeedUp(obj, 0x20000, 0x2000);
    else SevenSpeedDown(obj, 0x20000, 0x2000);
    SevenBPose(obj);
}

/* $02445A. Toward the target's height + $20, kept within y $B8-$110. */
void SevenBFollowY(uint16_t obj)
{
    uint16_t target = (uint16_t)rd32(0x9204);
    uint16_t want = (uint16_t)(rd16(target + O_Y) + 0x20);
    if (below16(obj + O_Y, 0xB8)) { seven_b_follow(obj, 0); return; }
    if (!below16(obj + O_Y, 0x110)) { seven_b_follow(obj, 1); return; }
    seven_b_follow(obj, neg16((uint16_t)(want - rd16(obj + O_Y))));
}

/* $0243B4. Unfolding (+$56 down by 4 to 0); then on: the target chosen, the wait ($9202) by
 * the difficulty (the words at $24402), its hit box. */
void SevenBUnfold(uint16_t obj)
{
    SevenBPose(obj);
    if (rd16(obj + 0x56)) { add16(obj + 0x56, (uint16_t)-4); return; }
    wr32(0x9204, 0xFFFF0000u | PlayerTarget());
    wr16(obj + O_STATE, 0xA0);
    wr16(0x9202, rom16(0x024402 + (uint32_t)(int16_t)rd16(v_difficulty)));
    SevenHitBox(obj);
    wr8(obj + 0x21, 0x30);
    wr16(obj + 0x26, 8);
    wr32(obj + 0x2C, 0xE818E818);
}

/* $02440A. Hovering at the target's height, the chain back; when the wait runs out ($100
 * again): the dive (state $B0: the target above y $130, 1 in 2) or the charge (state $BC). */
void SevenBHover(uint16_t obj)
{
    wr32(0x9204, 0xFFFF0000u | PlayerTarget());
    SevenBDriftX(obj);
    SevenBArmBack();
    if (!neg16(rd16(0x9202))) {
        add16(0x9202, 0xFFFF);
    } else {
        wr16(0x9202, 0x100);
        uint16_t target = (uint16_t)rd32(0x9204);
        if (below16(target + O_Y, 0x130) && !bit8(v_randomSeed + 1, 0)) wr16(obj + O_STATE, 0xB0);
        else wr16(obj + O_STATE, 0xBC);
    }
    SevenBFollowY(obj);
}

/* $024492. Swinging the chain; when the wait runs out and the chain is straight: +4 ($9216 by
 * chance). Up and down within y $C0-$100 by chance. */
void SevenBSwing(uint16_t obj)
{
    SevenBDriftX(obj);
    SevenBArmSwing();
    if (!neg16(rd16(0x9202))) {
        add16(0x9202, 0xFFFF);
    } else if (!rd16(0x920A)) {
        wr16(0x9202, 0x100);
        add16(obj + O_STATE, 4);
        wr16(0x9210, 0);
        wr16(0x9216, rd16(v_randomSeed) & 1);
    }
    uint16_t r = rd16(v_randomSeed);
    if (below16(obj + O_Y, 0xC0)) seven_b_follow(obj, 0);
    else if (!below16(obj + O_Y, 0x100)) seven_b_follow(obj, 1);
    else seven_b_follow(obj, !(r & 8));
}

/* $02450C. At the pose $27C6E: the pose $27C80 over $30 frames, $9212 $60. */
void SevenBCurl(uint16_t obj)
{
    if (rd16(obj + 0x5E) || rd32(obj + 0x58) != 0x027C6E) { SevenBPose(obj); return; }
    add16(obj + O_STATE, 4);
    wr16(0x9212, 0x60);
    wr16(obj + 0x4A, 0x30);
    SevenPoseTo(0x027C80, 0x30, 2);
}

/* $024540. Stopping, turning (+$56 up by 2) to $60; then the pose $27C7A over $10. */
void SevenBCurlTurn(uint16_t obj)
{
    SevenSlowX(obj, 0x1000);
    SevenSlowY(obj, 0x2000);
    uint16_t a = (uint16_t)(rd16(obj + 0x56) + 2) & 0x1FE;
    wr16(obj + 0x56, a);
    if (neg16((uint16_t)(a - 0x60))) { SevenBPoseRun(obj); return; }
    add16(obj + O_STATE, 4);
    wr16(obj + 0x4A, 0x10);
    SevenPoseTo(0x027C7A, 0x10, 2);
}

/* $024588. Turning back (+$56 down by 8) to 0 (then its box $CF41 on), moving along it. */
void SevenBRollStart(uint16_t obj)
{
    uint16_t a = (uint16_t)(rd16(obj + 0x56) - 8) & 0x1F8;
    wr16(obj + 0x56, a);
    if (!a) {
        add16(obj + O_STATE, 4);
        wr8(0xCF41, 0x10);
    }
    SevenBSpeedAt(obj);
    SevenBPoseRun(obj);
}

/* $0245AC. Rolling round the screen's edge: firing; its course by $9210: down to y $B8 (0),
 * left to x $E0 (1), down to y $130 (2), right to x $160 (3, $9216 times round), then up to y
 * $C0 (back to the hover); +$56 turned by 8 toward each side's angle (0, $80, $100, $180). */
void SevenBRoll(uint16_t obj)
{
    SevenBFire(obj);
    SevenBSpeedAt(obj);
    uint16_t n = rd16(0x9210), want;
    if (n == 0) {
        if (below16(obj + O_Y, 0xB8)) add16(0x9210, 1);
        want = 0;
    } else if (n == 1) {
        if (below16(obj + O_X, 0xE0)) add16(0x9210, 1);
        want = 0x80;
    } else if (n == 2) {
        if (!below16(obj + O_Y, 0x130)) add16(0x9210, 1);
        want = 0x100;
    } else if (n == 3) {
        if (!below16(obj + O_X, 0x160)) {
            add16(0x9216, 0xFFFF);
            if (!neg16(rd16(0x9216))) wr16(0x9210, 0);
            else add16(0x9210, 1);
        }
        want = 0x180;
    } else {
        if (below16(obj + O_Y, 0xC0)) wr16(obj + O_STATE, 0x98);
        want = 0;
    }
    uint16_t a = rd16(obj + 0x56);
    if (a != want) wr16(obj + 0x56, (uint16_t)(a + 8) & 0x1FE);
    SevenBPose(obj);
}

/* $024684. Stopping; at the pose $27C74: the pose $27C7A over $30. State $C0's too. */
void SevenBDive(uint16_t obj)
{
    SevenSlowX(obj, 0x4000);
    SevenSlowY(obj, 0x2000);
    if (rd16(obj + 0x5E) || rd32(obj + 0x58) != 0x027C74) { SevenBPose(obj); return; }
    add16(obj + O_STATE, 4);
    wr16(obj + 0x4A, 0x30);
    SevenPoseTo(0x027C7A, 0x30, 2);
}

/* $0246C6. The tail ($920E) wound to $18C; stopping, turning (+$56 down by 2) to below $1A0;
 * then $40 shots ($9214), the pose $27C86 over $10. */
void SevenBDiveTurn(uint16_t obj)
{
    uint16_t t = rd16(0x920E);
    if (!t || !neg16((uint16_t)(t - 0x190))) add16(0x920E, (uint16_t)-4);
    else wr16(0x920E, 0x18C);
    SevenSlowX(obj, 0x1000);
    SevenSlowY(obj, 0x2000);
    uint16_t a = (uint16_t)(rd16(obj + 0x56) - 2) & 0x1FE;
    wr16(obj + 0x56, a);
    if (!neg16((uint16_t)(a - 0x1A0))) { SevenBPoseRun(obj); return; }
    wr16(0x9214, 0x40);
    add16(obj + O_STATE, 4);
    wr16(obj + 0x4A, 0x10);
    SevenPoseTo(0x027C86, 0x10, 2);
}

/* $024732. Dashing right in bursts (4 every $10 frames; thrown back at x $170), the chain to
 * $1E8, firing; the shots out ($9214 below 0): back (state $9C 1 in 4, else the hover). */
void SevenBDash(uint16_t obj)
{
    if (rd16(0x920A) != 0x1E8) wr16(0x920A, (uint16_t)(rd16(0x920A) - 2) & 0x1FE);
    wr16(0x920E, 0x18C);
    SevenBShot(obj);
    SevenSlowX(obj, 0x4000);
    if (!(rd16(v_gameFrames) & 0x0F)) wr32(obj + 0x18, 0x40000);
    if (!below16(obj + O_X, 0x170)) {
        wr32(obj + 0x18, 0xFFFD0000);
        wr16(obj + O_X, 0x170);
    }
    uint16_t a = rd16(obj + 0x56);
    if (a) wr16(obj + 0x56, (uint16_t)(a + 8) & 0x1F8);
    if (neg16(rd16(0x9214))) {
        wr32(obj + 0x18, 0xFFFF0000);
        if (!(rd16(v_randomSeed) & 3)) {
            wr16(obj + O_STATE, 0x9C);
        } else {
            wr16(obj + O_STATE, 0x98);
            SevenWaitSet();
        }
    }
    SevenBPoseRun(obj);
}

/* $0247CE. The charge: at the target's height, left while x is $100 on; then +4. */
void SevenBCharge(uint16_t obj)
{
    SevenBFollowY(obj);
    if (!below16(obj + O_X, 0x100)) { SevenSpeedLeft(obj, 0x10000, 0x1000); return; }
    add16(obj + O_STATE, 4);
}

/* $0247EE. As $024684. */
void SevenBCharge2(uint16_t obj)
{
    SevenBDive(obj);
}

/* $0247F2. Stopping, turning (+$56 up by 2) to $60; then the bite ($9208 1, $920C by chance),
 * the pose $27C86 over 8. */
void SevenBBiteStart(uint16_t obj)
{
    SevenSlowX(obj, 0x1000);
    SevenSlowY(obj, 0x2000);
    uint16_t a = (uint16_t)(rd16(obj + 0x56) + 2) & 0x1FE;
    wr16(obj + 0x56, a);
    if (neg16((uint16_t)(a - 0x60))) { SevenBPoseRun(obj); return; }
    wr16(0x9208, 1);
    wr16(0x920C, rd16(v_randomSeed) & 1);
    add16(obj + O_STATE, 4);
    wr16(obj + 0x4A, 8);
    SevenPoseTo(0x027C86, 8, 2);
    QueueSound(0xB7);
}

/* $024856. The bite: every other frame a shot (type $30) from the jaw ($CF30); +$56 turned
 * back by $10; at $1C0, $9208 times: then thrown right (4) and +4. */
void SevenBBite(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 1)) {
        uint16_t s = slot_from_d580();
        if (s) {
            wr16(s + O_TYPE, 0x30);
            wr16(s + O_X, rd16(0xCF30));
            wr16(s + O_Y, rd16(0xCF34));
            wr16(s + 0x48, 0x20);
            wr16(s + 0x4A, 1);
        }
    }
    SevenSlowX(obj, 0x3000);
    SevenSlowY(obj, 0x2000);
    uint16_t a = (uint16_t)(rd16(obj + 0x56) - 0x10) & 0x1F0;
    wr16(obj + 0x56, a);
    if (a == 0x1C0) {
        add16(0x9208, 0xFFFF);
        if (neg16(rd16(0x9208))) {
            add16(obj + O_STATE, 4);
            wr32(obj + 0x18, 0x40000);
        }
    }
    SevenBPoseRun(obj);
}

/* $0248D2. Turning (+$56 up by 2) to 0; then another bite ($920C times: state $C8) or back
 * (state $9C 1 in 4, else the hover). */
void SevenBBiteEnd(uint16_t obj)
{
    SevenSlowX(obj, 0x3000);
    SevenSlowY(obj, 0x2000);
    uint16_t a = (uint16_t)(rd16(obj + 0x56) + 2) & 0x1FE;
    wr16(obj + 0x56, a);
    if (a) { SevenBPoseRun(obj); return; }
    add16(0x920C, 0xFFFF);
    if (!neg16(rd16(0x920C))) {
        wr16(obj + O_STATE, 0xC8);
        wr16(0x9208, 1);
        SevenBPoseRun(obj);
        QueueSound(0xB7);
        return;
    }
    if (!(rd16(v_randomSeed) & 3)) {
        wr16(obj + O_STATE, 0x9C);
    } else {
        wr16(obj + O_STATE, 0x98);
        SevenWaitSet();
    }
    SevenBPoseRun(obj);
}

/* $0271FE. Type $2C: a segment of form B's chain, following the one before it: the chain's
 * swing ($9218, growing by $920A) carried down it; its angle (from the history at $9080 +
 * +$48, shifted on a word), the anchor's place four frames back ($9100 / $9180 + +$50,
 * shifted on a long); at its radius (+$52 plus the common change, not below 0) round it, x kept
 * within $C0-$180 (but in stage 4's part $14); its place into the next history entry. */
void SevenChain(uint16_t obj)
{
    uint16_t swing = (uint16_t)(rd16(0x9218) + rd16(0x920A));
    wr16(0x9218, swing);
    uint16_t h = (uint16_t)(0x9080 + rd16(obj + 0x48));
    uint16_t a = rd16(h);
    wr16((uint16_t)(h + 2), a);
    for (int k = 0; k < 3; k++, h = (uint16_t)(h - 2)) wr16(h, rd16((uint16_t)(h - 2)));
    uint16_t ya = (uint16_t)(0x9100 + rd16(obj + 0x50)), yto = (uint16_t)(ya + 4);
    uint32_t y = rd32(ya);
    for (int k = 0; k < 3; k++, ya = (uint16_t)(ya - 4)) wr32(ya, rd32((uint16_t)(ya - 4)));
    uint16_t xa = (uint16_t)(0x9180 + rd16(obj + 0x50)), xto = (uint16_t)(xa + 4);
    uint32_t x = rd32(xa);
    for (int k = 0; k < 3; k++, xa = (uint16_t)(xa - 4)) wr32(xa, rd32((uint16_t)(xa - 4)));
    int16_t r = (int16_t)(rd16(obj + 0x52) + rd16(0xC9D0));
    if (r < 0) r = 0;
    a = (uint16_t)(a + swing + rd16(0xC9D6)) & 0x1FE;
    uint16_t b = (uint16_t)(a + rd16(0xC9D4)) & 0x1FE;
    uint32_t dy = (uint32_t)((int32_t)sine(a) * r) << 2, dx = (uint32_t)((int32_t)cosine(b) * r) << 2;
    wr32(obj + O_Y, dy + y);
    x += dx;
    if (rd16(v_stage4) != 0x14) {
        if (neg32(x - 0xC00000)) x = 0xC00001;
        if (!neg32(x - 0x1800000)) x = 0x17FFFFF;
    }
    wr32(obj + O_X, x);
    wr32(yto, rd32(obj + O_Y));
    wr32(xto, rd32(obj + O_X));
}

/* $027848. Type $610: form A's figure brought back (stage 6?): set up as form A (its segments
 * with sprite word $8000, radius full), at x $200, y $100, bobbing (the pose $4E57E over 18);
 * then its segments turned. */
void SevenAgain(uint16_t obj)
{
    if (rd16(obj + O_STATE)) { SevenAPoseRun(obj); return; }
    add16(obj + O_STATE, 4);
    BossColours(0x027FC2);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    wr16(0x8040, 0x8000);
    wr16(0x8042, 1);
    uint16_t slot = OrbiterSetup(obj, 0x04E528, 0x04E56C, 0x027B30, 16, obj, &look);
    OrbiterSetup(obj, 0x04E53C, 0x04E571, 0x027B52, 11, slot, &look);
    wr16(obj + O_TYPE, 0x610);
    SevenPoseBob(obj, 0x04E57E, 18);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_X, 0x200);
    wr16(obj + O_Y, 0x100);
    wr16(obj + 0x54, 0x100);
    wr16(obj + 0x56, 0x20);
}

/* ======================================================================================== */
/* ---- form C (states $D0-$F0): on four legs, a beam of 20 segments ($D0A0 on) ---- */

/* $024CA2. Form C's pose at rest: +$4A, its turn ($56 $100), the beam's root ($D036, $D096)
 * at $120, some segments' radii nudged, its pose list $27CF4. */
void SevenCReset(uint16_t obj)
{
    wr16(obj + 0x4A, 0);
    wr16(obj + 0x56, 0x100);
    wr16(0xD036, 0x120);
    wr16(0xD096, 0x120);
    add16(0xCBB2, 2);
    add16(0xCC12, 3);
    add16(0xCC72, 3);
    add16(0xCCD2, 2);
    add16(0xCD92, (uint16_t)-8);
    wr32(obj + 0x58, 0x027CF4);
    wr16(obj + 0x5E, 0);
}

/* $024C1C. Form C's start: its colours, 11 + 6 + 2 segments from itself on. */
void SevenCStart(uint16_t obj)
{
    QueueSound(0xB2);
    BossColours(0x027FE6);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, 0xC980);
    add16(obj + O_STATE, 4);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    uint16_t slot = OrbiterSetupLow(obj, 0x027C8C, 0x027CC0, 0x027CCE, 0x0A, obj, &look);
    slot = OrbiterSetupLow(obj, 0x027CA0, 0x027CC5, 0x027CE4, 5, slot, &look);
    OrbiterSetupLow(obj, 0x027CB8, 0x027CCB, 0x027CF0, 1, slot, &look);
    wr16(obj + O_TYPE, 0x24);
    wr16(obj + 0x50, 0x100);
    SevenCReset(obj);
}

/* $025256. Form C's segments turned by its nine joints (stepped first). */
void SevenCLimbs(void)
{
    SevenJointsTurn(8);
    uint16_t a = (uint16_t)(rd8(0x9000) << 1) & 0x1FE;
    wr16(0xCA96, a);
    a = (uint16_t)(a - 0x38) & 0x1FE;
    wr16(0xCAF6, a);
    a = (uint16_t)(rd8(0x9004) << 1) & 0x1FE;
    wr16(0xCBB6, a);
    wr16(0xCC16, a);
    wr16(0xCC76, a);
    a = (uint16_t)(rd8(0x9008) << 1) & 0x1FE;
    wr16(0xCCD6, a);
    wr16(0xCD36, a);
    a = (uint16_t)(rd8(0x900C) << 1) & 0x1FE;
    wr16(0xCD96, a);
    a = (uint16_t)((rd8(0x9010) << 1) - 0x10) & 0x1FE;
    wr16(0xCA36, a);
    a = (uint16_t)(rd8(0x9014) << 1) & 0x1FE;
    wr16(0xCB56, a);
    a = (uint16_t)(rd8(0x9018) << 1) & 0x1FE;
    wr16(0xCDF6, a);
    wr16(0xCE56, a);
    wr16(0xCEB6, a);
    a = (uint16_t)(rd8(0x901C) << 1) & 0x1FE;
    wr16(0xCF16, a);
    wr16(0xCF76, a);
    a = (uint16_t)(rd8(0x9020) << 1) & 0x1FE;
    wr16(0xCFD6, a);
}

/* $0251A2. Form C's gait: by its counter +$4A (0-$22, on by 1 a frame) the front legs' poses
 * (joints 0-4: at 0, $A, $14) and the back legs' (joints 5-8 from $9014: at 8, $17, $21). */
void SevenCGait(uint16_t obj)
{
    switch (rd16(obj + 0x4A)) {
    case 0x00: SevenPoseTo(0x027CFE, 0x0A, 4); break;
    case 0x0A: SevenPoseTo(0x027D08, 0x0A, 4); break;
    case 0x14: SevenPoseTo(0x027CF4, 0x0F, 4); break;
    case 0x08: SevenPoseToAt(0x027D22, 0x0F, 3, 0x9014); break;
    case 0x17: SevenPoseToAt(0x027D12, 0x0A, 3, 0x9014); break;
    case 0x21: SevenPoseToAt(0x027D1A, 0x0A, 3, 0x9014); break;
    default: break;
    }
    add16(obj + 0x4A, 1);
    if (!below16(obj + 0x4A, 0x23)) wr16(obj + 0x4A, 0);
}

/* $02517C. The gait, with a step's sound ($D0) at $18 and $10 (there a hop: y speed -2). */
void SevenCWalk(uint16_t obj)
{
    if (rd16(obj + 0x4A) != 0x18) {
        if (rd16(obj + 0x4A) != 0x10) { SevenCGait(obj); return; }
        wr32(obj + 0x1C, 0xFFFE0000);
    }
    QueueSound(0xD0);
    SevenCGait(obj);
}

/* $0252F8. Falling ($4000 a frame) to the floor at y $10C. */
void SevenCFall(uint16_t obj)
{
    if (!neg32(rd32(obj + 0x1C)) && !below16(obj + O_Y, 0x10C)) {
        wr16(obj + O_Y, 0x10C);
        wr32(obj + 0x1C, 0);
        return;
    }
    add32(obj + 0x1C, 0x4000);
}

/* $024FD0. A beam segment's look, flickering by the frame (bit 0). */
void SevenCBeamLook(uint16_t slot)
{
    if (!bit8(v_gameFrames + 1, 0)) {
        wr16(slot + 0x0E, 0xE7FD);
        wr16(slot + 0x08, 0);
        wr16(slot + 0x0A, 0xFCFC);
        return;
    }
    wr16(slot + 0x0E, 0xE7FE);
    wr16(slot + 0x08, 0x100);
    wr16(slot + 0x0A, 0xFCF8);
}

/* $024EAC. The beam's 20 segments (type $10, shown) set up from $D0A0. */
void SevenCBeamSetup(void)
{
    uint16_t s = 0xD0A0;
    for (uint16_t k = 0; k <= 0x13; k++, s = (uint16_t)(s + 0x60)) {
        wr16(s + O_TYPE, 0x10);
        wr16(s + O_FLAGS, 0x8000);
        SevenCBeamLook(s);
    }
}

/* $024ECC. The 20 slots after $D0A0 to be removed (flags $1000). */
void SevenCBeamEnd(void)
{
    uint16_t s = 0xD0A0;
    for (uint16_t k = 0; k <= 0x13; k++) {
        s = (uint16_t)(s + 0x60);
        wr16(s + O_FLAGS, 0x1000);
    }
}

/* $024EE4. The beam's looks; every $10 frames shown or hidden by turns. */
void SevenCBeamBlink(void)
{
    uint16_t s = 0xD0A0;
    for (uint16_t k = 0; k <= 0x13; k++, s = (uint16_t)(s + 0x60)) SevenCBeamLook(s);
    if (rd16(v_gameFrames) & 0x0F) return;
    uint16_t f = bit8(v_gameFrames + 1, 4) ? 0 : 0x8000;
    s = 0xD0A0;
    for (uint16_t k = 0; k <= 0x13; k++, s = (uint16_t)(s + 0x60)) wr16(s + O_FLAGS, f);
}

/* $024F36. The beam laid out from the segment at $D040 along its angle ($D096), 16 apart,
 * reflected off y $B0 / $140 (the angle's sine half) and x $90 / $1B0 (its cosine half): each
 * reflection starts again from the segment there. */
void SevenCBeam(void)
{
    uint16_t from = 0xD040, s = 0xD0A0, r = 16;
    uint16_t ay = rd16(from + 0x56), ax = ay;
    for (uint16_t k = 0; k <= 0x13; k++, s = (uint16_t)(s + 0x60)) {
        uint32_t y = ((uint32_t)((int32_t)sine(ay) * (int16_t)r) << 2) + rd32(from + O_Y);
        uint32_t x = ((uint32_t)((int32_t)cosine(ax) * (int16_t)r) << 2) + rd32(from + O_X);
        wr32(s + O_Y, y);
        wr32(s + O_X, x);
        r = (uint16_t)(r + 0x10);
        if (below16(s + O_Y, 0xB0) || !below16(s + O_Y, 0x140)) {
            ay = (uint16_t)(ay + 0x100) & 0x1FE;
            from = s;
            r = 16;
        }
        if (below16(s + O_X, 0x90) || !below16(s + O_X, 0x1B0)) {
            ax = (uint16_t)(ax + 0x100) & 0x1FE;
            from = s;
            r = 16;
        }
        SevenCBeamLook(s);
    }
}

/* $025002. Pacing fast ($28000 / $2000): right below x $C0, left from $180, between by the
 * random seed's bit 8. */
void SevenCPace(uint16_t obj)
{
    if (below16(obj + O_X, 0xC0)) { SevenSpeedRight(obj, 0x28000, 0x2000); return; }
    if (!below16(obj + O_X, 0x180)) { SevenSpeedLeft(obj, 0x28000, 0x2000); return; }
    if (bit8(v_randomSeed, 0)) SevenSpeedRight(obj, 0x28000, 0x2000);
    else SevenSpeedLeft(obj, 0x28000, 0x2000);
}

/* $025030. To the middle (x $13E-$140). */
void SevenCCentre(uint16_t obj)
{
    if (below16(obj + O_X, 0x13E)) { SevenSpeedRight(obj, 0x10000, 0x1000); return; }
    if (!below16(obj + O_X, 0x140)) SevenSpeedLeft(obj, 0x10000, 0x1000);
}

/* $025080. The beam's root ($D036, $D096) turned by 4 to $140 or (frame bit 4) back to $100. */
void SevenCBeamAim(void)
{
    uint16_t a = rd16(0xD036);
    if (!bit8(v_gameFrames + 1, 4)) {
        if (!neg16((uint16_t)(a - 0x140))) {
            wr16(0xD036, 0x140);
            wr16(0xD096, 0x140);
            return;
        }
        add16(0xD036, 4);
        add16(0xD096, 4);
        wr16(0xD036, rd16(0xD036) & 0x1FC);
        wr16(0xD096, rd16(0xD096) & 0x1FC);
        return;
    }
    if (a == 0x100) return;
    if (neg16((uint16_t)(a - 0x100))) {
        wr16(0xD036, 0x100);
        wr16(0xD096, 0x100);
        return;
    }
    add16(0xD036, (uint16_t)-4);
    add16(0xD096, (uint16_t)-4);
    wr16(0xD036, rd16(0xD036) & 0x1FC);
    wr16(0xD096, rd16(0xD096) & 0x1FC);
}

/* $0250EA. The beam swept by 8 between $140 and $1C0, turning at random times ($9214, the
 * way in $9212's bit 0). */
void SevenCBeamSweep(void)
{
    add16(0x9214, 0xFFFF);
    if (neg16(rd16(0x9214))) {
        wr16(0x9214, (uint16_t)((rd16(v_randomSeed) & 0x3F) + 8));
        wr16(0x9212, rd16(0x9212) ^ 1);
    }
    uint16_t a = rd16(0xD036);
    if (!bit8(0x9213, 0)) {
        if (!neg16((uint16_t)(a - 0x1C0))) {
            wr16(0xD036, 0x1C0);
            wr16(0xD096, 0x1C0);
            wr16(0x9212, 1);
            return;
        }
        add16(0xD036, 8);
        add16(0xD096, 8);
        wr16(0xD036, rd16(0xD036) & 0x1F8);
        wr16(0xD096, rd16(0xD096) & 0x1F8);
        return;
    }
    if (a != 0x140) {
        if (!neg16((uint16_t)(a - 0x140))) {
            add16(0xD036, (uint16_t)-8);
            add16(0xD096, (uint16_t)-8);
            wr16(0xD036, rd16(0xD036) & 0x1F8);
            wr16(0xD096, rd16(0xD096) & 0x1F8);
            return;
        }
        wr16(0xD036, 0x140);
        wr16(0xD096, 0x140);
    }
    wr16(0x9212, 0);
}

/* $024E76. When the walk's time ($9210) runs out: every fourth time ($9208) the jump (state
 * $EC, $80 frames), else the beam (+4, $40 frames). */
void SevenCChoose(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    if (!(rd16(0x9208) & 3)) {
        add16(0x9208, 1);
        wr16(obj + O_STATE, 0xEC);
        wr16(0x9210, 0x80);
        return;
    }
    add16(0x9208, 1);
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0x40);
}

/* $024CDC. Unfolding (+$50, +$56 down by 8 to 0); then on: its boxes, the walk. */
void SevenCUnfold(uint16_t obj)
{
    SevenCGait(obj);
    SevenCLimbs();
    if (rd16(obj + 0x50)) {
        add16(obj + 0x50, (uint16_t)-8);
        add16(obj + 0x56, (uint16_t)-8);
        return;
    }
    QueueSound(0xD1);
    add16(obj + O_STATE, 4);
    wr16(obj + 0x18, 1);
    wr16(0x9208, 0);
    wr32(0xCCAC, 0xF010F808);
    wr8(0xCCA1, 0x10);
    wr16(0xCCA6, 1);
    wr32(0xCEEC, 0xF010F808);
    wr8(0xCEE1, 0x10);
    wr16(0xCEE6, 1);
    SevenHitBox(obj);
    wr32(obj + 0x28, 0xFF20E818);
}

/* $024D4E. Walking (pacing), the next attack chosen. */
void SevenCWalkAbout(uint16_t obj)
{
    SevenCFall(obj);
    SevenCChoose(obj);
    SevenCPace(obj);
    SevenCBeamAim();
    SevenCWalk(obj);
    SevenCLimbs();
}

/* $024D68. To the middle; after $9210 frames: the beam set up (+4, $68 frames). */
void SevenCBeamStart(uint16_t obj)
{
    SevenCFall(obj);
    SevenCCentre(obj);
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) {
        add16(obj + O_STATE, 4);
        SevenCBeamSetup();
        wr16(0x9210, 0x68);
        wr16(0x9212, 0);
        wr16(0x9214, 4);
        wr32(obj + 0x18, 0);
    }
    SevenCBeamAim();
    SevenCWalk(obj);
    SevenCLimbs();
}

/* $024DA6. The beam swept for $9210 frames; then +4 ($60). */
void SevenCBeamFire(uint16_t obj)
{
    SevenCFall(obj);
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) {
        add16(obj + O_STATE, 4);
        wr16(0x9210, 0x60);
    }
    SevenCBeamSweep();
    SevenCBeam();
    SevenCWalk(obj);
    SevenCLimbs();
}

/* $024DCC. The beam blinking for $9210 frames; then gone (its first slot made type $204, the
 * others removed), +4 ($100). */
void SevenCBeamFade(uint16_t obj)
{
    SevenCFall(obj);
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) {
        add16(obj + O_STATE, 4);
        wr16(0x9210, 0x100);
        wr16(0xD0A0, 0x204);
        wr16(0xD0F6, rd16(0xD096));
        SevenCBeamEnd();
    } else {
        SevenCBeamBlink();
    }
    SevenCWalk(obj);
    SevenCLimbs();
}

/* $024E02. Waiting $9210 frames; then the walk (state $D8). */
void SevenCBeamDone(uint16_t obj)
{
    SevenCFall(obj);
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) wr16(obj + O_STATE, 0xD8);
    SevenCWalk(obj);
    SevenCLimbs();
}

/* $024E1C. Pacing $9210 frames; then +4 with the object at $DCA0 made type $230. */
void SevenCJump(uint16_t obj)
{
    SevenCFall(obj);
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) {
        add16(obj + O_STATE, 4);
        wr16(0xDCA0, 0x230);
        wr16(0xDCA4, 0);
    }
    SevenCPace(obj);
    SevenCBeamAim();
    SevenCWalk(obj);
    SevenCLimbs();
}

/* $024E48. Pacing until the object at $DCA0 is gone; then the walk (state $D8, $40-$7F
 * frames). */
void SevenCJumpWait(uint16_t obj)
{
    SevenCFall(obj);
    if (!rd16(0xDCA0)) {
        wr16(0x9210, (uint16_t)((rd16(v_randomSeed) & 0x3F) + 0x40));
        wr16(obj + O_STATE, 0xD8);
    }
    SevenCPace(obj);
    SevenCWalk(obj);
    SevenCLimbs();
}

/* ======================================================================================== */
/* ---- form G (states $188-$1CC): a lunging head on a neck of segments ---- */

/* $025634. Form G's segments' angles (15 from $CA36) from the words at $2564E. */
void SevenGRest(void)
{
    uint16_t s = 0xCA36;
    for (uint16_t k = 0; k <= 0x0E; k++, s = (uint16_t)(s + 0x60)) wr16(s, rom16(0x02564E + 2 * k));
}

/* $025320. Form G's start: its colours, 16 segments from itself on, at rest. */
void SevenGStart(uint16_t obj)
{
    QueueSound(0xB2);
    BossColours(0x027FF2);
    add16(obj + O_STATE, 4);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    OrbiterSetupLow(obj, 0x027D2A, 0x027D6A, 0x027D7A, 0x0F, obj, &look);
    wr16(obj + O_TYPE, 0x24);
    wr16(obj + 0x50, 0x100);
    SevenGRest();
}

/* $025366. Unfolding (+$50 down by 8 to 0); then on, $40 frames. */
void SevenGUnfold(uint16_t obj)
{
    if (rd16(obj + 0x50)) { add16(obj + 0x50, (uint16_t)-8); return; }
    QueueSound(0xD1);
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0x40);
    SevenHitBox(obj);
}

/* $02538E. After $9210 frames: the neck's segment ($CE56) turned by 2 to 0; then +4 (3 turns,
 * $B lunges). */
void SevenGRaise(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    wr16(0xCE56, (uint16_t)(rd16(0xCE56) - 2) & 0x1FE);
    if (rd16(0xCE56)) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 3);
    wr16(0x9212, 0x0B);
}

/* $02566C. The angle from it to the target, + half a turn, into $9208. */
void SevenGAim(uint16_t obj)
{
    uint16_t target = PlayerTarget();
    int16_t dx = (int16_t)(rd16(target + O_X) - rd16(obj + O_X));
    uint32_t dy = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t a = (uint16_t)Atan2(dx, &dy);
    wr16(0x9208, (uint16_t)(((int16_t)a >> 7) + 0x100) & 0x1FE);
}

/* $0253B6. Spinning (+$56 by -$20) $9210 times round; then aimed at the target. */
void SevenGSpin(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 0x20) & 0x1E0);
    if (rd16(obj + 0x56)) return;
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    add16(obj + O_STATE, 4);
    SevenGAim(obj);
}

/* $0253D8. Spinning on until within $22 of the aim; then facing it (less 8). */
void SevenGSpinAim(uint16_t obj)
{
    uint16_t want = rd16(0x9208);
    uint16_t d = (uint16_t)(((uint16_t)(want - rd16(obj + 0x56)) & 0x1FE) - 0x22);
    if (!neg16(d)) {
        wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 0x20) & 0x1E0);
        return;
    }
    add16(obj + O_STATE, 4);
    wr16(obj + 0x56, (uint16_t)(want - 8) & 0x1FE);
}

/* the head at the lunge's distance ($920C) along its angle ($920E) from where it started
 * ($9208, $920A) */
static void seven_g_lunge(uint16_t obj)
{
    uint16_t r = rd16(0x920C), a = rd16(0x920E) & 0x1FE;
    uint32_t y = (uint32_t)((int32_t)sine(a) * (int16_t)r) << 2;
    uint32_t x = (uint32_t)((int32_t)cosine(a) * (int16_t)r) << 2;
    wr16(obj + O_X, (uint16_t)((x >> 16) + rd16(0x9208)));
    wr16(obj + O_Y, (uint16_t)((y >> 16) + rd16(0x920A)));
}

/* $02540C. The neck drawn back ($CDF6 by the words at $2547C, by $A20C) while within $40-$1C0;
 * then the lunge: its start kept, the neck's radii set, a shot (type $25C) from the mouth
 * ($CB10) the other way. */
void SevenGDrawBack(uint16_t obj)
{
    wr16(0xCDF6, (uint16_t)(rd16(0xCDF6) + rom16(0x02547C + (uint32_t)(int16_t)rd16(0xA20C))) & 0x1FE);
    if (!below16(0xCDF6, 0x1C0) || below16(0xCDF6, 0x40)) return;
    QueueSound(0xC5);
    add16(obj + O_STATE, 4);
    wr16(0x9208, rd16(obj + O_X));
    wr16(0x920A, rd16(obj + O_Y));
    wr16(0x920C, 0);
    wr16(0x920E, rd16(obj + 0x56));
    wr16(0xCAF4, 0x10);
    wr16(0xCB54, 0x20);
    wr16(0xCF14, 2);
    wr16(0xCDF6, 0x1C0);
    uint16_t s = 0, t;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x25C);
    wr16(s + O_X, rd16(0xCB10));
    wr16(s + O_Y, rd16(0xCB14));
    wr16(s + 0x56, (uint16_t)(rd16(obj + 0x56) + 0x100) & 0x1FE);
}

/* $02548C. The lunge out ($920C by $20 to $40), spinning. */
void SevenGLunge(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 0x20) & 0x1FE);
    add16(0x920C, 0x20);
    if (!below16(0x920C, 0x40)) add16(obj + O_STATE, 4);
    seven_g_lunge(obj);
}

/* $0254B0. Back by 4; home: another lunge (state $194, $9212 times) or +4 (5). */
void SevenGLungeBack(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 2) & 0x1FE);
    add16(0x920C, (uint16_t)-4);
    uint16_t r = rd16(0x920C);
    if (r && !neg16(r)) { seven_g_lunge(obj); return; }
    wr16(obj + O_X, rd16(0x9208));
    wr16(obj + O_Y, rd16(0x920A));
    add16(0x9212, 0xFFFF);
    if (neg16(rd16(0x9212))) {
        add16(obj + O_STATE, 4);
        wr16(0x9210, 5);
        return;
    }
    wr16(obj + O_STATE, 0x194);
    wr16(0x9210, 1);
}

/* $0254F8. Spinning $9210 times round; then +4 ($30). */
void SevenGSpinDown(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 0x20) & 0x1E0);
    if (rd16(obj + 0x56)) return;
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0x30);
}

/* $02551C. $9210 frames; then +4 (1). States $1AC and $1B4. */
void SevenGWait(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 1);
}

/* $025530. The tail's segment ($CF12, $CF14) stretched by 4 for $9210 frames; then its fall
 * starts ($9208: its length 16.16, $9214 the speed). */
void SevenGStretch(uint16_t obj)
{
    add16(0xCF12, 4);
    add16(0xCF14, 4);
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0x20);
    wr32(0x9214, 0);
    wr32(0x9208, (uint32_t)rd16(0xCF12) << 16);
}

/* $025560. Stretching faster ($2000 a frame more) to $F0; then +4. */
void SevenGStretchFall(uint16_t obj)
{
    if (!below16(0xCF12, 0xF0)) { add16(obj + O_STATE, 4); return; }
    add32(0x9214, 0x2000);
    add32(0x9208, rd32(0x9214));
    wr16(0xCF12, rd16(0x9208));
    wr16(0xCF14, rd16(0xCF12));
}

/* $02558E. Turning (+$56 by -$10) to $1C0; then the tail back to $80. */
void SevenGTurn(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 0x10) & 0x1F0);
    if (rd16(obj + 0x56) != 0x1C0) return;
    add16(obj + O_STATE, 4);
    wr16(0xCF12, 0x80);
    wr16(0xCF14, 0x80);
}

/* $0255B6. The tail drawn in by 4 below 8; then set ($1A, 0), a sound. */
void SevenGDrawIn(uint16_t obj)
{
    add16(0xCF12, (uint16_t)-4);
    add16(0xCF14, (uint16_t)-4);
    if (!below16(0xCF12, 8)) return;
    add16(obj + O_STATE, 4);
    wr16(0xCF12, 0x1A);
    wr16(0xCF14, 0);
    QueueSound(0xB8);
}

/* $0255E4. Turning (+$56 by -8) below $1A0; then +4. */
void SevenGTurnBack(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 8) & 0x1FE);
    if (below16(obj + 0x56, 0x1A0)) add16(obj + O_STATE, 4);
}

/* $0255FE. Turning (+$56 by 4) to 0; then +4 ($20 frames). */
void SevenGTurnUp(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 4) & 0x1FC);
    if (rd16(obj + 0x56)) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0x20);
}

/* $025618. $9210 frames; then the spin again (state $194: 5 turns, $B lunges). */
void SevenGRestart(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    wr16(obj + O_STATE, 0x194);
    wr16(0x9210, 5);
    wr16(0x9212, 0x0B);
}

/* ======================================================================================== */
/* ---- form D (states $F4-$130): a ball of spokes, bouncing and spinning ---- */

/* eight segments from `s` set to angles from `a` and `b` alternately, each pair $80 on */
static void seven_d_spokes(uint16_t s, uint16_t a, uint16_t b)
{
    for (uint16_t k = 0; k <= 3; k++) {
        wr16(s, a);
        s = (uint16_t)(s + 0x60);
        wr16(s, b);
        s = (uint16_t)(s + 0x60);
        a = (uint16_t)(a + 0x80);
        b = (uint16_t)(b + 0x80) & 0x1FE;
    }
}

/* $025B68. The first eight spokes ($CA36 on) at 0, $1FC, $80 ... */
void SevenDSpokes(void)
{
    seven_d_spokes(0xCA36, 0, 0x1FC);
}

/* $025B5A. The next eight ($CD36 on) at $40, $3C, $C0 ... */
void SevenDSpokes2(void)
{
    seven_d_spokes(0xCD36, 0x40, 0x3C);
}

/* $025BAC. The roll (state $120): harmless, a speed ($9208, the longs at $25C06) and a turn
 * ($920C, the words at $25BFE) by chance, still. */
void SevenDRollStart(uint16_t obj)
{
    wr16(obj + O_STATE, 0x120);
    wr8(obj + 0x21, 0);
    wr32(0x9208, rom32(0x025C06 + (uint32_t)(rd16(v_randomSeed) & 0x0C)));
    wr16(0x920C, rom16(0x025BFE + (uint32_t)(rd16(v_gameFrames) & 6)));
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x54, 0);
    wr16(obj + 0x56, 0);
}

/* the spokes set; below $5DC vitality: the roll (its speed 1) */
static void seven_d_spokes_check(uint16_t obj)
{
    SevenDSpokes();
    SevenDSpokes2();
    if (below16(obj + O_VITALITY, 0x5DC)) {
        SevenDRollStart(obj);
        wr32(0x9208, 0x10000);
    }
}

/* spinning by $920A (a sound every 4 frames), the spokes set */
static void seven_d_spin(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 3)) QueueSound(0xC7);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - rd16(0x920A)) & 0x1FE);
    seven_d_spokes_check(obj);
}

/* $025702. Form D's start: its colours, 9 + 8 segments from itself on; its floor and ceiling
 * ($9222, $9224) by the difficulty. */
void SevenDStart(uint16_t obj)
{
    BossColours(0x027FFE);
    add16(obj + O_STATE, 4);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    uint16_t slot = OrbiterSetupLow(obj, 0x027E88, 0x027EAC, 0x027EB6, 8, obj, &look);
    OrbiterSetupLow(obj, 0x027E8C, 0x027EAD, 0x027EC8, 7, slot, &look);
    wr16(obj + O_TYPE, 0x24);
    wr16(obj + 0x50, 0x60);
    wr16(obj + 0x54, 0);
    wr16(obj + 0x56, 0);
    uint32_t d = (uint32_t)(int16_t)rd16(v_difficulty);
    wr16(0x9222, rom16(0x025788 + d));
    wr16(0x9224, rom16(0x025790 + d));
    QueueSound(0xB5);
}

/* $025798. Unfolding (+$50 counted down); then on: its boxes, $60 frames. */
void SevenDUnfold(uint16_t obj)
{
    add16(obj + 0x50, 0xFFFF);
    if (neg16(rd16(obj + 0x50))) {
        QueueSound(0xD1);
        wr16(obj + 0x50, 0);
        add16(obj + O_STATE, 4);
        wr16(0x9210, 0x60);
        SevenHitBox(obj);
        wr8(obj + 0x21, 0x30);
        wr32(obj + 0x2C, 0xE020E020);
        wr16(obj + 0x26, 4);
    }
    SevenDSpokes();
    SevenDSpokes2();
}

/* $0257DC. Turning (+$56 by -8) for $9210 frames; then +4. */
void SevenDTurn(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) add16(obj + O_STATE, 4);
    add16(obj + 0x56, (uint16_t)-8);
    seven_d_spokes_check(obj);
}

/* $0257F8. Rising ($2000 a frame faster) to the ceiling ($9222); then the bounces (4). */
void SevenDRise(uint16_t obj)
{
    if (neg16((uint16_t)(rd16(obj + O_Y) - rd16(0x9222)))) {
        wr32(obj + 0x1C, 0);
        add16(obj + O_STATE, 4);
        wr16(0x9208, 4);
    } else {
        wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x2000);
    }
    add16(obj + 0x56, (uint16_t)-8);
    seven_d_spokes_check(obj);
}

/* $025832. Bouncing between x $F0 and $150 ($9208 bit 0 the way; +$54 $100 by the height);
 * each wall a sound; after $9208 walls, 1 in 2: the spin toward a corner (state $10C-$118 by
 * where it is). */
void SevenDBounce(uint16_t obj)
{
    wr16(obj + 0x54, 0);
    int wall = 0;
    if (!bit8(0x9209, 0)) {
        if (!below16(obj + O_Y, 0x100)) wr16(obj + 0x54, 0x100);
        add32(obj + 0x18, 0x2000);
        if (!below16(obj + O_X, 0x150)) { wr16(obj + O_X, 0x150); wall = 1; }
    } else {
        if (below16(obj + O_Y, 0x100)) wr16(obj + 0x54, 0x100);
        wr32(obj + 0x18, rd32(obj + 0x18) - 0x2000);
        if (below16(obj + O_X, 0xF0)) { wr16(obj + O_X, 0xF0); wall = 1; }
    }
    if (wall) {
        QueueSound(0xA6);
        wr32(obj + 0x18, 0);
        add16(0x9208, 0xFFFF);
        if (neg16(rd16(0x9208)) && !bit8(v_randomSeed + 1, 0)) {
            uint16_t next = 0x10C;
            if (!below16(obj + O_Y, 0x100)) next = (uint16_t)(next + 8);
            if (!below16(obj + O_X, 0x120)) next = (uint16_t)(next + 4);
            wr16(0x9208, next);
            wr16(0x920A, 8);
            wr16(0x920C, 0x0F);
            add16(obj + O_STATE, 4);
        }
    }
    add16(obj + 0x56, (uint16_t)-8);
    seven_d_spokes_check(obj);
}

/* $0258FE. Spinning up (the spin $920A by 2 every other frame to $18), +$54 turning; after
 * $10 frames: state $9208. */
void SevenDSpinUp(uint16_t obj)
{
    if (below16(0x920A, 0x18) && !(rd16(v_gameFrames) & 1)) add16(0x920A, 2);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 4) & 0x1FE);
    add16(0x920C, 0xFFFF);
    if (neg16(rd16(0x920C))) {
        wr16(0x920C, 0x0F);
        wr16(obj + O_STATE, rd16(0x9208));
    }
    seven_d_spin(obj);
}

/* down ($2000 a frame faster) to the floor ($9224); there the spin down (state $11C) */
static void seven_d_spin_down(uint16_t obj)
{
    if (!neg16((uint16_t)(rd16(obj + O_Y) - rd16(0x9224)))) {
        wr32(obj + 0x1C, 0);
        wr16(obj + O_STATE, 0x11C);
    } else {
        add32(obj + 0x1C, 0x2000);
    }
    seven_d_spin(obj);
}

/* up ($2000 a frame faster) to the ceiling ($9222); there the spin down (state $11C) */
static void seven_d_spin_up(uint16_t obj)
{
    if (neg16((uint16_t)(rd16(obj + O_Y) - rd16(0x9222)))) {
        wr32(obj + 0x1C, 0);
        wr16(obj + O_STATE, 0x11C);
    } else {
        wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x2000);
    }
    seven_d_spin(obj);
}

/* $02593A. Spinning down to the floor. */
void SevenDSpinFloor(uint16_t obj)
{
    seven_d_spin_down(obj);
}

/* $025964. The same. */
void SevenDSpinFloor2(uint16_t obj)
{
    seven_d_spin_down(obj);
}

/* $02598E. Spinning up to the ceiling. */
void SevenDSpinCeiling(uint16_t obj)
{
    seven_d_spin_up(obj);
}

/* $0259B8. The same. */
void SevenDSpinCeiling2(uint16_t obj)
{
    seven_d_spin_up(obj);
}

/* $0259E2. Spinning down (the spin by -2 every other frame to $A), +$54 back; after $10
 * frames: the bounces again (state $104, 2 walls). */
void SevenDSpinDown(uint16_t obj)
{
    if (!below16(0x920A, 0x0A) && !(rd16(v_gameFrames) & 1)) add16(0x920A, (uint16_t)-2);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 4) & 0x1FE);
    add16(0x920C, 0xFFFF);
    if (neg16(rd16(0x920C))) {
        wr16(0x9208, 2);
        wr16(obj + O_STATE, 0x104);
    }
    seven_d_spin(obj);
}

/* $025A1E. The roll: opening (+$50 by 4 to $80), turning; then dropped in at x $120, y $A0
 * (its y speed $9208). */
void SevenDRoll(uint16_t obj)
{
    add16(obj + 0x50, 4);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 8) & 0x1FE);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + rd16(0x920C)) & 0x1FE);
    if (below16(obj + 0x50, 0x80)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0xA0);
    wr32(obj + 0x1C, rd32(0x9208));
    wr16(obj + 0x50, 0x60);
    wr16(obj + 0x54, 0);
    wr16(obj + 0x56, 0);
}

/* $025A70. Closing (+$50 by 2 below 0); then harmful again, still, $20 frames. */
void SevenDRollEnd(uint16_t obj)
{
    add16(obj + 0x50, (uint16_t)-2);
    if (!neg16(rd16(obj + 0x50))) return;
    wr16(obj + 0x50, 0);
    wr16(0x9210, 0x20);
    add16(obj + O_STATE, 4);
    wr8(obj + 0x21, 0x30);
    wr32(obj + 0x1C, 0);
    QueueSound(0xD1);
}

/* $025C48. Up to 8 ((random & 3) + the difficulty, at most 7, + 1) objects of type $238 from
 * the high slots, each given a segment (+$58: $CA40, $CB00 ...). */
void SevenDSpawn(void)
{
    uint16_t n = (uint16_t)((rd16(v_randomSeed) & 3) + rd16(v_difficulty));
    if (!neg16((uint16_t)(n - 7))) n = 7;
    uint16_t seg = 0xCA40;
    do {
        uint16_t s = 0, t;
        if (!SlotFreeHigh(&s, &t)) return;
        wr16(s + O_TYPE, 0x238);
        wr16(s + 0x58, seg);
        seg = (uint16_t)(seg + 0xC0);
    } while (n-- != 0);
}

/* $025A9E. Turning slowly $9210 frames; then the roll (1 in 8) or the spokes thrown (+4: $58
 * frames, objects $238). */
void SevenDWait(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 2) & 0x1FE);
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    if (!(rd16(v_randomSeed) & 7)) { SevenDRollStart(obj); return; }
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0x58);
    wr16(0x9212, 0);
    wr16(0x9214, 0);
    SevenDSpawn();
}

/* $025ADC. Turning by $9214 (a new turn from the words at $25B34 every 8-$27 frames) for
 * $9210 frames; then +4 ($40), a sound. */
void SevenDThrow(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) {
        add16(obj + O_STATE, 4);
        wr16(0x9210, 0x40);
        QueueSound(0xBA);
        return;
    }
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + rd16(0x9214)) & 0x1FE);
    add16(0x9212, 0xFFFF);
    if (!neg16(rd16(0x9212))) return;
    wr16(0x9212, (uint16_t)((rd8(v_randomSeed) & 0x1F) + 8));
    wr16(0x9214, rom16(0x025B34 + (uint32_t)(rd16(v_randomSeed) & 0x0E)));
}

/* $025B44. $9210 frames; then the wait again (state $128, $20). */
void SevenDThrowEnd(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    wr16(obj + O_STATE, 0x128);
    wr16(0x9210, 0x20);
}

/* ======================================================================================== */
/* ---- form E (states $134-$150): a body breathing in and out, sending out objects ---- */

/* $025C96. Form E's start: its colours, 23 segments from itself on. */
void SevenEStart(uint16_t obj)
{
    QueueSound(0xB2);
    BossColours(0x027FCE);
    add16(obj + O_STATE, 4);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    OrbiterSetupLow(obj, 0x027ED8, 0x027F34, 0x027F4C, 0x16, obj, &look);
    wr16(obj + O_TYPE, 0x24);
    wr16(obj + 0x50, 0x100);
    wr16(obj + 0x54, 0);
    wr16(obj + 0x56, 0x180);
    wr16(0x9210, 0);
    wr16(0x9212, 0);
    wr16(0x9208, 0);
    wr16(0x920E, 0);
    wr16(0x9214, 0);
    wr16(0x9216, 0x20);
}

/* $025D58. Form E's segments' angles (22 from $CA36) from the words at $25D72. States
 * $140-$150. */
void SevenEPose(uint16_t obj)
{
    (void)obj;
    uint16_t s = 0xCA36;
    for (uint16_t k = 0; k <= 0x15; k++, s = (uint16_t)(s + 0x60)) wr16(s, rom16(0x025D72 + 2 * k));
}

/* $025D0A. Unfolding (+$50 by 2 below 0); then on: its boxes, $80 frames. */
void SevenEUnfold(uint16_t obj)
{
    add16(obj + 0x50, (uint16_t)-2);
    if (neg16(rd16(obj + 0x50))) {
        QueueSound(0xD1);
        wr16(obj + 0x50, 0);
        add16(obj + O_STATE, 4);
        wr16(0x9210, 0x80);
        wr8(obj + 0x21, 0x30);
        wr32(obj + 0x2C, 0xA060800C);
        wr32(obj + 0x28, 0xC040F010);
        wr16(obj + 0x26, 8);
    }
    SevenEPose(obj);
}

/* $025E5C. Swaying between x $D0 and $DC ($9208 the way). */
void SevenESway(uint16_t obj)
{
    if (!rd16(0x9208)) {
        if (!bit8(obj + 0x18, 7) || !below32(obj + 0x18, 0xFFFF0000))
            wr32(obj + 0x18, rd32(obj + 0x18) - 0x2000);
        if (below16(obj + O_X, 0xD0)) wr16(0x9208, 2);
        return;
    }
    if (bit8(obj + 0x18, 7) || below32(obj + 0x18, 0x20000)) add32(obj + 0x18, 0x1000);
    if (!below16(obj + O_X, 0xDC)) wr16(0x9208, 0);
}

/* $025EC4. Sending out: the kind ($920C, the words at $25F5E) stepped every $9214 frames (by
 * $A20C) through four; every $9216 frames (by $A20C) one into the first free of the slots
 * $D400-$D520 (after the four: a kind by chance). */
void SevenESend(void)
{
    if (rd16(0x920E) != 8) {
        add16(0x9214, 0xFFFF);
        if (neg16(rd16(0x9214))) {
            add16(0x920E, 2);
            wr16(0x920C, rom16(0x025F5E + (uint32_t)(int16_t)rd16(0x920E)));
            wr16(0x9214, rom16(0x025F68 + (uint32_t)(int16_t)rd16(0xA20C)));
        }
    }
    add16(0x9216, 0xFFFF);
    if (!neg16(rd16(0x9216))) return;
    wr16(0x9216, rom16(0x025F78 + (uint32_t)(int16_t)rd16(0xA20C)));
    uint16_t s = 0xD400;
    for (int k = 0; rd16(s); k++) {
        if (k == 3) return;
        s = (uint16_t)(s + 0x60);
    }
    if (rd16(0x920E) != 8) wr16(s, rd16(0x920C));
    else wr16(s, rom16(0x025F5E + (uint32_t)(rd16(v_randomSeed) & 6)));
}

/* $025D4C. Sending out, swaying, breathing: some segments' radii (from their +$52) pushed
 * out and drawn in ($9212: up by 2 to $40, then down by 8 to 0). */
void SevenEBreathe(uint16_t obj)
{
    SevenESend();
    SevenESway(obj);
    if (!rd16(0x9210)) {
        add16(0x9212, 2);
        if (!below16(0x9212, 0x40)) {
            wr16(0x9210, 2);
            wr16(0x9212, 0x40);
        }
    } else {
        add16(0x9212, (uint16_t)-8);
        if (neg16(rd16(0x9212))) {
            wr16(0x9210, 0);
            wr16(0x9212, 0);
        }
    }
    int16_t d0 = (int16_t)((int16_t)rd16(0x9212) >> 2), d1 = (int16_t)(d0 >> 1), d2 = (int16_t)(d1 >> 1),
            d3 = (int16_t)(d2 >> 1);
    uint16_t v = (uint16_t)(rd16(0xCA92) + d1);
    wr16(0xCA94, v);
    wr16(0xCEB4, v);
    v = (uint16_t)(rd16(0xCB52) + d2);
    wr16(0xCB54, v);
    wr16(0xCF74, v);
    v = (uint16_t)(rd16(0xCAF2) - d0);
    wr16(0xCAF4, v);
    wr16(0xCF14, v);
    v = (uint16_t)(rd16(0xCBB2) - d1);
    wr16(0xCBB4, v);
    wr16(0xCFD4, v);
    v = (uint16_t)(rd16(0xCDF2) - d0);
    wr16(0xCDF4, v);
    wr16(0xD214, v);
    v = (uint16_t)(rd16(0xCC12) - d3);
    wr16(0xCC14, v);
    wr16(0xD034, v);
    v = (uint16_t)(rd16(0xCCD2) - d2);
    wr16(0xCCD4, v);
    wr16(0xD0F4, v);
    v = (uint16_t)(rd16(0xCD92) - d1);
    wr16(0xCD94, v);
    wr16(0xD1B4, v);
}

/* ======================================================================================== */
/* ---- form F (states $154-$184): hopping on two legs, a long arm swung ---- */

/* $026246. Form F's segments' angles at rest (29 from $CA36: the words at $26260). State
 * $184 too. */
void SevenFRest(uint16_t obj)
{
    (void)obj;
    uint16_t s = 0xCA36;
    for (uint16_t k = 0; k <= 0x1C; k++, s = (uint16_t)(s + 0x60)) wr16(s, rom16(0x026260 + 2 * k));
}

/* $0264C8. Above the easiest difficulty: facing ($56) by chance. */
void SevenFFace(uint16_t obj)
{
    if (rd16(v_difficulty)) wr16(obj + 0x56, rd16(v_randomSeed) & 0x180);
}

/* $025F88. Form F's start: its colours, 30 segments from itself on, rising (y speed -2). */
void SevenFStart(uint16_t obj)
{
    QueueSound(0xB2);
    BossColours(0x027FDA);
    add16(obj + O_STATE, 4);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    OrbiterSetupLow(obj, 0x027D9A, 0x027E12, 0x027E30, 0x1D, obj, &look);
    wr16(obj + O_TYPE, 0x24);
    wr16(0xCB0A, 0xF8F8);
    wr16(obj + 0x1C, 0xFFFE);
    wr16(0x9210, 0x40);
    SevenFRest(obj);
}

/* $025FDA. Slowing its rise for $9210 frames; then its hit box, +4. */
void SevenFRise(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) {
        add16(obj + O_STATE, 4);
        SevenHitBox(obj);
    }
    add32(obj + 0x1C, 0xBB8);
}

/* $025FF6. Falling to y $170; landed: the landing's bounce ($920C its height, $920A 8), then
 * state $168. */
void SevenFFall(uint16_t obj)
{
    add32(obj + 0x1C, 0xBB8);
    if (below16(obj + O_Y, 0x170)) return;
    QueueSound(0xA6);
    add16(obj + O_STATE, 4);
    wr32(obj + 0x1C, 0);
    wr16(0x9208, 0xFFFF);
    wr16(0x920A, 8);
    wr16(0x920C, rd16(obj + O_Y));
    wr16(0x9212, 0x168);
}

/* $02629A. The landing's shake: every fourth frame y at the floor ($920C) plus $920A, which
 * runs down; Z when it has run out. */
int SevenFShake(uint16_t obj)
{
    add16(0x9208, 1);
    uint16_t n = rd16(0x9208);
    if (n & 2) {
        wr16(obj + O_Y, rd16(0x920C));
        return rd16(0x920C) == 0;
    }
    if (n & 1) return 0;
    wr16(obj + O_Y, (uint16_t)(rd16(0x920C) + rd16(0x920A)));
    add16(0x920A, 0xFFFF);
    return rd16(0x920A) == 0;
}

/* $02603C. The shake; done: +4 ($20 frames). */
void SevenFLand(uint16_t obj)
{
    if (!SevenFShake(obj)) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0x20);
}

/* $026390. The legs ($CD36..., $CF16...) and the arm ($CBB6 and its 12 segments) turned
 * toward their targets ($922C, $922E, $9226; the ways $9230, $9232, $9228). */
void SevenFLimbs(void)
{
    if (rd16(0x922C) != rd16(0xCD36)) {
        if (!rd16(0x9230)) {
            add16(0xCCD6, 4);
            add16(0xCD36, 0x10);
            wr16(0xCD36, rd16(0xCD36) & 0x1FE);
            add16(0xCD96, 0x10);
            add16(0xCDF6, 0x10);
            add16(0xCE56, 0x10);
        } else {
            add16(0xCCD6, (uint16_t)-4);
            add16(0xCD36, (uint16_t)-0x10);
            wr16(0xCD36, rd16(0xCD36) & 0x1FE);
            add16(0xCD96, (uint16_t)-0x10);
            add16(0xCDF6, (uint16_t)-0x10);
            add16(0xCE56, (uint16_t)-0x10);
        }
    }
    if (rd16(0x922E) != rd16(0xCF16)) {
        if (rd16(0x9232)) {
            add16(0xCEB6, 4);
            add16(0xCF16, 0x10);
            wr16(0xCF16, rd16(0xCF16) & 0x1FE);
            add16(0xCF76, 0x10);
            add16(0xCFD6, 0x10);
            add16(0xD036, 0x10);
        } else {
            add16(0xCEB6, (uint16_t)-4);
            add16(0xCF16, (uint16_t)-0x10);
            wr16(0xCF16, rd16(0xCF16) & 0x1FE);
            add16(0xCF76, (uint16_t)-0x10);
            add16(0xCFD6, (uint16_t)-0x10);
            add16(0xD036, (uint16_t)-0x10);
        }
    }
    if (rd16(0x9226) == rd16(0xCBB6)) return;
    static const uint16_t arm[13] = {0xCBB6, 0xD096, 0xD0F6, 0xD156, 0xD1B6, 0xD216, 0xD276,
                                     0xD2D6, 0xD336, 0xD396, 0xD3F6, 0xD456, 0xD4B6};
    uint16_t by = rd16(0x9228) ? (uint16_t)-8 : 8;
    for (int k = 0; k < 13; k++) add16(arm[k], by);
}

/* $026038. The limbs turned; then as $02603C. */
void SevenFLandTurn(uint16_t obj)
{
    SevenFLimbs();
    SevenFLand(obj);
}

/* $026050. $9210 frames; then state $9212. States $164 and $180. */
void SevenFPause(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    wr16(obj + O_STATE, rd16(0x9212));
    wr16(0x9210, 0);
}

/* $026066. Rising by 4 (its boxes set) to below y $150; then the landing's shake, state $160
 * then $16C. */
void SevenFStand(uint16_t obj)
{
    add16(obj + O_Y, (uint16_t)-4);
    if (below16(obj + O_Y, 0x150)) {
        QueueSound(0xA6);
        wr16(obj + O_STATE, 0x160);
        wr16(0x9208, 0xFFFF);
        wr16(0x920A, 8);
        wr16(0x920C, rd16(obj + O_Y));
        wr16(0x9212, 0x16C);
        return;
    }
    wr8(0xCD01, 0x10);
    wr8(0xCEE1, 0x10);
    wr8(0xD121, 0x10);
    wr8(0xD241, 0x10);
    wr8(0xD361, 0x10);
    wr8(0xD481, 0x10);
    wr32(0xCD0C, 0xF808F002);
    wr32(0xCEEC, 0xF808F002);
    wr32(0xD12C, 0xFC04FC04);
    wr32(0xD24C, 0xFC04FC04);
    wr32(0xD36C, 0xFC04FC04);
    wr32(0xD48C, 0xFC04FC04);
    wr16(0xCD06, 8);
    wr16(0xCEE6, 8);
    wr16(0xD126, 8);
    wr16(0xD246, 8);
    wr16(0xD366, 8);
    wr16(0xD486, 8);
}

/* $0262CC. The next step's targets for the legs ($922A: 0, 2, 4 then round) and the arm (to
 * $C0 or $40: away from where it is, or by chance). */
void SevenFStride(void)
{
    uint16_t n = rd16(0x922A);
    if (n == 0) {
        add16(0x922A, 2);
        wr16(0x922C, 0x180);
        wr16(0x922E, 0x80);
        wr16(0x9230, 0);
        wr16(0x9232, 2);
    } else if (n == 4) {
        add16(0x922A, 2);
        wr16(0x922C, 0x80);
        wr16(0x922E, 0x180);
        wr16(0x9230, 2);
        wr16(0x9232, 0);
    } else {
        if (n == 2) {
            add16(0x922A, 2);
            wr16(0x9230, 2);
            wr16(0x9232, 0);
        } else {
            wr16(0x922A, 0);
            wr16(0x9230, 0);
            wr16(0x9232, 2);
        }
        wr16(0x922C, 0x100);
        wr16(0x922E, 0);
    }
    uint16_t a = rd16(0xCBB6);
    if (a != 0x40 && (a == 0xC0 || bit8(v_randomSeed + 1, 0))) {
        wr16(0x9226, 0x40);
        wr16(0x9228, 2);
    } else {
        wr16(0x9226, 0xC0);
        wr16(0x9228, 0);
    }
}

/* $0260FC. $9210 frames; then the walk (+4: $10 frames, the first stride). */
void SevenFWalkStart(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0x10);
    wr16(0x922A, 0);
    SevenFStride();
}

/* $026126. Climbing (y by -4) with the limbs turning; above y $30: the leap down (state $178,
 * at rest); otherwise every $9210 frames a step (its shake, state $17C, then $174 with
 * $9214 from the words at $261B6 by $A20C). */
void SevenFClimb(uint16_t obj)
{
    SevenFLimbs();
    add16(obj + O_Y, (uint16_t)-4);
    if (below16(obj + O_Y, 0x30)) {
        SevenFRest(obj);
        wr16(obj + O_STATE, 0x178);
        wr16(0x9226, 0x80);
        wr16(0x922C, 0x180);
        wr16(0x9230, 0);
        wr16(0x922E, 0x180);
        wr16(0x9232, 0);
        wr16(obj + 0x56, 0);
        return;
    }
    add16(0x9210, (uint16_t)-4);
    if (!neg16(rd16(0x9210))) return;
    QueueSound(0xA6);
    wr16(obj + O_STATE, 0x17C);
    wr16(0x9208, 0xFFFF);
    wr16(0x920A, 8);
    wr16(0x920C, rd16(obj + O_Y));
    wr16(0x9212, 0x174);
    wr16(0x9214, rom16(0x0261B6 + (uint32_t)(int16_t)rd16(0xA20C)));
}

/* $0261C6. The limbs turning for $9214 frames; then the next stride, back to the climb ($10
 * frames; from above y $E8, $200 frames and a sound). */
void SevenFStep(uint16_t obj)
{
    SevenFLimbs();
    add16(0x9214, 0xFFFF);
    if (!neg16(rd16(0x9214))) return;
    SevenFStride();
    add16(obj + O_STATE, (uint16_t)-4);
    if (!below16(obj + O_Y, 0xE8)) {
        wr16(0x9210, 0x10);
        return;
    }
    wr16(0x9210, 0x200);
    QueueSound(0xB5);
}

/* $0261FE. The leap down: falling ($4000 a frame to a speed of 4), the limbs turning below y
 * $120; at y $1E0: back up (state $168), facing by chance, at rest. */
void SevenFDrop(uint16_t obj)
{
    if (!below16(obj + O_Y, 0x1E0)) {
        wr32(obj + 0x1C, 0);
        wr16(obj + O_STATE, 0x168);
        SevenFFace(obj);
        SevenFRest(obj);
        return;
    }
    if (bit8(obj + 0x1C, 7) || below16(obj + 0x1C, 4)) add32(obj + 0x1C, 0x4000);
    if (!below16(obj + O_Y, 0x120)) SevenFLimbs();
}

/* ======================================================================================== */
/* ---- the boss's entrance, change of form and end (states $00-$5C) ---- */

/* the slot SlotFreeHigh will leave in a0 (the free one, or the last it looked at: $DE20) */
static uint16_t high_slot_peek(void)
{
    uint16_t a = 0xD400;
    for (int k = 0; rd16(a) && k < 27; k++) a = (uint16_t)(a + 0x60);
    return a;
}

/* $026726. The boss's carrier: 9 segments from itself (OrbiterSetup, sprite word $200), their
 * angles (the words at $2678A), the object at $DE80 (type $10, its mappings $E1984). */
void SevenCarrier(uint16_t obj)
{
    uint32_t look = 0;      /* a3 as the caller left it: the first entry sets it before any use */
    OrbiterSetupLow(obj, 0x027F82, 0x027FA6, 0x027FB0, 8, obj, &look);
    wr16(obj + O_TYPE, 0x24);
    for (uint16_t k = 0; k < 8; k++) wr16((uint16_t)(0xCA36 + 0x60 * k), rom16(0x02678A + 2 * k));
    wr16(0xDE80, 0x10);
    wr16(0xDE82, 0xEC00);
    wr16(0xDE8C, 0);
    wr32(0xDE88, 0x0E1984);
    wr8(0xDEA0, 0x10);
    wr16(0xDE8E, 0x8B9);
}

/* $0266C0. State 0: once $80C2 is 0: the music ($F6), its colours, the forms' random order
 * ($9200, $921C), their count by the difficulty ($921A, the words at $2679A); at x $240 coming
 * in at -2; its carrier. */
void SevenArrive(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    QueueSound(0xF6);
    add16(obj + O_STATE, 4);
    BossColours(0x027FC2);
    wr16(0x921E, 0);
    wr16(0x9220, 0);
    uint16_t r = rd16(v_randomSeed);
    wr16(0x9200, r & 0x1C);
    wr16(0x921C, (uint16_t)((int16_t)r >> 4) & 0x1C);
    wr16(0x921A, rom16(0x02679A + (uint32_t)(int16_t)rd16(v_difficulty)));
    wr16(obj + O_X, 0x240);
    wr32(obj + 0x18, 0xFFFE0000);
    SevenCarrier(obj);
}

/* the carrier's place: y $108 or $109 by the frame (bit 1), $DE94 $EC, $DE90 at its x */
static void seven_carrier_bob(uint16_t obj)
{
    wr16(obj + O_Y, (uint16_t)(((rd16(v_gameFrames) & 2) >> 1) + 0x108));
    wr16(0xDE94, 0xEC);
    wr16(0xDE90, rd16(obj + O_X));
}

/* $02693A. The spark at $DEE0 (type $10, mappings $8 0, still); its slot returned. */
uint16_t SevenSpark(void)
{
    wr16(0xDEE0, 0x10);
    wr16(0xDEE2, 0x8C00);
    wr8(0xDF00, 4);
    wr16(0xDEEE, 0x53B);
    wr16(0xDEE8, 0);
    wr16(0xDEEA, 0xFCFC);
    wr32(0xDEF8, 0);
    wr32(0xDEFC, 0);
    return 0xDEE0;
}

/* $0267A2. Coming in to x $160; there: stopped, $40 frames, a sound. */
void SevenArriveMove(uint16_t obj)
{
    if (below16(obj + O_X, 0x160)) {
        add16(obj + O_STATE, 4);
        wr32(obj + 0x18, 0);
        wr16(0x9210, 0x40);
        QueueSound(0x88);
    }
    seven_carrier_bob(obj);
}

/* $0267E4. $9210 frames; then the carrier opens ($DE88 $E1A58), the spark at x $14C, y $E0. */
void SevenArriveWait(uint16_t obj)
{
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) {
        wr16(0x9210, 0x40);
        add16(obj + O_STATE, 4);
        wr32(0xDE88, 0x0E1A58);
        wr16(0xDE8C, 0);
        uint16_t s = SevenSpark();
        wr16(s + O_X, 0x14C);
        wr16(s + O_Y, 0xE0);
    }
    seven_carrier_bob(obj);
}

/* $026818. Sparks at the spark for $9210 frames; then the carrier's look $E1BBA, the spark
 * hidden ($DEE2 $1000). */
void SevenArriveSparks(uint16_t obj)
{
    SparksAt(rd16(0xDEF0), rd16(0xDEF4));
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) { seven_carrier_bob(obj); return; }
    add16(obj + O_STATE, 4);
    wr32(0xDE88, 0x0E1BBA);
    wr16(0xDE8C, 0);
    wr16(0xDEE2, 0x1000);
}

/* $026848. Out to the left and down below x $C0; then the carrier thrown up, the gathering
 * (+$4C, +$4E its centre $C0, $F0; +$48 its next state $60, +$4A $A8 its radius). */
void SevenArriveOut(uint16_t obj)
{
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x2000);
    add32(obj + 0x1C, 0x400);
    if (!below16(obj + O_X, 0xC0)) {
        wr16(0xDE94, (uint16_t)(rd16(obj + O_Y) - 0x1C));
        wr16(0xDE90, rd16(obj + O_X));
        return;
    }
    add16(obj + O_STATE, 4);
    wr16(0xDE8E, rd16(0xDE8E) & 0xF7FF);
    wr32(0xDE88, 0x0E1AC2);
    wr16(0xDE8C, 0);
    wr32(0xDE9C, 0xFFFC0000);
    wr32(0xDE98, 0xFFFFC000);
    wr16(obj + 0x4C, 0xC0);
    wr16(obj + 0x4E, 0xF0);
    wr16(obj + 0x48, 0x60);
    wr16(obj + 0x4A, 0xA8);
}

/* $026BFA. The gathering: +$4A (the radius) down by 4; every other frame four motes (type $250)
 * at the corners round +$4C, +$4E; below 8: the form appears (state $28, $10 frames, its look
 * $F00). */
void SevenGather(uint16_t obj)
{
    add16(obj + 0x4A, (uint16_t)-4);
    if (below16(obj + 0x4A, 8)) {
        wr16(obj + O_STATE, 0x28);
        wr16(obj + 0x4A, 0x10);
        wr16(obj + O_FLAGS, 0x8080);
        wr16(obj + 0x0E, 0x6260);
        wr16(obj + 0x08, 0xF00);
        wr16(obj + 0x0A, 0xF0F0);
        return;
    }
    if (bit8(v_gameFrames + 1, 0)) return;
    for (uint16_t k = 0; k <= 3; k++) {
        uint16_t s = 0, t;
        if (!SlotFreeHigh(&s, &t)) return;
        wr16(s + O_TYPE, 0x250);
        uint16_t corner = rom16(0x026CB8 + 2 * k);
        wr16(s + 0x0E, rom16(0x026CC0 + 2 * k));
        wr16(s + O_FLAGS, 0xC000);
        wr32(s + 0x08, 0x0E0924);
        wr16(s + 0x48, 1);
        wr16(s + 0x4A, 0x10);
        uint16_t d = rd16(obj + 0x4A);
        if (corner & 1) d = (uint16_t)-d;
        wr16(s + O_X, (uint16_t)(d + rd16(obj + 0x4C)));
        d = rd16(obj + 0x4A);
        if (corner & 2) d = (uint16_t)-d;
        wr16(s + O_Y, (uint16_t)(d + rd16(obj + 0x4E)));
    }
}

/* $0268A4. The carrier falling away; the gathering; when the form appears: it at x $C0, y
 * $F0, still, the carrier hidden, the carrier's segments' (from $C9E0) flags cleared. */
void SevenArriveGather(uint16_t obj)
{
    add32(0xDE9C, 0x3000);
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x2000);
    add32(obj + 0x1C, 0x400);
    SevenGather(obj);
    if (rd16(obj + O_STATE) == 0x14) return;
    wr16(obj + O_X, 0xC0);
    wr16(obj + O_Y, 0xF0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(0xDE82, 0x1000);
    for (uint16_t s = 0xC9E0; s <= 0xCC80; s = (uint16_t)(s + 0x60)) wr16(s + O_FLAGS, 0);
}

/* $026AE0. The swirl's look by its side of the screen (x $120) and the stage's event. */
void SevenLook(uint16_t obj)
{
    wr16(obj + 0x0C, 0);
    if (below16(obj + O_X, 0x120)) {
        wr16(obj + 0x0E, 0x8B9);
        wr32(obj + 0x08, rd16(v_stageEvent) ? 0x0E1AC2 : 0x0E1B3E);
        return;
    }
    wr16(obj + 0x0E, 0xB9);
    wr32(obj + 0x08, rd16(v_stageEvent) ? 0x0E1B3E : 0x0E1AC2);
}

/* the change's start (shared by $026972 and $026D24): every object from $C9E0 gone, the joints
 * cleared, shown as the swirl */
static void seven_change_common(uint16_t obj)
{
    wr16(0x8014, 6);
    wr16(0x8016, 6);
    wr16(0x8018, 6);
    wr16(0x801A, 6);
    ObjectsVanishFrom(0xC9E0, 0, 0);
    SevenJointsClear();
    wr16(obj + O_FLAGS, 0xEC00);
    wr8(obj + 0x20, 4);
    SevenLook(obj);
}

/* $026972. State $18, beaten: the change of form starts: a sound, its colours, $30 frames
 * (+$50 its height, +$56 the debris's angle), the palette's flash ($8014-$801A). */
void SevenChange(uint16_t obj)
{
    QueueSound(0xBE);
    BossColours(0x027FC2);
    wr16(0x9236, 0);
    add16(obj + O_STATE, 4);
    wr16(obj + 0x4A, 0x30);
    wr16(obj + 0x50, rd16(obj + O_Y));
    wr16(obj + 0x56, 0x180);
    seven_change_common(obj);
}

/* $0269DE. The change: the palette flashing back, shaking, throwing debris every frame (type
 * $1E4 or $1EC by chance) round at +$56; after +$4A frames: +4. */
void SevenChangeShake(uint16_t obj)
{
    add16(obj + 0x4A, 0xFFFF);
    if (neg16(rd16(obj + 0x4A))) { add16(obj + O_STATE, 4); return; }
    Random();
    wr16(0x800A, 0x8000);
    wr16(0x8002, 0xF000);
    if (rd16(0x8014) && !(rd16(v_gameFrames) & 1) && below16(obj + 0x4A, 0x18)) {
        add16(0x8014, 0xFFFF);
        add16(0x8016, 0xFFFF);
        add16(0x8018, 0xFFFF);
        add16(0x801A, 0xFFFF);
    }
    AdjustPalettes();
    uint16_t y = rd16(obj + 0x50);
    if (!bit8(v_gameFrames + 1, 1)) y = (uint16_t)(y + 2);
    wr16(obj + O_Y, y);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 0x10E) & 0x1FE);
    uint16_t s = 0, t;
    if (!SlotFreeFromC980(&s, &t)) return;
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    uint16_t a = rd16(obj + 0x56);
    speed_at(s, a, a, 5);
    if (bit8(v_randomSeed + 1, 0)) { wr16(s + O_TYPE, 0x1EC); return; }
    wr16(s + O_TYPE, 0x1E4);
    wr16(s + 0x54, 0x8C00);
    wr32(s + 0x48, bit8(v_gameFrames + 1, 0) ? 0x0563EE : 0x05639C);
}

/* $026B32. The swirl moving 8 a frame to the next form's place (+$4C, +$4E), leaving a trail
 * (type $24C, every other frame); there: +4, $80 frames. */
void SevenChangeMove(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 0x0F)) SevenLook(obj);
    if (!bit8(v_gameFrames + 1, 0)) {
        uint16_t s = slot_from_d580();
        if (s) {
            wr16(s + O_TYPE, 0x24C);
            wr16(s + O_FLAGS, 0xE000);
            wr32(s + 0x08, rd32(obj + 0x08));
            wr16(s + 0x0C, rd16(obj + 0x0C));
            wr16(s + O_X, rd16(obj + O_X));
            wr16(s + O_Y, rd16(obj + O_Y));
            wr16(s + 0x0E, rd16(obj + 0x0E));
        }
    }
    int there = 0;
    uint16_t d = (uint16_t)(rd16(obj + O_X) - rd16(obj + 0x4C));
    if (!neg16((uint16_t)(d - 8))) add16(obj + O_X, (uint16_t)-8);
    else if (neg16((uint16_t)(d + 8))) add16(obj + O_X, 8);
    else { wr16(obj + O_X, rd16(obj + 0x4C)); there = 1; }
    d = (uint16_t)(rd16(obj + O_Y) - rd16(obj + 0x4E));
    if (!neg16((uint16_t)(d - 8))) { add16(obj + O_Y, (uint16_t)-8); return; }
    if (neg16((uint16_t)(d + 8))) { add16(obj + O_Y, 8); return; }
    wr16(obj + O_Y, rd16(obj + 0x4E));
    if (!there) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x4A, 0x80);
}

/* $026CC8. +$4A frames; then the next form (state +$48) with its fields and the boss's work
 * ($9202-$9216) cleared, its first step run. */
void SevenChangeDone(uint16_t obj)
{
    add16(obj + 0x4A, 0xFFFF);
    if (!neg16(rd16(obj + 0x4A))) return;
    wr16(obj + O_STATE, rd16(obj + 0x48));
    for (uint16_t f = 0x48; f <= 0x56; f += 2) wr16(obj + f, 0);
    for (uint16_t a = 0x9208; a <= 0x9216; a += 2) wr16(a, 0);
    wr16(0x9202, 0);
    SevenStep(obj);
}

/* $026D24. State $2C, the last form beaten: as the change, its colours. */
void SevenDefeat(uint16_t obj)
{
    BossColours(0x027FC2);
    add16(obj + O_STATE, 4);
    wr16(0x9236, 0);
    seven_change_common(obj);
}

/* $026D68. The players held ($A4C0, $A6C0 bit 7); $F0 frames; four explosions (type $29C) at
 * it. */
void SevenDefeatHold(uint16_t obj)
{
    set8(0xA4C0, 7);
    set8(0xA6C0, 7);
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0xF0);
    for (uint16_t k = 0; k <= 3; k++) {
        uint16_t s = 0, t;
        if (!SlotFreeHigh(&s, &t)) return;
        wr16(s + O_TYPE, 0x29C);
        wr16(s + O_X, rd16(obj + O_X));
        wr16(s + O_Y, rd16(obj + O_Y));
    }
}

/* $026DA2. Exploding: a sound every 8 frames, the sprites shaking, the palette flashing back;
 * after $9210 frames: the change (state $18) to the stage's event 6. */
void SevenDefeatBlast(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 7)) QueueSound(0xCB);
    wr16(v_shakeSprites, 8);
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) {
        wr16(obj + O_STATE, 0x18);
        wr16(v_stageEvent, 6);
        return;
    }
    wr16(0x800A, 0x8000);
    wr16(0x8002, 0xF000);
    if (rd16(0x8014) && !(rd16(v_gameFrames) & 1) && below16(0x9210, 0x48)) {
        add16(0x8014, 0xFFFF);
        add16(0x8016, 0xFFFF);
        add16(0x8018, 0xFFFF);
        add16(0x801A, 0xFFFF);
    }
    AdjustPalettes();
}

/* $026E16. The carrier back; $10 frames. */
void SevenCarrierBack(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    SevenCarrier(obj);
    wr16(0x9210, 0x10);
}

/* $0270FA. Rumbling (a sound every 8 frames until the stage is clear); every other frame a
 * piece of debris (type $100) thrown up from about x $120, y $168. */
void SevenRumble(void)
{
    if (!bit8(v_stageClear, 7) && !(rd16(v_gameFrames) & 7)) QueueSound(0xC5);
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t s = 0, t;
    if (!SlotFreeHigh(&s, &t)) return;
    uint16_t x = rd8(v_randomSeed) & 0x3F, y = rd8(v_randomSeed + 1) & 0x1F;
    if (bit8(0xFF0A, 7)) x = (uint16_t)~x;
    if (!bit8(0xFF0B, 7)) y = (uint16_t)~y;
    wr16(s + O_TYPE, 0x100);
    wr32(s + 0x48, 0x05639C);
    wr16(s + O_X, (uint16_t)(x + 0x120));
    wr16(s + O_Y, (uint16_t)(y + 0x168));
    wr32(s + 0x18, 0);
    wr32(s + 0x1C, 0xFFEE1000);
}

/* $0270EC. Sparks at the spark ($DEF0), rumbling. */
void SevenRumbleSparks(void)
{
    SparksAt(rd16(0xDEF0), rd16(0xDEF4));
    SevenRumble();
}

/* $0270A2. The spark (at $DEE0, until its state moves on) sparking and falling ($2000 a
 * frame); at y $120: stopped and hidden. The 68k does the falling through a0 as SparksAt left
 * it: on every eighth frame that is the slot SparksAt searched for its spark, not $DEE0. */
void SevenSparkFall(void)
{
    uint16_t s = 0xDEE0;
    if (rd16(s + O_STATE)) return;
    if (!(rd16(v_gameFrames) & 7)) s = high_slot_peek();
    SparksAt(rd16(0xDEF0), rd16(0xDEF4));
    add32(s + 0x1C, 0x2000);
    if (below16(s + O_Y, 0x120)) return;
    add16(s + O_STATE, 4);
    wr16(s + O_FLAGS, 0);
    wr32(s + 0x18, 0);
    wr32(s + 0x1C, 0);
}

/* $026E26. Rumbling $9210 frames; then the players' +$74 8, the carrier ($DE80) shown above
 * it, thrown up (-4). */
void SevenEndCarrier(uint16_t obj)
{
    SevenRumble();
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    add16(obj + O_STATE, 4);
    wr16(0xA474, 8);
    wr16(0xA674, 8);
    wr16(0xDE80, 0x10);
    wr16(0xDE82, 0xEC00);
    wr16(0xDE90, rd16(obj + O_X));
    wr16(0xDE94, rd16(obj + O_Y));
    add16(0xDE94, (uint16_t)-8);
    wr32(0xDE88, 0x0E1AC2);
    wr16(0xDE8C, 0);
    wr32(0xDE9C, 0xFFFC0000);
}

/* $026E76. The carrier falling back ($2000 a frame) onto it (y - $1C); then +4: closed
 * ($E1BBA), $80C2 $2C. */
void SevenEndCatch(uint16_t obj)
{
    SevenRumble();
    add32(0xDE9C, 0x2000);
    if (neg32(rd32(0xDE9C))) return;
    if (!neg16((uint16_t)((uint16_t)(rd16(obj + O_Y) - 0x1C) - rd16(0xDE94)))) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 8);
    wr32(0xDE9C, 0);
    wr16(0xDE8C, 0);
    wr32(0xDE88, 0x0E1BBA);
    wr16(0x80C2, 0x2C);
}

/* $026EBE. $9210 frames; then the carrier's look $E1984, $20 frames. */
void SevenEndClose(uint16_t obj)
{
    SevenRumble();
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0x20);
    wr16(0xDE8C, 0);
    wr32(0xDE88, 0x0E1984);
}

/* $026EE4. $9210 frames; then the carrier opening ($E1A58), the spark at x $140, y $AE rising
 * at 1. */
void SevenEndOpen(uint16_t obj)
{
    SevenRumble();
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 0x40);
    wr16(0xDE8C, 0);
    wr32(0xDE88, 0x0E1A58);
    uint16_t s = SevenSpark();
    wr16(s + O_X, 0x140);
    wr16(s + O_Y, 0xAE);
    wr32(s + 0x1C, 0xFFFF0000);
}

/* $026F22. The spark rising ($1000 a frame slower) out of the carrier (8 above $DE94); then
 * +4: the carrier closed, the spark at x $168, y $BA. */
void SevenEndSpark(uint16_t obj)
{
    SevenRumbleSparks();
    add32(0xDEFC, 0x1000);
    if (!neg16((uint16_t)((uint16_t)(rd16(0xDE94) - 8) - rd16(0xDEF4)))) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 8);
    wr16(0xDE8C, 0);
    wr32(0xDE88, 0x0E1BBA);
    wr16(0xDE8E, rd16(0xDE8E) & 0xF7FF);
    wr32(0xDEFC, 0);
    wr16(0xDEF0, 0x168);
    wr16(0xDEF4, 0xBA);
}

/* $026F6A. $9210 frames; then all thrown: the carrier ($E1C46, flipped, up at 2), the spark
 * (x $13C, y $B0), a lid at $DF40 (type $10, $E1C24) above it. */
void SevenEndThrow(uint16_t obj)
{
    SevenRumbleSparks();
    add16(0x9210, 0xFFFF);
    if (!neg16(rd16(0x9210))) return;
    add16(obj + O_STATE, 4);
    wr16(0x9210, 8);
    wr16(0xDE8C, 0);
    wr32(0xDE88, 0x0E1C46);
    wr16(0xDE8E, rd16(0xDE8E) | 0x800);
    wr32(0xDE9C, 0xFFFE0000);
    wr16(0xDEF0, 0x13C);
    wr16(0xDEF4, 0xB0);
    wr32(0xDEFC, 0xFFFE8000);
    wr32(0xDEF8, 0xFFFE0000);
    wr16(0xDEE4, 0);
    wr16(0xDF40, 0x10);
    wr16(0xDF42, 0xEC00);
    wr16(0xDF4E, 0xB9);
    wr16(0xDF50, rd16(obj + O_X));
    wr16(0xDF54, rd16(obj + O_Y));
    add16(0xDF54, (uint16_t)-0x34);
    wr32(0xDF48, 0x0E1C24);
    wr16(0xDF4C, 0);
    wr32(0xDF58, 0xFFFF4000);
    wr32(0xDF5C, 0xFFFFC000);
}

/* $027002. All falling ($2000 a frame); the carrier back on it: +4, it rising at 2.5, the
 * carrier's look $E1C6C. */
void SevenEndFall(uint16_t obj)
{
    SevenRumbleSparks();
    add32(0xDEFC, 0x2000);
    add32(0xDE9C, 0x2000);
    if (neg32(rd32(0xDE9C))) return;
    if (!neg16((uint16_t)((uint16_t)(rd16(obj + O_Y) - 0x1C) - rd16(0xDE94)))) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x1C, 0x28000);
    wr32(0xDE9C, 0);
    wr16(0xDE8C, 0);
    wr32(0xDE88, 0x0E1C6C);
}

/* $02704E. Carried up (slowing by $4000) with the carrier to y $80; then +4, $60 frames. */
void SevenEndRise(uint16_t obj)
{
    SevenRumble();
    SevenSparkFall();
    if (below16(obj + O_Y, 0x80)) {
        add16(obj + O_STATE, 4);
        wr16(0x9210, 0x60);
        wr32(obj + 0x1C, 0);
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x4000);
    wr16(0xDE94, (uint16_t)(rd16(obj + O_Y) - 0x1C));
}

/* $02708A. $9210 frames; then the stage clear (v_stageClear bit 7). */
void SevenEndDone(uint16_t obj)
{
    (void)obj;
    SevenRumble();
    SevenSparkFall();
    add16(0x9210, 0xFFFF);
    if (neg16(rd16(0x9210))) set8(v_stageClear, 7);
}

/* ======================================================================================== */
/* ---- type $C0 ($38464): a hopper chasing the player ---- */

/* $03857E. By the player's height (`player` from PlayerBySlot): $28 or more below it: the jump
 * down (state $C, $10 frames); $60 or more above: the leap up (state 8, y speed -6); each with
 * its animation from a0, the player's slot. */
void PouncerReach(uint16_t obj, uint32_t player)
{
    uint16_t d = (uint16_t)(rd16((uint16_t)player + O_Y) - rd16(obj + O_Y));
    if (!neg16(d)) {
        if (neg16((uint16_t)(d - 0x28))) return;
        SelectAnimationKeep(obj, player);
        wr16(obj + O_STATE, 0x0C);
        wr16(obj + 0x4A, 0x10);
        return;
    }
    if (!neg16((uint16_t)(d - 0xFFA0))) return;
    SelectAnimationKeep(obj, player);
    wr16(obj + O_STATE, 8);
    wr16(obj + 0x4A, 0xFFFF);
    wr16(obj + 0x1C, 0xFFFA);
}

/* on the floor: toward the player at $2.2 when $20 or more away, then by its height */
static void pouncer_chase(uint16_t obj)
{
    uint16_t player = PlayerBySlot(obj);
    uint16_t d = (uint16_t)(rd16(player + O_X) - rd16(obj + O_X));
    if (!neg16(d)) {
        if (!neg16((uint16_t)(d - 0x20))) wr32(obj + 0x18, 0x22000);
    } else if (neg16((uint16_t)(d - 0xFFE0))) {
        wr32(obj + 0x18, 0xFFFDE000);
    }
    PouncerReach(obj, 0xFFFF0000u | player);
}

/* $03847E. State 0: shown, its animation lists ($3883E); its animation from a0 as the jump
 * table left it: this routine's own address. */
void PouncerStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x8F00);
    wr16(obj + 0x56, 0);
    wr32(obj + 0x58, 0x03883E);
    wr32(obj + 0x5C, 0x03883E);
    SelectAnimationKeep(obj, 0x03847E);
}

/* $0384A4. State 4: facing its way (its box $70 / $90); on the floor: the chase; in the air:
 * by the player's height, else the jump down (state $C, $10 frames: its animation from the
 * player's slot). */
void PouncerRun(uint16_t obj)
{
    if (neg16(rd16(obj + 0x18))) {
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
        wr8(obj + 0x20, 0x90);
    } else {
        wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
        wr8(obj + 0x20, 0x70);
    }
    uint16_t d3, d4;
    if (!ObjectFloor(obj, 4, &d3, &d4)) { pouncer_chase(obj); return; }
    uint16_t player = PlayerBySlot(obj);
    PouncerReach(obj, 0xFFFF0000u | player);
    if (rd16(obj + O_STATE) != 4) return;
    wr16(obj + 0x4A, 0xFFFF);
    SelectAnimationKeep(obj, 0xFFFF0000u | player);
    wr16(obj + O_STATE, 0x0C);
    wr16(obj + 0x4A, 0x10);
}

/* $0384F6. State 8: rising ($4000 a frame slower); at the top: +4. */
void PouncerRise(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (!neg32(rd32(obj + 0x1C))) add16(obj + O_STATE, 4);
}

/* $038508. State $C: falling; after +$4A frames, on a floor: running again (state 4, its
 * animation from a0 as ObjectFloor left it), the chase. */
void PouncerFall(uint16_t obj)
{
    add16(obj + 0x4A, 0xFFFF);
    if (!neg16(rd16(obj + 0x4A))) { add32(obj + 0x1C, 0x4000); return; }
    uint16_t d3, d4;
    uint32_t a0 = 0x038508;     /* the jump table's target */
    if (ObjectFloorA0(obj, 4, &d3, &d4, &a0)) { add32(obj + 0x1C, 0x4000); return; }
    wr16(obj + O_STATE, 4);
    wr32(obj + 0x58, 0x03883E);
    wr32(obj + 0x5C, 0x03883E);
    SelectAnimationKeep(obj, a0);
    pouncer_chase(obj);
}

/* $038464. Type $C0: its state (the table at $3846E). */
void Pouncer(uint16_t obj)
{
    call_object(rom32(0x03846E + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $E4 ($385C8): a wreck falling in, burning; $E8 and $EC its parts on the boss ---- */

/* shown with its box ($B4), its animation started */
static void wreck_show(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr8(obj + 0x20, 0xB4);
    wr8(obj + 0x09, 0);
    wr16(obj + O_FLAGS, 0x8D00);
}

/* $0385F6. State 0: its two parts (types $E8, $EC: into whatever slot SlotFreeOrPuff gives,
 * found or not), moving right, $C0 frames, $140 to the left; shown. */
void WreckStart(uint16_t obj)
{
    uint16_t s, t, d1 = 0;
    uint32_t a1 = 0;
    SlotFreeOrPuff(&s, &t, &d1, &a1);
    wr16(s + O_TYPE, 0xE8);
    SlotFreeOrPuff(&s, &t, &d1, &a1);
    wr16(s + O_TYPE, 0xEC);
    wr16(obj + 0x18, 1);
    wr16(obj + 0x48, 0xC0);
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 0x140));
    wreck_show(obj);
}

/* $038634. State 4: animated (from a0 as the jump table left it), $48 frames; then thrown up
 * (-4.75), $30. */
void WreckFly(uint16_t obj)
{
    SelectAnimationKeep(obj, 0x038634);
    add16(obj + 0x48, 0xFFFF);
    if (!neg16(rd16(obj + 0x48))) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x1C, 0xFFFB4000);
    wr16(obj + 0x48, 0x30);
}

/* $038656. State 8: rising ($2000 a frame slower); at the top +4. */
void WreckRise(uint16_t obj)
{
    SelectAnimationKeep(obj, 0x038656);
    add32(obj + 0x1C, 0x2000);
    if (!neg32(rd32(obj + 0x1C))) add16(obj + O_STATE, 4);
}

/* $03866E. State $C: falling onto the floor; there +4, its animation restarted. */
void WreckFall(uint16_t obj)
{
    SelectAnimationKeep(obj, 0x03866E);
    add32(obj + 0x1C, 0x2000);
    uint16_t d3, d4;
    if (ObjectFloor(obj, 4, &d3, &d4)) return;
    add16(obj + O_STATE, 4);
    wr8(obj + 0x09, 0);
}

/* $038696. State $10: $48 frames; then burning ($168 frames; +$4C, +$50 its flames), 8 up, 2
 * left. */
void WreckLand(uint16_t obj)
{
    SelectAnimationKeep(obj, 0x038696);
    add16(obj + 0x48, 0xFFFF);
    if (!neg16(rd16(obj + 0x48))) return;
    wr16(obj + O_FLAGS, 0x300);
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x168);
    wr32(obj + 0x4C, 0x0565BA);
    wr16(obj + 0x50, 6);
    add16(obj + O_Y, (uint16_t)-8);
    add16(obj + O_X, (uint16_t)-2);
}

/* $0386CC. State $14: burning: the sprites shaking 1 in 4, every 8 frames a spark thrown (3 in
 * 4 at half speed); after $48 frames +4 ($80). */
void WreckBurn(uint16_t obj)
{
    add16(obj + 0x48, 0xFFFF);
    if (neg16(rd16(obj + 0x48))) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x48, 0x80);
        return;
    }
    if (!(rd8(v_randomSeed) & 3)) wr16(v_shakeSprites, 8);
    if (rd16(v_gameFrames) & 7) return;
    uint16_t s;
    if (!SparkThrow(obj, &s)) return;
    if (!(rd16(v_randomSeed) & 3)) return;
    wr32(s + 0x18, (uint32_t)((int32_t)rd32(s + 0x18) >> 1));
    wr32(s + 0x1C, (uint32_t)((int32_t)rd32(s + 0x1C) >> 1));
}

/* $03872A. State $18: $48 frames; then its broken look ($DF4A4), moving right at 3. */
void WreckBroken(uint16_t obj)
{
    add16(obj + 0x48, 0xFFFF);
    if (!neg16(rd16(obj + 0x48))) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x08, 0x0DF4A4);
    wr16(obj + O_FLAGS, 0xEF00);
    wr16(obj + 0x0E, 0x300);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x18, 3);
}

/* $038758. State $1C: falling to y $140; there still, its look $DFC34. */
void WreckDrop(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (below16(obj + O_Y, 0x140)) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x18, 0);
    wr16(obj + O_Y, 0x140);
    wr32(obj + 0x08, 0x0DFC34);
    wr16(obj + 0x0C, 0);
}

/* $038794. State $20: nothing. */
void WreckDone(uint16_t obj)
{
    (void)obj;
}

/* $0385C8. Type $E4: its state (the table at $385D2). */
void Wreck(uint16_t obj)
{
    call_object(rom32(0x0385D2 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $038796. Type $E8: a part held $10 left of the boss ($C980), animated (from a0: the boss's
 * slot); removed when the boss is in state $14. */
void WreckPart(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) { wreck_show(obj); return; }
    if (rd16(0xC984) == 0x14) { wr16(obj + O_FLAGS, 0x1000); return; }
    wr16(obj + O_X, (uint16_t)(rd16(0xC990) - 0x10));
    wr16(obj + O_Y, rd16(0xC994));
    SelectAnimationKeep(obj, 0xFFFFC980);
}

/* $0387CC. Type $EC: a flame held 8 left of the boss, $E above, flickering up and down by the
 * frame; removed when the boss is in state $14. */
void WreckFlame(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr8(obj + 0x20, 0xB0);
        add16(obj + O_STATE, 1);
        wr16(obj + 0x0E, 0x300);
        wr16(obj + O_FLAGS, 0xC500);
        wr32(obj + 0x08, 0x0DF4E4);
        return;
    }
    if (rd16(0xC984) == 0x14) { wr16(obj + O_FLAGS, 0x1000); return; }
    wr16(obj + O_X, (uint16_t)(rd16(0xC990) - 8));
    uint16_t d = (uint16_t)((int16_t)(rd16(v_gameFrames) & 0x0C) >> 1);
    wr16(obj + O_Y, (uint16_t)(rd16(0xC994) - 0x0E - d));
}

/* ======================================================================================== */
/* ---- type $280 ($3AC62): the stomper, a walker on two feet ($CC80, $CF80) with legs of
 * segments ($C9E0, $CCE0: +$54, +$56 the legs' angles, $9200-$9206 their bend), its eye at
 * $DFA0; its steps from the table at $3CADE (+$52 the step: a long each, the next move, the
 * attack, the eye's look). Then its pieces: types $340-$358. ---- */

#define STOMP_STEPS 0x03CADEu
#define FOOT_A      0xCC80u
#define FOOT_B      0xCF80u

/* the boss's look, $DFA0 (the eye), legs: each member of the group from it placed (GroupPlace,
 * 17), or (with $9230 bit 0) by the two feet: x from one (+$52 bit 4 which), y from the other */
static void stomper_place(uint16_t obj)
{
    if (!bit8(0x9230, 0)) { GroupPlace(obj, 0x10); return; }
    uint16_t ax, ay;
    if (!bit8(obj + 0x53, 4)) { ax = 0xC980; ay = rd16(obj + 0x50); }
    else { ay = 0xC980; ax = rd16(obj + 0x50); }
    uint16_t x = (uint16_t)(rd16(ax + O_X) - rd16(ax + 0x48)), y = (uint16_t)(rd16(ay + O_Y) - rd16(ay + 0x4C));
    uint16_t a = obj;
    for (uint16_t k = 0; k <= 0x10; k++, a = (uint16_t)(a + 0x60)) {
        if (a != ax) wr16(a + O_X, (uint16_t)(x + rd16(a + 0x48)));
        if (a != ay) wr16(a + O_Y, (uint16_t)(y + rd16(a + 0x4C)));
    }
}

/* the frame's common end: the eye, the chunks' DMA, the planes, the legs, the group placed */
static void stomper_frame(uint16_t obj)
{
    StomperEye(obj);
    StomperChunks();
    StomperPlanes(obj);
    StomperLegs(obj);
    stomper_place(obj);
}

/* a foot's look (three words from `looks`, its sprite word | mask) */
static void stomper_foot_look(uint16_t foot, uint32_t looks, uint16_t mask)
{
    wr16(foot + 0x0E, bus_rd16(looks));
    wr16(foot + 0x08, bus_rd16(looks + 2));
    wr16(foot + 0x0A, bus_rd16(looks + 4));
    wr16(foot + 0x0E, rd16(foot + 0x0E) | mask);
}

/* $03D3FA. Eight colours from `colours`: four into palette line 0's $02 on, four into its $16
 * on, each into the fade's target too. */
void StomperColours(uint32_t colours)
{
    for (uint16_t k = 0; k < 4; k++, colours += 2) {
        wr16((uint16_t)(0xE302 + 2 * k), bus_rd16(colours));
        wr16((uint16_t)(0xE382 + 2 * k), bus_rd16(colours));
    }
    for (uint16_t k = 0; k < 4; k++, colours += 2) {
        wr16((uint16_t)(0xE316 + 2 * k), bus_rd16(colours));
        wr16((uint16_t)(0xE396 + 2 * k), bus_rd16(colours));
    }
}

/* $03CAB2. A colour pair (at `at` and $80 on, the fade's target) set, or flashing $222 on the
 * frames with bit 1. */
void StomperGlow(uint16_t at, uint16_t colour)
{
    uint16_t c = bit8(v_gameFrames + 1, 1) ? 0x222 : colour;
    wr16(at, c);
    wr16((uint16_t)(at + 0x80), c);
}

/* $03CAD2. A colour pair set. */
void StomperGlowSet(uint16_t at, uint16_t colour)
{
    wr16(at, colour);
    wr16((uint16_t)(at + 0x80), colour);
}

/* $03BA0C. Its four glows ($9210-$9216) set dark. */
void StomperGlowsOff(void)
{
    wr16(0x9210, 6);
    wr16(0x9212, 0x260);
    wr16(0x9214, 0x66);
    wr16(0x9216, 0x620);
    StomperGlowSet(0xE336, rd16(0x9210));
    StomperGlowSet(0xE33A, rd16(0x9212));
    StomperGlowSet(0xE338, rd16(0x9214));
    StomperGlowSet(0xE320, rd16(0x9216));
}

/* $03BA2E. Its four glows lit, flashing. */
void StomperGlowsOn(void)
{
    wr16(0x9210, 0x2E);
    wr16(0x9212, 0x4C0);
    wr16(0x9214, 0xCE);
    wr16(0x9216, 0xE60);
    StomperGlow(0xE336, rd16(0x9210));
    StomperGlow(0xE33A, rd16(0x9212));
    StomperGlow(0xE338, rd16(0x9214));
    StomperGlow(0xE320, rd16(0x9216));
}

/* $03C9F6. The eye ($DFA0's sprite word) rolling through the words at $3CA0A by the frame. */
void StomperEyeRoll(uint16_t frames, uint16_t eye)
{
    wr16(eye + 0x0E, rom16(0x03CA0A + (uint32_t)(int16_t)(uint16_t)((int16_t)(frames & 0x18) >> 2)));
}

/* $03CA12. The eye: $C3D8 / $CBD8 by the frame's bit 4. */
void StomperEyeA(uint16_t frames, uint16_t eye)
{
    wr16(eye + 0x0E, (frames & 0x10) ? 0xCBD8 : 0xC3D8);
}

/* $03CA2A. $CBC0 / $CBCC. */
void StomperEyeB(uint16_t frames, uint16_t eye)
{
    wr16(eye + 0x0E, (frames & 0x10) ? 0xCBCC : 0xCBC0);
}

/* $03CA42. $C3B4 / $CBB4. */
void StomperEyeC(uint16_t frames, uint16_t eye)
{
    wr16(eye + 0x0E, (frames & 0x10) ? 0xCBB4 : 0xC3B4);
}

/* $03CA5A. $C3C0 / $C3CC. */
void StomperEyeD(uint16_t frames, uint16_t eye)
{
    wr16(eye + 0x0E, (frames & 0x10) ? 0xC3CC : 0xC3C0);
}

/* $03CA72. The eye hurt ($C3E4) while $920C counts down, then $C3F0. */
void StomperEyeHurt(uint16_t eye)
{
    if (neg16(rd16(0x920C))) { wr16(eye + 0x0E, 0xC3F0); return; }
    add16(0x920C, 0xFFFF);
    wr16(eye + 0x0E, 0xC3E4);
}

/* $03CA92. $C39C / $C3A8 by the frame's bit 2. */
void StomperEyeFlick(uint16_t frames, uint16_t eye)
{
    wr16(eye + 0x0E, (frames & 4) ? 0xC3A8 : 0xC39C);
}

/* $03C9C0. The eye's look: $C39C while flashing ($922C), else by +$5A (the table at $3C9DA). */
void StomperEye(uint16_t obj)
{
    uint16_t eye = 0xDFA0;
    if (!neg16(rd16(0x922C))) { wr16(eye + 0x0E, 0xC39C); return; }
    uint16_t f = rd16(v_gameFrames);
    uint32_t to = rom32(0x03C9DA + (uint32_t)(int16_t)rd16(obj + 0x5A));
    switch (to) {
    case 0x03C9F6: StomperEyeRoll(f, eye); break;
    case 0x03CA12: StomperEyeA(f, eye); break;
    case 0x03CA2A: StomperEyeB(f, eye); break;
    case 0x03CA42: StomperEyeC(f, eye); break;
    case 0x03CA5A: StomperEyeD(f, eye); break;
    case 0x03CA72: StomperEyeHurt(eye); break;
    case 0x03CA92: StomperEyeFlick(f, eye); break;
    default: call68k_missing(to); break;
    }
}

/* $03C8D0. One of its four parts' tiles a frame (by the frame & 3: $9224-$922A its frame of 3,
 * stepped toward $921C-$9222's choice: 1 closing, else opening) DMAed from RAM $FF3100 on. */
void StomperChunks(void)
{
    static const uint16_t step[4] = {0x9224, 0x9226, 0x9228, 0x922A}, want[4] = {0x921C, 0x921E, 0x9220, 0x9222},
                          vram[4] = {0xE000, 0xE010, 0xE400, 0xE410}, base[4] = {0, 0x40, 0x100, 0x140};
    uint16_t k = rd16(v_gameFrames) & 3, a = step[k], d1 = vram[k], d2 = base[k];
    if (rd16(a)) d2 = (uint16_t)(d2 + (rd16(a) == 2 ? 0x200 : 0x400));
    if (rd16(want[k]) == 1) {
        if (rd16(a)) add16(a, (uint16_t)-2);
    } else if (below16(a, 4)) {
        add16(a, 2);
    }
    for (uint16_t j = 0; j < 4; j++) {
        uint32_t src = rom32(0x03C9B0 + 4 * j);
        src = (src & 0xFFFF0000u) | (uint16_t)(src + d2);
        ChunkDma(src, d1);
        d1 = (uint16_t)(d1 + (j == 1 ? 0x1F8 : 8));
    }
}

/* $03C6C4. The planes' offsets ($A908, $A90C) from its place. */
void StomperPlanes(uint16_t obj)
{
    wr16(0xA908, (uint16_t)(~rd16(obj + O_X) - 0x140));
    wr16(0xA90C, (uint16_t)(~rd16(obj + O_Y) + 0xC0));
}

/* $03C5A8. A leg: seven segments' offsets (+$48 x, +$4C y, longs) each from the one before
 * (`from` on, into `to` on) at `radius` along an angle turning by `bend` (starting 4 bends
 * back from `angle`). */
void StomperChain(uint16_t angle, uint16_t radius, uint16_t bend, uint16_t from, uint16_t to, uint32_t sines,
                  uint32_t cosines)
{
    uint16_t a = (uint16_t)(angle - (uint16_t)(bend << 2)) & 0x1FE;
    for (uint16_t k = 0; k <= 6; k++) {
        int32_t s = (int16_t)bus_rd16(sines + (uint32_t)(int16_t)a), c = (int16_t)bus_rd16(cosines + (uint32_t)(int16_t)a);
        a = (uint16_t)(a + bend) & 0x1FE;
        uint32_t y = (uint32_t)(s * (int16_t)radius) << 2, x = (uint32_t)(c * (int16_t)radius) << 2;
        wr32(to + 0x48, x + rd32(from + 0x48));
        wr32(to + 0x4C, y + rd32(from + 0x4C));
        from = (uint16_t)(from + 0x60);
        to = (uint16_t)(to + 0x60);
    }
}

/* $03C558. The eye at it ($1E higher); both legs laid out. */
void StomperLegs(uint16_t obj)
{
    wr16(0xDFB0, rd16(obj + O_X));
    wr16(0xDFB4, rd16(obj + O_Y));
    add16(0xDFB4, 0xFFE2);
    StomperChain(rd16(obj + 0x54), rd16(0x9200), rd16(0x9204), 0xC9E0, 0xCA40, SINES, COSINES);
    StomperChain(rd16(obj + 0x56), rd16(0x9202), rd16(0x9206), 0xCCE0, 0xCD40, SINES, COSINES);
}

/* $03C470. Eight leg segments (type $10, shown, harmless) from `slot`, their sizes $40 down by
 * 4. */
void StomperLegSetup(uint16_t slot)
{
    uint8_t size = 0x40;
    for (uint16_t k = 0; k <= 7; k++, slot = (uint16_t)(slot + 0x60), size = (uint8_t)(size - 4)) {
        wr16(slot + O_TYPE, 0x10);
        wr16(slot + O_FLAGS, 0x8000);
        wr16(slot + 0x0E, 0x360);
        wr16(slot + 0x08, 0x500);
        wr16(slot + 0x0A, 0xF8F8);
        wr8(slot + 0x21, 0);
        wr8(slot + 0x20, size);
    }
}

/* $03C4AC. The eye (type $10, shown). */
void StomperEyeSetup(uint16_t slot)
{
    wr16(slot + O_TYPE, 0x10);
    wr16(slot + O_FLAGS, 0x8000);
    wr8(slot + 0x20, 0);
    wr16(slot + 0x0E, 0xC390);
    wr16(slot + 0x08, 0xE00);
    wr16(slot + 0x0A, 0xF0F4);
}

/* $03C4D0. A foot's look and its hit boxes. */
void StomperFootSetup(uint16_t slot)
{
    wr16(slot + 0x0E, 0x370);
    wr16(slot + 0x08, 0xD00);
    wr16(slot + 0x0A, 0xF0F8);
    wr8(slot + 0x21, 0x24);
    wr8(slot + 0x46, 0x8F);
    wr32(slot + 0x2C, 0xF808F808);
    wr32(slot + 0x28, 0xF808F808);
}

/* $03C448. Its legs, eye and feet set up. */
void StomperBuild(void)
{
    StomperLegSetup(0xC9E0);
    StomperLegSetup(0xCCE0);
    StomperEyeSetup(0xDFA0);
    StomperFootSetup(FOOT_A);
    StomperFootSetup(FOOT_B);
}

/* where a step's long puts `foot` (+$50 the foot stood on): x its high word >> 7, y * 2 */
static void stomper_foot_at(uint16_t obj, uint16_t foot, uint16_t step)
{
    wr16(obj + 0x50, foot);
    uint16_t w = (uint16_t)(rom32(STOMP_STEPS + (uint32_t)(int16_t)step) >> 16);
    wr16(foot + O_X, (uint16_t)((int16_t)w >> 7) & 0x1FE);
    wr16(foot + O_Y, (uint16_t)(w << 1) & 0x1FE);
}

/* $03C5EA. `foot` back at the step before (+$52 - 4). */
void StomperFootBack(uint16_t obj, uint16_t foot)
{
    stomper_foot_at(obj, foot, (uint16_t)(rd16(obj + 0x52) - 4) & 0x3C);
}

/* $03C5F8. The next step (+$52 on by 4), `foot` there. */
void StomperFootNext(uint16_t obj, uint16_t foot)
{
    wr16(obj + 0x52, (uint16_t)(rd16(obj + 0x52) + 4) & 0x3C);
    stomper_foot_at(obj, foot, rd16(obj + 0x52));
}

/* $03C62E. A foot down: the screen shaking, a thud. */
void StomperThud(void)
{
    wr16(v_shakeSprites, 0x10);
    wr16(v_shakePlane, 0x10);
    QueueSound(0xA6);
}

/* $03C69C. The step's move (its state, the step's long's low byte) and the eye's look (its top
 * nibble / 4 + 4). */
void StomperMove(uint16_t obj)
{
    uint32_t v = rom32(STOMP_STEPS + (uint32_t)(int16_t)rd16(obj + 0x52));
    wr16(obj + O_STATE, (uint16_t)v & 0xFC);
    wr16(obj + 0x5A, (uint16_t)(((int16_t)((uint16_t)v & 0xF000) >> 10) + 4));
}

/* $03C644. The next move ($8048 by 2 while it lasts), or the attack: the feet hold ($9230 bit
 * 0), its state from the words at $3C694 (by bits 9-11 of the step's long), $40 frames, the
 * attack's kind (+$5C, bits 12-15). */
void StomperNext(uint16_t obj)
{
    add16(0x8048, (uint16_t)-2);
    if (!neg16(rd16(0x8048))) { StomperMove(obj); return; }
    set8(0x9230, 0);
    wr16(0x922E, 0);
    uint32_t v = rom32(STOMP_STEPS + (uint32_t)(int16_t)rd16(obj + 0x52));
    uint16_t k = (uint16_t)((int16_t)((uint16_t)v & 0xE00) >> 8);
    wr16(obj + O_STATE, rom16(0x03C694 + (uint32_t)(int16_t)k));
    wr16(0x9208, 0x40);
    wr16(obj + 0x5C, (uint16_t)((int16_t)((uint16_t)v & 0xF000) >> 11));
    wr16(obj + 0x5A, 0);
}

/* $03C500. The core ($DE80) thrown out (type $27C) away from the middle; the attack's flag
 * ($8044 bit 7) cleared. */
void StomperCoreOut(uint16_t obj)
{
    wr16(0x804E, 0);
    clr8(0x8044, 7);
    wr16(0xDE80, 0x27C);
    wr16(0xDE84, 0);
    wr16(0xDE82, 0);
    wr16(0xDE90, rd16(0xDE90));
    wr16(0xDE94, rd16(0xDE94));
    wr32(0xDE9C, 0xFFFE0000);
    wr16(0xDEC8, 0);
    wr32(0xDE98, below16(obj + O_X, 0x120) ? 0x28000 : 0xFFFD8000);
}

/* $03B126. The H-interrupt's line table ($9600): +$48 + $33 lines counting down from +$48,
 * then $A90C to $97C0. */
void StomperLines(uint16_t obj)
{
    uint16_t a = 0x9600, v = rd16(obj + 0x48);
    uint16_t end = (uint16_t)(((uint16_t)(rd16(obj + 0x48) + 0x33) << 1) + 0x9600);
    do {
        v = (uint16_t)(v - 1);
        wr16(a, v);
        a = (uint16_t)(a + 2);
    } while ((int32_t)(int16_t)a - (int32_t)(int16_t)end < 0);
    v = rd16(0xA90C);
    do {
        wr16(a, v);
        a = (uint16_t)(a + 2);
    } while ((int32_t)(int16_t)a - (int32_t)(int16_t)0x97C0 < 0);
}

/* $03B210. Swaying (when the step's long has bit 8): $922E counting round $20; the legs'
 * angles turned by 2 one way or the other, their bends ($9204, $9206) from it. */
void StomperSway(uint16_t obj)
{
    uint32_t v = rom32(STOMP_STEPS + (uint32_t)(int16_t)rd16(obj + 0x52));
    if (!(v & 0x100)) return;
    uint16_t b6 = 0x10, b4 = 0x1F0;
    wr16(0x922E, (uint16_t)(rd16(0x922E) - 1) & 0x1F);
    uint16_t n = rd16(0x922E);
    if (!(n & 0x10)) {
        wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 2) & 0x1FE);
        wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 2) & 0x1FE);
        n = (uint16_t)((n & 0x0F) << 1);
    } else {
        wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 2) & 0x1FE);
        wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 2) & 0x1FE);
        n = (uint16_t)((((n ^ 0x1F) + 1) & 0x1F) << 1);
    }
    wr16(0x9206, (uint16_t)(b6 + n));
    wr16(0x9204, (uint16_t)(b4 - n));
}

/* $03D386. A shot of type $340 (its kind +$5C). */
void StomperShotA(uint16_t obj)
{
    uint16_t s = 0, t;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x340);
    wr16(s + 0x48, rd16(obj + 0x5C));
}

/* $03D38E. A shot of type $344. */
void StomperShotB(uint16_t obj)
{
    uint16_t s = 0, t;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x344);
    wr16(s + 0x48, rd16(obj + 0x5C));
}

/* four of `type` (+$48 0, 2, 4, 6; +$56 the kind) */
static void stomper_four(uint16_t obj, uint16_t type)
{
    uint16_t n = 0;
    for (uint16_t k = 0; k <= 3; k++, n = (uint16_t)(n + 2)) {
        uint16_t s = 0, t;
        if (!SlotFreeHigh(&s, &t)) return;
        wr16(s + O_TYPE, type);
        wr16(s + 0x48, n);
        wr16(s + 0x56, rd16(obj + 0x5C));
    }
}

/* $03D3A6. Four turrets (type $348). */
void StomperTurrets(uint16_t obj)
{
    stomper_four(obj, 0x348);
}

/* $03D3AE. Four rams (type $350). */
void StomperRams(uint16_t obj)
{
    stomper_four(obj, 0x350);
}

/* $03D3D6. The attack's time ($9208): `frames` plus the words at $3D3EA by $A20C. */
void StomperAttackTime(uint16_t frames)
{
    wr16(0x9208, (uint16_t)(frames + rom16(0x03D3EA + (uint32_t)(int16_t)rd16(0xA20C))));
}

/* $03C12E. The step's end: $9208 frames; then the next step, the first foot ($CC80) there. */
void StomperStepEnd(uint16_t obj)
{
    add16(0x9208, 0xFFFF);
    if (neg16(rd16(0x9208))) {
        add16(obj + O_STATE, 4);
        StomperFootNext(obj, FOOT_A);
    }
    stomper_frame(obj);
}

/* $03AC62. Type $280: the stomper: its colours flashing when hit ($922C), beaten (vitality
 * below 0, shown): 65816 points, the collapse (state $13C) with every part harmless and the
 * core ($DE80) gone; its state (the table at $3AD0E). */
void Stomper(uint16_t obj)
{
    if (rd16(obj + O_STATE)) {
        if (!neg16(rd16(0x922C))) {
            add16(0x922C, 0xFFFF);
            StomperColours(0x03D428);
        } else if (bit8(obj + 0x22, 5) && !bit8(obj + O_FLAGS, 7)) {
            wr16(0x922C, 2);
            StomperColours(0x03D438);
        }
        if (neg16(rd16(obj + O_VITALITY)) && !bit8(obj + O_FLAGS, 7)) {
            AddScore(obj, 0x65816);
            wr16(obj + O_STATE, 0x13C);
            wr16(obj + 0x48, 0xCC80);
            wr16(obj + 0x5A, 0x18);
            wr16(obj + O_VITALITY, 0);
            for (uint16_t a = 0xC980, k = 0; k <= 0x10; k++, a = (uint16_t)(a + 0x60)) wr8(a + 0x21, 0);
            if (rd16(0xDE80) == 0x27C) wr16(0xDE82, 0x1000);
        }
    }
    call_object(rom32(0x03AD0E + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $03AE9A. State 0: once $80C2 is 0: its colours, the plane's offset; two players: +4 ($60
 * frames, the second's entrance), else the start (state $120, $40). */
void StomperWait(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    wr16(0xE352, 0x866);
    wr16(0xE3D2, 0x866);
    wr16(0xE354, 0xCAA);
    wr16(0xE3D4, 0xCAA);
    wr16(0xE356, 0x422);
    wr16(0xE3D6, 0x422);
    wr16(0x922C, 0);
    wr16(0x922E, 0);
    wr16(0x9230, 0);
    wr16(0xA908, 0xA0);
    if (rd8(v_players) == 3) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x48, 0x60);
        return;
    }
    wr16(obj + O_STATE, 0x120);
    wr16(obj + 0x48, 0x40);
}

/* $03AF02. Two players: every other frame an object $2F8; after +$48 frames the other player
 * ($80D8) placed at x $C0, y $120, the start (state $120, $20). */
void StomperJoin(uint16_t obj)
{
    if (!bit8(v_gameFrames + 1, 0)) {
        uint16_t s = 0, t;
        if (SlotFreeHigh(&s, &t)) {
            wr16(s + O_TYPE, 0x2F8);
            wr16(s + 0x48, 0xC0);
            wr16(s + 0x4A, 0x120);
            wr16(s + 0x52, 1);
            wr16(s + 0x54, 2);
        }
    }
    add16(obj + 0x48, 0xFFFF);
    if (!neg16(rd16(obj + 0x48))) return;
    wr16(obj + O_STATE, 0x120);
    wr16(obj + 0x48, 0x20);
    if (rd16(0x80D8) == 0xA400) Player2Place(0xC0, 0x120, 0);
    else Player1Place(0xC0, 0x120, 0);
}

/* $03AF72. +$48 frames; then the boss's music ($80C2 $34, $80C8 $39A54, the next sound $F6
 * in $40). */
void StomperMusic(uint16_t obj)
{
    add16(obj + 0x48, 0xFFFF);
    if (!neg16(rd16(obj + 0x48))) return;
    add16(obj + O_STATE, 4);
    wr16(0x80C2, 0x34);
    wr32(0x80C8, 0x039A54);
    wr8(v_soundNext, 0xF6);
    wr16(v_soundDelay, 0x40);
}

/* $03AF9A. Once $80C2 is 0: +4. */
void StomperMusicWait(uint16_t obj)
{
    if (!rd16(0x80C2)) add16(obj + O_STATE, 4);
}

/* $03B156. Rising into view: $B0 lines, the planes' offsets, the H-interrupt effect $C, the
 * parts' frames reset. */
void StomperRiseStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0xB0);
    wr16(0xA908, 0xFD5B);
    wr16(0xA90C, 0xFFCC);
    wr16(v_hintEffect, 0x0C);
    wr16(v_hintState, 0);
    wr16(0x921C, 0);
    wr16(0x921E, 0);
    wr16(0x9220, 0);
    wr16(0x9222, 0);
    wr16(0x9224, 4);
    wr16(0x9226, 4);
    wr16(0x9228, 4);
    wr16(0x922A, 4);
    StomperLines(obj);
    StomperChunks();
}

/* $03AFA8. Rising (+$48 lines less a frame), rumbling; done: the H-interrupt effect off. */
void StomperRise(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 3)) QueueSound(0xD4);
    wr16(v_shakeSprites, 2);
    add16(obj + 0x48, 0xFFFF);
    if (neg16(rd16(obj + 0x48))) {
        add16(obj + O_STATE, 4);
        wr16(v_hintEffect, 0);
        wr16(v_hintState, 0);
    }
    StomperLines(obj);
    StomperChunks();
}

/* $03AFE0. Set up: vitality $1951, its boxes, its legs, eye and feet, at x $164, y $F3, the
 * step 4, its legs folded ($9204 $1A0, $9206 $60). */
void StomperSetup(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, 0xC980);
    wr16(obj + O_VITALITY, 0x1951);
    wr8(obj + 0x21, 0x24);
    wr8(obj + 0x46, 0x8F);
    wr32(obj + 0x2C, 0xE416E020);
    wr32(obj + 0x28, 0xDC24D030);
    wr16(obj + 0x5A, 0);
    wr16(0x921C, 1);
    wr16(0x921E, 1);
    wr16(0x9220, 0);
    wr16(0x9222, 0);
    wr16(0x9224, 0);
    wr16(0x9226, 0);
    wr16(0x9228, 4);
    wr16(0x922A, 4);
    StomperBuild();
    wr16(obj + 0x50, 0xC980);
    wr16(obj + O_X, 0x164);
    wr16(obj + O_Y, 0xF3);
    wr16(obj + 0x52, 4);
    wr16(obj + 0x54, 0x6C);
    wr16(obj + 0x56, 0x94);
    wr16(0xCA28, 0xFFDB);
    wr16(0xCA2C, 0x1E);
    wr16(0x9200, 8);
    wr16(0x9204, 0x1A0);
    wr16(0xCD28, 0x25);
    wr16(0xCD2C, 0x1E);
    wr16(0x9202, 8);
    wr16(0x9206, 0x60);
    wr16(0x9208, 0x20);
    wr16(obj + 0x5A, 0x14);
    wr16(0x920C, 0x18);
    stomper_frame(obj);
}

/* $03B0B6. Unfolding the legs ($9204 up, $9206 down by 4) to $10; then landed: shaking, the
 * players let go, $20 frames. */
void StomperUnfold(uint16_t obj)
{
    add16(0x9204, 4);
    add16(0x9206, (uint16_t)-4);
    if (rd16(0x9206) == 0x10) {
        wr16(v_shakeSprites, 0x10);
        wr16(v_shakePlane, 0x10);
        add16(obj + O_STATE, 4);
        wr16(obj + 0x5E, 0x20);
        wr16(obj + 0x50, 0xCC80);
        wr16(0xCC90, 0x142);
        wr16(0xCC94, 0x144);
        QueueSound(0x90);
        QueueSound(0xA6);
        clr8(0xA4C0, 7);
        clr8(0xA6C0, 7);
    }
    stomper_frame(obj);
}

/* $03B114. +$5E frames; then state $E8. */
void StomperReady(uint16_t obj)
{
    add16(obj + 0x5E, 0xFFFF);
    if (neg16(rd16(obj + 0x5E))) wr16(obj + O_STATE, 0xE8);
    stomper_frame(obj);
}

/* $03B1C8. $9208 frames; then the core thrown out. */
void StomperCoreWait(uint16_t obj)
{
    add16(0x9208, 0xFFFF);
    if (neg16(rd16(0x9208))) {
        add16(obj + O_STATE, 4);
        StomperCoreOut(obj);
    }
    stomper_frame(obj);
}

/* $03B1DC. When the core is back ($8044 bit 7): its count kept ($8048), $10 frames. */
void StomperCoreBack(uint16_t obj)
{
    if (bit8(0x8044, 7)) {
        add16(obj + O_STATE, 4);
        clr8(0x8044, 7);
        wr16(0x8048, rd16(0x8044));
        wr16(0x9208, 0x10);
    }
    stomper_frame(obj);
}

/* $03B200. $9208 frames; then the step's move. States $F0, $F4. */
void StomperMoveWait(uint16_t obj)
{
    add16(0x9208, 0xFFFF);
    if (neg16(rd16(0x9208))) StomperMove(obj);
    stomper_frame(obj);
}

/* an attack's end: back to state $E8 with the glow `at` set to `colour` */
static void stomper_attack_end(uint16_t obj, uint16_t at, uint16_t colour)
{
    clr8(0x9230, 0);
    wr16(obj + O_STATE, 0xE8);
    wr16(0x922E, 0);
    wr16(0x9208, 0x40);
    wr16(0x920C, 0x38);
    wr16(obj + 0x5A, 0x14);
    StomperGlowSet(at, colour);
    stomper_frame(obj);
}

/* the attack's glow flashing, the frame */
static void stomper_glow_frame(uint16_t obj, uint16_t at, uint16_t colour)
{
    StomperGlow(at, colour);
    stomper_frame(obj);
}

/* $03B294. Attack A: swaying $9208 frames; then +4 ($20). */
void StomperAttackA(uint16_t obj)
{
    StomperSway(obj);
    add16(0x9208, 0xFFFF);
    if (neg16(rd16(0x9208))) {
        add16(obj + O_STATE, 4);
        wr16(0x9208, 0x20);
    }
    stomper_glow_frame(obj, 0xE336, 0x2E);
}

/* $03B2BA. Firing type $340 every 4 frames; then +4 (its time from $170). */
void StomperAttackAFire(uint16_t obj)
{
    StomperSway(obj);
    if (!(rd16(0x9208) & 3)) StomperShotA(obj);
    add16(0x9208, 0xFFFF);
    if (neg16(rd16(0x9208))) {
        add16(obj + O_STATE, 4);
        StomperAttackTime(0x170);
    }
    stomper_glow_frame(obj, 0xE336, 0x2E);
}

/* $03B2E6. Swaying until its time is out and the sway is back; then the attack's end. */
void StomperAttackAEnd(uint16_t obj)
{
    StomperSway(obj);
    add16(0x9208, 0xFFFF);
    if (!neg16(rd16(0x9208)) || rd16(0x922E)) { stomper_glow_frame(obj, 0xE336, 0x2E); return; }
    stomper_attack_end(obj, 0xE336, 0x2E);
}

/* $03B32E. Attack B: swaying $9208 frames; then +4 ($30). */
void StomperAttackB(uint16_t obj)
{
    StomperSway(obj);
    add16(0x9208, 0xFFFF);
    if (neg16(rd16(0x9208))) {
        add16(obj + O_STATE, 4);
        wr16(0x9208, 0x30);
    }
    stomper_glow_frame(obj, 0xE33A, 0x4C0);
}

/* $03B354. Firing type $344 every 4 frames; then +4 (its time from $A0). */
void StomperAttackBFire(uint16_t obj)
{
    StomperSway(obj);
    if (!(rd16(0x9208) & 3)) StomperShotB(obj);
    add16(0x9208, 0xFFFF);
    if (neg16(rd16(0x9208))) {
        add16(obj + O_STATE, 4);
        StomperAttackTime(0xA0);
    }
    stomper_glow_frame(obj, 0xE33A, 0x4C0);
}

/* $03B380. As $03B2E6 for attack B. */
void StomperAttackBEnd(uint16_t obj)
{
    StomperSway(obj);
    add16(0x9208, 0xFFFF);
    if (!neg16(rd16(0x9208)) || rd16(0x922E)) { stomper_glow_frame(obj, 0xE33A, 0x4C0); return; }
    stomper_attack_end(obj, 0xE33A, 0x4C0);
}

/* $03B3C8. Attack C: four turrets; +4 (its time from $78). */
void StomperAttackC(uint16_t obj)
{
    StomperSway(obj);
    StomperTurrets(obj);
    add16(obj + O_STATE, 4);
    StomperAttackTime(0x78);
    stomper_glow_frame(obj, 0xE338, 0xCE);
}

/* $03B3EC. As $03B2E6 for attack C. */
void StomperAttackCEnd(uint16_t obj)
{
    StomperSway(obj);
    add16(0x9208, 0xFFFF);
    if (!neg16(rd16(0x9208)) || rd16(0x922E)) { stomper_glow_frame(obj, 0xE338, 0xCE); return; }
    stomper_attack_end(obj, 0xE338, 0xCE);
}

/* $03B434. Attack D: four rams; +4 (its time from $60). */
void StomperAttackD(uint16_t obj)
{
    StomperSway(obj);
    StomperRams(obj);
    add16(obj + O_STATE, 4);
    StomperAttackTime(0x60);
    stomper_glow_frame(obj, 0xE320, 0xE60);
}

/* $03B458. As $03B2E6 for attack D. */
void StomperAttackDEnd(uint16_t obj)
{
    StomperSway(obj);
    add16(0x9208, 0xFFFF);
    if (!neg16(rd16(0x9208)) || rd16(0x922E)) { stomper_glow_frame(obj, 0xE320, 0xE60); return; }
    stomper_attack_end(obj, 0xE320, 0xE60);
}

/* $03B4A0. Collapsing: its glows flashing; every 4 frames two parts (from +$48 and $300 on)
 * made debris (type $100), +$48 back by $60; at $C980: the music, $80 frames, the job at
 * $3B54C. */
void StomperCollapse(uint16_t obj)
{
    StomperGlowsOn();
    if (!(rd16(v_gameFrames) & 3)) {
        wr16(v_shakeSprites, 0x10);
        wr16(v_shakePlane, 0x10);
        QueueSound(0xBE);
        uint16_t a = rd16(obj + 0x48);
        for (int k = 0; k < 2; k++, a = (uint16_t)(a + 0x300)) {
            wr16(a + O_TYPE, 0x100);
            wr32(a + 0x48, 0x05639C);
            wr16(a + O_STATE, 0);
        }
        add16(obj + 0x48, (uint16_t)-0x60);
        if (rd16(obj + 0x48) == 0xC980) {
            QueueSound(0xF6);
            add16(obj + O_STATE, 4);
            wr16(obj + 0x48, 0x80);
            wr16(0x808A, 0x8000);
            wr16(0x921C, 0);
            wr16(0x921E, 0);
            wr16(0x9220, 0);
            wr16(0x9222, 0);
            uint32_t list = 0x03B54C;
            NextJob(&list);
        }
    }
    StomperEye(obj);
    StomperChunks();
}

/* $03B556. Shaking, puffs about it for +$48 frames; then every object from $C9E0 gone. */
void StomperCollapseEnd(uint16_t obj)
{
    StomperGlowsOn();
    wr16(v_shakeSprites, 8);
    wr16(v_shakePlane, 8);
    add16(obj + 0x48, 0xFFFF);
    if (neg16(rd16(obj + 0x48))) {
        add16(obj + O_STATE, 4);
        wr16(0xA908, 0xA0);
        ObjectsVanishFrom(0xC9E0, 0, 0);
        QueueSound(0x9B);
        return;
    }
    PuffNear(obj);
    StomperEye(obj);
    StomperChunks();
}

/* $03B686. A slot made debris (type $100) at the boss. */
void StomperDebrisAt(uint16_t obj, uint16_t slot)
{
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0563EE);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* the 68k's byte to a speed: sign-extended, swapped (the high word into the low) */
static uint32_t byte_speed(uint8_t b)
{
    uint32_t v = (uint32_t)(int32_t)(int8_t)b;
    return v << 16 | v >> 16;
}

/* $03B626. count+1 slots from `slot` made debris, thrown by the bytes at `dx` (right) and `dy`
 * (* $10000); the slot after returned. */
uint16_t StomperShards(uint16_t obj, uint16_t slot, uint32_t dx, uint32_t dy, uint16_t count)
{
    uint16_t k = count;
    do {
        StomperDebrisAt(obj, slot);
        wr32(slot + 0x18, byte_speed(bus_rd8(dx++)));
        wr32(slot + 0x1C, byte_speed(bus_rd8(dy++)) & 0xFFFF0000u);
        slot = (uint16_t)(slot + 0x60);
    } while (k-- != 0);
    return slot;
}

/* $03B59C. Burst: its glows dark, its body thrown up (-4.2) away from the wall, 25 pieces of
 * debris thrown (15 right, 10 left). */
void StomperBurst(uint16_t obj)
{
    StomperGlowsOff();
    add16(obj + O_STATE, 4);
    wr16(v_shakeSprites, 0x16);
    QueueSound(0xA3);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(obj + 0x0E, 0x8B00);
    wr32(obj + 0x08, 0x0E23B0);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x20, 4);
    wr32(obj + 0x18, below16(obj + O_X, 0x120) ? 0x18000 : 0x4000);
    wr32(obj + 0x1C, 0xFFFBCB00);
    uint16_t slot = StomperShards(obj, 0xC9E0, 0x03B6A0, 0x03B6AF, 0x0E);
    uint32_t dx = 0x03B6A5, dy = 0x03B6B4;
    for (uint16_t k = 0; k <= 9; k++, slot = (uint16_t)(slot + 0x60)) {
        StomperDebrisAt(obj, slot);
        wr32(slot + 0x18, (uint32_t)0 - byte_speed(bus_rd8(dx++)));
        wr32(slot + 0x1C, byte_speed(bus_rd8(dy++)) & 0xFFFF0000u);
    }
}

/* $03B6BE. Rising out of view (to y $20); then dropped back in at x $152, y $80 (down at 3),
 * its box and kind. */
void StomperBodyUp(uint16_t obj)
{
    if (!below16(obj + O_Y, 0x20)) return;
    add16(obj + O_STATE, 4);
    wr8(obj + 0x20, 0xBC);
    wr32(obj + 0x18, 0x8000);
    wr16(obj + O_X, 0x152);
    wr16(obj + O_Y, 0x80);
    wr32(obj + 0x1C, 0x30000);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xF010F010);
}

/* $03B6FE. Falling (x held below $1A0) to y $13C; there bounced (-2), +4 (into state $150:
 * $80C2 $2C). States $14C, $150, $170, $184. */
void StomperBodyBounce(uint16_t obj)
{
    if (!below16(obj + O_X, 0x1A0)) wr16(obj + O_X, 0x1A0);
    add32(obj + 0x1C, 0x4000);
    if (bit8(obj + 0x1C, 7) || below16(obj + O_Y, 0x13C)) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x1C, 0xFFFE0000);
    if (rd16(obj + O_STATE) == 0x150) wr16(0x80C2, 0x2C);
}

/* $03B748. Falling to y $13C; landed: still, $80 frames, its look $E22C0 (+4) or, in state $188,
 * $E22F0 (state $180). States $154, $174, $188. */
void StomperBodyLand(uint16_t obj)
{
    if (!below16(obj + O_X, 0x1A0)) wr16(obj + O_X, 0x1A0);
    add32(obj + 0x1C, 0x4000);
    if (bit8(obj + 0x1C, 7) || below16(obj + O_Y, 0x13C)) return;
    wr16(obj + 0x48, 0x80);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x18, 0);
    if (rd16(obj + O_STATE) == 0x188) {
        wr16(obj + O_STATE, 0x180);
        wr32(obj + 0x08, 0x0E22F0);
        wr16(obj + 0x0C, 0);
        return;
    }
    add16(obj + O_STATE, 4);
    wr32(obj + 0x08, 0x0E22C0);
    wr16(obj + 0x0C, 0);
}

/* the core at $DE80 placed at the body ($16 left, 4 up) */
static void stomper_core_at(uint16_t obj)
{
    wr16(0xDE90, rd16(obj + O_X));
    wr16(0xDE94, rd16(obj + O_Y));
    add16(0xDE90, 0xFFEA);
    add16(0xDE94, 0xFFFC);
}

/* $03B7BE. Lying (its look $E233A at $28 frames left); then opening ($E2320): the core
 * (type $10 at $DE80) shown at it, $20 frames. */
void StomperBodyOpen(uint16_t obj)
{
    if (rd16(obj + 0x48) == 0x28) {
        wr32(obj + 0x08, 0x0E233A);
        wr16(obj + 0x0C, 0);
    }
    add16(obj + 0x48, 0xFFFF);
    if (!neg16(rd16(obj + 0x48))) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x08, 0x0E2320);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x48, 0x20);
    wr16(0xDE80, 0x10);
    wr16(0xDE82, 0x8C00);
    wr16(0xDE8E, 0xC683);
    wr16(0xDE88, 0);
    wr16(0xDE8A, 0xFCFC);
    wr8(0xDEA0, 0);
    stomper_core_at(obj);
    wr32(0xDE98, 0);
    wr32(0xDE9C, 0);
}

/* $03B83E. Sparks at the core, $48 frames; then +4 ($20). */
void StomperCoreSparks(uint16_t obj)
{
    SparksAtDE80();
    add16(obj + 0x48, 0xFFFF);
    if (!neg16(rd16(obj + 0x48))) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x20);
}

/* $03B856. Sparks, $48 frames; then +4. */
void StomperCoreSparks2(uint16_t obj)
{
    SparksAtDE80();
    add16(obj + 0x48, 0xFFFF);
    if (neg16(rd16(obj + 0x48))) add16(obj + O_STATE, 4);
}

/* $03B868. Sparks; at +$48 4 (counting up): the core thrown out (up -4.75, left -0.75), its
 * look $E23C4. */
void StomperCoreThrow(uint16_t obj)
{
    SparksAtDE80();
    add16(obj + 0x48, 1);
    if (rd16(obj + 0x48) != 4) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x08, 0x0E23C4);
    wr16(obj + 0x0C, 0);
    wr32(0xDE98, 0xFFFF4000);
    wr32(0xDE9C, 0xFFFB4000);
}

/* $03B8A2. Sparks; the core falling to y $148; there: still, its spark (type $354 at $DEE0) 8
 * above it, the core moved to x $1D0; its look $E236A. */
void StomperCoreFall(uint16_t obj)
{
    SparksAtDE80();
    add32(0xDE9C, 0x2000);
    if (below16(0xDE94, 0x148)) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x08, 0x0E236A);
    wr16(obj + 0x0C, 0);
    wr32(0xDE9C, 0);
    wr32(0xDE98, 0);
    wr16(0xDEE0, 0x354);
    wr16(0xDEE4, 0);
    wr16(0xDEF0, rd16(0xDE90));
    wr16(0xDEF4, rd16(0xDE94));
    add16(0xDEF4, (uint16_t)-8);
    wr16(0xDE90, 0x1D0);
}

/* $03B906. Waiting to be hit (+$22 bit 5); then knocked up (-1.5) and right. States $16C,
 * $180. */
void StomperBodyWait(uint16_t obj)
{
    if (!bit8(obj + 0x22, 5)) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x08, 0x0E23B0);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0x4000);
    wr32(obj + 0x1C, 0xFFFE8000);
}

/* hit while lying: knocked up (-2.5) (state $170), the core moved off to x $1D0 */
static void stomper_knocked(uint16_t obj)
{
    wr16(obj + O_STATE, 0x170);
    wr32(obj + 0x08, 0x0E23B0);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0x4000);
    wr32(obj + 0x1C, 0xFFFD8000);
    wr16(0xDE90, 0x1D0);
}

/* $03B934. Lying (its look $E233A at $28 frames left); hit: knocked up; after +$48 frames:
 * opening, the core at it, $40 frames. */
void StomperBodyLie(uint16_t obj)
{
    if (rd16(obj + 0x48) == 0x28) {
        wr32(obj + 0x08, 0x0E233A);
        wr16(obj + 0x0C, 0);
    }
    if (bit8(obj + 0x22, 5)) { stomper_knocked(obj); return; }
    add16(obj + 0x48, 0xFFFF);
    if (!neg16(rd16(obj + 0x48))) return;
    wr32(obj + 0x08, 0x0E2320);
    wr16(obj + 0x0C, 0);
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x40);
    stomper_core_at(obj);
}

/* $03B9C4. Sparks; hit: knocked up; after +$48 frames the core thrown out as its last (type
 * $358). */
void StomperBodyLast(uint16_t obj)
{
    SparksAtDE80();
    if (bit8(obj + 0x22, 5)) { stomper_knocked(obj); return; }
    add16(obj + 0x48, 0xFFFF);
    if (!neg16(rd16(obj + 0x48))) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x08, 0x0E23C4);
    wr16(obj + 0x0C, 0);
    wr16(0xDE80, 0x358);
    wr16(0xDE84, 0);
    wr32(0xDE98, 0xFFFF4000);
    wr32(0xDE9C, 0xFFFB4000);
}

/* ---- the stomper's walk: its moves (states $08-$E4), each a few states sharing these ---- */

/* leaning one way (+$54 up, +$56 down by 2) to +$54 = $9210: foot A's look the second of
 * `looks`, at the end the first (and a thud) */
static void stomp_lean_a(uint16_t obj, uint32_t looks, uint16_t mask)
{
    wr16(obj + 0x50, FOOT_B);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 2) & 0x1FE);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 2) & 0x1FE);
    if (rd16(obj + 0x54) != rd16(0x9210)) {
        stomper_foot_look(FOOT_A, looks + 6, mask);
        stomper_frame(obj);
        return;
    }
    add16(obj + O_STATE, 4);
    wr16(0x9208, 0);
    stomper_foot_look(FOOT_A, looks, mask);
    StomperThud();
    stomper_frame(obj);
}

/* the other way (+$54 down, +$56 up): foot B's look; at the end $20 frames, foot B back a step */
static void stomp_lean_b(uint16_t obj, uint32_t looks, uint16_t mask)
{
    wr16(obj + 0x50, FOOT_A);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 2) & 0x1FE);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 2) & 0x1FE);
    if (rd16(obj + 0x54) != rd16(0x9210)) {
        stomper_foot_look(FOOT_B, looks + 6, mask);
        stomper_frame(obj);
        return;
    }
    add16(obj + O_STATE, 4);
    wr16(0x9208, 0x20);
    stomper_foot_look(FOOT_B, looks, mask);
    StomperThud();
    StomperFootBack(obj, FOOT_B);
    stomper_frame(obj);
}

/* $9208 frames; then the next move */
static void stomp_wait(uint16_t obj)
{
    add16(0x9208, 0xFFFF);
    if (neg16(rd16(0x9208))) StomperNext(obj);
    stomper_frame(obj);
}

/* leg A lifting: $9200 counted down; then the segment's word `field` = $9210, +$54 = $9212,
 * foot A's look */
static void stomp_lift_a(uint16_t obj, uint16_t field, uint32_t looks, uint16_t mask)
{
    wr16(obj + 0x50, FOOT_B);
    add16(0x9200, 0xFFFF);
    if (neg16(rd16(0x9200))) {
        add16(obj + O_STATE, 4);
        wr16(field, rd16(0x9210));
        wr16(obj + 0x54, rd16(0x9212));
        wr16(0x9200, 0);
        wr16(0x9204, 0);
        stomper_foot_look(FOOT_A, looks, mask);
    }
    stomper_frame(obj);
}

/* leg A swinging: $9200 up to $9210, $9204 down by 2 to $9212; there a thud, foot B back */
static void stomp_swing_a(uint16_t obj)
{
    if (neg16((uint16_t)(rd16(0x9200) - rd16(0x9210)))) add16(0x9200, 1);
    wr16(0x9204, (uint16_t)(rd16(0x9204) - 2) & 0x1FE);
    if (rd16(0x9204) == rd16(0x9212)) {
        add16(obj + O_STATE, 4);
        wr16(0x9208, 0);
        StomperThud();
        StomperFootBack(obj, FOOT_B);
    }
    stomper_frame(obj);
}

/* the stride with foot A down: $9200 by d1 to $9210, $9204 by d2 to $9212, $9206 by d3 to
 * $9214, +$54 by d4 to $9216, +$56 by d5 until $9218; foot B's look (| mask) */
static void stomp_stride_a(uint16_t obj, uint16_t d1, uint16_t d2, uint16_t d3, uint16_t d4, uint16_t d5,
                           uint32_t looks, uint16_t mask)
{
    if (rd16(0x9200) != rd16(0x9210)) add16(0x9200, d1);
    if (rd16(0x9204) != rd16(0x9212)) wr16(0x9204, (uint16_t)(rd16(0x9204) + d2) & 0x1FE);
    if (rd16(0x9206) != rd16(0x9214)) wr16(0x9206, (uint16_t)(rd16(0x9206) + d3) & 0x1FE);
    if (rd16(obj + 0x54) != rd16(0x9216)) wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + d4) & 0x1FE);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + d5) & 0x1FE);
    if (rd16(obj + 0x56) != rd16(0x9218)) {
        stomper_foot_look(FOOT_B, looks + 6, mask);
        stomper_frame(obj);
        return;
    }
    add16(obj + O_STATE, 4);
    stomper_foot_look(FOOT_B, looks, mask);
    wr16(0x9208, 0x20);
    StomperThud();
    StomperFootBack(obj, FOOT_B);
    stomper_frame(obj);
}

/* the stride with foot B down: $9202 by d1 to $9210, $9204, $9206 as above, +$56 by d4 to
 * $9216, +$54 by d5 until $9218; foot A's look */
static void stomp_stride_b(uint16_t obj, uint16_t d1, uint16_t d2, uint16_t d3, uint16_t d4, uint16_t d5,
                           uint32_t looks, uint16_t mask)
{
    if (rd16(0x9202) != rd16(0x9210)) add16(0x9202, d1);
    if (rd16(0x9204) != rd16(0x9212)) wr16(0x9204, (uint16_t)(rd16(0x9204) + d2) & 0x1FE);
    if (rd16(0x9206) != rd16(0x9214)) wr16(0x9206, (uint16_t)(rd16(0x9206) + d3) & 0x1FE);
    if (rd16(obj + 0x56) != rd16(0x9216)) wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + d4) & 0x1FE);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + d5) & 0x1FE);
    if (rd16(obj + 0x54) != rd16(0x9218)) {
        stomper_foot_look(FOOT_A, looks + 6, mask);
        stomper_frame(obj);
        return;
    }
    add16(obj + O_STATE, 4);
    stomper_foot_look(FOOT_A, looks, mask);
    wr16(0x9208, 0);
    StomperThud();
    StomperFootBack(obj, FOOT_B);
    stomper_frame(obj);
}

/* leg B lifting: $9202 counted down; then `field` = $9210, +$56 = $9212, foot B's look */
static void stomp_lift_b(uint16_t obj, uint16_t field, uint32_t looks, uint16_t mask)
{
    wr16(obj + 0x50, FOOT_A);
    add16(0x9202, 0xFFFF);
    if (neg16(rd16(0x9202))) {
        add16(obj + O_STATE, 4);
        wr16(field, rd16(0x9210));
        wr16(obj + 0x56, rd16(0x9212));
        wr16(0x9202, 0);
        wr16(0x9206, 0);
        stomper_foot_look(FOOT_B, looks, mask);
    }
    stomper_frame(obj);
}

/* leg B swinging: $9202 up to 8, $9206 up by 2 to $10; there $20 frames, a thud, foot B back */
static void stomp_swing_b(uint16_t obj)
{
    if (below16(0x9202, 8)) add16(0x9202, 1);
    add16(0x9206, 2);
    if (rd16(0x9206) == 0x10) {
        add16(obj + O_STATE, 4);
        wr16(0x9208, 0x20);
        wr16(0x9202, 8);
        StomperThud();
        StomperFootBack(obj, FOOT_B);
    }
    stomper_frame(obj);
}

/* leg A swinging the other way: $9200 up to $C, $9204 down by 2 to $1E8 */
static void stomp_swing_c(uint16_t obj)
{
    if (rd16(0x9200) != 0x0C) add16(0x9200, 1);
    wr16(0x9204, (uint16_t)(rd16(0x9204) - 2) & 0x1FE);
    if (rd16(0x9204) == 0x1E8) {
        add16(obj + O_STATE, 4);
        wr16(0x9208, 0);
        StomperThud();
        StomperFootBack(obj, FOOT_B);
    }
    stomper_frame(obj);
}

/* +$56 toward `a` by 4, $9206 toward `b` by 2 (sub-moves $3C6E6, $3C73A, $3C78E, $3C7E2) */
static void stomp_tuck_b(uint16_t obj, uint16_t a, uint16_t b)
{
    if (rd16(obj + 0x56) != a) wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 4) & 0x1FC);
    if (rd16(0x9206) != b) wr16(0x9206, (uint16_t)(rd16(0x9206) + 2) & 0x1FE);
}

/* +$54 toward `a` by 4, $9204 toward $1F0 by 2 (sub-moves $3C710, $3C764, $3C7B8, $3C80C) */
static void stomp_tuck_a(uint16_t obj, uint16_t a)
{
    if (rd16(obj + 0x54) != a) wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 4) & 0x1FC);
    if (rd16(0x9204) != 0x1F0) wr16(0x9204, (uint16_t)(rd16(0x9204) + 2) & 0x1FE);
}

/* $03C6E6. Leg B tucked ($E8, $28). */
void StomperTuck1(uint16_t obj) { stomp_tuck_b(obj, 0xE8, 0x28); }
/* $03C710. Leg A tucked ($E0). */
void StomperTuck2(uint16_t obj) { stomp_tuck_a(obj, 0xE0); }
/* $03C73A. Leg B tucked ($178, $20). */
void StomperTuck3(uint16_t obj) { stomp_tuck_b(obj, 0x178, 0x20); }
/* $03C764. Leg A tucked ($16C). */
void StomperTuck4(uint16_t obj) { stomp_tuck_a(obj, 0x16C); }
/* $03C78E. Leg B tucked ($1E8, $28). */
void StomperTuck5(uint16_t obj) { stomp_tuck_b(obj, 0x1E8, 0x28); }
/* $03C7B8. Leg A tucked ($1E0). */
void StomperTuck6(uint16_t obj) { stomp_tuck_a(obj, 0x1E0); }
/* $03C7E2. Leg B tucked ($78, $20). */
void StomperTuck7(uint16_t obj) { stomp_tuck_b(obj, 0x78, 0x20); }
/* $03C80C. Leg A tucked ($6C). */
void StomperTuck8(uint16_t obj) { stomp_tuck_a(obj, 0x6C); }

/* the bends for a lean: below `edge` $9204 by -4 and $9206 by 2, else the other way ($9204
 * masked first) */
static void stomp_bend_a(uint16_t obj, uint16_t edge)
{
    if (below16(obj + 0x54, edge)) {
        add16(0x9204, (uint16_t)-4);
        add16(0x9206, 2);
    } else {
        add16(0x9204, 4);
        add16(0x9206, (uint16_t)-2);
    }
    wr16(0x9204, rd16(0x9204) & 0x1FE);
    wr16(0x9206, rd16(0x9206) & 0x1FE);
}

/* the same with $9206 by -4 and $9204 by 2 (`first6`: $9206 masked first) */
static void stomp_bend_b(uint16_t obj, uint16_t edge, int first6)
{
    if (below16(obj + 0x54, edge)) {
        add16(0x9206, (uint16_t)-4);
        add16(0x9204, 2);
    } else {
        add16(0x9206, 4);
        add16(0x9204, (uint16_t)-2);
    }
    if (first6) wr16(0x9206, rd16(0x9206) & 0x1FE);
    wr16(0x9204, rd16(0x9204) & 0x1FE);
    if (!first6) wr16(0x9206, rd16(0x9206) & 0x1FE);
}

/* $03BA74. Move 1: leaning to $A4. */
void StomperLean1(uint16_t obj)
{
    stomp_bend_a(obj, 0x88);
    wr16(0x9210, 0xA4);
    stomp_lean_a(obj, 0x03CB1E, 0);
}

/* $03BAB0. The step's end. */
void StomperLean1End(uint16_t obj) { StomperStepEnd(obj); }

/* $03BAB4. Leaning back to $6C. */
void StomperLean1Back(uint16_t obj)
{
    stomp_bend_b(obj, 0x8A, 1);
    wr16(0x9210, 0x6C);
    stomp_lean_b(obj, 0x03CB1E, 0);
}

/* $03BAF0. Waiting. */
void StomperLean1Wait(uint16_t obj) { stomp_wait(obj); }

/* $03BAF4. Move 2: leg A lifted (its segment $CA2C), parts 1 and 3 set ($9200 8 first). */
void StomperStep2(uint16_t obj)
{
    if (rd16(0x9200) == 8) {
        wr16(0x921C, 0);
        wr16(0x9220, 1);
    }
    wr16(0x9210, 0xFFE2);
    wr16(0x9212, 0x98);
    StomperTuck1(obj);
    stomp_lift_a(obj, 0xCA2C, 0x03CB2A, 0);
}

/* $03BB2A. Leg A swung. */
void StomperStep2Swing(uint16_t obj)
{
    wr16(0x9210, 0x0F);
    wr16(0x9212, 0x1E4);
    StomperTuck1(obj);
    stomp_swing_a(obj);
}

/* $03BB3E. The step's end. */
void StomperStep2End(uint16_t obj)
{
    StomperTuck1(obj);
    StomperStepEnd(obj);
}

/* $03BB46. The stride on foot A. */
void StomperStride3(uint16_t obj)
{
    wr16(obj + 0x50, FOOT_A);
    wr16(0x9210, 8);
    wr16(0x9212, 0x1E8);
    wr16(0x9214, 0x18);
    wr16(0x9216, 0x6C);
    wr16(0x9218, 0x114);
    stomp_stride_a(obj, 0xFFFF, 2, 0xFFFE, 0xFFFE, 2, 0x03CB1E, 0);
}

/* $03BB80. Waiting. */
void StomperStride3Wait(uint16_t obj) { stomp_wait(obj); }

/* $03BB84. The stride on foot B. */
void StomperStride3B(uint16_t obj)
{
    wr16(obj + 0x50, FOOT_B);
    wr16(0x9210, 0x0C);
    wr16(0x9212, 0x1E0);
    wr16(0x9214, 0x18);
    wr16(0x9216, 0xF4);
    wr16(0x9218, 0x88);
    stomp_stride_b(obj, 1, 0xFFFE, 2, 0xFFFE, 2, 0x03CB2A, 0);
}

/* $03BBBE. The step's end; on foot A: parts 3 and 4 set. */
void StomperStride3End(uint16_t obj)
{
    StomperStepEnd(obj);
    if (rd16(obj + 0x50) != FOOT_A) return;
    wr16(0x9220, 0);
    wr16(0x9222, 1);
}

/* $03BBDA. Leg B lifted (its segment $CD28). */
void StomperStep4(uint16_t obj)
{
    wr16(0x9210, 0xFFDB);
    wr16(0x9212, 0x120);
    StomperTuck2(obj);
    stomp_lift_b(obj, 0xCD28, 0x03CB2A, 0);
}

/* $03BBFA. Leg B swung. */
void StomperStep4Swing(uint16_t obj)
{
    StomperTuck2(obj);
    stomp_swing_b(obj);
}

/* $03BC02. Waiting. */
void StomperStep4Wait(uint16_t obj)
{
    StomperTuck2(obj);
    stomp_wait(obj);
}

/* $03BC0A. Move 5: leaning to $FC. */
void StomperLean5(uint16_t obj)
{
    stomp_bend_a(obj, 0xEE);
    wr16(0x9210, 0xFC);
    stomp_lean_a(obj, 0x03CB2A, 0);
}

/* $03BC46. The step's end. */
void StomperLean5End(uint16_t obj) { StomperStepEnd(obj); }

/* $03BC4A. Leaning back to $E0. */
void StomperLean5Back(uint16_t obj)
{
    stomp_bend_b(obj, 0xF0, 0);
    wr16(0x9210, 0xE0);
    stomp_lean_b(obj, 0x03CB2A, 0);
}

/* $03BC86. Waiting. */
void StomperLean5Wait(uint16_t obj) { stomp_wait(obj); }

/* $03BC8A. Move 6: leg A lifted (its segment $CA28), parts 1 and 2 set ($9200 8 first). */
void StomperStep6(uint16_t obj)
{
    if (rd16(0x9200) == 8) {
        wr16(0x921C, 1);
        wr16(0x921E, 0);
    }
    wr16(0x9210, 0x25);
    wr16(0x9212, 0x10C);
    StomperTuck3(obj);
    stomp_lift_a(obj, 0xCA28, 0x03CB1E, 0x1000);
}

/* $03BCC2. Leg A swung the other way. */
void StomperStep6Swing(uint16_t obj)
{
    StomperTuck3(obj);
    stomp_swing_c(obj);
}

/* $03BCCA. The step's end. */
void StomperStep6End(uint16_t obj)
{
    StomperTuck3(obj);
    StomperStepEnd(obj);
}

/* $03BCD2. The stride on foot A. */
void StomperStride7(uint16_t obj)
{
    wr16(obj + 0x50, FOOT_A);
    wr16(0x9210, 8);
    wr16(0x9212, 0x1E8);
    wr16(0x9214, 0x18);
    wr16(0x9216, 0xEC);
    wr16(0x9218, 0x194);
    stomp_stride_a(obj, 0xFFFF, 2, 0xFFFE, 0xFFFE, 2, 0x03CB2A, 0);
}

/* $03BD0C. Waiting. */
void StomperStride7Wait(uint16_t obj) { stomp_wait(obj); }

/* $03BD10. The stride on foot B. */
void StomperStride7B(uint16_t obj)
{
    wr16(obj + 0x50, FOOT_B);
    wr16(0x9210, 0x0F);
    wr16(0x9212, 0x1D8);
    wr16(0x9214, 0x1C);
    wr16(0x9216, 0x168);
    wr16(0x9218, 0x11A);
    stomp_stride_b(obj, 1, 0xFFFE, 2, 0xFFFE, 2, 0x03CB1E, 0x1000);
}

/* $03BD4C. The step's end; on foot A: parts 1 and 3 set. */
void StomperStride7End(uint16_t obj)
{
    StomperStepEnd(obj);
    if (rd16(obj + 0x50) != FOOT_A) return;
    wr16(0x921C, 0);
    wr16(0x9220, 1);
}

/* $03BD68. Leg B lifted (its segment $CD2C). */
void StomperStep8(uint16_t obj)
{
    wr16(0x9210, 0xFFE2);
    wr16(0x9212, 0x194);
    StomperTuck4(obj);
    stomp_lift_b(obj, 0xCD2C, 0x03CB1E, 0x1000);
}

/* $03BD8A. Leg B swung. */
void StomperStep8Swing(uint16_t obj)
{
    StomperTuck4(obj);
    stomp_swing_b(obj);
}

/* $03BD92. Waiting. */
void StomperStep8Wait(uint16_t obj)
{
    StomperTuck4(obj);
    stomp_wait(obj);
}

/* $03BD9A. Move 9: leaning to $1A4 ($9206 by 2, $9204 by 4). */
void StomperLean9(uint16_t obj)
{
    if (below16(obj + 0x54, 0x188)) {
        add16(0x9206, (uint16_t)-2);
        add16(0x9204, 4);
    } else {
        add16(0x9206, 2);
        add16(0x9204, (uint16_t)-4);
    }
    wr16(0x9204, rd16(0x9204) & 0x1FE);
    wr16(0x9206, rd16(0x9206) & 0x1FE);
    wr16(0x9210, 0x1A4);
    stomp_lean_a(obj, 0x03CB1E, 0x1000);
}

/* $03BDD8. The step's end. */
void StomperLean9End(uint16_t obj) { StomperStepEnd(obj); }

/* $03BDDC. Leaning back to $16C ($9204 by 2, $9206 by 4). */
void StomperLean9Back(uint16_t obj)
{
    if (below16(obj + 0x54, 0x18A)) {
        add16(0x9204, (uint16_t)-2);
        add16(0x9206, 4);
    } else {
        add16(0x9204, 2);
        add16(0x9206, (uint16_t)-4);
    }
    wr16(0x9204, rd16(0x9204) & 0x1FE);
    wr16(0x9206, rd16(0x9206) & 0x1FE);
    wr16(0x9210, 0x16C);
    stomp_lean_b(obj, 0x03CB1E, 0x1000);
}

/* $03BE1A. Waiting. */
void StomperLean9Wait(uint16_t obj) { stomp_wait(obj); }

/* $03BE1E. Move 10: leg A lifted ($CA2C), parts 2 and 4 set ($9200 8 first). */
void StomperStep10(uint16_t obj)
{
    if (rd16(0x9200) == 8) {
        wr16(0x921E, 1);
        wr16(0x9222, 0);
    }
    wr16(0x9210, 0x1E);
    wr16(0x9212, 0x198);
    StomperTuck5(obj);
    stomp_lift_a(obj, 0xCA2C, 0x03CB2A, 0x800);
}

/* $03BE56. Leg A swung. */
void StomperStep10Swing(uint16_t obj)
{
    wr16(0x9210, 0x0F);
    wr16(0x9212, 0x1E4);
    StomperTuck5(obj);
    stomp_swing_a(obj);
}

/* $03BE6A. The step's end. */
void StomperStep10End(uint16_t obj)
{
    StomperTuck5(obj);
    StomperStepEnd(obj);
}

/* $03BE72. The stride on foot A. */
void StomperStride11(uint16_t obj)
{
    wr16(obj + 0x50, FOOT_A);
    wr16(0x9210, 8);
    wr16(0x9212, 0x1E8);
    wr16(0x9214, 0x18);
    wr16(0x9216, 0x16C);
    wr16(0x9218, 0x14);
    stomp_stride_a(obj, 0xFFFF, 2, 0xFFFE, 0xFFFE, 2, 0x03CB1E, 0x1000);
}

/* $03BEAE. Waiting. */
void StomperStride11Wait(uint16_t obj) { stomp_wait(obj); }

/* $03BEB2. The stride on foot B. */
void StomperStride11B(uint16_t obj)
{
    wr16(obj + 0x50, FOOT_B);
    wr16(0x9210, 0x0C);
    wr16(0x9212, 0x1E0);
    wr16(0x9214, 0x18);
    wr16(0x9216, 0x1F4);
    wr16(0x9218, 0x188);
    stomp_stride_b(obj, 1, 0xFFFE, 2, 0xFFFE, 2, 0x03CB2A, 0x800);
}

/* $03BEEE. The step's end; on foot A: parts 1 and 2 set. */
void StomperStride11End(uint16_t obj)
{
    StomperStepEnd(obj);
    if (rd16(obj + 0x50) != FOOT_A) return;
    wr16(0x921C, 1);
    wr16(0x921E, 0);
}

/* $03BF0A. Leg B lifted ($CD28). */
void StomperStep12(uint16_t obj)
{
    wr16(0x9210, 0x25);
    wr16(0x9212, 0x20);
    StomperTuck6(obj);
    stomp_lift_b(obj, 0xCD28, 0x03CB2A, 0x800);
}

/* $03BF2C. Leg B swung. */
void StomperStep12Swing(uint16_t obj)
{
    StomperTuck6(obj);
    stomp_swing_b(obj);
}

/* $03BF34. Waiting. */
void StomperStep12Wait(uint16_t obj)
{
    StomperTuck6(obj);
    stomp_wait(obj);
}

/* $03BF3C. Move 13: leaning to $1FC. */
void StomperLean13(uint16_t obj)
{
    stomp_bend_a(obj, 0x1EE);
    wr16(0x9210, 0x1FC);
    stomp_lean_a(obj, 0x03CB2A, 0x800);
}

/* $03BF7A. The step's end. */
void StomperLean13End(uint16_t obj) { StomperStepEnd(obj); }

/* $03BF7E. Leaning back to $1E0. */
void StomperLean13Back(uint16_t obj)
{
    stomp_bend_b(obj, 0x1F0, 0);
    wr16(0x9210, 0x1E0);
    stomp_lean_b(obj, 0x03CB2A, 0x800);
}

/* $03BFBC. Waiting. */
void StomperLean13Wait(uint16_t obj) { stomp_wait(obj); }

/* $03BFC0. Move 14: leg A lifted ($CA28), parts 3 and 4 set ($9200 8 first). */
void StomperStep14(uint16_t obj)
{
    if (rd16(0x9200) == 8) {
        wr16(0x9220, 0);
        wr16(0x9222, 1);
    }
    wr16(0x9210, 0xFFDB);
    wr16(0x9212, 0x0C);
    StomperTuck7(obj);
    stomp_lift_a(obj, 0xCA28, 0x03CB1E, 0);
}

/* $03BFF6. Leg A swung the other way. */
void StomperStep14Swing(uint16_t obj)
{
    StomperTuck7(obj);
    stomp_swing_c(obj);
}

/* $03BFFE. The step's end. */
void StomperStep14End(uint16_t obj)
{
    StomperTuck7(obj);
    StomperStepEnd(obj);
}

/* $03C006. The stride on foot A. */
void StomperStride15(uint16_t obj)
{
    wr16(obj + 0x50, FOOT_A);
    wr16(0x9210, 8);
    wr16(0x9212, 0x1E8);
    wr16(0x9214, 0x18);
    wr16(0x9216, 0x1EC);
    wr16(0x9218, 0x94);
    stomp_stride_a(obj, 0xFFFF, 2, 0xFFFE, 0xFFFE, 2, 0x03CB2A, 0x800);
}

/* $03C042. Waiting. */
void StomperStride15Wait(uint16_t obj) { stomp_wait(obj); }

/* $03C046. The stride on foot B. */
void StomperStride15B(uint16_t obj)
{
    wr16(obj + 0x50, FOOT_B);
    wr16(0x9210, 0x0F);
    wr16(0x9212, 0x1D8);
    wr16(0x9214, 0x1C);
    wr16(0x9216, 0x68);
    wr16(0x9218, 0x1A);
    stomp_stride_b(obj, 1, 0xFFFE, 2, 0xFFFE, 2, 0x03CB1E, 0);
}

/* $03C080. The step's end; on foot A: parts 2 and 4 set. */
void StomperStride15End(uint16_t obj)
{
    StomperStepEnd(obj);
    if (rd16(obj + 0x50) != FOOT_A) return;
    wr16(0x921E, 1);
    wr16(0x9222, 0);
}

/* $03C09C. Leg B lifted ($CD2C). */
void StomperStep16(uint16_t obj)
{
    wr16(0x9210, 0x1E);
    wr16(0x9212, 0x94);
    StomperTuck8(obj);
    stomp_lift_b(obj, 0xCD2C, 0x03CB1E, 0);
}

/* $03C0BC. Leg B swung. */
void StomperStep16Swing(uint16_t obj)
{
    StomperTuck8(obj);
    stomp_swing_b(obj);
}

/* $03C0C4. Waiting. */
void StomperStep16Wait(uint16_t obj)
{
    StomperTuck8(obj);
    stomp_wait(obj);
}

/* ---- the stomper's pieces: types $340-$358 ---- */

/* loc_03D28A: the object itself made a puff (type $100, its list $565F6), thrown in a random
 * one of 16 directions (DebrisPuff's end) */
static void become_puff(uint16_t obj)
{
    wr16(obj + O_STATE, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr8(obj + 0x21, 0);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0565F6);
    uint16_t k = rd16(v_randomSeed) & 0x1E0;
    wr32(obj + 0x1C, (uint32_t)(int32_t)sine(k) << 3);
    wr32(obj + 0x18, (uint32_t)(int32_t)cosine(k) << 3);
}

/* type $340's look by the frame (the words at $3CDAE), harmful on the frames with bit 1 */
static void ricochet_look(uint16_t obj)
{
    wr16(obj + 0x0E, rom16(0x03CDAE + (uint32_t)(rd16(v_gameFrames) & 6)));
    wr8(obj + 0x20, bit8(v_gameFrames + 1, 1) ? 0xB0 : 0);
}

/* $03CB36. Type $340: a ball bouncing round the walls (rumbling, the screen shaking); hidden
 * while +$4C counts down; its state (the table at $3CB82). */
void Ricochet(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 3)) QueueSound(0xCA);
    wr16(v_shakeSprites, 2);
    wr16(v_shakePlane, 2);
    uint16_t t = rd16(obj + 0x4C);
    if (t && !neg16(t)) {
        add16(obj + 0x4C, 0xFFFF);
        wr16(obj + O_FLAGS, 0);
        return;
    }
    wr16(obj + O_FLAGS, 0x8C00);
    call_object(rom32(0x03CB82 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $03CBA6. State 0: by its kind (+$48: the words at $3CBFE, $3CC06, $3CC0E) its first state
 * and place, 3 bounces, $20 frames hidden; a puff. */
void RicochetStart(uint16_t obj)
{
    wr16(obj + 0x08, 0xA00);
    wr16(obj + 0x0A, 0xF4F4);
    wr16(obj + 0x4A, 3);
    wr16(obj + 0x4C, 0x20);
    uint32_t k = (uint32_t)(int16_t)rd16(obj + 0x48);
    wr16(obj + O_STATE, rom16(0x03CBFE + k));
    wr16(obj + O_X, rom16(0x03CC06 + k));
    wr16(obj + O_Y, rom16(0x03CC0E + k));
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xF808F808);
    wr16(obj + 0x26, 3);
    DebrisPuff(obj);
}

/* $03CC16. Up ($1000 a frame faster) to y $C3; there turned left. */
void RicochetUp(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x1000);
    if (below16(obj + O_Y, 0xC3)) {
        wr16(obj + O_Y, 0xC3);
        wr32(obj + 0x18, rd32(obj + 0x1C));
        wr32(obj + 0x1C, 0);
        add16(obj + O_STATE, 4);
    }
    ricochet_look(obj);
}

/* $03CC44. Left to x $90; there a bounce used (none left: a puff), a puff. */
void RicochetLeft(uint16_t obj)
{
    if (!below16(obj + O_X, 0x90)) { ricochet_look(obj); return; }
    add16(obj + 0x4A, 0xFFFF);
    if (neg16(rd16(obj + 0x4A))) { become_puff(obj); return; }
    add16(obj + O_STATE, 4);
    wr32(obj + 0x18, 0);
    DebrisPuff(obj);
}

/* $03CC6A. Right ($1000 a frame faster) to x $186; there turned up. */
void RicochetRight(uint16_t obj)
{
    add32(obj + 0x18, 0x1000);
    if (below16(obj + O_X, 0x186)) { ricochet_look(obj); return; }
    wr16(obj + O_X, 0x186);
    wr32(obj + 0x1C, (uint32_t)0 - rd32(obj + 0x18));
    wr32(obj + 0x18, 0);
    add16(obj + O_STATE, 4);
    ricochet_look(obj);
}

/* $03CCA4. Up to y $98; there a bounce used, a puff. */
void RicochetTop(uint16_t obj)
{
    if (!below16(obj + O_Y, 0x98)) { ricochet_look(obj); return; }
    add16(obj + 0x4A, 0xFFFF);
    if (neg16(rd16(obj + 0x4A))) { become_puff(obj); return; }
    add16(obj + O_STATE, 4);
    wr32(obj + 0x1C, 0);
    DebrisPuff(obj);
}

/* $03CCCA. Down ($1000 a frame faster) to y $135; there turned right. */
void RicochetDown(uint16_t obj)
{
    add32(obj + 0x1C, 0x1000);
    if (below16(obj + O_Y, 0x135)) { ricochet_look(obj); return; }
    wr16(obj + O_Y, 0x135);
    wr32(obj + 0x18, rd32(obj + 0x1C));
    wr32(obj + 0x1C, 0);
    add16(obj + O_STATE, 4);
    ricochet_look(obj);
}

/* $03CCF8. Right to x $1B0; there a bounce used, a puff. */
void RicochetWall(uint16_t obj)
{
    if (below16(obj + O_X, 0x1B0)) { ricochet_look(obj); return; }
    add16(obj + 0x4A, 0xFFFF);
    if (neg16(rd16(obj + 0x4A))) { become_puff(obj); return; }
    add16(obj + O_STATE, 4);
    wr32(obj + 0x18, 0);
    DebrisPuff(obj);
}

/* $03CD1E. Left ($1000 a frame faster) to x $BA; there turned down. */
void RicochetBack(uint16_t obj)
{
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x1000);
    if (!below16(obj + O_X, 0xBA)) { ricochet_look(obj); return; }
    wr16(obj + O_X, 0xBA);
    wr32(obj + 0x1C, (uint32_t)0 - rd32(obj + 0x18));
    wr32(obj + 0x18, 0);
    add16(obj + O_STATE, 4);
    ricochet_look(obj);
}

/* $03CD58. Down to y $150; there a bounce used, a puff, round again (state 4). */
void RicochetFloor(uint16_t obj)
{
    if (below16(obj + O_Y, 0x150)) { ricochet_look(obj); return; }
    add16(obj + 0x4A, 0xFFFF);
    if (neg16(rd16(obj + 0x4A))) { become_puff(obj); return; }
    wr16(obj + O_STATE, 4);
    wr32(obj + 0x1C, 0);
    DebrisPuff(obj);
}

/* $03CDB6. Type $344: a dart between four points (a sound every 4 frames); its state (the
 * table at $3CDD6). */
void Darter(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 3)) QueueSound(0xC7);
    call_object(rom32(0x03CDD6 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $03CDE2. State 0: shown at its kind's point (+$48: the words at $3CE36, $3CE3E), 3 darts. */
void DarterStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x8C00);
    wr16(obj + 0x0E, 0x838F);
    wr16(obj + 0x08, 0);
    wr16(obj + 0x0A, 0xFCFC);
    wr16(obj + 0x4A, 3);
    uint32_t k = (uint32_t)(int16_t)rd16(obj + 0x48);
    wr16(obj + O_X, rom16(0x03CE36 + k));
    wr16(obj + O_Y, rom16(0x03CE3E + k));
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xFC04FC04);
    wr16(obj + 0x26, 2);
}

/* $03CE46. A puff; then darting (at 1) to the next point (+$48 on by 2; +$4C, +$4E). */
void DarterAim(uint16_t obj)
{
    wr16(v_shakeSprites, 2);
    wr16(v_shakePlane, 2);
    DebrisPuff(obj);
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) + 2) & 6);
    uint32_t k = (uint32_t)(int16_t)rd16(obj + 0x48);
    uint16_t x = rom16(0x03CE36 + k), y = rom16(0x03CE3E + k);
    wr16(obj + 0x4C, x);
    wr16(obj + 0x4E, y);
    int16_t dx = (int16_t)(x - rd16(obj + O_X));
    uint32_t dy = (uint16_t)(y - rd16(obj + O_Y));
    uint16_t a = (uint16_t)((int16_t)Atan2(dx, &dy) >> 7) & 0x1FE;
    speed_at(obj, a, a, 4);
}

/* $03CEBE. Darting; within 5 of the point: there (3 darts, then gone with a puff), state 4. */
void DarterFly(uint16_t obj)
{
    uint16_t d = (uint16_t)(rd16(obj + O_X) - rd16(obj + 0x4C));
    if (!neg16(d)) {
        if (!neg16((uint16_t)(d - 5))) return;
    } else if (neg16((uint16_t)(d + 5))) {
        return;
    }
    d = (uint16_t)(rd16(obj + O_Y) - rd16(obj + 0x4E));
    if (!neg16(d)) {
        if (!neg16((uint16_t)(d - 5))) return;
    } else if (neg16((uint16_t)(d + 5))) {
        return;
    }
    add16(obj + 0x4A, 0xFFFF);
    if (neg16(rd16(obj + 0x4A))) {
        wr16(obj + O_FLAGS, 0x1000);
        DebrisPuff(obj);
        return;
    }
    add16(obj + O_STATE, (uint16_t)-4);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_X, rd16(obj + 0x4C));
    wr16(obj + O_Y, rd16(obj + 0x4E));
}

/* $03CF30. Type $348: a turret; its state (the table at $3CF3A). */
void Spitter(uint16_t obj)
{
    call_object(rom32(0x03CF3A + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $03CF46. State 0: at its kind's place (+$48: the words at $3CF74, $3CF7C), $40 frames, $20
 * shots' time. */
void SpitterStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    uint32_t k = (uint32_t)(int16_t)rd16(obj + 0x48);
    wr16(obj + O_X, rom16(0x03CF74 + k));
    wr16(obj + O_Y, rom16(0x03CF7C + k));
    wr16(obj + 0x4A, 0x40);
    wr16(obj + 0x4C, 0x20);
}

/* $03CF84. +$4A frames; then +4. */
void SpitterWait(uint16_t obj)
{
    add16(obj + 0x4A, 0xFFFF);
    if (neg16(rd16(obj + 0x4A))) add16(obj + O_STATE, 4);
}

/* $03CF92. Firing every 4 frames for +$4C frames (type $34C, its speed by its kind: the words
 * at $3D054, $3D05C; upright or flat by +$49 bit 1); then gone. */
void SpitterFire(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 3)) QueueSound(0xB6);
    add16(obj + 0x4C, 0xFFFF);
    if (neg16(rd16(obj + 0x4C))) { wr16(obj + O_FLAGS, 0x1000); return; }
    if (rd16(obj + 0x4C) & 3) return;
    uint32_t k = (uint32_t)(int16_t)rd16(obj + 0x48);
    uint16_t s = 0, t;
    if (!SlotFreeHigh(&s, &t)) return;
    wr16(s + O_TYPE, 0x34C);
    wr8(s + 0x21, 0x10);
    wr16(s + 0x26, 2);
    wr16(s + 0x18, rom16(0x03D054 + k));
    wr16(s + 0x1C, rom16(0x03D05C + k));
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
    if (bit8(obj + 0x49, 1)) {
        wr16(s + 0x0E, 0x848A);
        wr16(s + 0x08, 0x800);
        wr16(s + 0x0A, 0xF6FE);
        wr16(s + O_FLAGS, 0x8800);
        wr32(s + 0x2C, 0xFF01F808);
        return;
    }
    wr16(s + 0x0E, 0x84AB);
    wr16(s + 0x08, 0x200);
    wr16(s + 0x0A, 0xFEF6);
    wr16(s + O_FLAGS, 0x8400);
    wr32(s + 0x2C, 0xF808FF01);
}

/* $03D064. Type $34C: the turret's shot, a puff past x $A0-$1A0 or y $C0-$140 (its way). */
void SpitterShot(uint16_t obj)
{
    if (rd16(obj + 0x18)) {
        if (bit8(obj + 0x18, 7)) {
            if (below16(obj + O_X, 0xA0)) become_puff(obj);
        } else if (!below16(obj + O_X, 0x1A0)) {
            become_puff(obj);
        }
        return;
    }
    if (bit8(obj + 0x1C, 7)) {
        if (below16(obj + O_Y, 0xC0)) become_puff(obj);
    } else if (!below16(obj + O_Y, 0x140)) {
        become_puff(obj);
    }
}

/* $03D0B0. Type $350: a ram; its state (the table at $3D0BA). */
void Rammer(uint16_t obj)
{
    call_object(rom32(0x03D0BA + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $03D0C2. State 0: gone if its kind (+$48) is the attack's (+$56); else shown at its place
 * (the words at $3D16A, $3D172), moving in slowly (by +$49 bits 1, 2), $40 frames. */
void RammerStart(uint16_t obj)
{
    uint16_t k = rd16(obj + 0x48);
    if (k == rd16(obj + 0x56)) { wr16(obj + O_FLAGS, 0x1000); return; }
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x8C00);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr16(obj + 0x0E, 0x85BC);
    wr32(obj + 0x2C, 0xD030D030);
    wr16(obj + 0x26, 0x12);
    wr8(obj + 0x23, 0x20);
    wr16(obj + O_X, rom16(0x03D16A + (uint32_t)(int16_t)k));
    wr16(obj + O_Y, rom16(0x03D172 + (uint32_t)(int16_t)k));
    wr16(obj + 0x4A, 0x40);
    if (bit8(obj + 0x49, 1)) {
        wr32(obj + 0x18, bit8(obj + 0x49, 2) ? 0xFFFF4000 : 0xC000);
        return;
    }
    wr32(obj + 0x1C, bit8(obj + 0x49, 2) ? 0xC000 : 0xFFFF4000);
}

/* $03D17A. +$4A frames (at the last its kind $10); then bursting: shaking, gone, 8 puffs
 * (type $100) thrown round. */
void RammerBurst(uint16_t obj)
{
    add16(obj + 0x4A, 0xFFFF);
    if (!rd16(obj + 0x4A)) {
        wr8(obj + 0x21, 0x10);
        return;
    }
    if (!neg16(rd16(obj + 0x4A))) return;
    wr16(v_shakeSprites, 0x10);
    wr16(v_shakePlane, 0x10);
    wr16(obj + O_FLAGS, 0x1000);
    QueueSound(0xCE);
    uint16_t a = 0;
    for (uint16_t k = 0; k <= 7; k++, a = (uint16_t)(a + 0x40)) {
        uint16_t s = 0, t;
        if (!SlotFreeHigh(&s, &t)) return;
        wr16(s + O_TYPE, 0x100);
        wr32(s + 0x48, 0x05663A);
        wr16(s + O_X, rd16(obj + O_X));
        wr16(s + O_Y, rd16(obj + O_Y));
        speed_at(s, a, a, 3);
    }
}

/* $03D1FC. Type $358: the stomper's core thrown out last: sparking, falling ($2000 a frame),
 * bouncing on y $144 (each bounce half as high, faster right) until it stops: the scene's
 * signal, $C0 frames, the stage clear. */
void StomperHeart(uint16_t obj)
{
    SparksAtDE80();
    if (rd16(obj + O_STATE)) {
        add16(obj + 0x48, 0xFFFF);
        if (neg16(rd16(obj + 0x48))) set8(v_stageClear, 7);
        return;
    }
    add32(obj + 0x1C, 0x2000);
    if (bit8(obj + 0x1C, 7) || below16(obj + O_Y, 0x144)) return;
    QueueSound(0xDC);
    uint32_t v = (uint32_t)((int32_t)rd32(obj + 0x1C) >> 1);
    if (neg32(v - 0x4000)) {
        add16(obj + O_STATE, 4);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x48, 0xC0);
        set8(v_sceneSignal, 0);
        return;
    }
    wr32(obj + 0x1C, (uint32_t)0 - v);
    if (below32(obj + 0x18, 0x1000)) return;
    add32(obj + 0x18, 0xE00);
}

/* ---- shared helpers ---- */

/* $04DA2A. A pose (9 joints) over `frames` frames (kept in +$4A), form A's segments turned,
 * then bobbing: while the pose runs, the y speed eased to 0 by $1400; after it, up (to -$1.2,
 * above y $C0 turned back) or down (to $1.2, below $140 turned back) by $1000 ($8050 bit 1 the
 * way, changed now and then by chance at full speed). The bobbing is code of type $4D196's
 * ($4E15C). */
void SevenPoseBob(uint16_t obj, uint32_t pose, uint16_t frames)
{
    SevenPoseTimed(obj, pose, frames, 8);
    SevenAPoseRun(obj);
    if (!neg16(rd16(obj + 0x4A))) {
        if (!bit8(obj + 0x1C, 7)) {
            if (below32(obj + 0x1C, 0x2000)) wr32(obj + 0x1C, 0);
            else wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x1400);
        } else {
            if (!below32(obj + 0x1C, 0xFFFFE000)) wr32(obj + 0x1C, 0);
            else add32(obj + 0x1C, 0x1400);
        }
        return;
    }
    int full;
    if (bit8(0x8050, 1)) {
        if (!below16(obj + O_Y, 0x140)) { wr8(0x8050, rd8(0x8050) ^ 2); return; }
        full = !below32(obj + 0x1C, 0x12000);
        if (!full) { add32(obj + 0x1C, 0x1000); return; }
    } else {
        if (below16(obj + O_Y, 0xC0)) { wr8(0x8050, rd8(0x8050) ^ 2); return; }
        full = below32(obj + 0x1C, 0xFFFEE000);
        if (!full) { wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x1000); return; }
    }
    if (rd16(v_randomSeed) & 0x3F) return;
    if (bit8(v_randomSeed, 7)) clr8(0x8050, 1);
    else set8(0x8050, 1);
}

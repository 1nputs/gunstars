/* Figures and small enemies of the stages (the types from $37000 to $38900). */
#include "game.h"

/* ---- type $BC: a figure that walks and hops, stands, gives off sparks, or bounces; its kind
 * (+6) picks which ---- */

/* loc_038830: a fixed look: the sprite word and the two words at +8 */
static void figure_look(uint16_t obj, uint32_t look)
{
    wr16(obj + 0x0E, bus_rd16(look));
    wr16(obj + 0x08, bus_rd16(look + 2));
    wr16(obj + 0x0A, bus_rd16(look + 4));
}

/* $0381E8 */
void BouncerStart(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8F00);
    wr8(obj + 0x20, 0xA0);
    wr8(obj + 0x09, 0);
    uint16_t k = rd16(obj + 6);
    wr16(obj + O_STATE, rom16(0x038236 + (uint32_t)(int16_t)k));
    k = (uint16_t)(k << 1);
    wr32(obj + 0x18, rom32(0x038240 + (uint32_t)(int16_t)k));
    wr32(obj + 0x58, rom32(0x038254 + (uint32_t)(int16_t)k));
    wr32(obj + 0x5C, rd32(obj + 0x58));
    wr16(obj + 0x56, 0);
    FrameList(obj);
}

/* loc_03844E: a hop */
static void bouncer_hop(uint16_t obj)
{
    wr16(obj + 0x1C, 0xFFFB);
    wr16(obj + O_STATE, 8);
    figure_look(obj, 0x038874);
}

/* $038268. Walking (facing its way): off the floor it falls, or hops on every $40th frame; on
 * it, hops every $40 frames. */
void BouncerWalk(uint16_t obj)
{
    FrameList(obj);
    if (rd16(obj + 0x18) & 0x8000) wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
    else wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
    uint16_t d3, d4;
    if (ObjectFloor(obj, 4, &d3, &d4)) {
        if (rd16(v_gameFrames) & 0x40) {
            bouncer_hop(obj);
            return;
        }
        wr16(obj + O_STATE, 0xC);
        return;
    }
    if (!(rd16(obj + 0x48) & 0x3F)) bouncer_hop(obj);
}

/* $0382A2. Rising, until it falls. */
void BouncerRise(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
}

/* $0382B4. Falling, until the floor: then walking again, its way by the frame count. Its
 * animation is stepped from what a0 holds then, which is no animation table: the floor kind's
 * routine, or this routine (the jump table's target) when the floor check takes no kind. */
void BouncerFall(uint16_t obj)
{
    uint16_t d3, d4;
    uint32_t a0 = 0x0382B4;
    if (ObjectFloorA0(obj, 4, &d3, &d4, &a0)) {
        wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
        return;
    }
    wr32(obj + 0x58, 0x03883E);
    wr32(obj + 0x5C, 0x03883E);
    wr16(obj + 0x56, 0);
    SelectAnimationKeep(obj, a0);
    wr16(obj + O_STATE, 4);
    wr8(obj + 0x09, 0);
    if ((rd16(v_gameFrames) >> 2) & 1) {
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
        wr32(obj + 0x18, 0x30000);
    } else {
        wr32(obj + 0x18, 0xFFFD0000);
    }
}

/* $0382CC. Standing. */
void BouncerStill(uint16_t obj)
{
    (void)obj;
}

/* $0382CE. Giving off a spark (type $CC) every 4th frame, at a random angle at half speed. */
void BouncerSparks(uint16_t obj)
{
    FrameList(obj);
    if (rd16(v_gameFrames) & 3) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) - 4));
    wr16(slot + O_TYPE, 0xCC);
    wr32(slot + 0x08, 0x0E0FF2);
    wr16(slot + 0x48, 8);
    uint16_t k = (uint16_t)((rd16(v_randomSeed) & 0xC) + 0x68);
    int32_t vy = (int32_t)rom32(0x05818C + k), vx = (int32_t)rom32(0x0581AC + k);
    wr32(slot + 0x18, (uint32_t)(vx >> 1));
    wr32(slot + 0x1C, (uint32_t)(vy >> 1));
}

/* $03833A. A burst (type $D0, tied to it both ways). Its animation is stepped from this routine's
 * own code, which a0 holds (the jump table's target). */
void BouncerBurst(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x48, 0x20);
    wr8(obj + 0x09, 0);
    SelectAnimationKeep(obj, 0x03833A);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0xD0);
    wr32(slot + 0x08, 0x0E0FF2);
    wr16(slot + 0x48, 0x200);
    wr16(slot + 0x4A, 0);
    wr16(slot + 0x4C, 0xFFF4);
    wr16(slot + 0x4E, 3);
    wr16(slot + 0x50, 8);
    wr16(slot + 0x54, 0x28);
    wr16(slot + 0x56, 0);
    wr16(slot + 0x58, obj);
    wr16(obj + 0x58, slot);
}

/* $03839A. After the burst's $20 frames: moving left. */
void BouncerBurstWait(uint16_t obj)
{
    if (rd16(obj + 0x48) & 0x8000) wr16(obj + 0x18, 0xFFFE);
}

/* $0383AA. Bouncing: a random speed's fraction, its look. */
void BouncerBounceStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x1A, rd16(v_randomSeed) & 0x7FFF);
    figure_look(obj, 0x038874);
}

/* $0383C6. Falling; off the floor it bounces back up. */
void BouncerBounce(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x3000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    uint16_t d3, d4;
    if (ObjectFloor(obj, 4, &d3, &d4)) return;
    wr32(obj + 0x1C, 0xFFFD0000);
}

/* $0381B2. Type $BC. */
void Bouncer(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    call_object(rom32(0x0381C0 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $88: a flyer that bobs, darts about in a box, fires at the player (type $90), and
 * leaves (as the hopper, type $200) after a few rounds; a player that catches it makes it a
 * $B0 ---- */

#define FLYER_BOB    0x58        /* its bobbing: 9 frames down, 9 up */
#define FLYER_FLAGS  0x57        /* bit 0: stays; 1: no bobbing; 2: facing the other way */

/* loc_037B84: defeated: points, then one of the dying types ($8C, $D4, $7C) by the random */
static void flyer_defeated(uint16_t obj)
{
    AddScore(obj, 48);
    wr16(obj + O_STATE, 0);
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0);
    wr8(obj + 0x23, 0);
    int16_t d1 = (int16_t)((rd16(v_randomSeed) & 0x1E) - 0x1A);
    if (d1 < 0) {
        wr16(obj + O_TYPE, 0x8C);
        wr16(obj + 0x48, 0x60);
        wr16(obj + 0x4A, 6);
        wr16(obj + 0x4C, (uint16_t)((rd16(v_randomSeed) & 4) + 0xC));
        return;
    }
    wr16(obj + O_TYPE, d1 == 0 ? 0xD4 : 0x7C);
}

/* loc_037D34: leaving, as the hopper */
static void flyer_leave(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x200);
    wr16(obj + O_STATE, 0);
    Hopper(obj);
}

/* $03792E. At a random height on the left or right, a few rounds of darting (+$4A) each of a
 * few darts (+$4E). */
void FlyerStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0xEF80);
    wr16(obj + 0x0E, 0x6D6);
    wr32(obj + 0x08, 0x0E06E4);
    wr8(obj + 0x20, 0x84);
    wr32(obj + 0x2C, 0xF808FC04);
    wr32(obj + 0x28, 0xF808FC04);
    wr8(obj + 0x21, 0x2A);
    uint16_t r = rd16(v_randomSeed);
    wr16(obj + 0x4E, (uint16_t)((r & 3) + 2));
    wr16(obj + 0x4A, r & 1);
    uint16_t f = rd16(v_gameFrames);
    wr16(obj + O_Y, (uint16_t)((f & 0x3F) + 0xA0));
    wr16(obj + O_X, (f & 2) ? 0x80 : 0x1C0);
}

/* loc_037A34: a dart in direction `dir` (a speed table's offset); the look by its way */
static void flyer_dart(uint16_t obj, uint16_t dir)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(obj + 0x1C, rom32(0x05818C + dir));
    wr32(obj + 0x18, rom32(0x0581AC + dir));
    wr16(obj + 0x0C, 0);
    if ((int16_t)(dir - 0x20) >= 0 && (int16_t)(dir - 0x60) < 0) {
        wr32(obj + 0x08, 0x0E0718);
        set8(obj + FLYER_FLAGS, 2);
    } else {
        wr32(obj + 0x08, 0x0E06E4);
        clr8(obj + FLYER_FLAGS, 2);
    }
}

/* $0379A4. The next dart: back into the box if out of it, else a random one; the round's
 * darts done, aiming (or, the rounds done, leaving). */
void FlyerFly(uint16_t obj)
{
    wr16(obj + 0x48, 8);
    if ((int16_t)(rd16(obj + O_X) - 0x180) >= 0) { flyer_dart(obj, 0x40); return; }
    if ((int16_t)(rd16(obj + O_X) - 0xC0) < 0) { flyer_dart(obj, 0); return; }
    if ((int16_t)(rd16(obj + O_Y) - 0xB8) < 0) { flyer_dart(obj, 0x20); return; }
    if ((int16_t)(rd16(obj + O_Y) - 0x110) >= 0) { flyer_dart(obj, 0x60); return; }
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if (!(rd16(obj + 0x4E) & 0x8000)) {
        uint16_t r = rd16(v_randomSeed);
        wr16(obj + 0x48, (uint16_t)((r & 7) + 0x14));
        flyer_dart(obj, r & 0x7C);
        return;
    }
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (rd16(obj + 0x4A) & 0x8000) {
        flyer_leave(obj);
        return;
    }
    wr16(obj + O_STATE, 0xC);
    wr16(obj + 0x4E, rd16(obj + 0x54) & 7);
    wr16(obj + 0x56, 0x20);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x08, bit8(obj + FLYER_FLAGS, 2) ? 0x0E07A8 : 0x0E0774);
}

/* $037AAA. Darting, +$48 frames. */
void FlyerDart(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

/* $037AC8. Aiming: then a few shots by the difficulty. */
void FlyerAim(uint16_t obj)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if (!(rd16(obj + 0x4C) & 0x8000)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x50, rom16(0x037AEC + (uint32_t)(int16_t)rd16(0xA20C)));
    wr16(obj + 0x52, 0xF);
}

/* $037AFC. Every $10 frames a type $90 toward the player, its direction in +$48; the shots
 * done, darting again. */
void FlyerFire(uint16_t obj)
{
    wr16(obj + 0x52, (uint16_t)(rd16(obj + 0x52) - 1));
    if (!(rd16(obj + 0x52) & 0x8000)) return;
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if (rd16(obj + 0x50) & 0x8000) {
        wr16(obj + O_STATE, 4);
        wr16(obj + 0x0C, 0);
        wr32(obj + 0x08, bit8(obj + FLYER_FLAGS, 2) ? 0x0E0718 : 0x0E06E4);
        return;
    }
    wr16(obj + 0x52, 0xF);
    uint16_t target = PlayerTarget();
    uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t d2 = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
    d2 = (uint16_t)((((int16_t)d2 >> 8) >> 1) & 0x7C);
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x90);
    wr16(slot + 0x48, d2);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + 4));
}

/* $037910. Its steps. */
void FlyerStep(uint16_t obj)
{
    call_object(rom32(0x03791A + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $037858. Type $88: hit by a player's throw it is caught (a $B0, points); by anything else
 * (or out of vitality) defeated. */
void Flyer(uint16_t obj)
{
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 1));
    if (rd16(obj + O_VITALITY) & 0x8000) {
        flyer_defeated(obj);
        return;
    }
    if (bit8(0x8080, 0)) {
        wr16(obj + O_VITALITY, 0xFFFF);
    } else if (bit8(0x8080, 1) && !bit8(obj + FLYER_FLAGS, 0)) {
        flyer_leave(obj);
        return;
    }
    if (bit8(obj + 0x22, 1)) {
        clr8(obj + 0x22, 1);
        uint16_t by = rd16(obj + 0x42);
        if (by) {
            if (by != 0xA400 && by != 0xA600) {
                flyer_defeated(obj);
                return;
            }
            AddScore(obj, 48);
            wr16(obj + O_TYPE, 0xB0);
            wr16(obj + O_STATE, 0);
            FlyerCaught(obj);
            return;
        }
    }
    if (!bit8(obj + FLYER_FLAGS, 1)) {
        wr16(obj + FLYER_BOB, (uint16_t)(rd16(obj + FLYER_BOB) - 1));
        if (!(rd16(obj + FLYER_BOB) & 0x8000)) {
            wr32(obj + O_Y, rd32(obj + O_Y) + 0x8000);
        } else {
            if ((int16_t)(rd16(obj + FLYER_BOB) - 0xFFF8) < 0) wr16(obj + FLYER_BOB, 9);
            wr32(obj + O_Y, rd32(obj + O_Y) - 0x8000);
        }
    }
    call_object(rom32(0x03790C + (uint32_t)(int16_t)rd16(obj + 6)), obj);
}

/* ---- type $B0: the flyer caught: held, then thrown; it breaks on what it hits ---- */

/* $037C08 */
void CaughtStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x08, 0x0E0760);
    wr16(obj + O_FLAGS, 0xEF80);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x1000);
    wr8(obj + 0x21, 0);
}

/* $037C2E. Held (+$23 bit 1 each frame); let go of without a throw (bit 4) it breaks. */
void CaughtHeld(uint16_t obj)
{
    if (bit8(obj + 0x23, 1)) {
        clr8(obj + 0x23, 1);
        return;
    }
    if (!bit8(obj + 0x23, 4)) {
        wr16(obj + O_VITALITY, 0xFFFF);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x26, 0xA);
    uint16_t by = rd16(obj + 0x42);
    wr32(obj + 0x2C, 0xF808FC04);
    wr32(obj + 0x28, 0xF808FC04);
    wr8(obj + 0x21, (by == 0xA400 || by == 0xA600) ? 0x42 : 0x12);
}

/* $037C98. Thrown, rising; it breaks on a wall above. */
void CaughtRise(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    uint16_t a = MapAttr(rd16(obj + O_X), (uint16_t)(rd16(obj + O_Y) - 8));
    if (a && (int16_t)(a - 0x30) < 0) wr16(obj + O_VITALITY, 0xFFFF);
}

/* $037CC6. Falling (at most 6 a frame), until the ground: a bounce at half the speed. */
void CaughtFall(uint16_t obj)
{
    uint32_t vy = rd32(obj + 0x1C);
    if ((int32_t)(vy - 0x60000) < 0) wr32(obj + 0x1C, vy + 0x4000);
    if (!MapAttr(rd16(obj + O_X), (uint16_t)(rd16(obj + O_Y) + 0xC))) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr8(obj + 0x21, 0);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x1C, 0xFFFD0000);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x08, 0x0E06E4);
}

/* $037D20. The bounce: falling again, it breaks. */
void CaughtBounce(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    wr16(obj + O_VITALITY, 0xFFFF);
}

/* $037BE2. Type $B0. */
void FlyerCaught(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        flyer_defeated(obj);
        return;
    }
    call_object(rom32(0x037BF4 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $B4: a wall that breaks in stages as it is hit (its vitality), drawn into plane A by
 * its kind (+6); broken, it crumbles in debris and smoke. Kind 2 is the rubble left of one,
 * crumbling with figures (type $BC) running out ---- */

/* the wall's place in plane A by its kind */
static uint16_t wall_vram(uint16_t obj)
{
    return rom16(0x0381AA + (uint32_t)(int16_t)rd16(obj + 6));
}

/* loc_0380BC: hit to below $140: its first damage */
static void wall_damaged(uint16_t obj)
{
    wr16(obj + O_STATE, 8);
    uint16_t v = wall_vram(obj);
    PlaneAChunk(0xFFFF45A0, v);
    PlaneAChunk(0xFFFF4800, (uint16_t)(v + 8));
}

/* loc_0380EC: below $A0: its second */
static void wall_broken(uint16_t obj)
{
    wr16(obj + O_STATE, 0xC);
    uint16_t v = wall_vram(obj);
    PlaneAChunk(0xFFFF45A0, v);
    PlaneAChunk(0xFFFF45C0, (uint16_t)(v + 8));
    PlaneAChunk(0xFFFF45E0, (uint16_t)(v + 16));
}

/* loc_038140: gone from plane A */
static void wall_gone(uint16_t obj)
{
    wr8(obj + 0x21, 0);
    uint16_t v = wall_vram(obj);
    PlaneAChunk(0xFFFF4600, v);
    v = (uint16_t)(v + 8);
    PlaneAChunk(0xFFFF4000, v);
    v = (uint16_t)(v + 8);
    PlaneAChunk(0xFFFF4620, v);
    v = (uint16_t)(v + 0x200);
    PlaneAChunk(0xFFFF4680, v);
    v = (uint16_t)(v - 8);
    PlaneAChunk(0xFFFF4660, v);
    v = (uint16_t)(v - 8);
    PlaneAChunk(0xFFFF4640, v);
}

/* loc_038134: destroyed */
static void wall_destroyed(uint16_t obj)
{
    wr16(obj + O_STATE, 0x10);
    wr16(v_shakeSprites, 0x10);
    wall_gone(obj);
}

/* loc_037FDC: 4 frames after a hit, hit again: a chip (type $B8) flies off it */
static void wall_hit(uint16_t obj)
{
    if (!(rd16(obj + 0x4A) & 0x8000)) {
        wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
        return;
    }
    if (!bit8(obj + 0x22, 5)) return;
    wr16(obj + 0x4A, 3);
    wr16(v_shakeSprites, 4);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_X, rd16(obj + O_X));
    uint16_t d1 = (uint16_t)(((int16_t)rd16(v_randomSeed) >> 2) & 7);
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x20 - d1));
    wr16(slot + O_TYPE, 0xB8);
    wr16(slot + O_FLAGS, 0x8D00);
    wr16(slot + 0x48, 0x18);
    wr16(slot + 0x4A, 1);
    uint16_t k = rd8(0xFF0A) & 2;
    wr16(slot + 0x0E, rom16(0x0380B0 + k));
    wr32(slot + 0x08, rom32(0x0380B4 + k));
    wr32(slot + 0x0A, rom32(0x0380B8 + k));
    wr16(slot + 0x0E, rd16(slot + 0x0E) | (rd16(obj + 0x48) & 0x1800));
    wr16(slot + 0x1C, (uint16_t)((rd8(0xFF08) & 3) - 4));
    wr16(slot + 0x1A, rd16(v_randomSeed));
    uint16_t side = rd8(0xFF0A) & 3;
    wr16(slot + 0x18, (rd8(0xFF0A) & 0x80) ? (uint16_t)~side : side);
}

/* $037E00 */
void WallStart(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x300);
    if (rd16(obj + 6) == 2) {
        wr16(obj + O_STATE, 0x20);
        wr16(obj + O_VITALITY, 0x258);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_VITALITY, 0x1E0);
    wr32(obj + 0x28, 0xE00FE020);
    wr8(obj + 0x21, 0x20);
}

/* $037E38. Whole. */
void WallWhole(uint16_t obj)
{
    int16_t v = (int16_t)rd16(obj + O_VITALITY);
    if (v < 0) wall_destroyed(obj);
    else if (v - 0x140 >= 0) wall_hit(obj);
    else if (v - 0xA0 >= 0) wall_damaged(obj);
    else wall_broken(obj);
}

/* $037E58. Damaged. */
void WallDamaged(uint16_t obj)
{
    int16_t v = (int16_t)rd16(obj + O_VITALITY);
    if (v < 0) wall_destroyed(obj);
    else if (v - 0xA0 >= 0) wall_hit(obj);
    else wall_broken(obj);
}

/* $037E70. Broken. */
void WallBroken(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) wall_destroyed(obj);
    else wall_hit(obj);
}

/* $037E7C. Destroyed: $B5 frames of debris. */
void WallFallStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x4A, 2);
    wr16(obj + 0x4C, 0x1F);
    wr16(obj + 0x4E, 7);
    wr16(obj + 0x50, 0xB4);
    wr32(obj + 0x54, 0x056450);
    wr16(obj + 0x58, 0x104);
    wr16(obj + 0x5A, 0);
}

/* $037EAE. The debris, and every 4th frame a cloud (type $100) rising from it. */
void WallFall(uint16_t obj)
{
    wr16(v_shakeSprites, 8);
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if (rd16(obj + 0x50) & 0x8000) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        return;
    }
    DebrisBurst(obj);
    if (rd16(v_gameFrames) & 3) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0563EE);
    wr16(slot + O_X, (uint16_t)((rd8(v_randomSeed) & 7) + rd16(obj + O_X) - 4));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + 0x1C, 0xFFFC);
}

/* $037F0A. Then smoke rising, for good. */
void WallSmokeStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x4A, 4);
    wr16(obj + 0x4C, 0xF);
    wr16(obj + 0x4E, 0);
    wr16(obj + 0x50, 1);
    wr16(obj + 0x52, 0xFFFE);
    wr32(obj + 0x54, 0x05652E);
    wr16(obj + 0x58, 0x100);
    wr16(obj + 0x5A, 0);
}

/* $037F9E */
void WallSmoke(uint16_t obj)
{
    DebrisEmit(obj);
}

/* $037FA4. Kind 2: crumbling. */
void WallCrumbleStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x4A, 5);
    wr16(obj + 0x4C, 7);
    wr16(obj + 0x4E, 3);
    wr16(obj + 0x50, 1);
    wr16(obj + 0x52, 0xFFFE);
    wr32(obj + 0x54, 0x05639C);
    wr16(obj + 0x58, 0x100);
    wr16(obj + 0x5A, 0);
}

/* $037F42. Every $20 frames a figure (type $BC, kind 8: bouncing) out of it, for its vitality's
 * frames; then gone from plane A, smoking. */
void WallCrumble(uint16_t obj)
{
    wr16(v_shakeSprites, 4);
    if (!(rd16(v_gameFrames) & 0x1F)) {
        uint16_t slot, t;
        if (SlotFreeHigh(&slot, &t)) {
            wr16(slot + O_TYPE, 0xBC);
            wr16(slot + 6, 8);
            wr16(slot + O_STATE, 0);
            wr16(slot + O_X, (uint16_t)(rd16(obj + O_X) - 0x10));
            wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) + 0x1A));
        }
    }
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 1));
    if (!(rd16(obj + O_VITALITY) & 0x8000)) {
        DebrisEmit(obj);
        return;
    }
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 0x10));
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 0x18));
    wr16(obj + O_STATE, 0x18);
    wall_gone(obj);
}

/* $037DC2. Type $B4. +$48 runs on by the random (the chips' flip). */
void Wall(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) + rd16(v_randomSeed)));
    call_object(rom32(0x037DD8 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $124: a rock that rises, waits, then drops from a random place above ---- */

/* $037332 */
void RockStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x8C00);
    wr8(obj + 0x20, 0xB0);
    wr16(obj + 0x0E, 0x616D);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr16(obj + 0x18, (uint16_t)(7 - (rd16(v_randomSeed) & 0xF)));
    wr16(obj + 0x1C, (uint16_t)(0xFFFF - (rd16(v_gameFrames) & 7)));
}

/* $03737A. Rising to $88. */
void RockRise(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_Y) - 0x88) >= 0) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x48, (uint16_t)((rd8(v_randomSeed) & 0x1F) + 8));
}

/* $037398. Waiting, then dropping at a random x in one of three bands. */
void RockWait(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (!(rd16(obj + 0x48) & 0x8000)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    uint16_t band = rom16(0x0373EE + (uint32_t)((((int16_t)rd16(v_gameFrames) >> 6)) & 6));
    wr16(obj + O_X, (uint16_t)((rd16(v_randomSeed) & 0x7F) + band));
    wr16(obj + 0x1C, 2);
    wr16(obj + 0x1E, rd16(v_randomSeed));
    wr16(obj + 0x18, 0);
    wr32(obj + 0x2C, 0xFE01FE02);
    wr8(obj + 0x21, 0x10);
    wr16(obj + 0x26, 2);
}

/* $0373F6. Dropping: shot, landed on something or past $148, a burst (type $100) and a point. */
void RockDrop(uint16_t obj)
{
    if (!(rd16(obj + O_VITALITY) & 0x8000) && (int16_t)(rd16(obj + O_Y) - 0x148) < 0 && !bit8(obj + 0x22, 4)) return;
    AddScore(obj, 1);
    wr16(obj + O_STATE, 0);
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x05652E);
    wr8(obj + 0x21, 0);
    wr16(obj + 0x1C, 0xFFFF);
}

/* $037318. Type $124. */
void Rock(uint16_t obj)
{
    call_object(rom32(0x037322 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $128: a worm of seven segments (type $12C, slots $CE60 to $D0A0; the head itself must
 * be in slot $CE00) that drops in from the left or right (+$4E), stretches out and sways; gone
 * when the boss in slot $C980 reaches its step $10 ---- */

#define WORM_SEGMENTS 0xCE60
#define WORM_LAST     0xD0A0

/* $03745C */
void WormStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x8400);
    wr8(obj + 0x20, 0x70);
    wr16(obj + 0x0E, 0x4C2);
    wr16(obj + 0x08, 0xF00);
    wr16(obj + 0x0A, 0xF0F0);
    uint16_t r = rd16(v_randomSeed);
    wr16(obj + 0x1C, 2);
    wr16(obj + O_Y, 0x88);
    if (r & 2) {
        wr16(obj + O_X, 0xB0);
        wr16(obj + 0x4E, 0);
    } else {
        wr16(obj + O_X, 0x190);
        wr16(obj + 0x4E, 1);
    }
}

/* $0374B2. Dropping to $142. */
void WormDrop(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_Y) - 0x142) < 0) return;
    wr16(obj + 0x48, 0x40);
    wr16(obj + 0x1C, 0);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
}

/* $0374CE. $40 frames. */
void WormWait(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (!(rd16(obj + 0x48) & 0x8000)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
}

/* $0374DC. Its segments, the head hidden behind them; the two ends' looks by its side. */
void WormSegments(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0);
    wr16(obj + O_VITALITY, 0x50);
    wr16(obj + 0x48, 0);
    uint16_t look = rd16(obj + 0x4E) ? 0x4181 : 0x4981;
    for (uint16_t s = WORM_SEGMENTS; s <= WORM_LAST; s = (uint16_t)(s + 0x60)) {
        wr16(s + O_TYPE, 0x12C);
        wr16(s + O_FLAGS, 0x8000);
        wr8(s + 0x20, 0x70);
        wr16(s + 0x0E, look);
        wr16(s + 0x08, 0xF00);
        wr16(s + 0x0A, 0xF0F0);
        wr16(s + O_X, rd16(obj + O_X));
        wr16(s + O_Y, rd16(obj + O_Y));
        wr16(s + 0x58, 0xCE00);
        wr16(s + O_VITALITY, 0x4000);
        wr16(s + 0x26, 8);
        wr32(s + 0x2C, 0xF807F808);
        wr32(s + 0x28, 0xF807F808);
        wr8(s + 0x21, 0x30);
        wr8(s + 0x23, 0x20);
    }
    wr16(WORM_SEGMENTS + 0x0E, (uint16_t)(rd16(WORM_SEGMENTS + 0x0E) - 0x10));
    wr16(WORM_LAST + 0x0E, (uint16_t)(rd16(WORM_LAST + 0x0E) + 0x10));
    wr16(WORM_LAST + 0x08, 0xB00);
    if (rd16(obj + 0x4E)) {
        wr16(WORM_SEGMENTS + 0x0A, 0xE0F0);
        wr16(WORM_LAST + 0x0A, 0x1F0);
    } else {
        wr16(WORM_SEGMENTS + 0x0A, 0xF0);
        wr16(WORM_LAST + 0x0A, 0xE6F0);
    }
}

/* loc_037788: destroyed: points, its segments to bursts (type $100) thrown out around it */
static void worm_destroyed(uint16_t obj)
{
    AddScore(obj, 100);
    wr16(obj + O_FLAGS, 0x1000);
    uint16_t a = 0;
    for (uint16_t s = WORM_SEGMENTS; s <= WORM_LAST; s = (uint16_t)(s + 0x60)) {
        wr16(s + O_TYPE, 0x100);
        wr32(s + 0x48, 0x05639C);
        wr32(s + 0x1C, rom32(0x05818C + a));
        wr32(s + 0x18, rom32(0x0581AC + a));
        wr8(s + 0x21, 0);
        a = (uint16_t)((a - 0xC) & 0x7C);
    }
    QueueSound(0xCF);
}

/* the boss's tilt set anew (at random) when the worm goes */
static void worm_tilt(void)
{
    wr16(0xC9D4, (uint16_t)((rd16(v_randomSeed) & 0x7F) + 0x40));
}

/* the worm's end: by the boss (C984, its step, past $10) or out of vitality; 1 when it ended */
static int worm_ended(uint16_t obj)
{
    if ((int16_t)(rd16(0xC984) - 0x10) >= 0) {
        worm_destroyed(obj);
        return 1;
    }
    if (rd16(obj + O_VITALITY) & 0x8000) {
        worm_tilt();
        worm_destroyed(obj);
        return 1;
    }
    return 0;
}

/* $0375AA. Stretching out sideways (+$48 the stretch, to $20 one way or -$22 the other). */
void WormStretch(uint16_t obj)
{
    if (worm_ended(obj)) return;
    int16_t stretch = (int16_t)rd16(obj + 0x48);
    uint16_t s = WORM_SEGMENTS;
    for (uint16_t k = 0; k < 7; k++, s = (uint16_t)(s + 0x60)) {
        uint16_t a = rom16(0x037728 + 2 * k);
        int32_t p = (int32_t)(int16_t)rom16(0x0297FC + a) * stretch;
        wr16(s + O_X, (uint16_t)((uint16_t)(((uint32_t)p << 2) >> 16) + rd16(obj + O_X)));
    }
    if (!rd16(obj + 0x4E)) {
        wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) + 2));
        if ((int16_t)(rd16(obj + 0x48) - 0x20) < 0) return;
    } else {
        wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 2));
        if ((int16_t)(rd16(obj + 0x48) - 0xFFE0) >= 0) return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
}

/* $037690. The segments on an arc (+$4A its height, the stretch less it), from one end or the
 * other (+$4C): that end stays put and the head goes with it. */
void WormPlace(uint16_t obj)
{
    int16_t h = (int16_t)rd16(obj + 0x4A);
    int16_t w = rd16(obj + 0x4E) ? (int16_t)(h - 0x20) : (int16_t)(0x20 - h);
    uint16_t s = WORM_SEGMENTS;
    for (uint16_t k = 0; k < 7; k++, s = (uint16_t)(s + 0x60)) {
        uint16_t a = rom16(0x037728 + 2 * k);
        uint16_t dy = (uint16_t)(((uint32_t)((int32_t)(int16_t)rom16(0x02977C + a) * h) << 2) >> 16);
        uint16_t dx = (uint16_t)(((uint32_t)((int32_t)(int16_t)rom16(0x0297FC + a) * w) << 2) >> 16);
        if (s == (rd16(obj + 0x4C) ? WORM_SEGMENTS : WORM_LAST))
            wr16(obj + O_X, (uint16_t)(rd16(s + O_X) - dx));
        else
            wr16(s + O_X, (uint16_t)(dx + rd16(obj + O_X)));
        wr16(s + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
    }
}

/* loc_037736: gone off the sides */
static void worm_gone(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x1000);
    for (uint16_t s = WORM_SEGMENTS; s <= WORM_LAST; s = (uint16_t)(s + 0x60)) wr16(s + O_FLAGS, 0x1000);
    worm_tilt();
}

/* $037630. Swaying: its arc rising and falling (+$4A, +$4C which way), inching along. */
void WormSway(uint16_t obj)
{
    if (worm_ended(obj)) return;
    if ((int16_t)(rd16(obj + O_X) - 0x200) >= 0 || (int16_t)(rd16(obj + O_X) - 0x40) < 0) {
        worm_gone(obj);
        return;
    }
    WormPlace(obj);
    if (!rd16(obj + 0x4C)) {
        wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
        if (!(rd16(obj + 0x4A) & 0x8000) && rd16(obj + 0x4A)) return;
        wr16(obj + 0x4C, 1);
        return;
    }
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) + 1));
    if (rd16(obj + 0x4A) & 0x8000) return;
    if ((int16_t)(rd16(obj + 0x4A) - 0x12) < 0) return;
    wr16(obj + 0x4C, 0);
}

/* $03743A. Type $128. */
void Worm(uint16_t obj)
{
    call_object(rom32(0x037444 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $0377EC. Type $12C: a segment: the damage it takes goes to the head (+$58). */
void WormSegment(uint16_t obj)
{
    uint16_t taken = (uint16_t)(0x4000 - rd16(obj + O_VITALITY));
    uint16_t head = rd16(obj + 0x58);
    wr16(head + O_VITALITY, (uint16_t)(rd16(head + O_VITALITY) - taken));
    wr16(obj + O_VITALITY, 0x4000);
}

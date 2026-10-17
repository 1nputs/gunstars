/* Object types of several stages and scenes: the battleship's bow pieces ($290), stage 4's two
 * blocks after its boss ($324), the mine's item carrier ($1F0), the story's cued figures ($49C,
 * $4A0, $4FC, $62C-$63C, $660), the boss with the orbiting segments ($24) and its parts ($2C,
 * $610). */
#include "game.h"

/* ---- type $290: the battleship's eight bow pieces (numbered by 4 at +$48): carried with the bow
 * (their speed the bow's, negated, a frame late), put back up from y $180 until the camera's step
 * $C; the first two become the bow's parts ($46 negative) ---- */

#define BOW_SPEED 0xA99C         /* long: the bow's y speed (stage 2's) */

/* $00660A. Its place, box, look and touch by its number (+$48). */
void BowPartSetup(uint16_t obj)
{
    wr8(obj + 0x20, 0x88);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    uint16_t n = rd16(obj + 0x48);
    uint32_t w = (uint32_t)(int16_t)((int16_t)n >> 1), b = (uint32_t)(int16_t)((int16_t)n >> 2);
    uint32_t l = (uint32_t)(int16_t)n;
    wr16(obj + O_X, rom16(0x00665A + w));
    wr16(obj + O_Y, rom16(0x00666A + w));
    wr16(obj + 0x4C, rom16(0x0066A2 + w));
    wr32(obj + 0x2C, rom32(0x00667A + l));
    wr8(obj + 0x46, rom8(0x00669A + b));
    wr16(obj + O_FLAGS, rom16(0x0066B2 + w));
    wr16(obj + 0x0E, rom16(0x0066C2 + w));
    wr32(obj + 0x08, rom32(0x0066D2 + l));
}

/* $006608. Gone. */
void BowPartGone(uint16_t obj)
{
    (void)obj;
}

/* $0065F8. State 8: nothing. */
void BowPartIdle(uint16_t obj)
{
    (void)obj;
}

/* $00652E. Moving with the bow; below y $180 put back up (by +$4C's low byte), the first two
 * turned into the bow's parts, until the camera's step $C ends it; touchable below y $90. */
void BowPartMove(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_Y) >= 0x180) {
        if ((int16_t)rd16(v_cameraStep) >= 0xC) {
            wr8(obj + 0x21, 0);
            wr16(obj + O_FLAGS, 0);
            wr16(obj + O_STATE, 0xC);
            BowPartGone(obj);
            return;
        }
        wr16(obj + O_Y, (uint16_t)((rd16(obj + 0x4C) & 0xFF) + 0x30));
        if (rd8(obj + 0x46) & 0x80) {
            if (!(rd16(obj + 0x48) & 4)) {
                wr32(obj + 0x2C, 0xF808E020);
                wr16(obj + O_X, 0xA0);
                wr8(obj + 0x46, 1);
                wr16(obj + O_FLAGS, 0);
                wr16(obj + 0x0E, 0x68C);
                wr32(obj + 0x08, 0x0500F8F8);
            } else {
                wr32(obj + 0x2C, 0x0010B010);
                wr16(obj + O_X, 0x110);
                wr8(obj + 0x46, 1);
                wr16(obj + O_FLAGS, 0xC000);
                wr16(obj + 0x0E, 0x6B8);
                wr32(obj + 0x08, 0x6704);
            }
        }
    }
    wr8(obj + 0x21, (int16_t)rd16(obj + O_Y) > 0x90 ? 4 : 0);
    uint32_t v = rd32(obj + 0x1C);
    wr32(obj + 0x4C, rd32(obj + 0x4C) + v);
    wr32(obj + O_Y, rd32(obj + O_Y) + v);
    wr32(obj + 0x1C, (uint32_t)-rd32(BOW_SPEED));
}

/* $006524 */
void BowPartStart(uint16_t obj)
{
    BowPartSetup(obj);
    wr16(obj + O_STATE, 4);
    BowPartMove(obj);
}

/* $00650E. Type $290 (a table of word offsets from itself). */
void BowPart(uint16_t obj)
{
    uint32_t at = 0x00651C + (uint32_t)(int16_t)((int16_t)rd16(obj + O_STATE) >> 1);
    call_object(at + (uint32_t)(int16_t)rom16(at), obj);
}

/* ---- type $324 ---- */

/* $007656. Type $324: stage 4's two blocks after its boss: at y $160, +$21 bit 2, a box, +$46
 * $8F. */
void Stage4Block(uint16_t obj)
{
    if (rd16(obj + O_STATE)) return;
    wr16(obj + O_Y, 0x160);
    set8(obj + 0x21, 2);
    wr32(obj + 0x2C, 0xF0108060);
    wr8(obj + 0x46, 0x8F);
    wr16(obj + O_STATE, 4);
}

/* ---- type $1F0: the mine's item carrier, as $144 (items.c, whose CarrierFace and CarrierBob it
 * uses) but placed by its spawner (x $80 only when +6 is positive), with no box of its own at
 * +$2C, falling onto the first cell below it, and leaving its item still ($8E00, +6 = 1) ---- */

#define CB_DROPPED 0x4A          /* bits 0, 1: a drop already made for player 1, 2 */
#define CB_WAIT    0x48
#define CB_BOB     0x50
#define CB_FROM_X  0x4E

/* $01AB5E. Its item into `slot`, at it (4 to the right), still. */
void CarrierBDrop(uint16_t obj, uint16_t slot)
{
    if (rd16(obj + 0x4C)) {
        wr16(slot + O_TYPE, 0x68);
        wr16(slot + 0x50, 0x14);
    } else {
        uint16_t r = (uint16_t)Random() & 6;
        wr16(slot + O_TYPE, rom16(0x01A93E + r));
    }
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + 4));
    wr32(slot + 0x18, 0);
    wr32(slot + 0x1C, 0);
    wr16(slot + O_FLAGS, 0x8E00);
    wr16(slot + 6, 1);
}

/* loc_01A9B4: flying again (its first mappings), then the flying frame */
static void carrier_b_again(uint16_t obj)
{
    wr32(obj + 0x08, 0x0E1000);
    wr16(obj + 0x0C, 0);
    CarrierFace(obj);
    wr8(obj + 0x22, 0);
    wr16(obj + O_STATE, 4);
    CarrierBFly(obj);
}

/* loc_01AAC8: a drop, stopped for $10 frames (no free slot: flying on) */
static void carrier_b_stop(uint16_t obj)
{
    wr32(obj + 0x08, 0x0E108C);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    uint16_t slot;
    if (!SlotFreeLow(&slot)) {
        carrier_b_again(obj);
        return;
    }
    CarrierBDrop(obj, slot);
    wr16(obj + CB_WAIT, 0x10);
    wr16(obj + O_STATE, 0xC);
    CarrierBWait(obj);
}

/* loc_01AAFC: thrown ahead at 4 by a hit from below, a drop */
static void carrier_b_thrown(uint16_t obj)
{
    if (bit8(obj + 0x23, 7)) set8(obj + CB_DROPPED, 0);
    if (bit8(obj + 0x23, 6)) set8(obj + CB_DROPPED, 1);
    wr32(obj + 0x08, 0x0E108C);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0x40000);
    wr16(obj + CB_FROM_X, rd16(obj + O_X));
    uint16_t slot;
    if (!SlotFreeLow(&slot)) {
        carrier_b_again(obj);
        return;
    }
    CarrierBDrop(obj, slot);
    wr16(obj + CB_WAIT, 0x10);
    wr16(obj + O_STATE, 0x10);
    CarrierBThrown(obj);
}

/* a drop's bit `bit` set; whether it was clear */
static int carrier_b_first(uint16_t obj, int bit)
{
    uint8_t b = rd8(obj + CB_DROPPED);
    wr8(obj + CB_DROPPED, (uint8_t)(b | (1u << bit)));
    return !(b & (1u << bit));
}

/* $01A964. Step 0: its item (+6 bit 2: $68), at x $80 unless +6 is negative, touchable, flying. */
void CarrierBStart(uint16_t obj)
{
    wr16(obj + 0x4C, rd16(obj + 6) & 4);
    wr16(obj + 6, rd16(obj + 6) & 0x8000);
    if (!rd16(obj + 6)) wr16(obj + O_X, 0x80);
    wr16(obj + O_FLAGS, 0xEE00);
    wr8(obj + 0x21, 0xA0);
    wr32(obj + 0x28, 0xF808F808);
    wr16(obj + O_VITALITY, 0x14);
    wr16(obj + 0x0E, 0x8480);
    CarrierFace(obj);
    wr32(obj + 0x1C, 0x10000);
    wr16(obj + CB_DROPPED, 0);
    wr32(obj + CB_BOB, 0);
    carrier_b_again(obj);
}

/* $01A9CE. Flying: shot down (+$22 bit 7) it falls; touched (bit 5) by a player who has not had
 * a drop (or hit from below, +$23 bit 4), or with a player within 8 of its x, it drops. */
void CarrierBFly(uint16_t obj)
{
    uint8_t h = rd8(obj + 0x22);
    wr8(obj + 0x22, (uint8_t)(h & 0x7F));
    if (h & 0x80) {
        wr16(obj + O_STATE, 8);
        CarrierBFall(obj);
        return;
    }
    h = rd8(obj + 0x22);
    wr8(obj + 0x22, (uint8_t)(h & ~0x20));
    if (h & 0x20) {
        if (bit8(obj + 0x23, 4)) {
            carrier_b_thrown(obj);
            return;
        }
        if (bit8(obj + 0x23, 7) && carrier_b_first(obj, 0)) {
            carrier_b_stop(obj);
            return;
        }
        if (bit8(obj + 0x23, 6) && carrier_b_first(obj, 1)) {
            carrier_b_stop(obj);
            return;
        }
    }
    uint16_t x = rd16(obj + O_X);
    if (bit8(v_players, 0) && (uint16_t)(rd16(v_player1 + O_X) - x + 8) < 0x10 && carrier_b_first(obj, 0)) {
        carrier_b_stop(obj);
        return;
    }
    if (bit8(v_players, 1) && (uint16_t)(rd16(v_player2 + O_X) - x + 8) < 0x10 && carrier_b_first(obj, 1)) {
        carrier_b_stop(obj);
        return;
    }
    CarrierBob(obj);
}

/* $01AA6A. Falling, faster to 7; on a cell or the bounds' bottom 8 below it, it becomes type $70
 * and drops its item. */
void CarrierBFall(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if ((int32_t)rd32(obj + 0x1C) >= 0x70000) wr32(obj + 0x1C, 0x70000);
    uint16_t x = rd16(obj + O_X), y = (uint16_t)(rd16(obj + O_Y) + 8);
    if (!MapAttr(x, y) && !MapBoundsY(x, &y)) return;
    wr16(obj + O_TYPE, 0x70);
    wr16(obj + O_STATE, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x5E, 0);
    uint16_t slot;
    if (SlotFreeLow(&slot)) CarrierBDrop(obj, slot);
}

/* $01AAF0. Stopped after a drop, bobbing. */
void CarrierBWait(uint16_t obj)
{
    wr16(obj + CB_WAIT, (uint16_t)(rd16(obj + CB_WAIT) - 1));
    if (!rd16(obj + CB_WAIT)) {
        carrier_b_again(obj);
        return;
    }
    CarrierBob(obj);
}

/* $01AB4E. Thrown: swinging back toward where it was hit (by 2 a frame; CarrierThrown's end). */
void CarrierBThrown(uint16_t obj)
{
    wr16(obj + CB_WAIT, (uint16_t)(rd16(obj + CB_WAIT) - 1));
    if (!rd16(obj + CB_WAIT)) {
        carrier_b_again(obj);
        return;
    }
    CarrierBob(obj);
    if (rd16(obj + O_X) < rd16(obj + CB_FROM_X)) wr32(obj + 0x18, rd32(obj + 0x18) + 0x20000);
    else wr32(obj + 0x18, rd32(obj + 0x18) - 0x20000);
}

/* $01A946. Type $1F0. */
void CarrierB(uint16_t obj)
{
    call_object(rom32(0x01A950 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- the story's cued figures: a scene's script writes a cue at +$48; the figure takes it
 * (into +$4A, its step +$4C cleared) and runs that cue's routine every frame (cue 0: nothing) ---- */

#define CUE_NEW  0x48
#define CUE      0x4A
#define CUE_STEP 0x4C
#define CUE_TIME 0x4E

/* the cue's routine from `table` (a new cue taken, its step cleared; for `same`, a cue it
 * already has keeps its step); with no cue the code falls into table[0]'s routine, which does
 * nothing */
static void by_cue(uint16_t obj, uint32_t table, int same)
{
    uint16_t c = rd16(obj + CUE_NEW);
    if (c) {
        wr16(obj + CUE_NEW, 0);
        if (!same || c != rd16(obj + CUE)) {
            wr16(obj + CUE, c);
            wr16(obj + CUE_STEP, 0);
        }
    } else {
        c = rd16(obj + CUE);
    }
    call_object(rom32(table + (uint32_t)(int16_t)c), obj);
}

/* the cue's step routine from `table` by +$4C */
static void by_cue_step(uint16_t obj, uint32_t table)
{
    call_object(rom32(table + (uint32_t)(int16_t)rd16(obj + CUE_STEP)), obj);
}

/* the cue's pose: its mappings, step 4 */
static void cue_pose(uint16_t obj, uint16_t step, uint32_t map)
{
    wr16(obj + CUE_STEP, step);
    wr32(obj + 0x08, map);
    wr16(obj + 0x0C, 0);
}

/* the figure at $CA40 has no cue (+$48, +$4A both 0) */
#define CUE_CA40_IDLE() (!rd32(0xCA88))

/* ---- type $49C ---- */

/* $020940. No cue. */
void Cue49CNone(uint16_t obj)
{
    (void)obj;
}

/* $020980 */
void Cue49CHold1(uint16_t obj)
{
    (void)obj;
}

/* $02096E */
void Cue49CPose1(uint16_t obj)
{
    cue_pose(obj, 4, 0x0E49E6);
    Cue49CHold1(obj);
}

/* $02095C. Cue 4: its first pose. */
void Cue49CCue4(uint16_t obj)
{
    by_cue_step(obj, 0x020966);
}

/* $020994 */
void Cue49CPose2(uint16_t obj)
{
    cue_pose(obj, 4, 0x0E4A00);
}

/* $0209A8. Until the figure at $CA40 has no cue: then cue 4. */
void Cue49CWait(uint16_t obj)
{
    if (CUE_CA40_IDLE()) wr16(obj + CUE_NEW, 4);
}

/* $020982. Cue 8: its second pose, then back to cue 4 when the figure at $CA40 is done. */
void Cue49CCue8(uint16_t obj)
{
    by_cue_step(obj, 0x02098C);
}

/* $0209DC */
void Cue49CHold3(uint16_t obj)
{
    (void)obj;
}

/* $0209CA */
void Cue49CPose3(uint16_t obj)
{
    cue_pose(obj, 4, 0x0E4A3C);
    Cue49CHold3(obj);
}

/* $0209B8. Cue $C: its third pose. */
void Cue49CCueC(uint16_t obj)
{
    by_cue_step(obj, 0x0209C2);
}

/* $020A02 */
void Cue49CHold4(uint16_t obj)
{
    (void)obj;
}

/* $0209F0 */
void Cue49CPose4(uint16_t obj)
{
    cue_pose(obj, 4, 0x0E4A88);
    Cue49CHold4(obj);
}

/* $0209DE. Cue $10: its fourth pose. */
void Cue49CCue10(uint16_t obj)
{
    by_cue_step(obj, 0x0209E8);
}

/* $0208E8. Type $49C: set up at x $A0, y $140 in its first pose, then by its cue. */
void Cue49C(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0xE000);
        wr16(obj + 0x0E, 0x300);
        wr16(obj + O_X, 0xA0);
        wr16(obj + O_Y, 0x140);
        wr32(obj + 0x08, 0x0E49E6);
        wr16(obj + 0x0C, 0);
        wr16(obj + CUE_STEP, 0);
        return;
    }
    by_cue(obj, 0x020948, 0);
}

/* ---- type $4A0 ---- */

/* $020A5C. No cue. */
void Cue4A0None(uint16_t obj)
{
    (void)obj;
}

/* $020AA2. The first pose's $140 frames, then the second's $55. */
void Cue4A0Wait1(uint16_t obj)
{
    wr16(obj + CUE_TIME, (uint16_t)(rd16(obj + CUE_TIME) - 1));
    if (!(rd16(obj + CUE_TIME) & 0x8000)) return;
    cue_pose(obj, 8, 0x0E4ADA);
    wr16(obj + CUE_TIME, 0x55);
    Cue4A0Wait2(obj);
}

/* $020A8A */
void Cue4A0Pose1(uint16_t obj)
{
    cue_pose(obj, 4, 0x0E4AB6);
    wr16(obj + CUE_TIME, 0x140);
    Cue4A0Wait1(obj);
}

/* $020AC4. The second pose's frames, then the first again. */
void Cue4A0Wait2(uint16_t obj)
{
    wr16(obj + CUE_TIME, (uint16_t)(rd16(obj + CUE_TIME) - 1));
    if (rd16(obj + CUE_TIME) & 0x8000) Cue4A0Pose1(obj);
}

/* $020A74. Cue 4: two poses in turn. */
void Cue4A0Cue4(uint16_t obj)
{
    by_cue_step(obj, 0x020A7E);
}

/* $020AE2 */
void Cue4A0Pose3(uint16_t obj)
{
    cue_pose(obj, 4, 0x0E4B92);
}

/* $020B16. $55 frames, then cue 4. */
void Cue4A0Wait4(uint16_t obj)
{
    wr16(obj + CUE_TIME, (uint16_t)(rd16(obj + CUE_TIME) - 1));
    if (rd16(obj + CUE_TIME) & 0x8000) wr16(obj + CUE_NEW, 4);
}

/* $020AF6. Until the figure at $CA40 has no cue: then another pose for $55 frames. */
void Cue4A0Wait3(uint16_t obj)
{
    if (!CUE_CA40_IDLE()) return;
    cue_pose(obj, 8, 0x0E4B22);
    wr16(obj + CUE_TIME, 0x55);
    Cue4A0Wait4(obj);
}

/* $020ACC. Cue 8: a pose while the figure at $CA40 acts, another, then cue 4. */
void Cue4A0Cue8(uint16_t obj)
{
    by_cue_step(obj, 0x020AD6);
}

/* $020B68 */
void Cue4A0Done(uint16_t obj)
{
    (void)obj;
}

/* $020B4E. To the animation's end (+$D negative): then another. */
void Cue4A0Play(uint16_t obj)
{
    if (rd8(obj + 0x0D) & 0x80) cue_pose(obj, 8, 0x0E4AFE);
    Cue4A0Done(obj);
}

/* $020B3C */
void Cue4A0Pose5(uint16_t obj)
{
    cue_pose(obj, 4, 0x0E4BB6);
    Cue4A0Play(obj);
}

/* $020B26. Cue $C: an animation, then another. */
void Cue4A0CueC(uint16_t obj)
{
    by_cue_step(obj, 0x020B30);
}

/* $020A04. Type $4A0: set up at x $1A0, y $14C, then by its cue. */
void Cue4A0(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0xE000);
        wr16(obj + 0x0E, 0xB80);
        wr16(obj + O_X, 0x1A0);
        wr16(obj + O_Y, 0x14C);
        wr32(obj + 0x08, 0x0E4AA2);
        wr16(obj + 0x0C, 0);
        wr16(obj + CUE_STEP, 0);
        return;
    }
    by_cue(obj, 0x020A64, 0);
}

/* ---- type $4FC ---- */

/* $020BBE. No cue. */
void Cue4FCNone(uint16_t obj)
{
    (void)obj;
}

/* $020BDE. Cue 4: hidden, the cue dropped. */
void Cue4FCHide(uint16_t obj)
{
    clr8(obj + O_FLAGS, 7);
    wr16(obj + CUE, 0);
}

/* shown, its animation started (once per cue) */
static void cue_4fc_show(uint16_t obj, uint32_t map)
{
    if (rd16(obj + CUE_STEP)) return;
    wr16(obj + CUE_STEP, 4);
    set8(obj + O_FLAGS, 7);
    wr32(obj + 0x08, map);
    wr16(obj + 0x0C, 0);
}

/* $020BEA. Cue 8: shown, an animation. */
void Cue4FCShow1(uint16_t obj)
{
    cue_4fc_show(obj, 0x020C8A);
}

/* $020C0C. Cue $C: shown, an animation; its end drops the cue. */
void Cue4FCShow2(uint16_t obj)
{
    cue_4fc_show(obj, 0x020CAE);
    if (rd8(obj + 0x0D) & 0x80) wr16(obj + CUE, 0);
}

/* $020C3A. Cue $10: the same with another animation. */
void Cue4FCShow3(uint16_t obj)
{
    cue_4fc_show(obj, 0x020CBA);
    if (rd8(obj + 0x0D) & 0x80) wr16(obj + CUE, 0);
}

/* $020C68. Cue $14: shown, an animation. */
void Cue4FCShow4(uint16_t obj)
{
    cue_4fc_show(obj, 0x020CF2);
}

/* $020B6A. Type $4FC: set up still and hidden, then by its cue (a cue it already has goes on). */
void Cue4FC(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 4);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x0E, 0x280);
        wr8(obj + 0x20, 0xC0);
        wr16(obj + O_FLAGS, 0x6100);
        wr16(obj + CUE_STEP, 0);
    }
    by_cue(obj, 0x020BC6, 1);
}

/* ---- the story's figures run by their scene (+$48: a new scene restarts them at step 0) ---- */

#define S_SCENE      0x48
#define S_SCENE_SEEN 0x4A

static void by_scene(uint16_t obj, uint32_t table)
{
    uint16_t s = rd16(obj + S_SCENE);
    if (s != rd16(obj + S_SCENE_SEEN)) {
        wr16(obj + S_SCENE_SEEN, s);
        wr16(obj + O_STATE, 0);
    }
    call_object(rom32(table + (uint32_t)(int16_t)s), obj);
}

/* a routine's step through a table of word offsets from itself */
static void by_step(uint16_t obj, uint32_t table)
{
    uint32_t at = table + (uint32_t)(int16_t)((int16_t)rd16(obj + O_STATE) >> 1);
    call_object(at + (uint32_t)(int16_t)rom16(at), obj);
}

static void next_state(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
}

static void set_map(uint16_t obj, uint32_t map)
{
    wr32(obj + 0x08, map);
    wr16(obj + 0x0C, 0);
}

/* ---- type $62C: Blue (the player's look), drifting about, sinking at the scene's step $C ---- */

/* $0225F0. Its y speed by bit 7 of its count: up or down at 1/8. */
void BlueDriftY(uint16_t obj, uint8_t n)
{
    wr32(obj + 0x1C, (n & 0x80) ? 0xFFFFE000 : 0x2000);
}

/* $0225C6. Drifting: its x speed by bit 5 of its count, its y speed by bit 7. */
void BlueDrift(uint16_t obj)
{
    uint16_t n = (uint16_t)(rd16(obj + 0x4E) + 1);
    wr16(obj + 0x4E, n);
    BlueDriftY(obj, (uint8_t)n);
    wr32(obj + 0x18, (n & 0x20) ? 0xFFFFC000 : 0x4000);
}

/* $0225A6. Sinking at 1/4 to y $110. */
void BlueSink(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_Y) < 0x110) return;
    next_state(obj);
    wr32(obj + 0x1C, 0);
    set_map(obj, 0x0DC9B0);
    BlueDrift(obj);
}

/* $022580. Drifting until the scene's step $C. */
void BlueWait(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) < 0xC) {
        BlueDrift(obj);
        return;
    }
    next_state(obj);
    wr32(obj + 0x1C, 0x4000);
    set_map(obj, 0x0DD430);
    BlueSink(obj);
}

/* $022542. At x $130, y $F0, facing left. */
void BlueStart(uint16_t obj)
{
    next_state(obj);
    SceneBlue(obj);
    set8(obj + 0x0E, 3);
    wr8(obj + 0x20, 0x88);
    wr16(obj + O_FLAGS, 0xEC00);
    set_map(obj, 0x0DC9B0);
    wr16(obj + O_X, 0x130);
    wr16(obj + O_Y, 0xF0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x4E, 0);
    BlueWait(obj);
}

/* $02252C */
void BlueScene0(uint16_t obj)
{
    by_step(obj, 0x02253A);
}

/* $02250E. Type $62C. */
void BlueType(uint16_t obj)
{
    by_scene(obj, 0x022528);
}

/* ---- type $630: a still figure by its scene (its sprite word, the players' character) ---- */

/* $02274C. Shown at x $B0, y $138. */
void StillAt(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xC000);
    wr8(obj + 0x20, 0);
    wr16(obj + O_X, 0xB0);
    wr16(obj + O_Y, 0x138);
    set_map(obj, 0x022772);
}

/* $022642. Scene 0: at x $88, y $140. */
void Still0(uint16_t obj)
{
    if (rd16(obj + O_STATE)) return;
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x4000);
    wr16(obj + 0x0E, 0xE100);
    wr8(obj + 0x20, 0);
    wr16(obj + O_X, 0x88);
    wr16(obj + O_Y, 0x140);
    set_map(obj, 0x022772);
}

static void still_with(uint16_t obj, uint16_t sprite)
{
    if (rd16(obj + O_STATE)) return;
    wr16(obj + O_STATE, 4);
    wr16(obj + 0x0E, sprite);
    StillAt(obj);
}

/* $02267C */
void Still4(uint16_t obj)
{
    still_with(obj, 0xE100);
}

/* $022696 */
void Still8(uint16_t obj)
{
    still_with(obj, 0xE110);
}

/* $0226B0 */
void StillC(uint16_t obj)
{
    still_with(obj, 0xE120);
}

/* $0226CA */
void Still18(uint16_t obj)
{
    still_with(obj, 0xE160);
}

/* $0226E4 */
void Still10(uint16_t obj)
{
    still_with(obj, 0xE130);
}

/* $0226FE. Scene $14: its look by the character player 1 plays (player 2's without player 1). */
void Still14(uint16_t obj)
{
    if (rd16(obj + O_STATE)) return;
    wr16(obj + O_STATE, 4);
    StillAt(obj);
    if (bit8(v_players, 0)) wr16(obj + 0x0E, rd16(v_player1 + 0xCA) ? 0xC140 : 0xC150);
    else wr16(obj + 0x0E, rd16(v_player2 + 0xCA) ? 0xE140 : 0xE150);
}

/* $02260C. Type $630. */
void StillType(uint16_t obj)
{
    by_scene(obj, 0x022626);
}

/* ---- type $634: a figure bobbing in from the left carrying the one at $D700 (type $63C) ---- */

#define CARRIED 0xD700

/* $0228D2. Its y speed by bit 5 of its count: down or up at 1/4. */
void BearerBob(uint16_t obj)
{
    uint16_t n = (uint16_t)(rd16(obj + 0x4E) + 1);
    wr16(obj + 0x4E, n);
    wr32(obj + 0x1C, (n & 0x20) ? 0xFFFFC000 : 0x4000);
}

/* loc_0228F8: the carried one held 4 right, 2 up */
static void carried_held(uint16_t obj)
{
    wr16(CARRIED + O_X, (uint16_t)(rd16(obj + O_X) + 4));
    wr16(CARRIED + O_Y, (uint16_t)(rd16(obj + O_Y) - 2));
}

/* loc_022912: the carried one held out, $18 right, 5 up */
static void carried_out(uint16_t obj)
{
    wr16(CARRIED + O_X, (uint16_t)(rd16(obj + O_X) + 0x18));
    wr16(CARRIED + O_Y, (uint16_t)(rd16(obj + O_Y) - 5));
}

/* $0228C0. Holding it out: after $20 frames the carried one's scene 4. */
void BearerShow2(uint16_t obj)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if (!rd16(obj + 0x4C)) wr16(CARRIED + 0x48, 4);
    carried_out(obj);
}

/* $022888. Bobbing until the scene's step $24: then holding it out again. */
void BearerBob2(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) < 0x24) {
        BearerBob(obj);
        carried_held(obj);
        return;
    }
    next_state(obj);
    set8(obj + 0x0E, 3);
    set_map(obj, 0x0E5E94);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x4C, 0x20);
    wr16(CARRIED + 0x48, 0);
    BearerShow2(obj);
}

/* $022850. Holding it out (after $20 frames its scene 4) until the scene's step $18. */
void BearerShow(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) < 0x18) {
        wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
        if (!rd16(obj + 0x4C)) wr16(CARRIED + 0x48, 4);
        carried_out(obj);
        return;
    }
    next_state(obj);
    set_map(obj, 0x0E5E6C);
    clr8(obj + 0x0E, 3);
    wr16(CARRIED + 0x48, 8);
    BearerBob2(obj);
}

/* $022818. Bobbing until the scene's step $10: then holding it out. */
void BearerWait(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) < 0x10) {
        BearerBob(obj);
        carried_held(obj);
        return;
    }
    next_state(obj);
    set8(obj + 0x0E, 3);
    set_map(obj, 0x0E5E94);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x4C, 0x20);
    wr16(CARRIED + 0x48, 0);
    BearerShow(obj);
}

/* $0227FE. In at 1/4 to x $C0. */
void BearerIn(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_X) < 0xC0) {
        BearerBob(obj);
        carried_held(obj);
        return;
    }
    next_state(obj);
    wr32(obj + 0x18, 0);
    BearerWait(obj);
}

/* $0227B0. At x $40, y $D8; the carried one ($63C, its scene 8) set up. */
void BearerStart(uint16_t obj)
{
    next_state(obj);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(obj + 0x0E, 0xE980);
    wr8(obj + 0x20, 0x80);
    wr16(obj + O_X, 0x40);
    wr16(obj + O_Y, 0xD8);
    set_map(obj, 0x0E5E80);
    wr32(obj + 0x18, 0x4000);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x4E, 0);
    wr16(CARRIED + O_TYPE, 0x63C);
    wr16(CARRIED + O_STATE, 0);
    wr16(CARRIED + 0x48, 8);
    BearerIn(obj);
}

/* $022796 */
void BearerScene0(uint16_t obj)
{
    by_step(obj, 0x0227A4);
}

/* $022778. Type $634. */
void BearerType(uint16_t obj)
{
    by_scene(obj, 0x022792);
}

/* ---- type $63C: the carried figure: hidden (scene 0) or shown in one of two animations ---- */

/* $022956 */
void CarriedHidden(uint16_t obj)
{
    if (rd16(obj + O_STATE)) return;
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

static void carried_shown(uint16_t obj, uint8_t b20, uint32_t map)
{
    if (rd16(obj + O_STATE)) return;
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(obj + 0x0E, 0x8400);
    wr8(obj + 0x20, b20);
    set_map(obj, map);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

/* $022972 */
void CarriedShown4(uint16_t obj)
{
    carried_shown(obj, 0x80, 0x022A14);
}

/* $0229A8 */
void CarriedShown8(uint16_t obj)
{
    carried_shown(obj, 0x80, 0x022A3C);
}

/* $0229DE */
void CarriedShownC(uint16_t obj)
{
    carried_shown(obj, 0, 0x022A14);
}

/* $02292C. Type $63C. */
void CarriedType(uint16_t obj)
{
    by_scene(obj, 0x022946);
}

/* ---- type $638: one of four figures riding the craft at $D940 (held at an offset from it)
 * until the scene's step $18, then thrown off, turning ---- */

#define CRAFT_X 0xD950
#define CRAFT_Y 0xD954

/* the first step: shown, riding */
static void rider_start(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0xCC00);
    wr16(obj + 0x0E, 0x8A00);
    wr8(obj + 0x20, 0xE0);
    set_map(obj, 0x022FAA);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

/* riding the craft, at dx, dy from it */
static void rider_at(uint16_t obj, int16_t dx, int16_t dy)
{
    wr16(obj + O_X, (uint16_t)(rd16(CRAFT_X) + dx));
    wr16(obj + O_Y, (uint16_t)(rd16(CRAFT_Y) + dy));
}

/* at the scene's step `at` its next pose; whether it is still before it */
static int rider_pose(uint16_t obj, int16_t at, uint32_t map)
{
    if ((int16_t)rd16(v_sceneStep) < at) return 1;
    next_state(obj);
    set_map(obj, map);
    return 0;
}

/* slowing (by half a frame) until the scene's step $18; whether it still is */
static int rider_slow(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) >= 0x18) return 0;
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x1C, 0);
    return 1;
}

/* thrown off from x, y at vx, vy */
static void rider_thrown(uint16_t obj, uint16_t x, uint16_t y, uint32_t vx, uint32_t vy)
{
    next_state(obj);
    wr16(obj + O_FLAGS, 0xCE00);
    set_map(obj, 0x022FC8);
    wr16(obj + O_X, x);
    wr16(obj + O_Y, y);
    wr32(obj + 0x18, vx);
    wr32(obj + 0x1C, vy);
}

/* turning every `period` frames */
static void rider_turn(uint16_t obj, uint16_t period)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if (rd16(obj + 0x4C)) return;
    wr16(obj + 0x4C, period);
    wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) ^ 8));
}

/* -- the first ($30, 8 from the craft) -- */

/* $022CA6 */
void CraftRiderATurn(uint16_t obj)
{
    rider_turn(obj, 0xC0);
}

/* $022C54 */
void CraftRiderASlow(uint16_t obj)
{
    if (rider_slow(obj)) return;
    rider_thrown(obj, 0x80, 0xD0, 0x6000, 0x1000);
    wr16(obj + 0x4C, 0xC0);
    CraftRiderATurn(obj);
}

/* $022C3A */
void CraftRiderAPose3(uint16_t obj)
{
    if (rider_pose(obj, 0x14, 0x022FC2)) {
        rider_at(obj, 0x30, 8);
        return;
    }
    CraftRiderASlow(obj);
}

/* $022C20 */
void CraftRiderAPose2(uint16_t obj)
{
    if (rider_pose(obj, 0xC, 0x022FB6)) {
        rider_at(obj, 0x30, 8);
        return;
    }
    CraftRiderAPose3(obj);
}

/* $022BF4 */
void CraftRiderAStart(uint16_t obj)
{
    rider_start(obj);
    CraftRiderAPose2(obj);
}

/* $022BDC. Scene 0. */
void CraftRiderA(uint16_t obj)
{
    by_step(obj, 0x022BEA);
}

/* -- the second ($10, $18 from the craft) -- */

/* $022DA0 */
void CraftRiderBTurn(uint16_t obj)
{
    rider_turn(obj, 0xA0);
}

/* $022D4E */
void CraftRiderBSlow(uint16_t obj)
{
    if (rider_slow(obj)) return;
    rider_thrown(obj, 0x80, 0xF0, 0xC000, 0x800);
    wr16(obj + 0x4C, 0xA0);
    CraftRiderBTurn(obj);
}

/* $022D34 */
void CraftRiderBPose3(uint16_t obj)
{
    if (rider_pose(obj, 0x14, 0x022FC2)) {
        rider_at(obj, 0x10, 0x18);
        return;
    }
    CraftRiderBSlow(obj);
}

/* $022D1A */
void CraftRiderBPose2(uint16_t obj)
{
    if (rider_pose(obj, 0xC, 0x022FB6)) {
        rider_at(obj, 0x10, 0x18);
        return;
    }
    CraftRiderBPose3(obj);
}

/* $022CEE */
void CraftRiderBStart(uint16_t obj)
{
    rider_start(obj);
    CraftRiderBPose2(obj);
}

/* $022CD6. Scene 4. */
void CraftRiderB(uint16_t obj)
{
    by_step(obj, 0x022CE4);
}

/* -- the third ($28, -$10 from the craft) -- */

/* $022E9A */
void CraftRiderCTurn(uint16_t obj)
{
    rider_turn(obj, 0x80);
}

/* $022E48 */
void CraftRiderCSlow(uint16_t obj)
{
    if (rider_slow(obj)) return;
    rider_thrown(obj, 0x1C0, 0x100, 0xFFFF0000, 0);
    wr16(obj + 0x4C, 0x80);
    CraftRiderCTurn(obj);
}

/* $022E2E */
void CraftRiderCPose3(uint16_t obj)
{
    if (rider_pose(obj, 0x14, 0x022FC2)) {
        rider_at(obj, 0x28, -0x10);
        return;
    }
    CraftRiderCSlow(obj);
}

/* $022E14 */
void CraftRiderCPose2(uint16_t obj)
{
    if (rider_pose(obj, 0xC, 0x022FB6)) {
        rider_at(obj, 0x28, -0x10);
        return;
    }
    CraftRiderCPose3(obj);
}

/* $022DE8 */
void CraftRiderCStart(uint16_t obj)
{
    rider_start(obj);
    CraftRiderCPose2(obj);
}

/* $022DD0. Scene 8. */
void CraftRiderC(uint16_t obj)
{
    by_step(obj, 0x022DDE);
}

/* -- the fourth (8, -$20 from the craft), thrown without turning -- */

/* $022F8E */
void CraftRiderDGone(uint16_t obj)
{
    (void)obj;
}

/* $022F42 */
void CraftRiderDSlow(uint16_t obj)
{
    if (rider_slow(obj)) return;
    rider_thrown(obj, 0x1C0, 0x120, 0xFFFFC000, 0xFFFFFC00);
    CraftRiderDGone(obj);
}

/* $022F28 */
void CraftRiderDPose3(uint16_t obj)
{
    if (rider_pose(obj, 0x14, 0x022FC2)) {
        rider_at(obj, 8, -0x20);
        return;
    }
    CraftRiderDSlow(obj);
}

/* $022F0E */
void CraftRiderDPose2(uint16_t obj)
{
    if (rider_pose(obj, 0xC, 0x022FB6)) {
        rider_at(obj, 8, -0x20);
        return;
    }
    CraftRiderDPose3(obj);
}

/* $022EE2 */
void CraftRiderDStart(uint16_t obj)
{
    rider_start(obj);
    CraftRiderDPose2(obj);
}

/* $022ECA. Scene $C. */
void CraftRiderD(uint16_t obj)
{
    by_step(obj, 0x022ED8);
}

/* $022BB2. Type $638. */
void CraftRiderType(uint16_t obj)
{
    by_scene(obj, 0x022BCC);
}

/* ---- type $660: one of four figures by its scene, bobbing, giving off smoke (type $100, an
 * animation list) in the slot after it ---- */

/* $02318C. Its y speed by bit 4 of its count: up or down at 1/8. */
void SmokerBob(uint16_t obj)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) + 1));
    wr32(obj + 0x1C, bit8(obj + 0x4D, 4) ? 0xFFFFE000 : 0x2000);
}

/* loc_02313E: smoke at a random offset (-8 to 7 each way) in the next slot when it is free */
static void smoker_smoke(uint16_t obj)
{
    uint16_t s = (uint16_t)(obj + 0x60);
    if (rd16(s + O_TYPE)) return;
    wr16(s + O_TYPE, 0x100);
    wr32(s + 0x48, 0x05668C);
    wr16(s + O_STATE, 0);
    uint16_t dy = (uint16_t)Random();
    uint16_t dx = (uint16_t)Random();
    dx = (uint16_t)((dx & 0xF) - 8);
    dy = (uint16_t)((dy & 0xF) - 8);
    wr16(s + O_X, (uint16_t)(rd16(obj + O_X) + dx));
    wr16(s + O_Y, (uint16_t)(rd16(obj + O_Y) + dy));
}

/* $022FFC. Scene 0: hidden. */
void SmokerHidden(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) wr16(obj + O_FLAGS, 0);
}

static void smoker(uint16_t obj, uint16_t sprite, uint16_t x, uint16_t count)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0xCC00);
        wr16(obj + 0x0E, sprite);
        wr8(obj + 0x20, 0xE0);
        set_map(obj, 0x0231AE);
        wr16(obj + O_X, x);
        wr16(obj + O_Y, 0xB4);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x4C, count);
    }
    SmokerBob(obj);
    smoker_smoke(obj);
}

/* $02300A */
void Smoker4(uint16_t obj)
{
    smoker(obj, 0x6687, 0xB4, 0);
}

/* $023058 */
void Smoker8(uint16_t obj)
{
    smoker(obj, 0x53B, 0xFC, 1);
}

/* $0230A6 */
void SmokerC(uint16_t obj)
{
    smoker(obj, 0x468B, 0x144, 3);
}

/* $0230F4 */
void Smoker10(uint16_t obj)
{
    smoker(obj, 0x4683, 0x18C, 2);
}

/* $022FCE. Type $660. */
void SmokerType(uint16_t obj)
{
    by_scene(obj, 0x022FE8);
}

/* Object types from $32000 to $36FFF that src/objects31.c left: stones, balloons, a follower,
 * a message cue, and the larger ones of the same stages. */
#include "game.h"

/* $033270. Type $2C4: a stone flying for +$48 frames; into anything solid it turns into a puff
 * (type $100) thrown back slowly. */
void Pebble(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr16(obj + O_FLAGS, 0xC00);
    if (!MapAttribute(rd16(obj + O_X), rd16(obj + O_Y))) return;
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x05652E);
    wr32(obj + 0x1C, ~(uint32_t)((int32_t)rd32(obj + 0x1C) >> 3));
}

/* $03577E. Type $1C: a cue: the stage's message from the script at $39904. */
void MessageCue(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(0x80C2, 8);
    wr32(0x80C8, 0x039904);
}

/* $036E54. Type $4EC: a piece falling. */
void FallingPiece(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + O_FLAGS, 0x8E00);
        wr16(obj + 0x0E, 0x21AB);
        wr16(obj + 0x08, 0xF00);
        wr16(obj + 0x0A, 0xF0F0);
        wr8(obj + 0x20, 0);
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x1800);
}

/* ---- type $218: a craft keeping $30 left of the object at $A800 ---- */

/* $03567E */
void FollowerStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x0E, 0x8300);
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0E08DA);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x20, 0x80);
    wr16(obj + O_Y, 0x13F);
    wr16(obj + O_X, 0x60);
}

/* $0356B6. Speeding toward its place ($2000 a frame, at most $22000). */
void FollowerSteer(uint16_t obj)
{
    int16_t d = (int16_t)(rd16(0xA810) - 0x30 - rd16(obj + O_X));
    uint32_t v = rd32(obj + 0x18);
    if (d >= 0) {
        if ((int32_t)(v - 0x22000) >= 0) return;
        wr32(obj + 0x18, v + 0x2000);
        return;
    }
    if ((int32_t)(v - 0xFFFDE000u) < 0) return;
    wr32(obj + 0x18, v - 0x2000);
}

/* $0356B0 */
void FollowerMove(uint16_t obj)
{
    FollowerSteer(obj);
}

/* $03566C. Type $218. */
void Follower(uint16_t obj)
{
    call_object(rom32(0x035676 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $1E8: a balloon bobbing between y $B0 and $140 (+$48 its pull), heavier after
 * $18 frames; it bursts at either end or shot (2 points) ---- */

/* loc_035160 */
static void balloon_burst(uint16_t obj)
{
    QueueSound(0xC5);
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x05639C);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
}

/* $0350CC. Type $1E8. */
void Balloon(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 2);
        balloon_burst(obj);
        return;
    }
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + O_FLAGS, 0xCE00);
        wr32(obj + 0x08, 0x0DFFD6);
        wr16(obj + 0x4C, 0x18);
        wr16(obj + 0x26, 4);
        wr32(obj + 0x2C, 0xF807F808);
        wr32(obj + 0x28, 0xF807F808);
        wr8(obj + 0x21, 0x30);
        return;
    }
    int16_t y = (int16_t)rd16(obj + O_Y);
    if (y - 0x140 >= 0) {
        wr16(obj + 0x1C, 0xFFFF);
        balloon_burst(obj);
        return;
    }
    if (y - 0xB0 < 0) {
        wr16(obj + 0x1C, 1);
        balloon_burst(obj);
        return;
    }
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if (rd16(obj + 0x4C) & 0x8000) wr32(obj + 0x08, 0x0DFFFA);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + rd32(obj + 0x48));
}

/* $034122. A part (the object at a6) put by its index `i` (a table's offset): its offset from this
 * object (mirrored with it), its look, the object's flips and priority. */
void PartPlace(uint16_t obj, uint16_t i)
{
    uint16_t part = (uint16_t)reg_a6;
    uint16_t dx = rom16(0x03418C + (uint32_t)(int16_t)i), dy = rom16(0x034196 + (uint32_t)(int16_t)i);
    if (bit8(obj + 0x0E, 3)) dx = (uint16_t)-dx;
    wr16(part + O_X, (uint16_t)(rd16(obj + O_X) + dx));
    if (bit8(obj + 0x0E, 4)) dy = (uint16_t)-dy;
    wr16(part + O_Y, (uint16_t)(rd16(obj + O_Y) + dy));
    wr32(part + 0x08, rom32(0x0341A0 + (uint32_t)(int16_t)(uint16_t)(i << 1)));
    wr16(part + 0x0E, (uint16_t)((rd16(part + 0x0E) & 0x7FF) | (rd16(obj + 0x0E) & 0x9800)));
}

/* ---- type $228: a gunboat crossing (its turret, a type $10 in the next slot, at a6) that fires
 * at intervals by the difficulty: shells ($21C) or, of kind 1, seekers ($1F4) ---- */

/* $0351A8 */
void GunboatStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x0E, 0xE6A0);
    wr16(obj + O_FLAGS, 0x8C00);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr8(obj + 0x20, 0x90);
    wr16(obj + O_VITALITY, 0x96);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xFE10F808);
    wr16(obj + 0x4A, rom16(0x035276 + (uint32_t)(int16_t)rd16(0xA20C)));
    uint16_t turret = (uint16_t)(obj + 0x60);
    reg_a6 = 0xFFFF0000u | turret;
    wr16(turret + O_TYPE, 0x10);
    wr16(turret + O_FLAGS, 0xC000);
    wr32(turret + 0x08, 0x0E16DA);
    wr16(turret + 0x0E, 0x86C8);
    wr8(turret + 0x20, 0x80);
    wr16(obj + 0x5A, turret);
    uint16_t k = rd16(v_randomSeed) & 6;
    wr16(obj + O_X, rom16(0x035262 + k));
    wr16(obj + O_Y, rom16(0x03526A + k));
    if ((int16_t)(k - 4) >= 0) wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x1000);
    wr16(obj + 0x18, rom16(0x035272 + (k & 2)));
    wr16(obj + 0x58, (uint16_t)((k & 4) >> 1));
}

/* $035286. Crossing between x $84 and $1BC (then gone, with its turret); firing when +$4A runs
 * out. */
void GunboatFly(uint16_t obj)
{
    int16_t x = (int16_t)rd16(obj + O_X);
    if (x - 0x84 < 0 || x - 0x1BC >= 0) {
        wr16(obj + O_FLAGS, 0x1000);
        wr16((uint16_t)reg_a6 + O_FLAGS, 0x1000);
        return;
    }
    PartPlace(obj, 8);
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (!(rd16(obj + 0x4A) & 0x8000)) return;
    uint16_t target = PlayerTarget();
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    uint16_t side = rd16(obj + 0x58);
    if (!rd16(obj + 6)) {
        wr16(slot + O_TYPE, 0x21C);
        uint16_t k = (uint16_t)((rd8(v_randomSeed) & 8) + (uint16_t)(side << 1));
        wr32(slot + 0x1C, rom32(0x03533E + k));
        wr32(slot + 0x18, rom32(0x03534E + (rd16(v_randomSeed) & 0xC)));
        wr16(obj + 0x4A, rom16(0x03535E + (uint32_t)(int16_t)rd16(v_difficulty)));
        wr16(slot + 0x48, bit8(obj + 0x0E, 4) ? 2 : 0);
        return;
    }
    wr16(slot + O_TYPE, 0x1F4);
    wr16(slot + 0x48, rom16(0x0353A4 + (uint32_t)(int16_t)side));
    wr16(slot + 0x1C, rom16(0x0353A8 + (uint32_t)(int16_t)side));
    wr16(slot + 0x4A, 0xC0);
    wr16(slot + 0x4C, target);
    wr16(slot + 0x4E, 1);
    wr16(obj + 0x4A, rom16(0x0353AC + (uint32_t)(int16_t)rd16(v_difficulty)));
}

/* $03518A. Type $228 (a6: its turret, +$5A); shot down: 540 points, a fan of smoke ($1D8). */
void Gunboat(uint16_t obj)
{
    reg_a6 = (uint32_t)(int32_t)(int16_t)rd16(obj + 0x5A);
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x540);
        wr16((uint16_t)reg_a6 + O_FLAGS, 0x1000);
        wr16(obj + O_TYPE, 0x1D8);
        wr16(obj + O_STATE, 0);
        wr16(obj + 0x48, (int16_t)(rd16(obj + O_Y) - 0x100) < 0 ? 0xA0 : 0x160);
        return;
    }
    call_object(rom32(0x0351A0 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $22C: a craft of two halves (each a type $10 with its flame, the four slots from it;
 * the halves' vitality $DC each), with a light ($C9E0) flashing its engine, crossing up or
 * down; shot down: 4,300 points, both halves to smoke ($1D8) ---- */

#define CRAFT_LIGHT 0xC9E0

/* $0353FC */
void TwinCraftStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x4A, rom16(0x03550A + (uint32_t)(int16_t)rd16(0xA20C)));
    wr16(obj + 0x4C, 0);
    wr16(obj + 0x4E, 0x40);
    wr16(obj + O_X, 0xCC);
    uint16_t s = obj;
    for (int k = 1; k >= 0; k--) {
        wr16(s + O_TYPE, 0x10);
        wr16(s + 0x0E, 0xE6A0);
        wr16(s + O_FLAGS, 0x8C00);
        wr16(s + 0x08, 0x500);
        wr16(s + 0x0A, 0xF8F8);
        wr8(s + 0x20, 0x90);
        wr16(s + O_VITALITY, 0xDC);
        wr8(s + 0x21, 0x20);
        wr32(s + 0x28, 0x0418F008);
        if (!(k & 1)) wr16(s + 0x0E, rd16(s + 0x0E) | 0x800);
        s = (uint16_t)(s + 0x60);
        wr16(s + O_TYPE, 0x10);
        wr16(s + O_FLAGS, 0xC000);
        wr32(s + 0x08, 0x0E1734);
        wr16(s + 0x0E, 0x86C8);
        wr8(s + 0x20, 0x80);
        if (!(k & 1)) wr16(s + 0x0E, rd16(s + 0x0E) | 0x800);
        s = (uint16_t)(s + 0x60);
    }
    wr16(obj + O_TYPE, 0x22C);
    uint16_t r = rd16(v_randomSeed) & 2;
    wr16(obj + O_Y, rom16(0x03551A + r));
    wr16(obj + 0x1C, rom16(0x03551A + r + 4));
    wr16(CRAFT_LIGHT + O_TYPE, 0x10);
    wr16(CRAFT_LIGHT + 0x0E, 0x8480);
    wr32(CRAFT_LIGHT + 0x08, 0x0E08EE);
    wr16(CRAFT_LIGHT + O_FLAGS, 0xC000);
    wr8(CRAFT_LIGHT + 0x20, 0x90);
    wr8(CRAFT_LIGHT + 0x21, 0x70);
    wr32(CRAFT_LIGHT + 0x2C, 0xFE02B050);
    wr32(CRAFT_LIGHT + 0x28, 0xF808A060);
    wr16(CRAFT_LIGHT + O_X, 0x120);
    wr16(CRAFT_LIGHT + 0x26, 0x10);
}

/* $035584. The other three parts at their offsets from the first. */
void TwinCraftParts(uint16_t obj)
{
    uint16_t s = (uint16_t)(obj + 0x60);
    for (uint16_t k = 0; k < 6; k += 2, s = (uint16_t)(s + 0x60)) {
        wr16(s + O_X, (uint16_t)(rom16(0x0355C0 + k) + rd16(obj + O_X)));
        wr16(s + O_Y, (uint16_t)(rom16(0x0355C6 + k) + rd16(obj + O_Y)));
    }
}

/* $0355CC. The engine's light: off while +$4C counts down; then on for $40 frames (flickering
 * a line, alternate frames) before the next count. */
void TwinCraftLight(uint16_t obj)
{
    if (!bit8(obj + 0x4C, 7)) {
        wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
        wr16(CRAFT_LIGHT + O_Y, 0);
        return;
    }
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if (rd16(obj + 0x4E) & 0x8000) {
        wr16(obj + 0x4C, rd16(obj + 0x4A));
        wr16(obj + 0x4E, 0x40);
    }
    uint16_t y = rd16(obj + O_Y);
    if (!bit8(v_gameFrames + 1, 0)) {
        wr16(CRAFT_LIGHT + 0x0E, 0x8480);
        clr8(CRAFT_LIGHT + 0x0E, 4);
        wr16(CRAFT_LIGHT + O_Y, y);
        return;
    }
    wr16(CRAFT_LIGHT + 0x0E, 0xE480);
    set8(CRAFT_LIGHT + 0x0E, 4);
    wr16(CRAFT_LIGHT + O_Y, (uint16_t)(y - 1));
}

/* $035522 */
void TwinCraftFly(uint16_t obj)
{
    uint16_t other = (uint16_t)(obj + 0xC0);
    if ((rd16(obj + O_VITALITY) & 0x8000) || (rd16(other + O_VITALITY) & 0x8000)) {
        AddScore(obj, 0x4300);
        wr16((uint16_t)(obj + 0x60) + O_FLAGS, 0x1000);
        wr16(obj + O_TYPE, 0x1D8);
        wr16(other + O_TYPE, 0x1D8);
        wr16(obj + 0x48, 0);
        wr16(other + 0x48, 0x100);
        wr16((uint16_t)(other + 0x60) + O_FLAGS, 0x1000);
        wr16(CRAFT_LIGHT + O_FLAGS, 0x1000);
        return;
    }
    int16_t y = (int16_t)rd16(obj + O_Y);
    if (y - 0x164 >= 0 || y - 0x84 < 0) {
        for (uint16_t s = obj, k = 0; k < 4; k++, s = (uint16_t)(s + 0x60)) wr16(s + O_FLAGS, 0x1000);
        wr16(CRAFT_LIGHT + O_FLAGS, 0x1000);
        return;
    }
    TwinCraftParts(obj);
    TwinCraftLight(obj);
}

/* $0353EA. Type $22C. */
void TwinCraft(uint16_t obj)
{
    call_object(rom32(0x0353F4 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $2A4: the battleship's mast, rising behind it with the plane's scroll ($FFEC10), its
 * top a type $10 at $DE80: it calls the gunners ($37C) onto the rails' slots while the ship's
 * step ($A950) is under 8, sends seekers ($56C) and $1C0s until 14, then sinks; at $20 it
 * becomes the next part ($2C0) ---- */

#define MAST_TOP  0xDE80

/* loc_03322C: its top */
static void mast_top(uint16_t obj)
{
    wr16(MAST_TOP + O_TYPE, 0x10);
    wr16(MAST_TOP + O_FLAGS, 0x8000);
    wr16(MAST_TOP + 0x0E, 0x88);
    wr16(MAST_TOP + 0x08, 0xD00);
    wr16(MAST_TOP + 0x0A, 0xF0F8);
    wr16(MAST_TOP + O_X, rd16(obj + O_X));
    wr8(MAST_TOP + 0x20, 0);
}

/* $033262. Its y (and its top's, $16 below). */
void MastSetY(uint16_t obj, uint16_t y)
{
    wr16(obj + O_Y, y);
    wr16(MAST_TOP + O_Y, (uint16_t)(y + 0x16));
}

/* $03325A. With the plane's scroll. */
void MastY(uint16_t obj)
{
    MastSetY(obj, (uint16_t)(0x4C2 - rd16(0xEC10)));
}

/* the next part of the ship's tiles (the job list at $33222) */
static void mast_job(void)
{
    uint32_t list = 0x033222;
    NextJob(&list);
}

/* $03302A. The arrow message (step $60), the mast to the right. */
void MastStart(uint16_t obj)
{
    wr16(0x80C2, 0x60);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x48, 0x140);
    wr16(obj + O_FLAGS, 0xE000);
    wr16(obj + 0x0E, 0xAEC0);
    wr16(obj + 0xDA, 0xD800);
    wr32(obj + 0x08, 0x0BFB06);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_X, 0x190);
    wr8(obj + 0x20, 0x10);
    mast_top(obj);
}

/* $03306E. +$48 frames. */
void MastWait(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (!(rd16(obj + 0x48) & 0x8000)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
}

/* $03306A. Rising, $140 frames. */
void MastRiseWait(uint16_t obj)
{
    MastY(obj);
    MastWait(obj);
}

/* $03307C. Up: on odd random frames a gunner ($37C) for a rail's slot (the slot $CC80 on that
 * pairs with one from $DB80, shown and above y $90); the ship's step 8: the next stage. */
void MastRise(uint16_t obj)
{
    MastY(obj);
    if ((int16_t)(rd16(v_cameraStep) - 8) >= 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x48, 0x98);
        wr16(obj + O_FLAGS, 0);
        wr16(MAST_TOP + O_FLAGS, 0x1000);
        wr16(obj + 0x56, 0);
        mast_job();
        return;
    }
    if (bit8(v_randomSeed, 0)) return;
    uint16_t slot = 0xCC80, t;
    if (!SlotFreeBelowCE(&slot, &t)) return;
    uint16_t rail = (uint16_t)(((uint16_t)(slot - 0xCC80) << 1) + 0xDB80);
    if (!bit8(rail + O_FLAGS, 7)) return;
    if ((int16_t)(rd16(rail + O_Y) - 0x90) >= 0) return;
    wr16(slot + O_TYPE, 0x37C);
    wr16(slot + 6, 0);
    wr16(slot + O_X, 0x90);
    wr16(slot + O_Y, 0x88);
    wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + (rd16(v_randomSeed) & 0x3F)));
    wr16(slot + 0x5C, rail);
}

/* $03311A. Seekers ($56C) at the difficulty's interval and every $40 frames a $1C0, until the
 * ship's step $14: then sinking (the boss's timer 4). */
void MastFight(uint16_t obj)
{
    if ((int16_t)(rd16(v_cameraStep) - 0x14) >= 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + O_FLAGS, 0xE000);
        wr16(obj + 0x48, 0x20);
        wr32(obj + 0x08, 0x0C0192);
        wr16(obj + 0x0C, 0);
        wr16(v_bossTimer, 4);
        mast_top(obj);
        return;
    }
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 1));
    if (rd16(obj + 0x56) & 0x8000) {
        wr16(obj + 0x56, rom16(0x0331A6 + (uint32_t)(int16_t)rd16(0xA20C)));
        uint16_t slot, t;
        if (SlotFreeHigh(&slot, &t)) {
            wr16(slot + O_TYPE, 0x56C);
            wr16(slot + 0x48, 0);
            wr16(slot + O_Y, 0x88);
            wr16(slot + O_X, (uint16_t)((rd16(v_randomSeed) & 0xF0) + 0xB8));
        }
    }
    if (rd16(v_gameFrames) & 0x3F) return;
    uint16_t slot = 0xCCE0, t;
    if (!SlotFreeBelowCE(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1C0);
}

/* $0331B6. Sinking with the plane until y $80. */
void MastSink(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (!(rd16(obj + 0x48) & 0x8000)) return;
    MastSetY(obj, (uint16_t)(0x4BF - rd16(0xEC00)));
    if ((int16_t)(rd16(obj + O_Y) - 0x80) >= 0) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0);
    wr16(MAST_TOP + O_FLAGS, 0x1000);
    wr16(obj + 0x48, 0x40);
}

/* $0331EA. Down: with no job running, from the ship's step $20 and $40 frames on, the next
 * part ($2C0) and its tiles. */
void MastEnd(uint16_t obj)
{
    if (rd16(0xF720)) return;
    if ((int16_t)(rd16(v_cameraStep) - 0x20) < 0) return;
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (!(rd16(obj + 0x48) & 0x8000)) return;
    wr16(obj + O_TYPE, 0x2C0);
    wr16(obj + O_X, 0x200);
    wr16(obj + O_Y, 0x80);
    wr16(obj + O_STATE, 0);
    wr16(obj + 0x56, 0);
    mast_job();
}

/* $033004. Type $2A4. */
void Mast(uint16_t obj)
{
    call_object(rom32(0x03300E + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $0585DC: one of four items ($58, $5C, $60, $64) popped up from it */
void DropItemRandom(uint16_t obj)
{
    uint16_t slot;
    if (!SlotFreeLow(&slot)) return;
    wr16(slot + O_TYPE, rom16(0x058612 + (rd16(v_randomSeed) & 6)));
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x1C, 0xFFFD0000);
    wr16(slot + O_FLAGS, 0x8F00);
    wr16(slot + 6, 0);
}

/* $05861A. An item (type $68, kind 15) dropped here, thrown up. */
void DropItem(uint16_t obj)
{
    uint16_t slot;
    if (!SlotFreeLow(&slot)) return;
    wr16(slot + O_TYPE, 0x68);
    wr16(slot + 0x50, 15);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x1C, 0xFFFD0000);
    wr16(slot + O_FLAGS, 0x8F00);
    wr16(slot + 6, 0);
}

/* ---- type $2C0: the battleship's hovering boss, riding its mount (+$58, HoverMount), between
 * the camera's steps $20 and $24: drifting, steering, landing, leaping, slamming; grabbed it is
 * held; beaten, its mount scatters ($8C) and it leaves an item ---- */

/* loc_032B62: gone: its colours back, the stage's spawner ($1A8, kind $40) in its place, an item */
static void hover_boss_end(uint16_t obj)
{
    StageColoursCopy2(0x02A228);
    wr16(obj + O_TYPE, 0x1A8);
    wr16(obj + O_STATE, 0);
    wr16(obj + 6, 0x40);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + O_X, rd16(obj + 0x52));
    DropItem(obj);
}

/* loc_032B8C: landed: facing the player */
static void hover_boss_land(uint16_t obj)
{
    wr16(obj + O_STATE, 0xC);
    wr32(obj + 0x08, 0x0DDB84);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x50, 0x20);
    uint16_t target = PlayerTarget();
    if ((int16_t)(rd16(target + O_X) - rd16(obj + O_X)) < 0) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
}

/* loc_032BF8: back to drifting */
static void hover_boss_drift(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    wr32(obj + 0x08, 0x0DE258);
    wr16(obj + 0x50, 0);
    wr8(obj + 0x20, 0x70);
    wr8(obj + 0x21, 0x22);
}

static int hover_on_floor(uint16_t obj)
{
    uint16_t d3, d4;
    return !ObjectFloor16(obj, &d3, &d4);
}

/* $0328E2. Waiting for the camera's step $20; then on its mount at the right. */
void HoverBossWait(uint16_t obj)
{
    if ((int16_t)(rd16(v_cameraStep) - 0x20) < 0) return;
    if ((int16_t)(rd16(v_cameraStep) - 0x24) >= 0) {
        hover_boss_end(obj);
        return;
    }
    HoverMount(obj, 0xC);
    wr16(obj + O_VITALITY, 0x186);
    wr8(obj + 0x21, 0x22);
    wr16(obj + 0x4C, 0);
    wr16(obj + 0xDA, 0x20);
    wr16(obj + 0x0E, 1);
    wr16(obj + O_X, 0x1E0);
    wr16(obj + O_Y, 0xC0);
    wr16(obj + 0x4A, 0xE0);
    wr16(obj + 0x50, 0);
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0xFEFF);
}

/* $032950. Drifting; after +$50 frames steering. */
void HoverBossDrift(uint16_t obj)
{
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if (rd16(obj + 0x50) & 0x8000) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x50, 0x18);
        HoverWait(obj);
    }
    HoverFace(obj);
    HoverDrift(obj);
    HoverMountPlace(obj);
    HoverSparks(obj);
    if (hover_on_floor(obj)) hover_boss_land(obj);
}

/* $032982. Steering for $18 frames; then drifting $40. */
void HoverBossSteer(uint16_t obj)
{
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if (rd16(obj + 0x50) & 0x8000) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
        wr16(obj + 0x50, 0x40);
    }
    HoverSteer(obj);
    HoverFace(obj);
    HoverMountPlace(obj);
    HoverSparks(obj);
    if (hover_on_floor(obj)) hover_boss_land(obj);
}

/* $0329B0. Landed, sinking a little; then a leap. */
void HoverBossLanded(uint16_t obj)
{
    wr32(obj + 0x18, 0);
    HoverMountAt(obj, 2, 0xFFF0);
    HoverSparks(obj);
    if (!hover_on_floor(obj)) {
        wr16(obj + O_STATE, 0x10);
        wr32(obj + 0x08, 0x0DE258);
        wr16(obj + 0x50, 8);
        return;
    }
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if (rd16(obj + 0x50) & 0x8000) {
        wr16(obj + O_STATE, 0x14);
        wr32(obj + 0x08, 0x0DE258);
        wr32(obj + 0x1C, 0xFFFB8000);
        return;
    }
    if (!bit8(v_gameFrames + 1, 1)) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 2));
}

/* $0329E8. Rising off the floor, drifting. */
void HoverBossRise(uint16_t obj)
{
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if (rd16(obj + 0x50) & 0x8000) {
        wr16(obj + O_STATE, 4);
        wr16(obj + 0x50, 0);
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x4000);
    HoverDrift(obj);
    HoverFace(obj);
    HoverMountPlace(obj);
    HoverSparks(obj);
}

/* $032A16. Leaping up; above y $A0 the slam (its way by its facing). */
void HoverBossLeap(uint16_t obj)
{
    HoverMountPlace(obj);
    HoverSparks(obj);
    if ((int16_t)(rd16(obj + O_Y) - 0xA0) >= 0) return;
    wr8(obj + 0x20, 0x78);
    wr32(obj + 0x08, 0x0DE808);
    wr16(obj + 0x50, 0xC);
    wr32(obj + 0x1C, 0x68000);
    wr8(obj + 0x21, 0x30);
    wr8(obj + 0x23, 0x20);
    wr16(obj + 0x26, 0xA);
    wr32(obj + 0x2C, 0xF008F010);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + (bit8(obj + 0x0E, 3) ? 8 : 0xC)));
}

/* $032A76. Held by a player (+$23 bit 1 each frame); let go: drifting again. */
void HoverBossHeld(uint16_t obj)
{
    if (!bit8(obj + 0x23, 1)) {
        hover_boss_drift(obj);
        return;
    }
    clr8(obj + 0x23, 1);
    HoverMountPlace(obj);
}

/* loc_032A9E: the slam: its look turning, falling back; rising again under y $B0: drifting */
static void hover_boss_slam(uint16_t obj)
{
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if (!(rd16(obj + 0x50) & 0x8000)) wr32(obj + 0x08, rom32(0x032AE0 + (rd16(obj + 0x50) & 0xC)));
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x4000);
    HoverMountPlace(obj);
    HoverSparks(obj);
    if (!bit8(obj + 0x1C, 7)) return;
    if ((int16_t)(rd16(obj + O_Y) - 0xB0) < 0) hover_boss_drift(obj);
}

/* $032A96. Slamming to the left. */
void HoverBossSlamLeft(uint16_t obj)
{
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x2000);
    hover_boss_slam(obj);
}

/* $032A8A. To the right. */
void HoverBossSlam(uint16_t obj)
{
    wr32(obj + 0x18, rd32(obj + 0x18) + 0x2000);
    hover_boss_slam(obj);
}

/* $032AF0. Beaten: flickering where it was ($40 frames); then its mount scatters ($8C), the
 * message's arrow (step $58), the camera free, its end. */
void HoverBossBeaten(uint16_t obj)
{
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if (rd16(obj + 0x50) & 0x8000) {
        uint16_t mount = rd16(obj + 0x58);
        wr16(mount + O_TYPE, 0x8C);
        wr16(mount + 0x48, 0x60);
        wr16(mount + 0x4A, 0x10);
        wr16(mount + 0x4C, 8);
        wr16(mount + O_X, rd16(obj + 0x52));
        wr16(mount + O_Y, rd16(obj + O_Y));
        clr8(v_cameraHold, 7);
        wr16(0x80C2, 0x58);
        hover_boss_end(obj);
        return;
    }
    if (!bit8(v_gameFrames + 1, 0)) {
        wr16(obj + O_X, 0x240);
    } else {
        wr16(obj + O_X, rd16(obj + 0x52));
        uint16_t y = rd16(obj + 0x54);
        if (bit8(v_gameFrames + 1, 1)) y = (uint16_t)(y + 3);
        wr16(obj + O_Y, y);
    }
    HoverMountAt(obj, 2, 0xFFF0);
}

/* $0327F2. Type $2C0: its hum (not at a checkpoint), its colours flashing when hit, beaten or
 * grabbed, its step. */
void HoverBoss(uint16_t obj)
{
    if (!rd16(v_checkpoint)) HoverHum();
    if (rd16(obj + O_VITALITY) & 0x8000) {
        wr16(obj + O_STATE, 0x24);
        wr8(obj + 0x21, 0);
        wr16(obj + O_VITALITY, 0);
        wr32(obj + 0x08, 0x0DE980);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x50, 0x40);
        wr16(obj + 0x52, rd16(obj + O_X));
        wr16(obj + 0x54, rd16(obj + O_Y));
        wr8(obj + 0x20, 0x70);
        clr8(obj + 0x22, 1);
    } else if (bit8(obj + 0x22, 1)) {
        clr8(obj + 0x22, 1);
        wr16(obj + O_STATE, 0x18);
        wr32(obj + 0x08, 0x0DE258);
        wr8(obj + 0x20, 0x70);
        wr8(obj + 0x21, 0x22);
    } else {
        uint16_t flash = rd16(obj + 0x4C);
        int hit_check = flash & 0x8000;
        if (!hit_check) {
            wr16(obj + 0x4C, (uint16_t)(flash - 1));
            hit_check = rd16(obj + 0x4C) & 0x8000;
        }
        if (hit_check && bit8(obj + 0x22, 5)) {
            wr16(obj + 0x4C, 4);
            StageColoursCopy(0x02A1B6, flash);
        } else {
            StageColours();
        }
    }
    call_object(rom32(0x0328BA + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $1D0: a trooper on a jet pack (its flame the part at +$5A), bobbing, now and then
 * firing at its player ($1A0 shots); the stage's $8080 bit 0 downs it (bit 1 sends it up and
 * away); shot down: 1,600 points, falling aside ($1D8 its flame) ---- */

/* $0345B2: hovering again (state 4) for $20-$5F frames */
static void jet_hover(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + 0x48, (uint16_t)((rd16(v_randomSeed) & 0x3F) + 0x20));
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0E0334);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x58, 0);
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x28);
}

/* $0347B8: the climb or fall eased by $800 */
static void jet_ease(uint16_t obj)
{
    if (!bit8(obj + 0x1C, 7)) wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x800);
    else wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x800);
}

/* $033EBC: landed at a side, a pause of $C (state $10) */
static void jet_pause(uint16_t obj)
{
    wr16(obj + 0x48, 0x0C);
    wr16(obj + O_STATE, 0x10);
    wr16(obj + O_FLAGS, 0xE800);
    wr32(obj + 0x08, 0x0DF81A);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x58, 0);
    wr8(obj + 0x5E, (uint8_t)(rd8(obj + 0x5E) & ~0x10));
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x28);
}

/* $0346CC: downed: the flame to smoke ($1D8), falling away from the nearer side */
static void jet_downed(uint16_t obj)
{
    uint16_t flame = (uint16_t)reg_a6;
    wr8(obj + 0x21, 0);
    wr16(obj + O_VITALITY, 0);
    wr16(obj + O_FLAGS, 0xCE00);
    wr32(obj + 0x08, 0x0DFFD6);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x1C, 2);
    wr16(flame + O_TYPE, 0x1D8);
    if ((int16_t)(rd16(obj + O_X) - 0x120) >= 0) {
        wr16(obj + O_STATE, 0x1C);
        wr32(obj + 0x18, 0xFFFA0000);
        wr16(flame + 0x48, 0x160);
    } else {
        wr16(obj + O_STATE, 0x20);
        wr32(obj + 0x18, 0x60000);
        wr16(flame + 0x48, 0x1A0);
    }
}

/* $0343FE */
void JetTrooperStart(uint16_t obj)
{
    wr16(obj + 0x0E, 0x8300);
    uint16_t r = rd16(v_randomSeed);
    wr16(obj + 0x50, r);
    wr16(obj + 0x56, (uint16_t)(r + rd16(v_gameFrames)));
    wr8(obj + 0x20, 0xB0);
    r = rd16(v_randomSeed);
    wr16(obj + O_X, rom16(0x034448 + (r & 2)));
    wr16(obj + O_Y, rom16(0x03444C + ((uint16_t)((int16_t)r >> 4) & 2)));
    jet_hover(obj);
}

/* $034730: bobbing: every $20 frames a coin picks up or down; up to $90 (at most 2 a frame), down
 * to $160 */
static void jet_bob(uint16_t obj)
{
    if (!(rd16(obj + 0x50) & 0x1F)) wr16(obj + 0x52, (uint16_t)(rd16(v_randomSeed) & 1));
    uint32_t vy = rd32(obj + 0x1C);
    if (rd16(obj + 0x52)) {
        if ((int16_t)(rd16(obj + O_Y) - 0x90) < 0) { wr32(obj + 0x1C, 0x40000); return; }
        if ((vy & 0x80000000u) && (int32_t)(vy - 0xFFFE0000u) < 0) return;
        wr32(obj + 0x1C, vy - 0x2000);
    } else {
        if ((int16_t)(rd16(obj + O_Y) - 0x160) >= 0) { wr32(obj + 0x1C, 0xFFFC0000); return; }
        if (!(vy & 0x80000000u) && (int32_t)(vy - 0x20000) >= 0) return;
        wr32(obj + 0x1C, vy + 0x2000);
    }
}

/* $034450 */
void JetTrooperHover(uint16_t obj)
{
    if (bit8(0x8080, 1)) {
        wr16(obj + O_STATE, 0x24);
        wr16(obj + O_FLAGS, 0xEC00);
        wr32(obj + 0x08, 0x0E0334);
        wr16(obj + 0x0C, 0);
        wr32(obj + 0x28, 0xF410F808);
        wr8(obj + 0x21, 0x28);
        PartPlace(obj, 6);
        return;
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        wr16(obj + 0x48, 0x3C);
        wr16(obj + O_STATE, 0x14);
        wr16(obj + O_FLAGS, 0xCC00);
        wr32(obj + 0x08, 0x0E0340);
        wr16(obj + 0x0C, 0);
        wr32(obj + 0x28, 0xF410F808);
        wr8(obj + 0x21, 0x28);
        return;
    }
    jet_bob(obj);
    PartPlace(obj, 6);
}

/* $03447A */
void JetTrooperLeft(uint16_t obj)
{
    jet_ease(obj);
    if ((int16_t)(rd16(obj + O_X) - 0x120) < 0) wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) | 8));
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x4000);
    if ((int16_t)(rd16(obj + O_X) - 0xB9) < 0) {
        wr32(obj + 0x18, 0);
        wr16(obj + O_X, 0xB1);
        jet_pause(obj);
    }
    PartPlace(obj, 6);
}

/* $0344BA */
void JetTrooperRight(uint16_t obj)
{
    jet_ease(obj);
    if ((int16_t)(rd16(obj + O_X) - 0x120) >= 0) wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) & ~8));
    wr32(obj + 0x18, rd32(obj + 0x18) + 0x4000);
    if ((int16_t)(rd16(obj + O_X) - 0x168) >= 0) {
        wr32(obj + 0x18, 0);
        wr16(obj + O_X, 0x13F);
        jet_pause(obj);
    }
    PartPlace(obj, 6);
}

/* $0344FA */
void JetTrooperPause(uint16_t obj)
{
    jet_ease(obj);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) jet_hover(obj);
    PartPlace(obj, 6);
}

/* $034512: aiming at its player for a second, then one shot */
void JetTrooperAim(uint16_t obj)
{
    jet_ease(obj);
    uint16_t target = PlayerBySlot(obj);
    uint16_t d0, d5, d6, d7 = 0x1A0;
    AimFacing(obj, target, 0x02ADE6, &d0, &d5, &d6, &d7);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        wr16(obj + 0x48, 0x10);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        AimedSpawnHigh(obj, d5, d6, d7, 0x02AE06);
    }
    PartPlace(obj, 6);
}

/* $034552 */
void JetTrooperFired(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) jet_hover(obj);
    PartPlace(obj, 6);
}

/* $034566 */
void JetTrooperFallLeft(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x8000);
    wr32(obj + 0x18, rd32(obj + 0x18) + 0x2000);
}

/* $034578 */
void JetTrooperFallRight(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x8000);
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x2000);
}

/* $03458A: away up; gone above $80 */
void JetTrooperLeave(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_Y) - 0x80) < 0) {
        wr16(obj + O_FLAGS, 0x1000);
        wr16((uint16_t)reg_a6 + O_FLAGS, 0x1000);
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x2000);
    PartPlace(obj, 6);
}

/* $03438A */
void JetTrooper(uint16_t obj)
{
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) + 1));
    reg_a6 = (uint32_t)(int32_t)(int16_t)rd16(obj + 0x5A);
    if (!bit8(obj + 0x5E, 0)) {
        if (bit8(0x8080, 0)) { jet_downed(obj); return; }
        if (rd16(obj + O_VITALITY) & 0x8000) {
            AddScore(obj, 0x160);
            jet_downed(obj);
        }
    }
    if ((int16_t)(rd16(obj + O_X) - 0x100) < 0) wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) & ~8));
    else wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) | 8));
    call_object(rom32(0x0343D6 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $20: a boss painted on plane A (its x following plane A's scroll, the camera held
 * while it lives), rocked by what hits it (the lines' skew +$4A, its swing +$4E); beaten: 29,400
 * points, its picture broken up, sparks and wreckage thrown up ---- */

/* $0371A6: the 48 lines' scroll as plane A's */
static void hulk_lines(void)
{
    uint16_t v = (uint16_t)-rd16(0xA900);
    for (uint16_t a = 0xE6A0, k = 0; k < 0x30; k++, a += 4) wr16(a, v);
}

/* $0370CA: struck (+$22 bit 5) by something (+$42): rocked away from its push (straight down
 * the middle: away from the side it is on), the swing grown by $600 up to $C00; the skew swung
 * $200 a frame between +/-$1200, the swing spent $200 at each end */
static void hulk_rock(uint16_t obj)
{
    if (bit8(obj + 0x22, 5) && rd16(obj + 0x42)) {
        int16_t vx = (int16_t)rd16(rd16(obj + 0x42) + 0x18);
        if (vx < 0 || (vx == 0 && (int16_t)(rd16(obj + O_X) - 0x120) >= 0)) wr16(obj + 0x4C, 0xFFFF);
        else wr16(obj + 0x4C, 0);
        if ((int16_t)(rd16(obj + 0x4E) - 0xC00) < 0) wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) + 0x600));
    }
    if (!rd16(obj + 0x4E) && !rd16(obj + 0x4A)) return;
    uint16_t d0 = rd16(obj + 0x4A);
    int up;
    if (rd16(obj + 0x4C) & 0x8000) up = (int16_t)(d0 - 0xEE00) < 0;
    else up = (int16_t)(d0 - 0x1200) < 0;
    if (up) {
        d0 = (uint16_t)((d0 + 0x200) & 0xFE00);
        wr16(obj + 0x4A, d0);
        uint16_t r = (uint16_t)(d0 - rd16(obj + 0x4E));
        if (r && (int16_t)r < 0) return;
        if (rd16(obj + 0x4E)) wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 0x200));
        wr16(obj + 0x4C, 0xFFFF);
        return;
    }
    uint16_t lim = (uint16_t)-rd16(obj + 0x4E);
    d0 = (uint16_t)((d0 - 0x200) & 0xFE00);
    wr16(obj + 0x4A, d0);
    uint16_t r = (uint16_t)(d0 - lim);
    if (r && (int16_t)r >= 0) return;
    if (!rd16(obj + 0x4E)) { wr16(obj + 0x4C, 0xFFFF); return; }
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 0x200));
    wr16(obj + 0x4C, 0);
}

/* $0371C2: the skew down 72 lines (every other line, the odd frames' set a line on) */
static void hulk_skew(uint16_t obj)
{
    uint16_t a = (rd16(v_gameFrames) & 1) ? 0xE69C : 0xE6A0;
    uint32_t d0 = (uint32_t)-rd32(0xA900), d2 = 0;
    uint32_t d1 = (uint32_t)(int32_t)(int16_t)rd16(obj + 0x4A);
    for (int k = 0; k < 0x48; k++, a = (uint16_t)(a - 8)) {
        d0 += d2;
        d2 += d1;
        wr16(a, (uint16_t)(d0 >> 16));
    }
}

/* $0371FC */
static void hulk_place(uint16_t obj)
{
    wr16(obj + O_X, (uint16_t)(rd16(0xE4C0) - 0xF7F0));
}

static void hulk_frame(uint16_t obj)
{
    hulk_rock(obj);
    hulk_skew(obj);
    hulk_place(obj);
}

/* $03729A: in the fight: once $40 frames are out (if $A20A), a $128 in $CE00 when free; a $124
 * at it as often as the difficulty's mask says */
static void hulk_fire(uint16_t obj)
{
    if (rd16(0xA20A)) {
        wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 1));
        if ((int16_t)rd16(obj + 0x54) < 0 && !rd16(0xCE00)) {
            wr16(0xCE00 + O_TYPE, 0x128);
            wr16(0xCE00 + O_X, 0x100);
            wr16(0xCE00 + O_Y, 0x100);
            wr16(0xCE00 + O_STATE, 0);
        }
    }
    uint16_t mask = rom16(0x037308 + (uint32_t)(int16_t)rd16(0xA20C));
    if (rd16(v_gameFrames) & mask) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x124);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_STATE, 0);
}

/* $036EB0 */
void HulkStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(0x80C2, 8);
    wr32(0x80C8, 0x0398F0);
    wr8(v_cameraHold, (uint8_t)(rd8(v_cameraHold) | 1));
    wr16(obj + O_VITALITY, 0x7FFF);
    wr32(obj + 0x48, 0);
    wr16(obj + 0x54, 0x40);
    wr16(obj + O_FLAGS, 0x100);
    wr16(obj + O_Y, 0xB8);
    wr32(obj + 0x28, 0xF01CE020);
    wr8(obj + 0x21, 0x20);
    hulk_lines();
    hulk_frame(obj);
}

/* $036EF6: waiting for the camera to stop on it (no message showing): then the fight, $4D1 its
 * vitality */
void HulkWait(uint16_t obj)
{
    wr16(obj + O_VITALITY, 0x7FFF);
    wr8(v_planeFlagsA, (uint8_t)(rd8(v_planeFlagsA) | 0x80));
    if (!rd16(0x80C2) && bit8(v_cameraHold, 7)) {
        wr16(0x80C2, 0x10);
        wr16(0x80D6, obj);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + O_VITALITY, 0x4D1);
    }
    wr16(v_shakeSprites, 3);
    hulk_lines();
    hulk_frame(obj);
}

/* $036F36 */
void HulkFight(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        wr8(obj + 0x21, 0);
        wr16(obj + O_VITALITY, 0);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        ObjectsVanishFrom(0xC9E0, 0, 0);
    }
    hulk_fire(obj);
    hulk_frame(obj);
}

/* $036F6A: beaten, rocking still; once still, the points and its picture broken */
void HulkBeaten(uint16_t obj)
{
    hulk_rock(obj);
    hulk_skew(obj);
    if (rd16(obj + 0x4E) || rd16(obj + 0x4A)) return;
    AddScore(obj, 0x29400);
    wr8(v_planeFlagsA, (uint8_t)(rd8(v_planeFlagsA) & ~0x80));
    wr8(0xF7E7, 0);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x48, 0x80);
    static const uint16_t at[9] = { 0x260, 0x460, 0x660, 0x458, 0x468, 0x258, 0x268, 0xCA60, 0xC860 };
    static const uint32_t src[9] = { 0xFFFF4000, 0xFFFF4000, 0xFFFF4000, 0xFFFF4000, 0xFFFF4000,
                                     0xFFFF4000, 0xFFFF4000, 0xFFFF47C0, 0xFFFF4780 };
    for (int k = 0; k < 9; k++) PlaneAChunk(src[k], at[k]);
}

/* $036FA8: shaking $80 frames, a blast sound each 8, sparks ($104) flung up from it */
void HulkBlowUp(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 7)) QueueSound(0xC8);
    wr16(v_shakeSprites, 0x0A);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x48, 0x40);
    }
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x104);
    wr32(slot + 0x48, 0x05639C);
    wr16(slot + O_X, 0x120);
    wr16(slot + O_Y, 0x128);
    uint16_t i = rd16(v_randomSeed) & 0x0E, j = rd8(v_randomSeed) & 0x0E;
    uint32_t vx = (uint32_t)(int32_t)(int16_t)rom16(0x03703E + i) << 4;
    if (!bit8(v_gameFrames + 1, 0)) vx = ~vx;
    wr32(slot + 0x18, vx);
    wr32(slot + 0x1C, (uint32_t)(int32_t)(int16_t)rom16(0x03704E + j) << 4);
}

/* $03705E: wreckage ($1A4) thrown up every 4 frames for $40; then gone, the camera let go */
void HulkWreck(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        wr8(v_cameraHold, (uint8_t)(rd8(v_cameraHold) & ~0x80));
        wr16(obj + O_FLAGS, 0x1000);
        wr16(0x80C2, 0x58);
        return;
    }
    if (rd16(v_gameFrames) & 3) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1A4);
    wr16(slot + O_X, 0x118);
    wr16(slot + O_Y, 0x118);
    wr16(slot + 0x1C, 0xFFFD);
    uint32_t vx = rd16(v_randomSeed);
    if (!bit8(v_randomSeed, 0)) vx = ~vx;
    wr32(slot + 0x18, vx);
    wr32(slot + 0x18, rd32(slot + 0x18) - 0x10000);
}

/* $036E88 */
void Hulk(uint16_t obj)
{
    wr16(v_shakeSprites, 3);
    call_object(rom32(0x036E98 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $1D4: a wagon's kinds (+$4C picks; +$48 its x on the planes, +$4A its y): gunners
 * popping up, the engine driving the planes' speed ($A930) between two marks, its smokestack and
 * dust, the wheels bouncing, the cargo shaking; the boss wagon flashing the palette up and down,
 * then shelling; shot apart, the cargo flung away as wreckage ($1E4) ---- */

/* $034ED6 */
static void wagon_x(uint16_t obj)
{
    wr16(obj + O_X, (uint16_t)(rd16(obj + 0x48) - rd16(v_planes)));
}

/* $034EE4 */
static void wagon_y(uint16_t obj)
{
    wr16(obj + O_Y, (uint16_t)(rd16(obj + 0x4A) - rd16(0xA924)));
}

/* $034EF2: every 16th frame the engine's sound; every 4th a cinder ($1E4) blown back off the
 * planes' speed */
static void wagon_cinders(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 0x0F)) QueueSound(0xCF);
    if (rd16(v_gameFrames) & 3) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1E4);
    wr32(slot + 0x48, 0x05639C);
    wr16(slot + 0x54, 0x8C00);
    wr32(slot + 0x1C, 0xFFFF0000);
    uint32_t v = ~rd32(v_planeSpeeds);
    v = (v & 0xFFFF0000u) | (uint16_t)(v + 1);
    wr32(slot + 0x18, v);
    wr16(slot + O_X, (uint16_t)(rd16(obj + 0x48) - rd16(v_planes) - 0x100 + (rd16(v_randomSeed) & 0x7F)));
    wr16(slot + O_Y, (uint16_t)(0x12C - (rd8(v_randomSeed) & 7)));
}

/* $03385E: the planes driven back and forth between the marks `back` (to be passed: then +$4A 2)
 * and `ahead` (+$4A 0 again once reached): sped up by $1000 to $22000, slowed by $800 to -$18000 */
void WagonDrive(uint16_t obj, uint16_t back, uint16_t ahead)
{
    if (!rd16(obj + 0x4A)) {
        if ((int16_t)(back - rd16(v_planes)) < 0) wr16(obj + 0x4A, 2);
        if ((int32_t)(rd32(v_planeSpeeds) - 0x22000) < 0) wr32(v_planeSpeeds, rd32(v_planeSpeeds) + 0x1000);
        return;
    }
    if ((int16_t)(ahead - rd16(v_planes)) >= 0) wr16(obj + 0x4A, 0);
    if ((int32_t)(rd32(v_planeSpeeds) - 0xFFFE8000u) < 0) return;
    wr32(v_planeSpeeds, rd32(v_planeSpeeds) - 0x800);
}

/* $03503A: every 16th frame a shell ($1E8) lobbed up or down, its spread by the difficulty */
static void wagon_shell(uint16_t obj)
{
    if (rd16(v_gameFrames) & 0x0F) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1E8);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) - 8));
    uint16_t m = rom16(0x0350BC + (uint32_t)(int16_t)rd16(0xA20C));
    wr16(slot + 0x18, (uint16_t)(1 - (rd16(v_randomSeed) & m)));
    if (!bit8(v_randomSeed, 7)) {
        wr16(slot + 0x0E, 0x9B00);
        wr32(slot + 0x48, 0xFFFFF000);
        wr32(slot + 0x1C, 0x18000);
    } else {
        wr16(slot + 0x0E, 0x8B00);
        wr32(slot + 0x48, 0x1000);
        wr32(slot + 0x1C, 0xFFFE8000);
    }
}

/* $034F76: sinking: every 16th frame its sound, each frame wreckage ($1DC) thrown */
static void wagon_wreck(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 0x0F)) QueueSound(0xC5);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1DC);
    wr32(slot + 0x48, 0x05639C);
    wr16(slot + O_X, (uint16_t)(rd16(obj + 0x48) - rd16(v_planes) + (rd16(v_randomSeed) & 0x7F)));
    wr16(slot + O_Y, (uint16_t)(0x12C - (rd8(v_randomSeed) & 0x3F)));
    wr32(slot + 0x18, ~rd32(v_planeSpeeds) + 1 - 0x40000);
    wr32(slot + 0x1C, (int16_t)(rd16(slot + O_Y) - 0x10C) < 0 ? 0xFFFFC000 : 0x4000);
}

/* $033908: a wagon's kinds from `list` (x on the planes, y, kind, look, vitality; $FFFF ends) put
 * in the slots from `slot` on, at `x`, `y` */
void WagonGroup(uint32_t list, uint16_t slot, uint16_t x, uint16_t y)
{
    for (;;) {
        uint16_t w = bus_rd16(list); list += 2;
        if (w == 0xFFFF) return;
        wr16(slot + 0x48, w);
        wr16(slot + 0x4A, bus_rd16(list)); list += 2;
        wr16(slot + 0x4C, bus_rd16(list)); list += 2;
        wr16(slot + 0x0E, bus_rd16(list)); list += 2;
        wr16(slot + O_VITALITY, bus_rd16(list)); list += 2;
        wr16(slot + O_X, x);
        wr16(slot + O_Y, y);
        wr16(slot + O_TYPE, 0x1D4);
        wr16(slot + O_STATE, 0);
        slot = (uint16_t)(slot + 0x60);
    }
}

/* $033A8E: the stage's next part ($1F0, its job $8004) at $1C0,$110 */
void WagonNext(void)
{
    uint16_t slot;
    if (!SlotFreeLow(&slot)) return;
    wr16(slot + O_TYPE, 0x1F0);
    wr16(slot + 0x06, 0x8004);
    wr16(slot + O_X, 0x1C0);
    wr16(slot + O_Y, 0x110);
}

/* $035002: every wagon part in $CE00-$D3FF flung away as wreckage */
static void wagon_scatter(void)
{
    for (uint16_t a = 0xCE00; (int16_t)(a - 0xD400) < 0; a = (uint16_t)(a + 0x60)) {
        if (rd16(a + O_TYPE) != 0x1D4) continue;
        wr16(a + O_TYPE, 0x1E4);
        wr32(a + 0x48, 0x05639C);
        wr16(a + 0x54, 0x8C00);
        wr16(a + O_FLAGS, 0);
        wr16(a + 0x18, 0xFFFE);
    }
}

/* $0348B8: by its kind, its first state, count and look */
void WagonStart(uint16_t obj)
{
    wr8(obj + 0x20, 0xD0);
    wr16(obj + O_FLAGS, 0xE000);
    wr16(obj + 0x0C, 0);
    uint16_t k = rd16(obj + 0x4C);
    wr16(obj + O_STATE, rom16(0x0348FE + (uint32_t)(int16_t)k));
    wr16(obj + 0x4E, rom16(0x034910 + (uint32_t)(int16_t)k));
    wr32(obj + 0x08, rom32(0x034922 + (uint32_t)(int16_t)(uint16_t)(k << 1)));
    wr32(obj + 0x28, 0xF40CF40C);
}

/* $034946: a gunner rising to $120, hidden for its count; then up */
void WagonRise(uint16_t obj)
{
    if (rd16(obj + O_Y) != 0x120) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 1));
    wagon_x(obj);
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) >= 0) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr8(obj + 0x21, 0x20);
    wr16(obj + O_FLAGS, 0xC000);
    wr32(obj + 0x08, 0x0DF508);
    wr16(obj + 0x50, 0x3C);
}

/* $034980: up to $118, aiming at its player; then a shot */
void WagonAim(uint16_t obj)
{
    if (rd16(obj + O_Y) != 0x118) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 1));
    wagon_x(obj);
    uint16_t target = PlayerBySlot(obj);
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if ((int16_t)rd16(obj + 0x50) >= 0) {
        uint16_t d0, d5, d6, d7 = 0x1A0;
        FaceToward(obj, target, 0x02AD16, &d0, &d5, &d6, &d7);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x4E, 0x10);
    AimedSpawnHighToward(obj, target, 0x02AD16, 0x02AD56, 0x1A0);
}

/* $0349C6: after the shot: the first kind ducks again for $20-$3F, the others aim again */
void WagonShot(uint16_t obj)
{
    wagon_x(obj);
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) >= 0) return;
    if (rd16(obj + 0x4C)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
        wr16(obj + 0x50, 1);
        return;
    }
    wr8(obj + 0x21, 0);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 8));
    wr16(obj + O_FLAGS, 0xE000);
    wr32(obj + 0x08, 0x0DF4FC);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x4E, (uint16_t)((rd16(v_randomSeed) & 0x1F) + 0x20));
    Random();
}

/* $034A1E */
void WagonDust(uint16_t obj)
{
    wagon_y(obj);
    wr8(obj + 0x21, 0x20);
    TrailDust(obj);
}

/* $034B58: the engine at $11A driving the planes, cinders flying; after its count, the speed up */
void WagonEngine(uint16_t obj)
{
    wr16(obj + O_Y, 0x11A);
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 8));
        wr8(obj + 0x20, 0x90);
        wr16(obj + O_FLAGS, 0xE000);
        wr32(obj + 0x08, 0x0CFF2C);
        wr16(obj + 0x0C, 0);
    }
    WagonDrive(obj, 0x14A0, 0x1498);
    wagon_cinders(obj);
    wagon_x(obj);
}

/* $034A88: a wheel bouncing on $118, rolling one way then the other */
void WagonWheel(uint16_t obj)
{
    wr8(obj + 0x20, 0xA0);
    wagon_x(obj);
    uint32_t vy = rd32(obj + 0x1C) + 0x1000;
    wr32(obj + 0x1C, vy);
    if (!(vy & 0x80000000u) && (int16_t)(rd16(obj + O_Y) - 0x118) >= 0) {
        wr32(obj + 0x1C, 0xFFFF8000);
        wr16(obj + O_Y, 0x118);
        wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) ^ 1));
    }
    if (!rd16(obj + 0x4E)) {
        wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) | 8));
        wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    } else {
        wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) & ~8));
        wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) + 1));
    }
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + rd16(obj + 0x50)));
    wr32(obj + O_Y, rd32(obj + O_Y) + rd32(obj + 0x1C));
}

/* $034B9A: the planes sped up to 2 until $1760: then stopped, the palette's fade reset, the three
 * masts ($DE80, $DEE0, $DF40) gone */
void WagonArrive(uint16_t obj)
{
    if ((int32_t)(rd32(v_planeSpeeds) - 0x20000) < 0) wr32(v_planeSpeeds, rd32(v_planeSpeeds) + 0x2000);
    if ((int16_t)(rd16(v_planes) - 0x1760) >= 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(v_planeSpeeds, 0);
        wr16(0x8014, 0);
        wr16(0x8016, 0);
        wr16(0x8018, 0);
        wr16(0x801A, 0);
        wr16(0xDE82, 0x1000);
        wr16(0xDEE2, 0x1000);
        wr16(0xDF42, 0x1000);
    }
    wagon_cinders(obj);
    wagon_x(obj);
}

/* $034BF6: the palette brightened a step each 8 frames (a blast each) to 5: then the planes on,
 * a new wagon's parts ($33A1C) from $D100 */
void WagonFlashUp(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 7)) {
        QueueSound(0xC8);
        for (uint16_t a = 0x8014; a <= 0x801A; a += 2) wr16(a, (uint16_t)(rd16(a) + 1));
    }
    wr16(0x8002, 0xF000);
    PalettesFull();
    if (rd16(0x8014) != 5) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(0xA924, (uint16_t)(rd16(0xA924) + 0x200));
    wr32(v_planeSpeeds, 0xFFFC0000);
    WagonGroup(0x033A1C, 0xD100, 0, 0x120);
}

/* $034C5A: back down to 0: then the boss's sound, its part ($218 in $DFA0) */
void WagonFlashDown(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 7)) {
        QueueSound(0xC8);
        for (uint16_t a = 0x8014; a <= 0x801A; a += 2) wr16(a, (uint16_t)(rd16(a) - 1));
    }
    wr16(0x8002, 0xF000);
    PalettesFull();
    if (rd16(0x8014)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x4E, 0x100);
    QueueSound(0xDA);
    wr16(0xDFA0 + O_TYPE, 0x218);
    wr16(0xDFA0 + O_STATE, 0);
}

/* $034CC2 */
void WagonWait(uint16_t obj)
{
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x4E, 0x80);
        wr8(obj + 0x21, 0x20);
    }
    WagonDrive(obj, 0x15A0, 0x159E);
    wagon_x(obj);
}

/* $034CEC */
void WagonPace(uint16_t obj)
{
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) < 0) {
        wr32(obj + 0x08, 0x0D013C);
        wr16(obj + 0x0C, 0);
        wr16(obj + 0x4E, 0x100);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    }
    WagonDrive(obj, 0x1540, 0x1548);
    wagon_x(obj);
}

/* $034D1E: shelling for $100 (the look again each $20), then pacing $80 */
void WagonShelling(uint16_t obj)
{
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) < 0) {
        wr32(obj + 0x08, 0x0CFF98);
        wr16(obj + 0x0C, 0);
        wr16(obj + 0x4E, 0x80);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
    } else if (!(rd16(obj + 0x4E) & 0x1F)) {
        wr32(obj + 0x08, 0x0D013C);
        wr16(obj + 0x0C, 0);
    }
    wagon_shell(obj);
    WagonDrive(obj, 0x1560, 0x1568);
    wagon_x(obj);
}

/* $034AF6 */
void WagonRide(uint16_t obj)
{
    wr8(obj + 0x20, rd8(obj + 0x25));
    wr16(obj + O_Y, rd16(obj + 0x4A));
    wagon_x(obj);
}

/* $034B06: cargo: its look by its vitality */
void WagonCargoStart(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xC000);
    wr8(obj + 0x20, 0x84);
    wr16(obj + O_Y, rd16(obj + 0x4A));
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(obj + 0x08, rom32(0x034B2E + (uint32_t)(int16_t)rd16(obj + O_VITALITY)));
}

/* $034B42: rattling a line every other frame */
void WagonCargo(uint16_t obj)
{
    wr16(obj + O_Y, (uint16_t)((((int16_t)rd16(v_gameFrames) >> 1) & 1) + rd16(obj + 0x4A)));
    wagon_x(obj);
}

/* $034D72: the boss beaten: sinking (its sound) */
void WagonBeaten(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr8(obj + 0x21, 0);
    wr16(obj + O_VITALITY, 0);
    wr16(obj + O_FLAGS, 0xE400);
    wr32(obj + 0x08, 0x0D00CC);
    wr32(obj + 0x1C, 0xFFFBC000);
    wr16(obj + 0x0E, (uint16_t)(rd16(obj + 0x0E) | 0x8000));
    QueueSound(0xDF);
    WagonDrive(obj, 0x1560, 0x1568);
    wagon_x(obj);
}

/* $034DAC */
void WagonSinking(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C) + 0x2000;
    wr32(obj + 0x1C, v);
    if (!v) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(obj + 0x08, 0x0D0314);
    }
    wagon_wreck(obj);
    WagonDrive(obj, 0x1560, 0x1568);
    wagon_x(obj);
}

/* $034DCC: the planes on to $17C0: then gone, the stage's next part */
void WagonSunk(uint16_t obj)
{
    wr32(v_planeSpeeds, rd32(v_planeSpeeds) + 0x400);
    if ((int16_t)(rd16(v_planes) - 0x17C0) >= 0) {
        wr16(obj + O_FLAGS, 0x1000);
        wr16(0xDFA2, 0x1000);
        WagonNext();
    }
    wagon_wreck(obj);
    wagon_x(obj);
}

/* $034DF8: a thrower up to $118, $10 frames; then winding up */
void WagonThrowerUp(uint16_t obj)
{
    if (rd16(obj + O_Y) != 0x118) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 1));
    wagon_x(obj);
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 2));
    if ((int16_t)rd16(obj + 0x50) >= 0) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0xE000);
    wr32(obj + 0x08, 0x0DF6D4);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x50, 0x20);
}

/* $034E92: after the throw, down again for $20-$5F */
void WagonThrown(uint16_t obj)
{
    wagon_x(obj);
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if ((int16_t)rd16(obj + 0x50) >= 0) return;
    wr8(obj + 0x21, 0);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 0x0C));
    wr16(obj + O_FLAGS, 0xE000);
    wr32(obj + 0x08, 0x0DFC8E);
    wr16(obj + 0x0C, 0);
    Random();
    wr16(obj + 0x4E, (uint16_t)((rd16(v_randomSeed) & 0x3F) + 0x20));
}

/* $034E32: winding up $20 frames, at $C the bomb ($1F4) thrown (its y on the planes nonzero: the
 * slot left as found, and the throw's end taken at once) */
void WagonThrow(uint16_t obj)
{
    wagon_x(obj);
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if ((int16_t)rd16(obj + 0x50) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x50, 0x20);
        return;
    }
    if (rd16(obj + 0x50) != 0x0C) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) - 8));
    if (rd16(obj + 0x4A)) { WagonThrown(obj); return; }
    wr16(slot + O_TYPE, 0x1F4);
    wr16(slot + 0x48, 0x60);
    wr16(slot + 0x4A, 0x80);
    wr16(slot + 0x4E, 1);
}

/* $034814 */
void Wagon(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        if (rd16(obj + 0x4C) == 4) {
            wagon_scatter();
            wr16(obj + O_STATE, 0x40);
        } else {
            wr16(obj + O_TYPE, 0x1E0);
            wr16(obj + 0x4A, 0x1DC);
            wr16(obj + 0x48, 0x60);
            wr16(obj + O_STATE, 0);
            wr8(obj + 0x21, 0);
            return;
        }
    }
    call_object(rom32(0x03485C + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $1C8: the mine's director (in the boss's slot): the cart ride's waves of riders, jet
 * troopers ($1CC by the trooper tables, $1D0) between them, the planes driven ahead and up, the
 * cart boss ($1D4 in $A800), then the boss's job ---- */

/* $0338AE: as WagonDrive, the planes' y ($A924, its speed $A934) */
void WagonDriveY(uint16_t obj, uint16_t back, uint16_t ahead)
{
    if (!rd16(obj + 0x4A)) {
        if ((int16_t)(back - rd16(0xA924)) < 0) wr16(obj + 0x4A, 2);
        if ((int32_t)(rd32(0xA934) - 0x22000) < 0) wr32(0xA934, rd32(0xA934) + 0x1000);
        return;
    }
    if ((int16_t)(ahead - rd16(0xA924)) >= 0) wr16(obj + 0x4A, 0);
    if ((int32_t)(rd32(0xA934) - 0xFFFE8000u) < 0) return;
    wr32(0xA934, rd32(0xA934) - 0x800);
}

/* $033612: a jet trooper ($1CC) in `slot`, its flame ($10) $300 on, its kind from `kinds` */
static void director_trooper_at(uint16_t slot, uint32_t kinds)
{
    wr16(slot + O_TYPE, 0x1CC);
    uint16_t flame = (uint16_t)(slot + 0x300);
    wr16(flame + O_TYPE, 0x10);
    wr16(flame + O_FLAGS, 0xC000);
    wr32(flame + 0x08, 0x0E166E);
    wr16(flame + 0x0E, 0x86C8);
    wr16(flame + 0x0C, 0);
    wr16(slot + 0x5A, flame);
    wr16(slot + 0x06, rom16(kinds + (rd16(v_randomSeed) & 6)));
}

/* $033604 */
static void director_trooper(uint32_t kinds)
{
    uint16_t slot = 0xCC20, t;
    if (!SlotFreeBelowCE(&slot, &t)) return;
    director_trooper_at(slot, kinds);
}

/* $0335F8: one each $20 frames */
static void director_troopers(void)
{
    if (rd16(v_gameFrames) & 0x1F) return;
    director_trooper(0x03364E);
}

/* $033666: a type $1D0 each $40 frames */
static void director_jets(void)
{
    if (rd16(v_gameFrames) & 0x3F) return;
    uint16_t slot = 0xCC80, t;
    if (!SlotFreeBelowCE(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1D0);
    uint16_t flame = (uint16_t)(slot + 0x300);
    wr16(flame + O_TYPE, 0x10);
    wr16(flame + O_FLAGS, 0xC000);
    wr32(flame + 0x08, 0x0E166E);
    wr8(flame + 0x20, 0xAC);
    wr16(flame + 0x0E, 0x86C8);
    wr16(flame + 0x0C, 0);
    wr16(slot + 0x5A, flame);
}

/* $033A6E: the wave ($DE80-$DFDF) all gone: the next */
static void director_wave_done(uint16_t obj)
{
    for (uint16_t a = 0xDE80, k = 0; k < 4; k++, a = (uint16_t)(a + 0x60))
        if (rd16(a)) return;
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) + 4));
}

static void director_wave(uint16_t obj, uint32_t list, uint16_t x, uint16_t y)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) + 4));
    WagonGroup(list, 0xDE80, x, y);
}

static void director_next(uint16_t obj, uint16_t count)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x48, count);
}

static int director_count(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    return (int16_t)rd16(obj + 0x48) < 0;
}

/* $0336CA */
void Stage1DirectorWave1(uint16_t obj)
{
    director_wave(obj, 0x033950, 0, 0x120);
}

/* $0336DE */
void Stage1DirectorRide1(uint16_t obj)
{
    WagonDrive(obj, 0x7A0, 0x720);
    director_wave_done(obj);
}

/* $0336EE: the planes slowed back to $640: the ride stopped, its next stretch */
void Stage1DirectorStop1(uint16_t obj)
{
    wr8(0x8080, (uint8_t)(rd8(0x8080) | 2));
    if ((int32_t)(rd32(v_planeSpeeds) - 0xFFFC0000u) >= 0) wr32(v_planeSpeeds, rd32(v_planeSpeeds) - 0x800);
    if ((int16_t)(rd16(v_planes) - 0x640) >= 0) return;
    director_next(obj, 0x100);
    wr32(v_planeSpeeds, 0);
    wr16(0xA980, 2);
    wr16(0xC9E0, 0x228);
    wr16(0xC9E6, 2);
}

/* $03374E */
void Stage1DirectorWave2(uint16_t obj)
{
    director_wave(obj, 0x033970, 0xF0, 0);
}

/* $033762 */
void Stage1DirectorRide2(uint16_t obj)
{
    WagonDriveY(obj, 0x380, 0x370);
    director_wave_done(obj);
}

/* $033772 */
void Stage1DirectorStop2(uint16_t obj)
{
    wr8(0x8080, (uint8_t)(rd8(0x8080) | 2));
    if ((int32_t)(rd32(0xA934) - 0xFFFC0000u) >= 0) wr32(0xA934, rd32(0xA934) - 0x800);
    if ((int16_t)(rd16(0xA924) - 0x200) >= 0) return;
    director_next(obj, 0x80);
    wr32(0xA934, 0);
    wr16(0xA980, 2);
}

/* $0337E4 */
void Stage1DirectorRide3(uint16_t obj)
{
    WagonDrive(obj, 0x11A0, 0x1180);
    director_wave_done(obj);
}

/* $0337D6 */
void Stage1DirectorWave3(uint16_t obj)
{
    director_wave(obj, 0x033990, 0, 0x120);
    Stage1DirectorRide3(obj);
}

/* $033802 */
void Stage1DirectorRide4(uint16_t obj)
{
    WagonDrive(obj, 0x12A0, 0x1280);
    director_wave_done(obj);
}

/* $0337F4 */
void Stage1DirectorWave4(uint16_t obj)
{
    director_wave(obj, 0x0339B0, 0, 0x120);
    Stage1DirectorRide4(obj);
}

/* $033820 */
void Stage1DirectorRide5(uint16_t obj)
{
    WagonDrive(obj, 0x13A0, 0x1380);
    director_wave_done(obj);
}

/* $033812 */
void Stage1DirectorWave5(uint16_t obj)
{
    director_wave(obj, 0x0339D0, 0, 0x120);
    Stage1DirectorRide5(obj);
}

/* $033830: the cart boss ($A800) and its riders */
void Stage1DirectorBoss(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr8(0x8080, (uint8_t)(rd8(0x8080) | 2));
    wr16(0xA800 + 0xDA, 0x20);
    WagonGroup(0x0339F0, 0xA800, 0, 0x120);
    WagonGroup(0x0339FC, 0xDE80, 0, 0x120);
}

/* $03330A: the camera stopped: on */
void Stage1DirectorStart(uint16_t obj)
{
    if (bit8(v_cameraHold, 7)) director_next(obj, 0x40);
}

/* $033320 */
void Stage1DirectorScroll(uint16_t obj)
{
    if (!director_count(obj)) return;
    director_next(obj, 0x100);
    wr16(0xA980, 2);
}

/* $03333A */
void Stage1DirectorBegin(uint16_t obj)
{
    if (!director_count(obj)) return;
    director_next(obj, 0x600);
    wr16(0x8080, 0);
    wr16(0x808A, 0x8000);
}

/* $03335A: troopers for $600 frames: at $300 a $228 ($C9E0), at $100 the next part */
void Stage1DirectorTroopers(uint16_t obj)
{
    if (rd16(obj + 0x48) == 0x300) { wr16(0xC9E0, 0x228); wr16(0xC9E6, 0); }
    if (rd16(obj + 0x48) == 0x100) Spawn1F0();
    if (director_count(obj)) {
        director_next(obj, 0x40);
        wr16(0xA980, 2);
    }
    director_troopers();
}

/* $0333A0 */
void Stage1DirectorCarts(uint16_t obj)
{
    if (director_count(obj)) {
        Spawn1F0();
        QueueSound(0xDA);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4C, 0);
    }
    director_troopers();
}

/* $0333D0: the first ride; from difficulty 4 a trooper each $40 frames as well */
void Stage1DirectorRide(uint16_t obj)
{
    call_object(rom32(0x0336BE + (uint32_t)(int16_t)rd16(obj + 0x4C)), obj);
    if ((int16_t)(rd16(0xA20A) - 4) < 0) return;
    if (rd16(v_gameFrames) & 0x3F) return;
    uint16_t slot = 0xCCE0, t;
    if (!SlotFreeBelowCE(&slot, &t)) return;
    director_trooper_at(slot, 0x03364E);
}

/* $033404: the stage's $8080 bit 0 (the jet troopers downed) after $40 */
void Stage1DirectorClear(uint16_t obj)
{
    if (!director_count(obj)) return;
    director_next(obj, 0x40);
    wr8(0x8080, (uint8_t)(rd8(0x8080) | 1));
}

/* $03341E */
void Stage1DirectorCleared(uint16_t obj)
{
    if (!director_count(obj)) return;
    director_next(obj, 0x80);
    wr16(0x8080, 0);
    wr16(0xA980, 2);
}

/* $03343E */
void Stage1DirectorPause(uint16_t obj)
{
    if (director_count(obj)) director_next(obj, 0x600);
}

/* $033452: jets for $600 frames, a $22C ($D280) at $400, $300 and $200 */
void Stage1DirectorJets(uint16_t obj)
{
    uint16_t c = rd16(obj + 0x48);
    if (c == 0x400 || c == 0x300 || c == 0x200) wr16(0xD280, 0x22C);
    if (director_count(obj)) {
        QueueSound(0xDA);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(0xA980, 2);
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4C, 0);
    }
    director_jets();
}

/* $0334A2: the second ride */
void Stage1DirectorClimb(uint16_t obj)
{
    call_object(rom32(0x033742 + (uint32_t)(int16_t)rd16(obj + 0x4C)), obj);
    director_jets();
}

/* $0334AA: troopers again: a $228 at $200, the stage's next part at $500 and $80 */
void Stage1DirectorTroopers2(uint16_t obj)
{
    uint16_t c = rd16(obj + 0x48);
    if (c == 0x200) { wr16(0xC9E0, 0x228); wr16(0xC9E6, 0); }
    if (c == 0x500) WagonNext();
    if (c == 0x80) Spawn1F0();
    if (director_count(obj)) {
        QueueSound(0xDA);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(0xA980, 2);
        wr16(obj + 0x48, 0x40);
        wr16(obj + 0x4C, 0);
    }
    director_troopers();
}

/* $03350E: the last rides, troopers as often as the difficulty says */
void Stage1DirectorRides(uint16_t obj)
{
    call_object(rom32(0x0337BA + (uint32_t)(int16_t)rd16(obj + 0x4C)), obj);
    if (rd16(v_gameFrames) & rom16(0x033656 + (uint32_t)(int16_t)rd16(0xA20C))) return;
    director_trooper(0x03364E);
}

/* $033532: the cart boss gone */
void Stage1DirectorBossWait(uint16_t obj)
{
    if (rd16(0xA800) == 0x1D4) return;
    director_next(obj, 0x40);
    wr32(v_planeSpeeds, 0);
    wr16(0xA980, 2);
}

/* $033556: then the stage boss ($24 here) and its graphics' job */
void Stage1DirectorEnd(uint16_t obj)
{
    if (!director_count(obj)) return;
    wr16(0x80C2, 0x34);
    wr32(0x80C8, 0x039950);
    wr16(obj + O_TYPE, 0x24);
    wr16(obj + O_STATE, 0);
    for (uint16_t o = 0x48; o <= 0x5E; o += 2) wr16(obj + o, 0);
    wr16(obj + O_VITALITY, 0);
    uint32_t list = 0x0335BC;
    NextJob(&list);
}

/* $0332B8 */
void Stage1Director(uint16_t obj)
{
    call_object(rom32(0x0332C2 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $1CC: a jet trooper of the mine's shafts (its flame the part at +$5A), hovering
 * across at the top or bottom, crossing to the other, firing at its player ($1A0 shots), now and
 * then a bomb ($A8) carried over; the stage's $8080 bit 0 downs it, bit 1 sends it away; shot
 * down: 1,600 points ---- */

/* $033E26: hovering (state 4) for $20-$5F frames */
static void wall_hover(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + 0x48, (uint16_t)((rd16(v_randomSeed) & 0x3F) + 0x20));
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0E0334);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x58, 0);
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x28);
    wr8(obj + 0x5E, (uint8_t)(rd8(obj + 0x5E) & ~1));
    wr8(obj + 0x5E, (uint8_t)(rd8(obj + 0x5E) & ~0x10));
}

/* $034286: the drift across eased by $800 */
static void wall_ease(uint16_t obj)
{
    if (!bit8(obj + 0x18, 7)) wr32(obj + 0x18, rd32(obj + 0x18) - 0x800);
    else wr32(obj + 0x18, rd32(obj + 0x18) + 0x800);
}

/* $0341C4: the flame at its side (`y` its line), turned as it is */
static void wall_flame(uint16_t obj, uint16_t y)
{
    uint16_t flame = (uint16_t)reg_a6;
    wr16(flame + O_Y, y);
    uint16_t dx = bit8(obj + 0x0E, 3) ? 0xFFF8 : 8;
    wr16(flame + O_X, (uint16_t)(rd16(obj + O_X) + dx));
    wr16(flame + 0x0E, (uint16_t)((rd16(flame + 0x0E) & 0x7FF) | (rd16(obj + 0x0E) & 0x9800)));
    wr32(flame + 0x08, 0x0E166E);
}

/* $0347D6: every 4th frame a wreck's spark ($1DC) on it */
static void wall_sparks(uint16_t obj)
{
    if (rd16(v_gameFrames) & 3) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1DC);
    wr32(slot + 0x48, 0x05639C);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* $034070: downed: mostly its flame to smoke ($1D8) and a fall; else a blast, the flame gone */
static void wall_downed(uint16_t obj)
{
    uint16_t flame = (uint16_t)reg_a6;
    wr8(obj + 0x21, 0);
    wr16(obj + O_VITALITY, 0);
    wr16(obj + O_FLAGS, 0xCE00);
    wr32(obj + 0x08, 0x0DFFD6);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) & ~8));
    wr16(obj + 0x18, 2);
    wr16(obj + 0x58, 0);
    int low = (int16_t)(rd16(obj + O_Y) - 0x100) >= 0;
    if (rd16(v_randomSeed) & 3) {
        wr16(flame + O_TYPE, 0x1D8);
        wr16(obj + O_STATE, low ? 0x1C : 0x20);
        wr32(obj + 0x1C, low ? 0xFFFA0000 : 0x60000);
        wr16(flame + 0x48, low ? 0x120 : 0xE0);
        return;
    }
    QueueSound(0xC6);
    wr16(flame + O_FLAGS, 0x1000);
    wr16(obj + O_STATE, low ? 0x28 : 0x2C);
    wr32(obj + 0x1C, low ? 0xFFFA0000 : 0x60000);
}

/* $033E72: crossing to the other side (state 8 up, $C down) */
static void wall_cross(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0E0334);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x28);
    wr8(obj + 0x5E, (uint8_t)(rd8(obj + 0x5E) | 0x10));
    wr16(obj + O_STATE, (int16_t)(rd16(obj + O_Y) - 0x100) >= 0 ? 8 : 0x0C);
}

/* $033EF8: aiming (state $14) for a second */
static void wall_aim(uint16_t obj)
{
    wr16(obj + 0x48, 0x3C);
    wr16(obj + O_STATE, 0x14);
    wr16(obj + O_FLAGS, 0xC800);
    wr32(obj + 0x08, 0x0E0340);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x5E, (uint8_t)(rd8(obj + 0x5E) & ~0x10));
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x28);
}

/* $033FBC: a bomb ($A8, held at +$58) carried across (states $38/$3C) */
static void wall_bomb(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xEC00);
    wr32(obj + 0x08, 0x0DFE3E);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x5E, (uint8_t)(rd8(obj + 0x5E) | 0x10));
    wr32(obj + 0x28, 0xF410F808);
    wr8(obj + 0x21, 0x28);
    if (!bit8(obj + 0x18, 7)) wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) & ~8));
    else wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) | 8));
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + 0x48, 0x40);
    wr16(slot + 0x58, obj);
    wr16(obj + 0x58, slot);
    wr16(slot + O_TYPE, 0xA8);
    wr8(slot + 0x21, 0x10);
    wr32(slot + 0x2C, 0xFC03F808);
    wr16(slot + 0x26, 4);
    wr16(slot + 0x4A, 0);
    wr16(slot + 0x4C, 0);
    if ((int16_t)(rd16(obj + O_Y) - 0x100) >= 0) {
        wr16(obj + O_STATE, 0x38);
        wr32(obj + 0x1C, 0xFFFA0000);
    } else {
        wr16(obj + O_STATE, 0x3C);
        wr32(obj + 0x1C, 0x60000);
    }
}

/* $0341FE: drifting across: every $20 frames a coin picks left or right; at most 2 a frame, kept
 * within $80-$1C0 */
static void wall_drift(uint16_t obj)
{
    if (!(rd16(obj + 0x50) & 0x1F)) wr16(obj + 0x52, (uint16_t)(rd16(v_randomSeed) & 1));
    uint32_t vx = rd32(obj + 0x18);
    if (rd16(obj + 0x52)) {
        if ((int16_t)(rd16(obj + O_X) - 0x80) < 0) { wr32(obj + 0x18, 0x40000); return; }
        if ((vx & 0x80000000u) && (int32_t)(vx - 0xFFFE0000u) < 0) return;
        wr32(obj + 0x18, vx - 0x2000);
    } else {
        if ((int16_t)(rd16(obj + O_X) - 0x1C0) >= 0) { wr32(obj + 0x18, 0xFFFC0000); return; }
        if (!(vx & 0x80000000u) && (int32_t)(vx - 0x20000) >= 0) return;
        wr32(obj + 0x18, vx + 0x2000);
    }
}

/* $034316: every 8th frame, its player within $100 across: by the distance and its kind
 * (+6), a chance to aim, to cross (its player level with it, or not), or to carry a bomb */
static void wall_decide(uint16_t obj)
{
    if (rd16(obj + 0x50) & 7) return;
    uint16_t dx, dy, p;
    uint16_t d = (uint16_t)(NearestPlayer(obj, &dx, &dy, &p) & 0x7FE0);
    if ((int16_t)(d - 0x100) >= 0) return;
    d = (uint16_t)((((int16_t)d >> 4) + rd16(obj + 0x06)) & 0xFFFE);
    switch (rom16(0x03435A + (uint32_t)(int16_t)d)) {
    case 0x02:
        if (!ChanceRoll(obj, 0x10)) wall_aim(obj);
        break;
    case 0x12:
        if (ChanceRoll(obj, 0x80)) break;
        if ((int16_t)dy < 0 ? (int16_t)(dy - 0xFFC0) >= 0 : (int16_t)(dy - 0x40) < 0) wall_cross(obj);
        break;
    case 0x3A:
        if (ChanceRoll(obj, 0x80)) break;
        if ((int16_t)dy < 0 ? (int16_t)(dy - 0xFFC0) < 0 : (int16_t)(dy - 0x40) >= 0) wall_cross(obj);
        break;
    case 0x62:
        if (!ChanceRoll(obj, 0x66)) wall_bomb(obj);
        break;
    }
}

/* $033B60: at a corner by chance, its vitality $10-$1F, its own beat (+$54) */
void WallTrooperStart(uint16_t obj)
{
    wr16(obj + 0x0E, 0x8300);
    uint16_t r = rd16(v_randomSeed);
    wr16(obj + 0x50, r);
    wr16(obj + 0x56, (uint16_t)(r + rd16(v_gameFrames)));
    wr16(obj + O_VITALITY, (uint16_t)((rd8(v_randomSeed) & 0x0F) + 0x10));
    wr16(obj + 0x54, (uint16_t)(rd8(0xFF0A) & 0x0F));
    r = rd16(v_randomSeed);
    wr16(obj + O_X, rom16(0x033BC0 + (r & 2)));
    wr16(obj + O_Y, rom16(0x033BC4 + ((uint16_t)((int16_t)r >> 4) & 2)));
    if ((int16_t)(rd16(obj + O_Y) - 0x100) < 0) wr16(obj + 0x0E, (uint16_t)(rd16(obj + 0x0E) | 0x1000));
    else wr16(obj + 0x0E, (uint16_t)(rd16(obj + 0x0E) & 0xEFFF));
    wall_hover(obj);
}

/* $033BCA */
void WallTrooperHover(uint16_t obj)
{
    if (bit8(0x8080, 1)) {
        wr16(obj + O_STATE, 0x24);
        wr16(obj + O_FLAGS, 0xE800);
        wr32(obj + 0x08, 0x0E0334);
        wr16(obj + 0x0C, 0);
        wr16(obj + 0x58, 0);
        wr8(obj + 0x5E, (uint8_t)(rd8(obj + 0x5E) | 0x10));
        wr32(obj + 0x28, 0xF410F808);
        wr8(obj + 0x21, 0x28);
        PartPlace(obj, 0);
        return;
    }
    wall_drift(obj);
    wall_decide(obj);
    PartPlace(obj, 0);
}

/* $033BF0 */
void WallTrooperUp(uint16_t obj)
{
    wall_ease(obj);
    if ((int16_t)(rd16(obj + O_Y) - 0x100) < 0) wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) | 0x10));
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x4000);
    if ((int16_t)(rd16(obj + O_Y) - 0xB9) < 0) {
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, 0xB1);
        jet_pause(obj);
    }
    PartPlace(obj, 4);
}

/* $033C30 */
void WallTrooperDown(uint16_t obj)
{
    wall_ease(obj);
    if ((int16_t)(rd16(obj + O_Y) - 0x100) >= 0) wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) & ~0x10));
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if ((int16_t)(rd16(obj + O_Y) - 0x137) >= 0) {
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, 0x13F);
        jet_pause(obj);
    }
    PartPlace(obj, 4);
}

/* $033C70 */
void WallTrooperPause(uint16_t obj)
{
    wall_ease(obj);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) wall_hover(obj);
    PartPlace(obj, 2);
}

/* $033C88 */
void WallTrooperAim(uint16_t obj)
{
    wall_ease(obj);
    uint16_t target = PlayerBySlot(obj);
    uint16_t d0, d5, d6, d7 = 0x1A0;
    AimFacing(obj, target, 0x02ADE6, &d0, &d5, &d6, &d7);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        wr16(obj + 0x48, 0x10);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        AimedSpawnHigh(obj, d5, d6, d7, 0x02AE06);
    }
    PartPlace(obj, 0);
}

/* $033CC8 */
void WallTrooperFired(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) wall_hover(obj);
    PartPlace(obj, 0);
}

/* $033CE0: falling from the top */
void WallTrooperFallUp(uint16_t obj)
{
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x8000);
    if ((int16_t)(rd16(obj + O_Y) - 0x100) < 0) wr32(obj + 0x08, 0x0DFFFA);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2000);
}

/* $033D08: falling from the bottom (up) */
void WallTrooperFallDown(uint16_t obj)
{
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x8000);
    if ((int16_t)(rd16(obj + O_Y) - 0x100) >= 0) wr32(obj + 0x08, 0x0DFFFA);
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x2000);
}

/* $033CDC */
void WallTrooperBurnUp(uint16_t obj)
{
    wall_sparks(obj);
    WallTrooperFallUp(obj);
}

/* $033D04 */
void WallTrooperBurnDown(uint16_t obj)
{
    wall_sparks(obj);
    WallTrooperFallDown(obj);
}

/* $033D2C: away to the left; gone past $80 */
void WallTrooperLeave(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x80) < 0) {
        wr16(obj + O_FLAGS, 0x1000);
        wr16((uint16_t)reg_a6 + O_FLAGS, 0x1000);
        return;
    }
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x2000);
    PartPlace(obj, 0);
}

/* $033D54: struck at the bottom: knocked up, back down to hover */
void WallTrooperKnockedUp(uint16_t obj)
{
    wall_ease(obj);
    uint32_t v = rd32(obj + 0x1C) + 0x4000;
    wr32(obj + 0x1C, v);
    if (!(v & 0x80000000u) && (int16_t)(rd16(obj + O_Y) - 0x13D) >= 0) {
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, 0x13F);
        wall_hover(obj);
    }
    wall_flame(obj, 0x148);
}

/* $033D84: struck at the top: knocked down, back up to hover */
void WallTrooperKnockedDown(uint16_t obj)
{
    wall_ease(obj);
    uint32_t v = rd32(obj + 0x1C) - 0x4000;
    wr32(obj + 0x1C, v);
    if ((v & 0x80000000u) && (int16_t)(rd16(obj + O_Y) - 0xB3) < 0) {
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, 0xB1);
        wall_hover(obj);
    }
    wall_flame(obj, 0xA8);
}

/* $033DB4: the bomb carried down */
void WallTrooperBombDown(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C) + 0x4000;
    wr32(obj + 0x1C, v);
    if (!(v & 0x80000000u) && (int16_t)(rd16(obj + O_Y) - 0x13D) >= 0) {
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, 0x13F);
        jet_pause(obj);
    }
    wall_flame(obj, 0x148);
}

/* $033DE0: the bomb carried up */
void WallTrooperBombUp(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C) - 0x4000;
    wr32(obj + 0x1C, v);
    if ((v & 0x80000000u) && (int16_t)(rd16(obj + O_Y) - 0xB3) < 0) {
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, 0xB1);
        jet_pause(obj);
    }
    wall_flame(obj, 0xA8);
}

/* $033ABA: its flame's and its own size beating; struck (unguarded): knocked off its side */
void WallTrooper(uint16_t obj)
{
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) + 1));
    reg_a6 = (uint32_t)(int32_t)(int16_t)rd16(obj + 0x5A);
    uint16_t s = (uint16_t)(((rd16(obj + 0x50) << 2) & 0x0C) + 0xA0);
    wr8(obj + 0x20, (uint8_t)s);
    wr8((uint16_t)reg_a6 + 0x20, (uint8_t)(s - 4));
    int struck = 1;
    if (!bit8(obj + 0x5E, 0)) {
        if (bit8(0x8080, 0)) { wall_downed(obj); return; }
        if (rd16(obj + O_VITALITY) & 0x8000) {
            AddScore(obj, 0x160);
            wall_downed(obj);
            struck = 0;
        }
    }
    if (struck && !bit8(obj + 0x5E, 4) && bit8(obj + 0x22, 5)) {
        wr16(obj + O_FLAGS, 0xEC00);
        wr32(obj + 0x08, 0x0DFF4E);
        wr16(obj + 0x0C, 0);
        wr16(obj + 0x58, 0);
        wr8(obj + 0x5E, (uint8_t)(rd8(obj + 0x5E) | 0x10));
        wr32(obj + 0x28, 0xF410F808);
        wr8(obj + 0x21, 0x28);
        if ((int16_t)(rd16(obj + O_Y) - 0x100) >= 0) {
            wr16(obj + O_STATE, 0x30);
            wr32(obj + 0x1C, 0xFFFE0000);
        } else {
            wr16(obj + O_STATE, 0x34);
            wr32(obj + 0x1C, 0x20000);
        }
    }
    call_object(rom32(0x033B20 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $18: a walker of 17 balls ($C980 its body, its slot; the eight legs' angles at +$56
 * of $CCE0-$CF80; the skeleton's offsets at +$48/+$4A of each ball), from the sine table built
 * at $9400 of a size by the difficulty; walking, turning, swaying, hopping at its players;
 * beaten: 120,460 points, the balls flung apart ---- */

#define WALK_SIN 0x9400
#define WALK_COS 0x9480

static uint16_t asrw(uint16_t v, int n) { return (uint16_t)((int16_t)v >> n); }

/* $03658A: a leg's offset from its angle (`a`), as the body leans */
static void walker_leg_vec(uint16_t obj, uint16_t a, uint16_t *d3, uint16_t *d4)
{
    uint16_t s = asrw(rd16((uint16_t)(WALK_SIN + a)), 8);
    *d4 = rd16((uint16_t)(WALK_COS + a));
    uint16_t lean = (uint16_t)(((((rd16(obj + 0x54) + rd16(obj + 0x52)) & 0x1FE) + 0x80) & 0x1FE));
    *d3 = (uint16_t)(((int32_t)(int16_t)s * (int16_t)rom16(0x02977C + lean)) >> 6);
}

struct walker_regs { uint16_t d0, d1, d2, d7, a1, a2, a3, a4; };

/* $036462 */
static void walker_hip(uint16_t obj, struct walker_regs *r)
{
    uint16_t a = rd16(obj + 0x56);
    r->a1 = rd16((uint16_t)(WALK_SIN + a));
    r->a2 = rd16((uint16_t)(WALK_COS + a));
    r->d1 = (uint16_t)~r->a1;
    r->d2 = (uint16_t)~r->a2;
    r->d0 = a;
}

/* $0364CE */
static void walker_spine(uint16_t obj, struct walker_regs *r)
{
    uint16_t d0 = (uint16_t)((rd16(obj + 0x54) + rd16(obj + 0x52)) & 0x1FE);
    uint16_t d7 = asrw(rd16((uint16_t)(WALK_SIN + d0)), 8);
    d0 = (uint16_t)((rd16(obj + 0x56) + 0x80) & 0x1FE);
    uint16_t d3 = (uint16_t)(((int32_t)(int16_t)rom16(0x02977C + d0) * (int16_t)d7) >> 6);
    uint16_t d4 = (uint16_t)(((int32_t)(int16_t)rom16(0x0297FC + d0) * (int16_t)d7) >> 6);
    r->d0 = d3;
    r->d7 = d4;
    r->a3 = (uint16_t)~d3;
    r->a4 = (uint16_t)~d4;
}

static void walker_put(uint16_t ball, uint16_t x, uint16_t y)
{
    wr16(ball + 0x48, asrw(x, 8));
    wr16(ball + 0x4A, asrw(y, 8));
}

/* a leg's ball: its offset added to the chain (*x, *y), placed */
static void walker_link(uint16_t ball, uint16_t *x, uint16_t *y)
{
    *x = (uint16_t)(rd16(ball + 0x52) + *x);
    *y = (uint16_t)(rd16(ball + 0x54) + *y);
    walker_put(ball, *x, *y);
}

/* $03623C: the balls' offsets down the chains of the skeleton */
static void walker_bones(uint16_t obj, const struct walker_regs *r)
{
    uint16_t x, y;
    walker_put(0xCB00, r->a1, r->a2);
    x = (uint16_t)(asrw(r->a1, 1) + r->a1);
    y = (uint16_t)(asrw(r->a2, 1) + r->a2);
    wr16(obj + 0x4C, x);
    wr16(obj + 0x4E, y);
    x = (uint16_t)(r->a3 + x); y = (uint16_t)(r->a4 + y);
    walker_put(0xCC20, x, y);
    walker_link(0xCE60, &x, &y);
    walker_link(0xCEC0, &x, &y);
    x = (uint16_t)(r->d0 + rd16(obj + 0x4C)); y = (uint16_t)(r->d7 + rd16(obj + 0x4E));
    walker_put(0xCC80, x, y);
    walker_link(0xCF20, &x, &y);
    walker_link(0xCF80, &x, &y);
    walker_put(0xC9E0, (uint16_t)(r->d1 + r->d1), (uint16_t)(r->d2 + r->d2));
    x = (uint16_t)(asrw(r->a3, 1) + r->d1); y = (uint16_t)(asrw(r->a4, 1) + r->d2);
    walker_put(0xCA40, x, y);
    x = (uint16_t)(x + asrw(r->d1, 1)); y = (uint16_t)(y + asrw(r->d2, 1));
    x = (uint16_t)(r->a3 + x); y = (uint16_t)(r->a4 + y);
    walker_put(0xCB60, x, y);
    walker_link(0xCCE0, &x, &y);
    walker_link(0xCD40, &x, &y);
    x = (uint16_t)(asrw(r->d0, 1) + r->d1); y = (uint16_t)(asrw(r->d7, 1) + r->d2);
    walker_put(0xCAA0, x, y);
    x = (uint16_t)(x + asrw(r->d1, 1)); y = (uint16_t)(y + asrw(r->d2, 1));
    x = (uint16_t)(r->d0 + x); y = (uint16_t)(r->d7 + y);
    walker_put(0xCBC0, x, y);
    walker_link(0xCDA0, &x, &y);
    walker_link(0xCE00, &x, &y);
}

/* $0361B4: the skeleton, the legs leaning with the body */
static void walker_pose(uint16_t obj)
{
    struct walker_regs r;
    for (uint16_t leg = 0xCCE0, k = 0; k < 8; k++, leg = (uint16_t)(leg + 0x60)) {
        uint16_t d3, d4;
        walker_leg_vec(obj, rd16(leg + 0x56), &d3, &d4);
        wr16(leg + 0x52, d3);
        wr16(leg + 0x54, d4);
    }
    walker_hip(obj, &r);
    wr16(0x8040, rd16(obj + 0x56));
    wr16(obj + 0x56, 0);
    walker_spine(obj, &r);
    wr16(obj + 0x56, rd16(0x8040));
    walker_bones(obj, &r);
}

/* $0361DA: the skeleton upright, the legs straight */
static void walker_pose_upright(uint16_t obj)
{
    struct walker_regs r;
    for (uint16_t leg = 0xCCE0, k = 0; k < 8; k++, leg = (uint16_t)(leg + 0x60)) {
        uint16_t a = rd16(leg + 0x56);
        wr16(leg + 0x52, rd16((uint16_t)(WALK_SIN + a)));
        wr16(leg + 0x54, rd16((uint16_t)(WALK_COS + a)));
    }
    uint16_t a = rd16(obj + 0x56);
    r.a1 = rd16((uint16_t)(WALK_SIN + a));
    r.a2 = rd16((uint16_t)(WALK_COS + a));
    r.d1 = (uint16_t)~r.a1;
    r.d2 = (uint16_t)~r.a2;
    a = (uint16_t)((a + 0x80) & 0x1FE);
    r.d0 = rd16((uint16_t)(WALK_SIN + a));
    r.d7 = rd16((uint16_t)(WALK_COS + a));
    r.a3 = (uint16_t)~r.d0;
    r.a4 = (uint16_t)~r.d7;
    walker_bones(obj, &r);
}

/* $0361EA: the skeleton rolled into a ball (every leg along its bone) */
static void walker_pose_ball(uint16_t obj)
{
    struct walker_regs r;
    walker_hip(obj, &r);
    walker_spine(obj, &r);
    static const uint16_t legs[8] = { 0xCCE0, 0xCD40, 0xCDA0, 0xCE00, 0xCE60, 0xCEC0, 0xCF20, 0xCF80 };
    for (int k = 0; k < 8; k++) {
        uint16_t x = k < 2 ? r.a3 : k < 4 ? r.d0 : r.a1;
        uint16_t y = k < 2 ? r.a4 : k < 4 ? r.d7 : r.a2;
        wr16(legs[k] + 0x52, x);
        wr16(legs[k] + 0x54, y);
    }
    walker_bones(obj, &r);
}

/* $036620: the other 16 balls at their offsets from the body (`skip` left where it is) */
static void walker_place(uint16_t obj, uint16_t skip)
{
    for (uint16_t b = 0xC9E0, k = 0; k < 16; k++, b = (uint16_t)(b + 0x60)) {
        if (b == skip) continue;
        wr16(b + O_X, (uint16_t)(rd16(b + 0x48) + rd16(obj + O_X)));
        wr16(b + O_Y, (uint16_t)(rd16(b + 0x4A) + rd16(obj + O_Y)));
    }
}

/* $036608: the body stood on the ball `foot` (on the floor, $145) */
static void walker_stand_on(uint16_t obj, uint16_t foot)
{
    wr16(obj + O_X, (uint16_t)(rd16(foot + O_X) - rd16(foot + 0x48)));
    wr16(obj + O_Y, (uint16_t)(0x145 - rd16(foot + 0x4A)));
    walker_place(obj, foot);
}

/* $0365FE */
static void walker_place_all(uint16_t obj) { walker_place(obj, 0); }

/* $0365DE: on the back foot */
static void walker_on_back(uint16_t obj)
{
    wr16(0xCF80 + O_Y, 0x145);
    walker_stand_on(obj, 0xCF80);
}

/* $0365D6: on the foot it stands on */
static void walker_on_foot(uint16_t obj)
{
    if (rd16(obj + 0x50)) { walker_on_back(obj); return; }
    wr16(0xCEC0 + O_Y, 0x145);
    walker_stand_on(obj, 0xCEC0);
}

/* $036A80: the balls' draw order as it faces */
static void walker_order(uint16_t obj)
{
    uint16_t a = (uint16_t)((rd16(obj + 0x54) + 0x80 + rd16(obj + 0x52)) & 0x1FE);
    uint32_t t = (int16_t)(a - 0x100) < 0 ? 0x036AC2 : 0x036AE4;
    for (uint8_t d = 0, k = 0; k < 17; k++, d = (uint8_t)(d + 4), t += 2) wr8(rom16(t) + 0x20, d);
}

/* $036B1A: the legs' angles stepped toward `targets` (`step`, `mask`): any moved; *last the last
 * leg's angle */
static int walker_legs_to(uint32_t targets, uint16_t step, uint16_t mask, uint16_t *last)
{
    int moved = 0;
    uint16_t d2 = 0;
    for (uint16_t a = 0xCD36, k = 0; k < 8; k++, a = (uint16_t)(a + 0x60), targets += 2) {
        uint16_t want = bus_rd16(targets);
        d2 = rd16(a);
        if (want == d2) continue;
        if ((int16_t)(((want - d2) & 0x1FE) - 0x100) < 0) d2 = (uint16_t)(d2 + step);
        else d2 = (uint16_t)(d2 - step);
        d2 &= mask;
        wr16(a, d2);
        moved = 1;
    }
    if (last) *last = d2;
    return moved;
}

static int walker_legs_slow(uint32_t targets, uint16_t *last) { return walker_legs_to(targets, 4, 0x1FC, last); }
static int walker_legs_fast(uint32_t targets) { return walker_legs_to(targets, 8, 0x1F8, 0); }

/* $036BC8: the balls' colours by the body's angle (each flashing on alternate frames when hit) */
static void walker_colours(uint16_t obj, uint32_t table)
{
    uint16_t i = (uint16_t)((((rd16(obj + 0x54) + rd16(obj + 0x52)) & 0x7E) / 12) & 0x0E);
    uint16_t c = rom16(table + i);
    for (uint16_t b = 0xC980, k = 0; k < 17; k++, b = (uint16_t)(b + 0x60)) {
        wr16(b + 0x0E, c);
        if (bit8(v_gameFrames + 1, 0) && bit8(b + 0x5D, 0)) wr16(b + 0x0E, (uint16_t)(rd16(b + 0x0E) & 0x9FFF));
    }
    wr16(0xC9EE, rom16(table + 0x0C + i));
}

/* $036C62: the balls flying (falling at $2000) */
static void walker_fly(uint16_t obj)
{
    wr16(obj + 0x54, (uint16_t)((rd16(obj + 0x54) + 4) & 0x1FE));
    for (uint16_t b = 0xC980, k = 0; k < 17; k++, b = (uint16_t)(b + 0x60)) {
        wr32(b + 0x1C, rd32(b + 0x1C) - 0x2000);
        wr32(b + O_X, rd32(b + O_X) - rd32(b + 0x18));
        wr32(b + O_Y, rd32(b + O_Y) - rd32(b + 0x1C));
    }
}

/* the three or four balls' hit flags (+1 bit 4, +3 bit 5) and their guard (+$3C) */
static void walker_guard(uint16_t ball, int on)
{
    if (on) {
        wr8(ball + 1, (uint8_t)(rd8(ball + 1) | 0x10));
        wr8(ball + 3, (uint8_t)(rd8(ball + 3) | 0x20));
        wr16(ball + 0x3C, 1);
    } else {
        wr8(ball + 1, (uint8_t)(rd8(ball + 1) & ~0x10));
        wr16(ball + 0x3C, 0);
    }
}

static void walker_unguard_feet(void)
{
    walker_guard(0xCC40, 0);
    walker_guard(0xCE80, 0);
    walker_guard(0xCEE0, 0);
}

/* $036162: rolling along its angle */
static void walker_roll(uint16_t obj)
{
    uint32_t t = 0x02977C + ((rd16(obj + 0x56) + 0x180) & 0x1FE);
    wr32(obj + 0x1C, (uint32_t)(int32_t)(int16_t)rom16(t) << 3);
    wr32(obj + 0x18, (uint32_t)(int32_t)(int16_t)(uint16_t)-rom16(t + 0x80) << 3);
    walker_pose_ball(obj);
    walker_place_all(obj);
    walker_order(obj);
}

/* $03693E: curling up to roll (its rolling speed by the difficulty) */
static void walker_curl(uint16_t obj)
{
    wr16(obj + O_STATE, 0x38);
    walker_guard(0xCD00, 1);
    walker_guard(0xCD60, 1);
    walker_guard(0xCDC0, 1);
    walker_guard(0xCE20, 1);
    wr16(0xCAEE, rom16(0x0369BA + (uint32_t)(int16_t)rd16(0xA20A)));
    wr32(obj + 0x18, 0);
    if (rd16(obj + 0x52)) { wr16(0xCA2C, 0x180); wr16(0xCA8C, 4); }
    else { wr16(0xCA2C, 0x80); wr16(0xCA8C, 0xFFFC); }
}

/* $036650: walking: at the screen's edge turned about (one in four times curling up) */
static void walker_edge(uint16_t obj)
{
    uint16_t d2 = rd16(v_randomSeed) & 3;
    if (rd16(obj + O_TYPE) == 0x1C) {
        if (rd16(obj + O_STATE) != 0x14) return;
        d2 = (uint16_t)((rd16(0xA20C) ^ 0x0E) << 4);
        uint16_t px = rd16((uint16_t)rd32(0xCBAE) + O_X);
        if (!rd16(obj + 0x52)) {
            d2 = (uint16_t)~d2;
            if ((int16_t)(rd16(obj + O_X) - 0x1A0) >= 0) { wr16(obj + O_STATE, 0x18); return; }
            uint16_t dx = (uint16_t)(px - rd16(obj + O_X));
            if ((int16_t)dx >= 0 || (int16_t)(dx - d2) >= 0) return;
            wr16(obj + O_STATE, 0x18);
            return;
        }
        if ((int16_t)(rd16(obj + O_X) - 0x80) < 0) { wr16(obj + O_STATE, 0x1C); return; }
        uint16_t dx = (uint16_t)(px - rd16(obj + O_X));
        if ((int16_t)dx < 0 || (int16_t)(dx - d2) < 0) return;
        wr16(obj + O_STATE, 0x1C);
        return;
    }
    if (rd16(obj + O_STATE) != 0x14) return;
    if (!rd16(obj + 0x52)) {
        if ((int16_t)(rd16(obj + O_X) - 0x180) < 0) return;
        if (!d2) { walker_curl(obj); return; }
        wr16(obj + O_STATE, 0x18);
        return;
    }
    if ((int16_t)(rd16(obj + O_X) - 0xA0) >= 0) return;
    if (!d2) { walker_curl(obj); return; }
    wr16(obj + O_STATE, 0x1C);
}

/* $0367FC: stood still: a leap, a sway or a spin (the type $1C's own two leaps) */
static void walker_choose(uint16_t obj, uint16_t d2)
{
    if (rd16(obj + O_TYPE) == 0x1C) {
        wr16(0xCB4E, 0x10);
        wr16(obj + 0x50, 2);
        uint32_t vx = rd16(obj + 0x52) == 0x100 ? 0xFFFD8000 : 0x28000;
        if (!rd16(0xCD2E)) {
            wr16(obj + O_STATE, 0x28);
            wr16(obj + 0x56, 0);
            wr32(obj + 0x1C, 0xFFFA2000);
        } else {
            wr16(obj + O_STATE, 0x2C);
            wr32(obj + 0x1C, 0xFFFE0000);
        }
        wr32(obj + 0x18, vx);
        return;
    }
    uint16_t k = 3;
    if (rd16(0xCA30)) {
        d2 = rd16(v_randomSeed);
        k = d2 & 3;
    }
    walker_guard(0xCE80, 1);
    walker_guard(0xCEE0, 1);
    if (k == 3) {
        walker_guard(0xCF40, 1);
        walker_guard(0xCFA0, 1);
        wr16(obj + O_STATE, 0x30);
        wr32(0xCA8E, 0x036B88 + (d2 & 0x30));
        wr16(obj + 0x50, 2);
        wr32(obj + 0x1C, 0xFFFCC000);
        wr32(obj + 0x18, rd16(obj + 0x52) == 0x100 ? 0xFFFF8000 : 0x8000);
        return;
    }
    wr16(0xCA30, (uint16_t)(rd16(0xCA30) - 2));
    if (k == 2) {
        wr16(obj + O_STATE, 0x34);
        wr32(0xCA8E, 0x036B88 + (d2 & 0x10));
        wr16(obj + 0x50, 2);
        wr16(0xCB4C, (uint16_t)((rd16(v_randomSeed) & 1) + 1));
        return;
    }
    wr16(obj + O_STATE, 0x4C);
    wr16(obj + 0x50, 2);
}

/* $03671A: walking, a player close ahead ($40): it stops to act (in the middle, mostly), else
 * stands */
static void walker_spot(uint16_t obj)
{
    if (rd16(obj + O_STATE) != 0x14) return;
    uint16_t near = 0x40, far = 0xFFC0, p = 0xA410;
    for (int k = 0; k < 2; k++, p = 0xA610) {
        uint16_t dx = (uint16_t)(rd16(p) - rd16(obj + O_X));
        if (rd16(obj + 0x52) != 0x100) {
            if ((int16_t)dx < 0 || (int16_t)(dx - near) >= 0) continue;
            wr16(0xCA8C, 8);
        } else {
            if ((int16_t)dx >= 0 || (int16_t)(dx - far) < 0) continue;
            wr16(0xCA8C, 0xFFF8);
        }
        wr16(0xCB4E, (int16_t)(rd16(obj + 0x54) - 0x100) < 0 ? 0xFFF8 : 8);
        wr16(0xCA2C, rd16(obj + 0x52));
        if ((int16_t)(rd16(obj + O_X) - 0xA0) >= 0 && (int16_t)(rd16(obj + O_X) - 0x180) < 0) {
            wr16(0xCD2E, (uint16_t)(rd16(v_randomSeed) & 1));
            if (!(rd16(0xFF0A) & 0x30)) {
                wr16(0xCA30, rd16(0xA20C));
                wr16(obj + O_STATE, 0x20);
                return;
            }
        }
        wr32(obj + 0x18, 0);
        wr16(obj + O_STATE, 0x54);
        return;
    }
}

/* $035A6C: a step: the legs swung by the body's angle, the stride's thump at each end */
static void walker_step(uint16_t obj)
{
    uint16_t d0 = rd16(obj + 0x54);
    uint16_t d1 = (uint16_t)((0 - d0) & 0x1FE), d2 = d1, d3 = d0, d4 = d0;
    if ((int16_t)(((rd16(obj + 0x56) + 0x80) & 0x1FE) - 0x100) < 0) {
        d2 = (uint16_t)((d2 + 0x30) & 0x1FE);
        d4 = (uint16_t)((d4 + 0x30) & 0x1FE);
    }
    if (!rd16(obj + 0x50)) {
        d0 = (uint16_t)((d0 + rd16(0xCBAC)) & 0x1FE);
        if ((int16_t)(d0 - 0x100) < 0 && (int16_t)(d0 - 0x3E) >= 0) {
            wr16(v_shakeSprites, 8);
            QueueSound(0xD0);
            wr16(obj + 0x50, 2);
        }
    } else {
        d0 = (uint16_t)((d0 - rd16(0xCBAC)) & 0x1FE);
        if ((int16_t)(d0 - 0x100) >= 0 && (int16_t)(d0 - 0x1C4) < 0) {
            wr16(v_shakeSprites, 8);
            QueueSound(0xD0);
            wr16(obj + 0x50, 0);
        }
    }
    wr16(obj + 0x54, d0);
    wr16(0xCD36, d1);
    wr16(0xCD96, d2);
    wr16(0xCDF6, d3);
    wr16(0xCE56, d4);
    d3 = d0;
    d4 = d0;
    if (!rd16(obj + 0x50)) {
        if ((int16_t)(d4 - 0x100) >= 0) d4 = (uint16_t)-d4;
        d4 = (uint16_t)(((d4 << 1) & 0xFE) + 0x180 + d0) & 0x1FE;
    }
    d0 = (uint16_t)((uint16_t)-d0 & 0x1FE);
    d1 = d0;
    d2 = d0;
    if (rd16(obj + 0x50)) {
        if ((int16_t)(d2 - 0x100) >= 0) d2 = (uint16_t)-d2;
        d2 = (uint16_t)(((d2 << 1) & 0xFE) + 0x180 + d0) & 0x1FE;
    }
    wr16(0xCEB6, d1);
    wr16(0xCF16, d2);
    wr16(0xCF76, d3);
    wr16(0xCFD6, d4);
    walker_pose(obj);
    int ahead = (int16_t)(((rd16(obj + 0x56) + 0x80) & 0x1FE) - 0x100) >= 0;
    uint16_t foot = rd16(obj + 0x50) ? (ahead ? 0xCD40 : 0xCF80) : (ahead ? 0xCE00 : 0xCEC0);
    wr16(foot + O_Y, 0x145);
    walker_stand_on(obj, foot);
    walker_order(obj);
    walker_spot(obj);
}

/* $03577E is MessageCue */

/* $035792: the camera stopped, no message: its music */
void WalkerWait(uint16_t obj)
{
    if (!bit8(v_cameraHold, 7) || rd16(0xF720)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    QueueSound(0x86);
}

/* $0357BE: the message gone: its colours, the 16 other balls ($12C) flung from their places,
 * the sine table to be built (its size by the difficulty) */
void WalkerBuild(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    StageColoursCopy(0x02A1BC, 0);       /* d0: the colour index left by walker_colours, never $18 */
    wr16(obj + O_VITALITY, 0x7FFF);
    uint16_t size = rom16(0x0357B6 + (uint32_t)(int16_t)rd16(0xA20A));
    wr16(obj + O_X, 0x40);
    wr16(obj + O_Y, 0xC0);
    wr16(0xCBAC, 2);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x2C, 0xF40BF40C);
    wr32(obj + 0x28, 0xF40BF40C);
    wr16(obj + O_FLAGS, 0x8000);
    wr16(obj + 0x08, 0x0A00);
    wr16(obj + 0x0A, 0xF4F4);
    wr16(obj + 0x52, 0);
    wr16(obj + 0x54, 0);
    wr16(obj + 0x56, 0);
    wr16(0xCA8C, 0);
    wr16(obj + 0x5C, 0);
    for (uint16_t b = (uint16_t)(obj + 0x60), k = 0; k < 16; k++, b = (uint16_t)(b + 0x60)) {
        wr8(b + 0x21, 0x20);
        wr32(b + 0x2C, 0xF609F60A);
        wr32(b + 0x28, 0xF40BF40C);
        wr16(b + O_TYPE, 0x12C);
        wr16(b + O_FLAGS, 0x8000);
        wr16(b + O_VITALITY, 0x4000);
        wr16(b + 0x08, 0x0A00);
        wr16(b + 0x0A, 0xF4F4);
        wr16(b + 0x26, 8);
        wr16(b + 0x5C, 0);
        wr16(b + 0x58, 0xC980);
    }
    SineTableStart(WALK_SIN, size);
    wr16(0xCAEC, 0);
    uint32_t t = 0x036CC6;
    for (uint16_t b = 0xC980, k = 0; k < 17; k++, b = (uint16_t)(b + 0x60), t += 16) {
        wr32(b + 0x18, rom32(t));
        wr32(b + 0x1C, rom32(t + 4));
        wr32(b + O_X, rom32(t + 8));
        wr32(b + O_Y, rom32(t + 12));
    }
}

/* $0358AE: the table built: the balls' drop */
void WalkerGrow(uint16_t obj)
{
    wr16(obj + O_VITALITY, 0x7FFF);
    if (SineTableBuild()) return;
    wr16(obj + O_STATE, 0x5C);
    QueueSound(0xBA);
}

/* $0358D0: the balls falling together till the body's fall is -$28000 */
void WalkerDrop(uint16_t obj)
{
    wr16(obj + O_VITALITY, 0x7FFF);
    walker_fly(obj);
    if (rd32(obj + 0x1C) != 0xFFFD8000) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x8C00);
    QueueSound(0xC9);
}

/* $0358FC: unrolling, the legs set */
void WalkerUnfold(uint16_t obj)
{
    wr16(obj + O_VITALITY, 0x7FFF);
    wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) - 4) & 0x1F8));
    if (!rd16(obj + 0x56)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(0xCD36, 0x180);
        wr16(0xCD96, 0x180);
        wr16(0xCDF6, 0x80);
        wr16(0xCE56, 0x80);
    }
    wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x56) + 0x40) & 0x1FE));
    walker_roll(obj);
}

/* $035940: falling till its foot ($CF80) lands: the fight ($D5C its vitality), walking away from
 * its side */
void WalkerLand(uint16_t obj)
{
    wr16(obj + O_VITALITY, 0x7F00);
    uint32_t v = rd32(obj + 0x1C) + 0x1800;
    wr32(obj + 0x1C, v);
    if (!(v & 0x80000000u) && (int16_t)(rd16(0xCF94) - 0x145) >= 0) {
        wr16(v_shakeSprites, 0x10);
        QueueSound(0xD0);
        wr16(0x80C2, 0x10);
        wr16(0x80D6, 0xC980);
        wr16(obj + O_VITALITY, 0xD5C);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x52, 0x80);
        wr16(obj + 0x54, 0);
        if ((int16_t)(rd16(obj + O_X) - 0x120) < 0) {
            wr16(obj + 0x50, 2);
            wr16(obj + O_STATE, 0x1C);
        } else {
            wr16(obj + 0x50, 0);
            wr16(obj + O_STATE, 0x18);
        }
    }
    walker_legs_slow(0x036B58, 0);
    walker_pose_upright(obj);
    walker_place_all(obj);
    walker_order(obj);
}

/* $0359CE: standing up straight (legs together, the body upright, facing as chosen) */
void WalkerStand(uint16_t obj)
{
    int busy = walker_legs_slow(0x036B58, 0);
    if (rd16(obj + 0x54)) {
        wr16(obj + 0x54, (uint16_t)((rd16(obj + 0x54) - 4) & 0x1FC));
        busy = 1;
    }
    if (rd16(obj + 0x52) != rd16(0xCA2C))
        wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x52) + rd16(0xCB4E)) & 0x1F8));
    else if (!busy)
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    walker_pose(obj);
    walker_on_foot(obj);
    walker_order(obj);
}

/* $035A68 */
void WalkerWalk(uint16_t obj)
{
    walker_edge(obj);
    walker_step(obj);
}

/* $035A22: turning to face left ($100) */
void WalkerTurnLeft(uint16_t obj)
{
    wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x52) + 4) & 0x1FE));
    if (rd16(obj + 0x52) == 0x100) {
        wr16(obj + O_STATE, 0x14);
        wr16(0xCA30, rd16(0xA20C));
    }
    walker_step(obj);
}

/* $035A48: turning to face right (0) */
void WalkerTurnRight(uint16_t obj)
{
    wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x52) - 4) & 0x1FE));
    if (!rd16(obj + 0x52)) {
        wr16(obj + O_STATE, 0x14);
        wr16(0xCA30, rd16(0xA20C));
    }
    walker_step(obj);
}

/* $035C04: leaning back upright (the type $1C's leap from it) */
void WalkerLean(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) + rd16(0xCA8C)) & 0x1F8));
    if (!rd16(obj + 0x56)) {
        if (rd16(obj + O_TYPE) == 0x1C) {
            wr16(obj + O_STATE, 0x44);
            wr32(obj + 0x1C, 0);
            wr32(obj + 0x1C, 0xFFFE0000);
            if (rd16(obj + 0x52) == 0x100) {
                wr32(obj + 0x18, 0xFFFA8000);
                wr16(0xCA8C, 0x10);
            } else {
                wr32(obj + 0x18, 0x58000);
                wr16(0xCA8C, 0xFFF0);
            }
            return;
        }
        wr16(obj + O_STATE, 0x14);
    }
    walker_step(obj);
}

/* $035C28 */
void WalkerIdle(uint16_t obj)
{
    (void)obj;
}

/* $035D2A: in a leap: the legs to their pose (+$CA8E) */
static void walker_airborne(uint16_t obj)
{
    walker_legs_fast(rd32(0xCA8E));
    walker_pose(obj);
    walker_place_all(obj);
    walker_order(obj);
}

/* $035C2A: a leap, turning: landed, a pause by the difficulty */
void WalkerLeap(uint16_t obj)
{
    wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x52) + rd16(0xCB4E)) & 0x1FE));
    uint32_t v = rd32(obj + 0x1C) + 0x1800;
    wr32(obj + 0x1C, v);
    if (!(v & 0x80000000u)) {
        wr16(obj + O_STATE, 0x68);
        wr32(obj + 0x1C, 0);
        wr16(0xCD30, rom16(0x035C66 + (uint32_t)(int16_t)rd16(0xA20C)));
    }
    walker_airborne(obj);
}

/* $035C76: spinning fast at the top */
void WalkerSpin(uint16_t obj)
{
    wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x52) + (uint16_t)(rd16(0xCB4E) << 2)) & 0x1FE));
    wr16(0xCD30, (uint16_t)(rd16(0xCD30) - 1));
    if ((int16_t)rd16(0xCD30) < 0) wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    walker_airborne(obj);
}

/* $035C96: coming down, legs out: landed (a thump), the guards off */
void WalkerComeDown(uint16_t obj)
{
    walker_legs_fast(0x036B78);
    wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x52) + rd16(0xCB4E)) & 0x1FE));
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x1800);
    if ((int16_t)(rd16(0xCF94) - 0x145) >= 0) {
        wr16(v_shakeSprites, 0x10);
        QueueSound(0xD0);
        walker_unguard_feet();
        walker_guard(0xCCA0, 0);
        walker_guard(0xCF40, 0);
        walker_guard(0xCFA0, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + O_STATE, 0x10);
    }
    walker_pose(obj);
    walker_place_all(obj);
    walker_order(obj);
}

/* $035D40: swaying round (+$CB4C times): then standing */
void WalkerSway(uint16_t obj)
{
    uint16_t d0 = (uint16_t)((rd16(obj + 0x52) + rd16(0xCB4E)) & 0x1F8);
    wr16(obj + 0x52, d0);
    if (d0 == rd16(0xCA2C)) {
        wr16(0xCB4C, (uint16_t)(rd16(0xCB4C) - 1));
        if ((int16_t)rd16(0xCB4C) < 0) {
            walker_unguard_feet();
            wr16(obj + O_STATE, 0x10);
        }
    }
    if ((int16_t)(d0 - 0x108) < 0) {
        if ((int16_t)(d0 - 0x88) >= 0) d0 = (uint16_t)-d0 & 0x70;
        wr16(obj + 0x56, (uint16_t)(d0 >> 2));
    } else {
        if ((int16_t)(d0 - 0x188) >= 0) d0 = (uint16_t)-d0;
        d0 = (uint16_t)(((uint16_t)(d0 - 2) & 0x70) >> 2);
        wr16(obj + 0x56, (uint16_t)(0x1FE - d0));
    }
    walker_legs_fast(rd32(0xCA8E));
    walker_pose(obj);
    walker_on_back(obj);
    walker_order(obj);
}

/* $035DE6: curling: the body turned to face out ($80) */
void WalkerCurl(uint16_t obj)
{
    uint16_t f = rd16(obj + 0x52);
    if (f == 0x80) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x54, 0);
    } else if ((int16_t)(f - 0x80) >= 0 && (int16_t)(f - 0x180) < 0) {
        wr16(obj + 0x52, (uint16_t)(f - 8));
    } else {
        wr16(obj + 0x52, (uint16_t)(f + 8));
    }
    wr16(obj + 0x54, rd16(obj + 0x54) & 0x1F8);
    walker_pose(obj);
    walker_legs_slow(0x036B58, 0);
    walker_on_foot(obj);
    walker_order(obj);
}

/* $035E3E: the legs tucked: rolling ($13 frames' wind-up) */
void WalkerTuck(uint16_t obj)
{
    walker_pose_upright(obj);
    if (!walker_legs_slow(0x036B68, 0)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(0xCAEC, 0x13);
        QueueSound(0xB5);
    }
    walker_place_all(obj);
    walker_order(obj);
}

/* $035E6C: rolling to the far side: there, the guards off, $80 frames on */
void WalkerRolling(uint16_t obj)
{
    if ((int16_t)rd16(0xCAEC) >= 0) {
        wr16(0xCAEC, (uint16_t)(rd16(0xCAEC) - 1));
    } else if (rd16(obj + 0x56) != rd16(0xCA2C)) {
        wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) + rd16(0xCA8C)) & 0x1FC));
    } else {
        int there = (rd16(0xCA8C) & 0x8000) ? (int16_t)(rd16(obj + O_X) - 0xC0) < 0
                                            : (int16_t)(rd16(obj + O_X) - 0x160) >= 0;
        if (there) {
            wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
            wr16(0xCAEC, 0x80);
            walker_guard(0xCD00, 0);
            walker_guard(0xCD60, 0);
            walker_guard(0xCDC0, 0);
            walker_guard(0xCE20, 0);
        }
    }
    wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x52) - rd16(0xCAEE)) & 0x1FE));
    walker_roll(obj);
}

/* $035F0A: unrolling */
void WalkerUnroll(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) + rd16(0xCA8C)) & 0x1FC));
    if (!rd16(obj + 0x56)) wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x52) - rd16(0xCAEE)) & 0x1FE));
    walker_roll(obj);
}

/* $035F32: righting itself: then a message, and the fall again */
void WalkerRight(uint16_t obj)
{
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x52) - 8) & 0x1F8));
    if (rd16(obj + 0x52) == 0x80) {
        wr16(obj + 0x54, 0);
        wr16(obj + 0x52, 0x80);
        wr16(obj + O_STATE, 0x64);
        wr16(0x80C2, 0x28);
        wr16(0x80C6, 0x60);
        wr32(0x80C8, 0x03A519);
        wr16(0x80C4, 8);
    }
    walker_pose_ball(obj);
    walker_place_all(obj);
    walker_order(obj);
}

/* $035F86: a turn on the spot (to face away): then standing */
void WalkerTurnAbout(uint16_t obj)
{
    walker_legs_slow(0x036BA8, 0);
    uint16_t d0 = (uint16_t)((rd16(obj + 0x52) + rd16(0xCB4E)) & 0x1F8);
    wr16(obj + 0x52, d0);
    if (d0 == ((rd16(0xCA2C) + 0x100) & 0x1FE)) {
        walker_unguard_feet();
        wr16(obj + O_STATE, 0x10);
    }
    walker_pose(obj);
    walker_on_back(obj);
    walker_order(obj);
}

/* $036026: beaten, the balls tumbling down */
static void walker_tumble(uint16_t obj)
{
    wr16(obj + 0x52, (uint16_t)((rd16(obj + 0x52) + 0x10) & 0x1F0));
    for (uint16_t b = 0xC980, k = 0; k < 17; k++, b = (uint16_t)(b + 0x60))
        wr32(b + 0x1C, rd32(b + 0x1C) + 0x2000);
    walker_order(obj);
}

/* $036000: $40 frames down: all gone, the camera let go, a blast and sparks ($4EC) flung up */
void WalkerEnd(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) >= 0) { walker_tumble(obj); return; }
    wr8(v_cameraHold, (uint8_t)(rd8(v_cameraHold) & ~0x80));
    for (uint16_t b = obj, k = 0; k < 17; k++, b = (uint16_t)(b + 0x60)) wr16(b + O_FLAGS, 0x1000);
    QueueSound(0xCA);
    wr16(v_shakeSprites, 0x10);
    wr16(0xA014, 0x10);
    uint32_t t = 0x036E30, vx = 0xFFFF0000;
    uint16_t x = 0xA0;
    for (int k = 0; k < 9; k++, t += 4, vx += 0x4000, x = (uint16_t)(x + 0x20)) {
        uint16_t slot, ty;
        if (!SlotFreeHigh(&slot, &ty)) return;
        wr16(slot + O_TYPE, 0x4EC);
        wr16(slot + O_X, x);
        wr16(slot + O_Y, 0x140);
        wr32(slot + 0x18, vx);
        wr32(slot + 0x1C, rom32(t));
    }
}

/* $035FEA: the balls falling till the body is down at $168 */
void WalkerFall(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_Y) - 0x168) < 0) { walker_tumble(obj); return; }
    wr16(obj + 0x48, 0x40);
    wr16(obj + O_STATE, 0x58);
    WalkerEnd(obj);
}

/* $036054: stepping its legs to rest, the body to $30: then the next act */
void WalkerSettle(uint16_t obj)
{
    uint16_t last;
    int moved = walker_legs_slow(0x036B78, &last);
    wr16(obj + 0x54, rd16(obj + 0x54) & 0x1F8);
    uint16_t b = rd16(obj + 0x54);
    if (b != 0x30) {
        if ((int16_t)(b - 0x130) >= 0 || (int16_t)(b - 0x30) < 0) wr16(obj + 0x54, (uint16_t)(b + 8));
        else wr16(obj + 0x54, (uint16_t)(b - 8));
        wr16(obj + 0x54, rd16(obj + 0x54) & 0x1F8);
    } else if (!moved) {
        walker_choose(obj, last);
    }
    walker_pose(obj);
    uint16_t foot = rd16(obj + 0x50) ? 0xCF80 : 0xCEC0;
    wr16(foot + O_Y, 0x145);
    wr16(obj + O_Y, (uint16_t)(0x145 - rd16(foot + 0x4A)));
    walker_place_all(obj);
    walker_order(obj);
}

/* $0360C8: beaten: the points, the balls flung apart */
static void walker_beaten(uint16_t obj)
{
    AddScore(obj, 0x12046);
    uint32_t t = 0x036140;
    wr16(obj + O_VITALITY, 0);
    wr8(0xA028, 1);
    for (uint16_t b = 0xC980, k = 0; k < 17; k++, b = (uint16_t)(b + 0x60), t += 2) {
        wr16(b + O_FLAGS, 0x8C00);
        wr8(b + 0x21, 0);
        wr16(b + 0x5C, 0);
        wr32(b + 0x1C, (uint32_t)((int32_t)(int8_t)rom8(t) * 0x1000));
        wr32(b + 0x18, (uint32_t)((int32_t)(int8_t)rom8(t + 1) * 0x1000));
    }
    wr16(obj + O_FLAGS, 0x8C00);
    wr16(obj + O_STATE, 0x50);
    QueueSound(0xD2);
}

/* $0356F2 */
void Walker(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) { walker_beaten(obj); return; }
    walker_colours(obj, 0x036C22);
    call_object(rom32(0x03570E + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

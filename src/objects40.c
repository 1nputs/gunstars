/* Objects of the stages (the types from $40470 to $465C0): the boss parts' sparks and gun pods,
 * the lantern flames, the switch target and the bouncing ball. */
#include "game.h"

/* ---- type $2AC: a spark, flickering; struck by a shot: knocked back as a puff ($100) ---- */

/* $040470 */
void Spark2AC(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + O_FLAGS, 0x8F00);
        wr16(obj + 0x0E, 0x41E0);
        wr16(obj + 0x08, 0);
        wr16(obj + 0x0A, 0xFCFC);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xFC04FC04);
        wr16(obj + 0x26, 1);
        return;
    }
    if (!bit8(obj + 0x22, 4)) {
        wr16(obj + 0x0E, rom16(0x040506 + ((uint16_t)(rd16(v_gameFrames) << 1) & 6)));
        return;
    }
    wr32(obj + 0x18, ~(uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x1C, ~(uint32_t)((int32_t)rd32(obj + 0x1C) >> 1));
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x056450);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
}

/* ---- type $2B0: a gun pod on the floor line, firing a spark ($2AC) at its player as often as
 * the difficulty says; shot: 4,600 points, a blast ($2F4), an item ---- */

/* $040AB8 */
static void pod_on_floor(uint16_t obj)
{
    wr16(obj + O_Y, BelowCamera220(0x100));
}

/* $040628 */
static void pod_reload(uint16_t obj)
{
    pod_on_floor(obj);
    wr16(obj + 0x48, rom16(0x04063E + (uint32_t)(int16_t)rd16(0xA20C)));
}

/* $04064E */
static void pod_fire(uint16_t obj)
{
    pod_on_floor(obj);
    uint16_t p = PlayerTarget();
    int16_t dx = (int16_t)(rd16(p + O_X) - rd16(obj + O_X));
    uint32_t d1 = (uint16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
    uint16_t a = (uint16_t)(((int16_t)Atan2(dx, &d1) >> 7) & 0x1FE);
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x2AC);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x1C, (uint32_t)(int32_t)(int16_t)rom16(0x02977C + a) << 3);
    wr32(slot + 0x18, (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + a) << 3);
}

/* $040A6E: its colours flashing */
static void pod_flash(uint16_t obj)
{
    wr16(obj + 0x0E, rom16(0x040A6A + (rd16(v_gameFrames) & 2)));
}

/* $0405D2 */
void GunPodStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x8300);
    wr16(obj + 0x0E, 0x26C0);
    wr16(obj + 0x08, 0x0F00);
    wr16(obj + 0x0A, 0xF0F0);
    wr16(obj + O_VITALITY, 0x30);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xF010F010);
    wr8(obj + 0x20, 0xC0);
    pod_reload(obj);
}

/* $04060E */
void GunPodFire(uint16_t obj)
{
    pod_on_floor(obj);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        pod_fire(obj);
        pod_reload(obj);
    }
    pod_flash(obj);
}

/* $04059A */
void GunPod(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        wr16(obj + O_TYPE, 0x2F4);
        wr16(obj + O_STATE, 0);
        AddScore(obj, 0x460);
        DropItemRandom(obj);
        return;
    }
    call_object(rom32(0x0405CA + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $320: a lantern ($DE80 the first, $C9E0 its flame; the other's $CA40), lit while
 * $A200's bit says, a flame ($57C) each 16 frames; unlit: put back at $B0,$D0 ---- */

/* $041F1E */
void Lantern(uint16_t obj)
{
    uint16_t flame;
    uint32_t look;
    if (obj == 0xDE80) {
        if (!bit8(0xA200, 0)) {
            wr16(0x8040, 0);
            goto unlit;
        }
        flame = 0xC9E0;
        look = 0x0E0B82;
    } else {
        flame = 0xCA40;
        look = 0x0E0B8E;
        if (!bit8(0xA200, 1)) {
            wr16(0x8042, 0);
            goto unlit;
        }
    }
    wr16(obj + O_FLAGS, (uint16_t)(rd16(obj + O_FLAGS) | 0x8000));
    wr16(flame + O_X, rd16(obj + O_X));
    wr16(flame + O_Y, rd16(obj + O_Y));
    wr16(flame + O_Y, (uint16_t)(rd16(flame + O_Y) - 0x10));
    if (rd16(v_gameFrames) & 0x0F) return;
    wr16(flame + O_TYPE, 0x57C);
    wr16(flame + 0x0E, 0x87B4);
    wr16(flame + O_FLAGS, 0xC000);
    wr32(flame + 0x08, look);
    wr8(flame + 0x20, 0xBC);
    wr16(flame + 0x48, 8);
    return;
unlit:
    wr8(obj + O_FLAGS, (uint8_t)(rd8(obj + O_FLAGS) & ~0x80));
    wr16(obj + 0x48, 0);
    wr16(obj + O_X, 0xB0);
    wr16(obj + O_Y, 0xD0);
}

/* ---- type $318: a switch: struck, $CA2A turned to the next of four, then $80 frames' rest ---- */

/* $046094 */
void Switch318(uint16_t obj)
{
    if ((int16_t)rd16(0xCA2C) >= 0) {
        wr16(0xCA2C, (uint16_t)(rd16(0xCA2C) - 1));
        wr8(obj + 0x21, 0);
        return;
    }
    wr8(obj + 0x21, 0x20);
    if (!bit8(obj + 0x22, 5)) return;
    wr16(0xCA2A, (uint16_t)(rd16(0xCA2A) + 2));
    wr16(0xCA2A, rd16(0xCA2A) & 6);
    wr8(obj + 0x21, 0);
    wr16(0xCA2C, 0x80);
}

/* ---- type $328: a ball bouncing off the level's walls (their attributes $18, $1C, $28, $38)
 * while $808C is negative ---- */

/* $0460CC */
void BounceBall(uint16_t obj)
{
    if ((int16_t)rd16(0x808C) >= 0) { wr16(obj + O_FLAGS, 0x1000); return; }
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + O_FLAGS, 0x8E00);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xF808F808);
        wr16(obj + 0x26, 2);
        wr16(obj + 0x08, 0x0500);
        wr16(obj + 0x0A, 0xF8F8);
        wr32(obj + 0x18, 0xE000);
        wr32(obj + 0x1C, 0xE000);
    }
    wr16(obj + 0x0E, rom16(0x0461CA + (((uint16_t)((int16_t)rd16(v_gameFrames) >> 1)) & 6)));
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    if ((int16_t)(x - 0x80) < 0 || MapAttribute((uint16_t)(x - 6), y) == 0x1C) wr32(obj + 0x18, 0xE000);
    if (MapAttribute((uint16_t)(rd16(obj + O_X) + 6), rd16(obj + O_Y)) == 0x18) wr32(obj + 0x18, 0xFFFF2000);
    if (MapAttribute(rd16(obj + O_X), (uint16_t)(rd16(obj + O_Y) - 6)) == 0x28) wr32(obj + 0x1C, 0xE000);
    if ((int16_t)(rd16(obj + O_Y) - 0x160) >= 0 || MapAttribute(rd16(obj + O_X), (uint16_t)(rd16(obj + O_Y) + 6)) == 0x38)
        wr32(obj + 0x1C, 0xFFFF2000);
}

/* ---- type $2D0: the dice palace's prize room: four prizes ($43E02 by the roll), the room's
 * lines waving; left by its player at the far corner ($1A0,$130), or lost with its bit of $A200
 * ---- */

/* $043DB2: the four prizes in a row from $F0 */
static void prize_items(uint16_t obj)
{
    uint32_t t = 0x043E02 + (uint32_t)(int16_t)(uint16_t)(rd16(obj + 0x4C) << 2);
    uint16_t x = 0xF0;
    for (int k = 0; k < 4; k++, t += 2, x = (uint16_t)(x + 0x20)) {
        uint16_t slot;
        if (!SlotFreeLow(&slot)) return;
        wr16(slot + O_TYPE, rom16(t));
        if (rd16(slot + O_TYPE) == 0x68) wr16(slot + 0x50, 0x0F);
        wr16(slot + O_FLAGS, 0x8C00);
        wr16(slot + O_X, x);
        wr16(slot + O_Y, 0xF0);
        wr16(slot + 6, 0);
    }
}

/* $043E3A: the lines' wave at $965E (by the prize: a sine, a shear, a bow) */
static void prize_wave(uint16_t obj)
{
    uint16_t a = 0x965E, f = rd16(v_gameFrames);
    uint16_t kind = rd16(obj + 0x4C);
    if (!kind) {
        uint16_t d0 = (uint16_t)(f << 4) & 0x1FE;
        for (int k = 0; k <= 0x98; k++, a = (uint16_t)(a + 2), d0 = (uint16_t)((d0 + 2) & 0x1FE))
            wr16(a, (uint16_t)((((uint32_t)(int32_t)(int16_t)rom16(0x02977C + d0) << 5) >> 16) + f));
        return;
    }
    if (kind == 2) {
        for (int k = 0; k <= 0x4C; k++, a = (uint16_t)(a + 4)) {
            wr16(a, f);
            wr16((uint16_t)(a + 2), (uint16_t)-f);
        }
        return;
    }
    uint32_t d1 = 0, d3 = 0;
    uint16_t d5 = 0, d4 = (uint16_t)((uint16_t)(f << 1) ^ 0xFF);
    for (int k = 0; k <= 0x4C; k++, a = (uint16_t)(a + 2)) {
        wr16(a, (uint16_t)((d1 >> 16) + d4));
        d1 += d3;
        d3 += (uint32_t)(int32_t)(int16_t)rom16(0x02977C + (uint32_t)(int16_t)d5);
        d5 = (uint16_t)(d5 + 2);
    }
    d4 = (uint16_t)(f << 1);
    for (int k = 0; k <= 0x4C; k++, a = (uint16_t)(a + 2)) {
        wr16(a, (uint16_t)((d1 >> 16) + d4));
        d1 += d3;
        d3 -= (uint32_t)(int32_t)(int16_t)rom16(0x02977C + (uint32_t)(int16_t)d5);
        d5 = (uint16_t)(d5 - 2);
    }
}

/* $043D5E: the prize by its player's roll ($8040 / $8042: 4, $14, other) */
void PrizeRoomStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(0xF74A, 0x0C);
    wr16(0xF74E, 0);
    uint16_t roll = rd16(0x80D8) == 0xA400 ? rd16(0x8040) : rd16(0x8042);
    wr16(obj + 0x4C, 0);
    if (roll != 4) {
        wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) + 2));
        if (roll != 0x14) wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) + 2));
    }
    prize_items(obj);
    prize_wave(obj);
}

/* $043DAC */
void PrizeRoomWave(uint16_t obj)
{
    prize_wave(obj);
}

/* $043CDE */
void PrizeRoom(uint16_t obj)
{
    if (rd8(0xA200)) {
        int in;
        if (rd16(0x80D8) == 0xA400) in = bit8(0xA200, 0);
        else if (rd16(0x80D8) == 0xA600) in = bit8(0xA200, 1);
        else in = 0;
        if (!in) {
            wr16(0x8052, 1);
            DiceRoomEnd(obj);
            return;
        }
    }
    uint16_t p = rd16(0x80D8);
    if ((int16_t)(rd16(p + O_X) - 0x1A0) < 0 || (int16_t)(rd16(p + O_Y) - 0x130) < 0) {
        call_object(rom32(0x043D56 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
        return;
    }
    DiceRoomEnd(obj);
    wr16(0xF74A, 0);
    wr16(0xF74E, 0);
    for (uint16_t a = 0xC800, k = 0; k < 4; k++, a = (uint16_t)(a + 0x60))
        if (rd16(a)) wr16(a + O_FLAGS, 0x1000);
}

/* ---- type $338: a pod bouncing down to $148, opening its four spikes (the parts at +$58-+$5E)
 * out and in, bouncing on; held while the room's lead ($DE80) is in state 4, flung back while it
 * is in $C ---- */

/* $0467B8: its spikes ($10) */
static void pod_spikes(uint16_t obj)
{
    uint16_t at = (uint16_t)(obj + 0x58);
    for (int k = 0; k < 4; k++, at = (uint16_t)(at + 2)) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x10);
        wr32(slot + 0x2C, 0xFC04FC04);
        wr16(slot + 0x26, 8);
        wr16(at, slot);
    }
}

/* $0467E6: the spikes $48/2 out round it at the turn +$56 (their looks by the distance) */
static void pod_spikes_place(uint16_t obj)
{
    uint16_t at = (uint16_t)(obj + 0x58);
    uint16_t a = (uint16_t)(rd16(obj + 0x56) << 4) & 0x1FE;
    for (int k = 0; k < 4; k++, at = (uint16_t)(at + 2), a = (uint16_t)((a + 0x80) & 0x1FE)) {
        uint16_t s = rd16(at);
        if (!s) continue;
        uint16_t r = rd16(obj + 0x48);
        if ((int16_t)r < 0 || (int16_t)(r - 0x50) >= 0) {
            wr16(s + O_FLAGS, 0);
            wr8(s + 0x21, 0);
            continue;
        }
        wr16(s + O_FLAGS, 0x8000);
        wr8(s + 0x21, 0x10);
        uint32_t t = 0x05663A + (uint32_t)(int16_t)(uint16_t)((r & 0xFFF8) + 2);
        wr16(s + 0x0E, rom16(t));
        wr16(s + 0x08, rom16(t + 2));
        wr16(s + 0x0A, rom16(t + 4));
        int16_t h = (int16_t)((int16_t)r >> 1);
        uint16_t dy = (uint16_t)(((uint32_t)((int32_t)(int16_t)rom16(0x02977C + a) * h) << 2) >> 16);
        uint16_t dx = (uint16_t)(((uint32_t)((int32_t)(int16_t)rom16(0x0297FC + a) * h) << 2) >> 16);
        wr16(s + O_X, (uint16_t)(dx + rd16(obj + O_X)));
        wr16(s + O_Y, (uint16_t)(dy + rd16(obj + O_Y) - (uint16_t)h));
    }
}

/* $046674: flung back: gone with its spikes when its fall stops */
void SpikePodFlung(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8400);
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2000);
    if (rd32(obj + 0x1C)) return;
    wr16(obj + O_FLAGS, 0x1000);
    uint16_t at = (uint16_t)(obj + 0x58);
    for (int k = 0; k < 4; k++, at = (uint16_t)(at + 2))
        if (rd16(at)) wr16(rd16(at) + O_FLAGS, 0x1000);
}

/* $046614: dropping to $148; then a half bounce */
void SpikePodDrop(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8000);
    uint16_t lead = rd16(0xDE84);
    if (lead == 4) return;
    wr16(obj + O_FLAGS, 0x8400);
    if (lead == 0x0C) {
        wr16(obj + O_STATE, 0x18);
        wr32(obj + 0x1C, (uint32_t)-rd32(obj + 0x1C));
        SpikePodFlung(obj);
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2000);
    if ((int16_t)(rd16(obj + O_Y) - 0x148) < 0) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(obj + 0x1C, (uint32_t)-(uint32_t)((int32_t)rd32(obj + 0x1C) >> 1));
}

/* $0465E6 */
void SpikePodStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x8400);
    wr16(obj + 0x0E, 0x2306);
    wr16(obj + 0x08, 0x0500);
    wr16(obj + 0x0A, 0xF8F8);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xFE02FE02);
    pod_spikes(obj);
    SpikePodDrop(obj);
}

/* $046726: flung back up (state $14): down at $148, bounced up twice as hard */
void SpikePodRebound(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8400);
    uint32_t v = rd32(obj + 0x1C) + 0x2000;
    wr32(obj + 0x1C, v);
    if ((v & 0x80000000u) || (int16_t)(rd16(obj + O_Y) - 0x148) < 0) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(obj + 0x1C, (uint32_t)-v << 1);
}

/* $046690: bouncing: landed again, a thump, opening (its bounce kept at +$4C) */
void SpikePodBounce(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8000);
    uint16_t lead = rd16(0xDE84);
    if (lead == 4) return;
    wr16(obj + O_FLAGS, 0x8400);
    if (lead == 0x0C) {
        wr16(obj + O_STATE, 0x14);
        wr32(obj + 0x1C, (uint32_t)-rd32(obj + 0x1C));
        SpikePodRebound(obj);
        return;
    }
    uint32_t v = rd32(obj + 0x1C) + 0x2000;
    wr32(obj + 0x1C, v);
    if ((v & 0x80000000u) || (int16_t)(rd16(obj + O_Y) - 0x148) < 0) return;
    QueueSound(0xCE);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x8000);
    wr16(obj + 0x0E, 0x2300);
    wr16(obj + 0x08, 0x0900);
    wr16(obj + 0x0A, 0xF4F8);
    wr32(obj + 0x4C, (uint32_t)-(v - 0x2000));
    wr16(obj + 0x48, 0);
}

/* $046784: closing: shut, bouncing up again */
void SpikePodClose(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 2));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(obj + 0x1C, rd32(obj + 0x4C));
        wr16(obj + O_FLAGS, 0x8400);
        wr16(obj + 0x0E, 0x2306);
        wr16(obj + 0x08, 0x0500);
        wr16(obj + 0x0A, 0xF8F8);
    }
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 2));
    pod_spikes_place(obj);
}

/* $046760: opening, its spikes turning out */
void SpikePodOpen(uint16_t obj)
{
    uint16_t lead = rd16(0xDE84);
    if (lead == 4) return;
    if (lead == 0x0C) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        SpikePodClose(obj);
        return;
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) + 2));
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 2));
    pod_spikes_place(obj);
}

/* $0465C0 */
void SpikePod(uint16_t obj)
{
    call_object(rom32(0x0465CA + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $314: the dice palace's switch room: its timer ($31C in $DE80), fourteen switches
 * ($318 from $C9E0) and balls ($328, by the difficulty); the floor's tiles redrawn to the
 * switches' setting ($CA2A); its player on the goal ($180-$19F, $B6-$B9): the room won; the time
 * out ($DECE): lost ---- */

/* $045EC6: the timer: 4:00 */
static void switch_room_timer(void)
{
    wr16(0xDE80 + O_TYPE, 0x31C);
    wr16(0xDE80 + 0x48, 0x400);
    wr16(0xDE80 + 0x4A, 1);
    wr16(0xDE80 + 0x4C, 0);
    wr16(0xDE80 + 0x4E, 0);
    wr16(0xDE80 + 0x50, 0xF0);
    wr16(0xDE80 + 0x52, 0xA8);
}

/* $045FFE: the floor's tiles from a list (bit 0 of each place: the second look) */
static void switch_room_tiles(uint32_t list, uint32_t look, uint32_t look2)
{
    for (;;) {
        uint16_t at = bus_rd16(list);
        list += 2;
        if (at == 0xFFFF) return;
        if (at & 1) PlaneAChunk(look2, (uint16_t)(at & ~1));
        else PlaneAChunk(look, at);
    }
}

/* $045E50: its player on the goal: the room won (its message, the timer stopped) */
static int switch_room_goal(uint16_t obj)
{
    uint16_t p = rd16(0x80D8);
    if ((int16_t)(rd16(p + O_X) - 0x180) < 0 || (int16_t)(rd16(p + O_X) - 0x1A0) >= 0) return 0;
    if ((int16_t)(rd16(p + O_Y) - 0xB6) < 0 || (int16_t)(rd16(p + O_Y) - 0xBA) >= 0) return 0;
    QueueSound(0x82);
    wr16(0x808C, 4);
    wr16(obj + O_STATE, 0x10);
    wr16(0xDE84, 8);
    wr16(0x80C2, 0x3C);
    wr32(0x80C8, 0x03A438);
    wr32(0x80CE, (uint32_t)rd16(0xDE80 + 0x4A) << 16 | rd16(0xDE80 + 0x4C));
    wr16(0x80D6, rd16(0x80D8));
    return 1;
}

/* $045D7E: its player's +$72 kept; the timer, switches and balls placed */
void SwitchRoomStart(uint16_t obj)
{
    uint16_t p = rd16(0x80D8);
    wr16(obj + 0x5E, rd16(p + 0x72));
    wr16(p + 0x72, 0);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(0xCA2A, 8);
    wr16(0xCA28, 8);
    wr16(0xCA2C, 0);
    switch_room_timer();
    uint16_t a = 0xC9E0;
    uint32_t i = 0;
    for (int k = 0; k < 14; k++, i += 2, a = (uint16_t)(a + 0x60)) {
        wr16(a + O_TYPE, 0x318);
        wr8(a + 0x21, 0x20);
        wr32(a + 0x28, 0xE818F010);
        wr16(a + O_X, rom16(0x045F5A + i));
        wr16(a + O_Y, rom16(0x045F7C + i));
    }
    uint16_t n = rd16(0xA20A);
    if (!n) return;
    n = (uint16_t)(((int16_t)n >> 1) - 1);
    do {
        wr16(a + O_TYPE, 0x328);
        wr16(a + O_X, rom16(0x045F5A + i));
        wr16(a + O_Y, rom16(0x045F7C + i));
        i += 2;
        a = (uint16_t)(a + 0x60);
    } while (n-- != 0);
}

/* $045DAA: the time out: lost (its message, the floor's lead to state 4); else the goal, or a
 * switch turned */
void SwitchRoomPlay(uint16_t obj)
{
    if (rd16(0xDECE)) {
        wr16(0x808C, 4);
        wr16(obj + O_STATE, 0x18);
        wr16(obj + 0x48, 0x40);
        wr32(0x80C8, 0x03A42D);
        wr16(0x80C2, 0x28);
        wr16(0x80C4, 8);
        wr16(0x80C6, 0x100);
        return;
    }
    if (switch_room_goal(obj)) return;
    if (rd16(0xCA2A) != rd16(0xCA28)) wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
}

/* $045DFA: the old setting's tiles taken away */
void SwitchRoomErase(uint16_t obj)
{
    if (rd16(0xCA28) != 8) {
        uint16_t i = (uint16_t)(rd16(0xCA28) << 1);
        uint32_t list = rom32(0x046050 + (uint32_t)(int16_t)i);
        i = (uint16_t)(i << 1);
        switch_room_tiles(list, rom32(0x046030 + (uint32_t)(int16_t)i), rom32(0x046030 + (uint32_t)(int16_t)(uint16_t)(i + 4)));
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
}

/* $045E0E: the new setting's tiles, its scroll ($46028) */
void SwitchRoomDraw(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    uint16_t s = rom16(0x046028 + (uint32_t)(int16_t)rd16(0xCA2A));
    wr16(0xE33C, s);
    wr16(0xE3BC, s);
    uint32_t list = rom32(0x046050 + (uint32_t)(int16_t)(uint16_t)(rd16(0xCA2A) << 1));
    wr16(0xCA28, rd16(0xCA2A));
    switch_room_tiles(list, 0xFFFF4540, 0xFFFF4560);
}

/* $045E1C: won: its message gone, the room over */
void SwitchRoomWon(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    DiceRoomEnd(obj);
    wr16(rd16(0x80D8) + 0x72, rd16(obj + 0x5E));
}

/* $045E36: lost: $40 frames, the room over */
void SwitchRoomLost(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) >= 0) return;
    DiceRoomEnd(obj);
    wr16(rd16(0x80D8) + 0x72, rd16(obj + 0x5E));
}

/* $045D18 */
void SwitchRoom(uint16_t obj)
{
    if (rd8(0xA200)) {
        int in;
        if (rd16(0x80D8) == 0xA400) in = bit8(0xA200, 0);
        else if (rd16(0x80D8) == 0xA600) in = bit8(0xA200, 1);
        else in = 0;
        if (!in) {
            wr16(0x8052, 1);
            DiceRoomEnd(obj);
            return;
        }
    }
    call_object(rom32(0x045D62 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $27C: the dice palace's die: struck by a player ($22 bit 1), rolled: bouncing off
 * the walls till it settles on a face (+$48: 0, 2, 4), its number told ($8044); the striker
 * (+$42) taken into the room if its bit of $804F allows ---- */

/* $041C50: the planes' scroll for the room's view (four words) */
static void die_scroll(uint32_t t)
{
    wr16(0xE330, rom16(t));
    wr16(0xE3B0, rom16(t));
    wr16(0xE334, rom16(t + 2));
    wr16(0xE3B4, rom16(t + 2));
    wr16(0xE33C, rom16(t + 4));
    wr16(0xE3BC, rom16(t + 4));
    wr16(0xE33E, rom16(t + 6));
    wr16(0xE3BE, rom16(t + 6));
}

/* $04223E: tumbling */
static void die_tumble(uint16_t obj)
{
    wr16(obj + 0x08, 0x0F00);
    wr16(obj + 0x0A, 0xF0F0);
    wr16(obj + 0x0E, rom16(0x042262 + (((uint16_t)((int16_t)rd16(v_gameFrames) >> 1)) & 6)));
}

/* $0422AE: rolling slower by $200 (at most $400: stopped) */
static void die_slow(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x18);
    if (!(v & 0x80000000u)) {
        v -= 0x200;
        wr32(obj + 0x18, v);
        if ((int32_t)(v - 0x400) < 0) wr32(obj + 0x18, 0);
        return;
    }
    v += 0x200;
    wr32(obj + 0x18, v);
    if ((int32_t)(v - 0xFFFFFC00u) >= 0) wr32(obj + 0x18, 0);
}

/* $0422EC: off the room's walls, a knock */
static void die_walls(uint16_t obj)
{
    int bounce;
    int left = bit8(obj + 0x18, 7);
    if (!rd16(0x804E)) bounce = left ? (int16_t)(rd16(obj + O_X) - 0xA8) < 0 : (int16_t)(rd16(obj + O_X) - 0x198) >= 0;
    else bounce = left ? (int16_t)(rd16(obj + O_X) - 0x90) < 0 : (int16_t)(rd16(obj + O_X) - 0x1B0) >= 0;
    if (!bounce) return;
    wr32(obj + 0x18, (uint32_t)-rd32(obj + 0x18));
    if (rd32(obj + 0x18)) QueueSound(0xE6);
}

/* $04228E: a face by chance */
static void die_face(uint16_t obj)
{
    Random();
    int16_t r = (int16_t)((rd16(v_randomSeed) & 0xFF) + 2);
    wr16(obj + 0x48, (uint16_t)((r % 3) << 1));
}

/* $04226A */
static void die_show_face(uint16_t obj)
{
    wr16(obj + 0x08, 0x0A00);
    wr16(obj + 0x0A, 0xF4F4);
    wr16(obj + 0x0E, rom16(0x042288 + (uint32_t)(int16_t)rd16(obj + 0x48)));
}

/* $041FF0 */
void PalaceDieStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    if (!rd16(0x804E)) {
        wr16(obj + 0x4E, 0x8000);
        wr16(obj + 0x4C, 0);
        return;
    }
    wr16(obj + 0x4E, 0);
    wr16(obj + 0x4C, 0x40);
    die_scroll(0x041C72);
}

/* $042022: after $40, the room's message (by the roll; with both players, its striker's) */
void PalaceDieMessage(uint16_t obj)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if ((int16_t)rd16(obj + 0x4C) >= 0) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    uint16_t roll = rd16(0x804E);
    if (!roll) return;
    wr16(0x80C2, 0x28);
    wr16(0x80C6, 0x100);
    wr16(0x80C4, 0);
    if (rd8(0xA200) == 3) {
        wr32(0x80C8, rom32(0x042086 + (uint32_t)(int16_t)(uint16_t)(((roll & 7) << 2) - 4)));
        return;
    }
    wr32(0x80C8, bit8(0x804F, 2) ? 0x03A3A8 : 0x03A35B);
}

/* $04209E: dropped in at $120,$80 (or at the boss's place) */
void PalaceDieDrop(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x8C00);
    wr32(obj + 0x28, 0xFA06FA06);
    if (rd16(0x804E)) {
        wr8(obj + 0x21, 2);
        wr16(obj + O_X, 0x120);
        wr16(obj + O_Y, 0x80);
        return;
    }
    wr16(obj + O_X, rd16(0xC990));
    wr16(obj + O_Y, rd16(0xC994));
}

/* $0420DC: bouncing on the floor ($140) till still: then a face */
void PalaceDieBounce(uint16_t obj)
{
    die_tumble(obj);
    die_walls(obj);
    die_slow(obj);
    uint32_t v = rd32(obj + 0x1C) + 0x4000;
    wr32(obj + 0x1C, v);
    if (v & 0x80000000u) return;
    if ((int16_t)(rd16(obj + O_Y) - (rd16(0x804E) ? 0x140 : 0x144)) < 0) return;
    v = rd32(obj + 0x1C);
    if ((int32_t)(v - 0xA000) < 0) {
        if (bit8(obj + 0x4E, 7)) wr8(obj + 0x21, 0);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(obj + 0x1C, 0);
    } else {
        wr32(obj + 0x1C, (uint32_t)-(uint32_t)((int32_t)v >> 1));
        die_slow(obj);
        QueueSound(0xE6);
    }
    die_face(obj);
    die_show_face(obj);
}

/* $04216A: rolling to a stop, its number out */
void PalaceDieRoll(uint16_t obj)
{
    for (int k = 0; k < 6; k++) die_slow(obj);
    die_walls(obj);
    wr16(0x8044, rd16(obj + 0x48));
    wr16(0x8044, (uint16_t)(rd16(0x8044) | rd16(obj + 0x4E)));
}

/* $04218A: struck: rolling again, its striker taken in (or told no) */
void PalaceDieStruck(uint16_t obj)
{
    die_tumble(obj);
    uint8_t b = rd8(obj + 0x23);
    wr8(obj + 0x23, (uint8_t)(b & ~2));
    if (b & 2) return;
    wr16(obj + O_STATE, 0x0C);
    uint16_t by = rd16(obj + 0x42);
    if (by == 0xA400) {
        die_scroll(0x041C7A);
        if (bit8(0x804F, 0)) {
            wr16(0x80D8, 0xA400);
            wr16(0x8046, 0xDE80);
            wr16(obj + 0x4E, 0x8000);
            return;
        }
        wr32(0x80C8, 0x03A38A);
    } else if (by == 0xA600) {
        die_scroll(0x041C82);
        if (bit8(0x804F, 1)) {
            wr16(0x80D8, 0xA600);
            wr16(0x8046, 0xDEE0);
            wr16(obj + 0x4E, 0x8000);
            return;
        }
        wr32(0x80C8, 0x03A36C);
    } else {
        wr16(obj + 0x4E, 0);
        die_scroll(0x041C72);
        return;
    }
    wr16(0x80C2, 0x28);
    wr16(0x80C6, 0x80);
    wr16(0x80C4, 0);
    wr16(obj + 0x4E, 0);
}

/* $041FB8 */
void PalaceDie(uint16_t obj)
{
    if (bit8(obj + 0x22, 1)) {
        wr8(obj + 0x22, (uint8_t)(rd8(obj + 0x22) & ~2));
        wr16(obj + O_STATE, 0x14);
    }
    call_object(rom32(0x041FD8 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $334: a dice palace boss hovering over its player, dropping spike pods ($338), its
 * path kept at $9000 (+$4A) for its timer ($330 in $DE80) to wind back; beaten: 177,050
 * points, shaking, sinking away in blasts ---- */

/* loc_03A79A: plane A's lines to its scroll */
static void blimp_lines(void)
{
    wr8(v_planeFlagsA, (uint8_t)(rd8(v_planeFlagsA) | 0x80));
    uint16_t v = (uint16_t)-rd16(0xA900);
    for (uint16_t a = 0xE400, k = 0; k < 28; k++, a = (uint16_t)(a + 0x20)) wr16(a, v);
}

/* $0464CA: the background's two bands scrolled on by `d1` and `d2` */
static void blimp_scroll(uint16_t d1, uint16_t d2)
{
    uint16_t v = (uint16_t)(rd16(0xE400) + d1);
    for (uint16_t a = 0xE400, k = 0; k < 14; k++, a = (uint16_t)(a + 0x20)) wr16(a, v);
    v = (uint16_t)(rd16(0xE5C0) + d2);
    for (uint16_t a = 0xE5C0, k = 0; k < 7; k++, a = (uint16_t)(a + 0x20)) wr16(a, v);
}

/* $0464F8: its colours white for 3 frames when struck */
static void blimp_flash(uint16_t obj)
{
    uint16_t a1 = 0xE300, a2 = 0xE380;
    if (!(rd16(obj + 0x58) & 0x8000)) {
        wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 1));
    } else if (bit8(obj + 0x22, 5)) {
        wr16(obj + 0x58, 2);
        uint16_t c = 0xEEE;
        for (int k = 0; k < 4; k++, a1 = (uint16_t)(a1 + 2), a2 = (uint16_t)(a2 + 2), c = (uint16_t)(c - 0x111)) {
            wr16(a1, c);
            wr16(a2, c);
        }
        wr16(0xE396, c);
        wr16(0xE316, c);
        return;
    }
    for (uint32_t t = 0x0418C2, k = 0; k < 4; k++, t += 2, a1 = (uint16_t)(a1 + 2), a2 = (uint16_t)(a2 + 2)) {
        wr16(a1, rom16(t));
        wr16(a2, rom16(t));
    }
    wr16(0xE396, 0x40);
    wr16(0xE316, 0x40);
}

/* $046484: a spike pod each $40-$7F frames */
static void blimp_drop(uint16_t obj)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if ((int16_t)rd16(obj + 0x4C) >= 0) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x338);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + 0x30));
    wr16(obj + 0x4C, (uint16_t)((rd16(v_randomSeed) & 0x3F) + 0x40));
}

/* $04640C: after its player (as fast as the difficulty says, at most $12000), kept in $80-$1C0 */
static void blimp_chase(uint16_t obj)
{
    uint32_t step = rom16(0x04647C + (uint32_t)(int16_t)rd16(0xA20A));
    uint32_t v = rd32(obj + 0x18);
    int add;
    if ((int16_t)(rd16(rd16(0x80D8) + O_X) - rd16(obj + O_X)) >= 0) {
        if ((int16_t)(rd16(obj + O_X) - 0x1C0) >= 0) add = 0;
        else if (v & 0x80000000u) add = 1;
        else if ((int32_t)(v - 0x12000) >= 0) return;
        else add = 1;
    } else {
        if ((int16_t)(rd16(obj + O_X) - 0x80) < 0) add = 1;
        else if (!(v & 0x80000000u)) add = 0;
        else if ((int32_t)(v - 0xFFFEE000u) < 0) return;
        else add = 0;
    }
    wr32(obj + 0x18, add ? v + step : v - step);
}

/* $04655E: blasts, debris ($100) every other frame */
static void blimp_debris(uint16_t obj)
{
    BlastSound8();
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0563AC);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + 0x30));
    wr32(slot + 0x1C, 0x40000);
    wr32(slot + 0x18, (uint32_t)(int32_t)(int16_t)rom16(0x0465B8 + (rd16(v_randomSeed) & 6)));
}

/* $046280: the fight ($976), its timer ($330: 70:00 at $10E,$A8) */
void BlimpStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(0x80C2, 0x10);
    wr16(0x80D6, obj);
    wr16(obj + O_VITALITY, 0x976);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xD030E818);
    wr16(obj + O_Y, 0xC0);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_FLAGS, 0xCC00);
    wr16(obj + 0x0E, 0x0300);
    wr32(obj + 0x08, 0x0F4DBA);
    wr16(obj + 0x4A, 0x9000);
    wr16(obj + 0x4C, 0x60);
    wr16(0xDE80 + O_TYPE, 0x330);
    wr16(0xDE80 + 0x48, 0x7000);
    wr16(0xDE80 + 0x4A, 0);
    wr16(0xDE80 + 0x4C, 0);
    wr16(0xDE80 + 0x50, 0x10E);
    wr16(0xDE80 + 0x52, 0xA8);
    wr8(0xF7E7, 2);
    blimp_lines();
}

/* $046300: as its timer says: still (4), wound back along its path ($C), or on: chasing,
 * dropping pods, its place and look kept */
void BlimpFly(uint16_t obj)
{
    uint16_t st = rd16(0xDE80 + O_STATE);
    if (!st) return;
    if (st == 4) { wr32(obj + 0x18, 0); return; }
    if (st == 0x0C) {
        wr16(obj + 0x4C, 0x60);
        wr32(obj + 0x18, 0);
        wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 2));
        uint16_t e = rd16(rd16(obj + 0x4A));
        wr16(obj + O_X, e & 0x1FFF);
        wr32(obj + 0x08, rom32(0x046398 + ((uint16_t)((int16_t)e >> 11) & 0x1C)));
        blimp_scroll(0xFFFF, 0xFFFE);
        return;
    }
    blimp_drop(obj);
    blimp_chase(obj);
    uint16_t f = rd16(v_gameFrames) & 0x1C;
    wr32(obj + 0x08, rom32(0x046398 + f));
    wr16(rd16(obj + 0x4A), (uint16_t)(rd16(obj + O_X) | ((uint16_t)(f << 11) & 0xE000)));
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) + 2));
    blimp_scroll(1, 2);
}

/* $0463B8: beaten: shaking $40 frames */
void BlimpShake(uint16_t obj)
{
    wr16(v_shakeSprites, 4);
    uint16_t y = rd16(obj + 0x4A);
    if (rd16(v_gameFrames) & 4) y = (uint16_t)(y + (rd16(v_gameFrames) & 4));
    wr16(obj + O_Y, y);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) >= 0) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(obj + 0x1C, 0x28000);
}

/* $0463EA: sinking away in blasts: gone above the top, the room over */
void BlimpSink(uint16_t obj)
{
    wr16(v_shakeSprites, 4);
    blimp_debris(obj);
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x1000);
    if (rd16(obj + O_Y) & 0x8000) DiceRoomEnd(obj);
}

/* $0461D2 */
void Blimp(uint16_t obj)
{
    if (rd8(0xA200)) {
        int in;
        if (rd16(0x80D8) == 0xA400) in = bit8(0xA200, 0);
        else if (rd16(0x80D8) == 0xA600) in = bit8(0xA200, 1);
        else in = 0;
        if (!in) {
            wr16(0x8052, 1);
            DiceRoomEnd(obj);
            return;
        }
    }
    blimp_flash(obj);
    if (rd16(obj + O_STATE) && (rd16(obj + O_VITALITY) & 0x8000)) {
        wr16(obj + 0x48, 0x40);
        wr16(obj + 0x4A, rd16(obj + O_Y));
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, 0);
        wr16(obj + O_STATE, 8);
        ObjectsVanishFrom(0xC9E0, 0, 0);
        AddScore(obj, 0x17705);
    }
    call_object(rom32(0x046270 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $2B8: a stalk of four parts (+$48-+$4E, the last its head: hit, vitality $D4) and
 * its eye (+$5A), swaying, aiming and firing sparks ($2AC) at its player; its height +6 off the
 * floor line; gone left of $70; its head shot: 4,600 points, an item, the parts to debris ($8C)
 * ---- */

/* a part through a pointer word that may be empty (0: sign-extended, the ROM, where a write
 * does nothing) */
static void stalk_part_remove(uint16_t part)
{
    if (!(part & 0x8000)) return;
    if (rd16(part + O_FLAGS)) wr16(part + O_FLAGS, 0x1000);
}

static void stalk_eye_remove(uint16_t obj)
{
    uint16_t eye = rd16(obj + 0x5A);
    if (eye & 0x8000) wr16(eye + O_FLAGS, 0x1000);
}

/* $04085A: it and its parts gone */
static void stalk_remove(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x1000);
    for (uint16_t a = (uint16_t)(obj + 0x48), k = 0; k < 4; k++, a = (uint16_t)(a + 2)) stalk_part_remove(rd16(a));
}

/* $0407AA */
static void stalk_part_look(uint16_t part)
{
    wr16(part + 0x0E, 0x26E4);
    wr16(part + 0x08, 0x0300);
    wr16(part + 0x0A, 0xFCF0);
}

/* $040AAA: its foot at +6 over the floor line */
static void stalk_floor(uint16_t obj)
{
    wr16(obj + O_Y, BelowCamera220(rd16(obj + 0x06)));
}

/* $0408EC: its wait by the difficulty */
static void stalk_wait(uint16_t obj)
{
    wr16(obj + 0x50, rom16(0x0408FE + (uint32_t)(int16_t)rd16(0xA20C)));
}

/* $040880: the parts up the stalk at its lean (+$56), the eye on the head */
static void stalk_place(uint16_t obj)
{
    uint16_t a = rd16(obj + 0x56), ry = 0x70, rx = 0x20;
    for (uint16_t at = (uint16_t)(obj + 0x48), k = 0; k < 4; k++, at = (uint16_t)(at + 2), ry = (uint16_t)(ry + 0x70), rx = (uint16_t)(rx + 0x20)) {
        uint16_t part = rd16(at);
        uint16_t dy = (uint16_t)((uint32_t)((int32_t)(int16_t)rom16(0x02977C + a) * (int16_t)ry) >> 16);
        uint16_t dx = (uint16_t)((uint32_t)((int32_t)(int16_t)rom16(0x0297FC + a) * (int16_t)rx) >> 16);
        wr16(part + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
        wr16(part + O_X, (uint16_t)(dx + rd16(obj + O_X)));
    }
    uint16_t head = rd16(obj + 0x4E), eye = rd16(obj + 0x5A);
    wr16(eye + O_X, rd16(head + O_X));
    wr16(eye + O_Y, (uint16_t)(rd16(head + O_Y) - 0x0C));
}

/* $040990: swaying between $160 and $1A0, a speed of 2 or 4 by chance at each end */
static void stalk_sway(uint16_t obj)
{
    if (!rd16(obj + 0x5C)) {
        if ((int16_t)(rd16(obj + 0x56) - 0x160) >= 0) {
            wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) - rd16(obj + 0x5E)) & 0x1FE));
            return;
        }
        wr16(obj + 0x5C, 2);
    } else {
        if ((int16_t)(rd16(obj + 0x56) - 0x1A0) < 0) {
            wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) + rd16(obj + 0x5E)) & 0x1FE));
            return;
        }
        wr16(obj + 0x5C, 0);
    }
    wr16(obj + 0x5E, (uint16_t)((rd16(v_randomSeed) & 2) + 2));
}

/* $04090E: the head's aim at its player */
static void stalk_aim(uint16_t obj)
{
    uint16_t p = PlayerTarget(), head = rd16(obj + 0x4E);
    int16_t dx = (int16_t)(rd16(p + O_X) - rd16(head + O_X));
    uint32_t d1 = (uint16_t)(rd16(p + O_Y) - rd16(head + O_Y));
    wr16(obj + 0x58, (uint16_t)(((int16_t)Atan2(dx, &d1) >> 7) & 0x1FE));
}

/* $04093A: a spark from the head along its aim */
static void stalk_fire(uint16_t obj)
{
    uint16_t head = rd16(obj + 0x4E), a = rd16(obj + 0x58);
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x2AC);
    wr16(slot + O_X, rd16(head + O_X));
    wr16(slot + O_Y, rd16(head + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + 4));
    wr32(slot + 0x1C, (uint32_t)(int32_t)(int16_t)rom16(0x02977C + (uint32_t)(int16_t)a) << 3);
    wr32(slot + 0x18, (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + (uint32_t)(int16_t)a) << 3);
    QueueSound(0xB6);
}

/* $0406EE: its height by its kind (+6, $407BE), its parts and eye */
void StalkStart(uint16_t obj)
{
    wr16(obj + 0x06, rom16(0x0407BE + (uint32_t)(int16_t)rd16(obj + 0x06)));
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    uint8_t size = 0xB0;
    wr8(obj + 0x20, size);
    stalk_part_look(obj);
    uint16_t part = obj;
    for (uint16_t at = (uint16_t)(obj + 0x48), k = 0; k < 4; k++, at = (uint16_t)(at + 2)) {
        uint16_t t;
        if (!SlotFreeHigh(&part, &t)) { stalk_remove(obj); return; }
        wr16(part + O_TYPE, 0x10);
        wr16(part + O_FLAGS, 0x8000);
        size = (uint8_t)(size - 4);
        wr8(part + 0x20, size);
        stalk_part_look(part);
        wr16(at, part);
    }
    wr16(obj + O_FLAGS, 0x8100);
    wr16(obj + 0x50, 0x1C);
    wr16(obj + 0x56, 0x100);
    wr16(obj + 0x5E, 6);
    wr16(part + 0x0E, 0x26C0);
    wr16(part + 0x08, 0x0F00);
    wr16(part + 0x0A, 0xF0F0);
    wr16(part + O_VITALITY, 0xD4);
    wr8(part + 0x21, 0x20);
    wr32(part + 0x28, 0xF010F010);
    uint16_t eye, t;
    if (!SlotFreeHigh(&eye, &t)) { stalk_remove(obj); return; }
    wr16(obj + 0x5A, eye);
    wr16(eye + O_TYPE, 0x10);
    wr16(eye + O_FLAGS, 0xE000);
    wr32(eye + 0x08, 0x0DFC8E);
    wr8(eye + 0x20, 0xA4);
    wr16(eye + 0x0E, 0x0300);
}

/* $0407C2: rising */
void StalkRise(uint16_t obj)
{
    stalk_floor(obj);
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if ((int16_t)rd16(obj + 0x50) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        stalk_wait(obj);
    }
    stalk_place(obj);
}

/* $0407DA: swaying, the head flashing; then aiming */
void StalkSway(uint16_t obj)
{
    stalk_floor(obj);
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if ((int16_t)rd16(obj + 0x50) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x52, 0x20);
        stalk_aim(obj);
        stalk_wait(obj);
        return;
    }
    uint16_t head = rd16(obj + 0x4E);
    wr16(head + 0x0E, rom16(0x040A6A + (rd16(v_gameFrames) & 2)));
    stalk_sway(obj);
    stalk_place(obj);
}

/* $040804: $20 frames' aim */
void StalkAim(uint16_t obj)
{
    stalk_floor(obj);
    wr16(obj + 0x52, (uint16_t)(rd16(obj + 0x52) - 1));
    if ((int16_t)rd16(obj + 0x52) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x52, 0x20);
    }
    stalk_sway(obj);
    stalk_place(obj);
}

/* $040822: firing a spark each 8 frames for $20; then swaying again */
void StalkFire(uint16_t obj)
{
    stalk_floor(obj);
    stalk_sway(obj);
    wr16(obj + 0x52, (uint16_t)(rd16(obj + 0x52) - 1));
    if ((int16_t)rd16(obj + 0x52) < 0) wr16(obj + O_STATE, 8);
    else if (!(rd16(obj + 0x52) & 7)) stalk_fire(obj);
    stalk_place(obj);
}

/* $0406B4 */
void Stalk(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x70) < 0) {
        stalk_eye_remove(obj);
        stalk_remove(obj);
        return;
    }
    uint16_t head = rd16(obj + 0x4E);
    if (head && (rd16(head + O_VITALITY) & 0x8000)) {
        QueueSound(0xBE);
        DropItem(obj);
        AddScore(obj, 0x460);
        wr16(obj + O_FLAGS, 0x1000);
        uint16_t d = rd16(obj + 0x50);
        for (uint16_t at = (uint16_t)(obj + 0x48), k = 0; k < 4; k++, at = (uint16_t)(at + 2)) {
            uint16_t part = rd16(at);
            wr16(part + O_TYPE, 0x8C);
            wr8(part + 0x21, 0);
            wr16(part + 0x48, d);
            wr16(part + 0x4A, 4);
            wr16(part + 0x4C, 0x20);
            d = (uint16_t)((d + 0x10) & 0x7C);
        }
        stalk_eye_remove(obj);
        return;
    }
    call_object(rom32(0x0406DA + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $260: the dice palace's board: the players' pieces ($DE80, $DEE0: their squares at
 * +$48 into the board at $9800) moved the die's count ($8044, struck at $DF40) square by square
 * on the planes' warped lines; landing on a room's square: its message, the room ($6: its kind;
 * the type it becomes, $41A14) warped in; a room done: warped back to the board ---- */

#define BOARD 0x9800

static uint16_t board_square(uint16_t piece)
{
    return rd16((uint16_t)(BOARD + rd16(piece + 0x48)));
}

/* $041052: the board set out ($410E2 to the goal's $6205), the pieces at the start */
static void board_init(uint16_t obj)
{
    for (uint16_t a = 0x9000, k = 0; k <= 0x200; k++, a = (uint16_t)(a + 4)) wr32(a, 0);
    wr8(v_planeFlagsA, 0xC0);
    wr8(v_planeFlagsB, 0xC0);
    wr16(0x8044, 0);
    wr16(0x8048, 0);
    wr16(0x804E, 3);
    wr16(0x804A, 0);
    wr16(0x804C, 0);
    uint32_t t = 0x0410E2;
    uint16_t a = BOARD;
    for (;;) {
        uint16_t w = rom16(t);
        wr16(a, w);
        a = (uint16_t)(a + 2);
        t += 2;
        if (w == 0x6205) break;
    }
    wr16(0x8014, 0xFFF8);
    wr16(0x8016, 0xFFF8);
    wr16(0x8018, 0xFFF8);
    wr16(0x801A, 0xFFF8);
    wr16(0x800A, 0xE000);
    wr16(0x8002, 0xF000);
    wr16(obj + 0x06, 0);
    for (uint16_t r = 0x801C; r <= 0x8022; r = (uint16_t)(r + 2)) wr16(r, 0);
    wr16(0x8052, 0);
    wr16(0x8050, 0);
    wr16(0x8040, 0);
    wr16(0x8042, 0);
}

/* $041672: the room's tiles patched ($417AA by the room: a place, words to $8000) twice, $80
 * apart */
static void board_room_tiles(uint16_t obj)
{
    uint32_t t = rom32(0x0417AA + (uint32_t)(int16_t)(uint16_t)(rd16(obj + 0x06) << 1));
    uint16_t a1 = rom16(t), a2 = (uint16_t)(a1 + 0x80);
    t += 2;
    for (;;) {
        uint16_t w = rom16(t);
        t += 2;
        if (w & 0x8000) return;
        wr16(a1, w); a1 = (uint16_t)(a1 + 2);
        wr16(a2, w); a2 = (uint16_t)(a2 + 2);
    }
}

/* $041612: the room's tiles; its colours ($4169A: the lines of $416AA and $4172A) */
static void board_room_colours(uint16_t obj)
{
    board_room_tiles(obj);
    uint16_t r = rd16(obj + 0x06);
    if (!r) return;
    uint8_t b = rom8(0x04169A + (uint32_t)(uint16_t)((int16_t)r >> 1));
    uint32_t a0 = 0x04172A + (uint32_t)(uint16_t)((b & 0x0E) << 3);
    uint32_t a1 = 0x0416AA + (uint32_t)(uint16_t)((b & 0xE0) >> 1);
    uint16_t p = 0xE320, q = 0xE3A0;
    for (int k = 0; k < 8; k++, a1 += 2, p = (uint16_t)(p + 2), q = (uint16_t)(q + 2)) {
        wr16(p, rom16(a1));
        wr16(q, rom16(a1));
    }
    for (int k = 0; k < 8; k++, a0 += 2, p = (uint16_t)(p + 2), q = (uint16_t)(q + 2)) {
        wr16(p, rom16(a0));
        wr16(q, rom16(a0));
    }
}

/* $041250: the moving piece's player placed at the room's start ($4128A) */
static void board_player_place(uint16_t obj)
{
    uint32_t t = 0x04128A + (uint32_t)(int16_t)(uint16_t)(rd16(obj + 0x06) << 1);
    uint16_t x = rom16(t), y = rom16(t + 2);
    if (rd16(0x8046) == 0xDE80) Player1Place(x, y, 0);
    else Player2Place(x, y, 0);
}

/* $041B2C: the warp's lines cleared */
static void board_warp_clear(void)
{
    for (uint16_t a = 0xE460, k = 0; k < 200; k++, a = (uint16_t)(a + 4)) wr32(a, 0);
    wr16(0x801C, 0);
    wr16(0x801E, 0);
}

/* $041B1C */
static void board_warp_off(void)
{
    wr8(0xF7E7, 7);
    board_warp_clear();
}

/* $041B26 */
static void board_warp_on(void)
{
    wr8(0xF7E7, 3);
    board_warp_clear();
}

/* $041AAA: plane A's lines rippling */
static void board_ripple_a(void)
{
    uint16_t d0 = rd16(0x801C) & 0x1FE, d1 = rd16(0x801E);
    for (uint16_t a = 0xE462, k = 0; k < 0xA8; k++, a = (uint16_t)(a + 4)) {
        uint16_t s = rom16(0x02977C + d0);
        d0 = (uint16_t)((d0 + 0x0C) & 0x1FE);
        wr16(a, (uint16_t)((uint32_t)((int32_t)(int16_t)s * (int16_t)d1) >> 16));
    }
}

/* $041AE2: the columns rippling ($EC02) */
static void board_ripple_columns(void)
{
    uint16_t d0 = rd16(0x8020) & 0x1FE, d1 = rd16(0x8022);
    for (uint16_t a = 0xEC02, k = 0; k < 20; k++, a = (uint16_t)(a + 4)) {
        uint16_t s = rom16(0x02977C + d0);
        d0 = (uint16_t)((d0 + 0x64) & 0x1FE);
        wr16(a, (uint16_t)((uint32_t)((int32_t)(int16_t)s * (int16_t)d1) >> 16));
    }
}

/* $041B4E: the board's gentle ripple */
static void board_ripple(void)
{
    wr16(0x801C, (uint16_t)(rd16(0x801C) + 8));
    wr16(0x801E, 8);
    board_ripple_a();
    wr16(0x8020, (uint16_t)(rd16(0x8020) + 8));
    wr16(0x8022, 8);
    board_ripple_columns();
}

/* $041A68: the warp: every line out by the sine, alternate lines the other way */
static void board_warp(void)
{
    uint16_t d0 = rd16(0x801C) & 0x1FE, d1 = rd16(0x801E);
    uint16_t a = 0xE460;
    for (int d7 = 0xC7; d7 >= 0; d7--) {
        uint16_t s = rom16(0x02977C + d0);
        d0 = (uint16_t)((d0 + 8) & 0x1F8);
        uint16_t v = (uint16_t)((uint32_t)((int32_t)(int16_t)s * (int16_t)d1) >> 16);
        if (d7 & 1) v = (uint16_t)-v;
        wr16(a, v);
        wr16((uint16_t)(a + 2), v);
        a = (uint16_t)(a + 4);
    }
}

/* $041B6E: the board's colour cycling */
static void board_cycle(void)
{
    wr16(0xE332, rom16(0x041B86 + (((uint16_t)((int16_t)rd16(v_gameFrames) >> 2)) & 0x0E)));
}

/* $041C92: the square's place on the screen into $804A, $804C */
static void board_target(uint16_t piece)
{
    uint16_t sq = board_square(piece);
    wr16(0x804A, (uint16_t)(((sq & 0x1E00) >> 4) + 0x90));
    wr16(0x804C, (uint16_t)((uint16_t)((int16_t)(sq & 0xE000) >> 8) + 0xB0));
}

/* $041C1E: the die ($27C in $DF40) with the count +$8044 */
static void board_die(void)
{
    wr16(0xDF40 + O_TYPE, 0x27C);
    wr16(0xDF40 + O_STATE, 0);
    wr16(0xDF40 + O_FLAGS, 0);
    wr16(0xDF40 + O_Y, 0);
    wr32(0xDF40 + 0x18, 0);
    wr32(0xDF40 + 0x1C, 0);
    wr16(0xDF40 + 0x48, rd16(0x8044));
}

/* $041B96: the pieces ($320) on their squares, then the die */
static void board_pieces(void)
{
    static const uint16_t piece[2] = { 0xDE80, 0xDEE0 }, look[2] = { 0x035B, 0x235B }, at[2] = { 0x8040, 0x8042 };
    for (int k = 0; k < 2; k++) {
        uint16_t p = piece[k];
        wr16(p + O_TYPE, 0x320);
        wr16(p + 0x0E, look[k]);
        wr16(p + O_FLAGS, 0x6C00);
        wr32(p + 0x08, 0x0E21B4);
        wr16(p + 0x0C, 0);
        wr16(p + 0x48, rd16(at[k]));
        wr16(p + O_Y, 0);
        wr8(p + 0x20, 0x90);
        board_target(p);
        wr16(p + O_X, rd16(0x804A));
        wr16(p + O_Y, rd16(0x804C));
    }
    board_die();
}

/* $041C8A: the moving piece a square on */
static void board_step(void)
{
    uint16_t p = rd16(0x8046);
    wr16(p + 0x48, (uint16_t)(rd16(p + 0x48) + 2));
    board_target(p);
}

/* $041CBE: the other piece sliding to its square's column */
static void board_other_slide(void)
{
    uint16_t p = rd16(0x8046) == 0xDE80 ? 0xDEE0 : 0xDE80;
    uint16_t d = (uint16_t)(((board_square(p) & 0x1E00) >> 4) + 0x90 - rd16(p + O_X));
    if (!d) return;
    wr16(p + O_X, (uint16_t)(rd16(p + O_X) + ((int16_t)d < 0 ? 0xFFFE : 2)));
}

/* $041D02: two players on one square: the pieces apart */
static void board_apart(void)
{
    if (rd8(0xA200) != 3 || rd16(0x8040) != rd16(0x8042)) return;
    if ((int16_t)((uint16_t)(rd16(0xDEF0) - rd16(0xDE90)) - 0x14) >= 0) return;
    wr16(0xDE90, (uint16_t)(rd16(0xDE90) - 1));
    wr16(0xDEF0, (uint16_t)(rd16(0xDEF0) + 1));
}

/* $041D32: the moving piece 4 nearer its target: 0 when there (+$E's bit 11 its facing) */
static uint16_t board_move(void)
{
    uint16_t p = rd16(0x8046), d1 = 3, d2 = 0;
    uint16_t d = (uint16_t)(rd16(p + O_X) - rd16(0x804A));
    if ((int16_t)d < 0) {
        if ((int16_t)(d - 0xFFFD) >= 0) goto x_there;
        wr16(p + O_X, (uint16_t)(rd16(p + O_X) + 4));
    } else {
        if ((int16_t)(d - 5) < 0) goto x_there;
        wr16(p + O_X, (uint16_t)(rd16(p + O_X) - 4));
        d2 |= 0x800;
    }
    goto y;
x_there:
    wr16(p + O_X, rd16(0x804A));
    d1 &= ~1;
y:
    d = (uint16_t)(rd16(p + O_Y) - rd16(0x804C));
    if ((int16_t)d < 0) {
        if ((int16_t)(d - 0xFFFD) >= 0) goto y_there;
        wr16(p + O_Y, (uint16_t)(rd16(p + O_Y) + 4));
    } else {
        if ((int16_t)(d - 5) < 0) goto y_there;
        wr16(p + O_Y, (uint16_t)(rd16(p + O_Y) - 4));
    }
    goto done;
y_there:
    wr16(p + O_Y, rd16(0x804C));
    d1 &= ~2;
done:
    wr16(p + 0x0E, (uint16_t)(rd16(p + 0x0E) | d2));
    return d1;
}

/* $041DBC: whose turn next ($804E: 1 player 1, 2 player 2, 3 either) by the rooms done ($8050) */
static void board_turn(void)
{
    if (rd8(0xA200) != 3 || !rd8(0x8050)) { wr16(0x804E, 3); return; }
    if (rd8(0x8050) == 3) {
        wr8(0x8050, 0);
        wr16(0x804E, 3);
        return;
    }
    wr16(0x804E, bit8(0x8050, 0) ? 2 : 1);
}

/* $041E90: the squares of rooms done marked on the board's map ($6040) */
static void board_marks(void)
{
    uint16_t i = 0, kind;
    do {
        uint16_t w = rd16((uint16_t)(BOARD + i));
        kind = w & 0x1F;
        if (!kind) {
            uint8_t m = (uint8_t)rom16(0x041EFC + (uint32_t)(int16_t)(uint16_t)((w & 0x180) >> 6));
            uint16_t y = (uint16_t)((uint16_t)((int16_t)(uint16_t)((int16_t)(w & 0xE000) >> 8) >> 2) + 8);
            uint16_t x = (uint16_t)((uint16_t)((int16_t)(w & 0x1E00) >> 8) >> 1);
            if (x & 8) x = (uint16_t)((x & 7) + 0x40);
            wr8((uint16_t)(0x6040 + (y | x)), m);
        }
        i = (uint16_t)(i + 2);
    } while (kind != 5);
}

/* $04112A: the camera and planes at the room (the board's marks first, from the board) */
static void board_room_view(uint16_t obj)
{
    if (!rd16(obj + 0x06)) board_marks();
    uint32_t t = rom32(0x04116E + (uint32_t)(int16_t)(uint16_t)(rd16(obj + 0x06) << 1));
    CameraPlace(rom16(t), rom16(t + 2), rom16(t + 4), rom16(t + 6));
    PlaneAFillAhead();
    PlaneBFillAhead();
    wr8(v_cameraHold, 0x80);
}

/* $0413F4: the room's graphics' commands ($41482's extra, then $41436's) */
static void board_room_commands(uint16_t obj)
{
    uint8_t c = rom8(0x041482 + (uint32_t)(uint16_t)((int16_t)rd16(obj + 0x06) >> 1));
    if (c != 0xFF) RunCommands(rom32(0x041476 + c));
    RunCommands(rom32(0x041436 + (uint32_t)(int16_t)(uint16_t)(rd16(obj + 0x06) << 1)));
}

/* $04111A */
static void board_room_load(uint16_t obj)
{
    board_room_commands(obj);
    board_room_view(obj);
    board_room_colours(obj);
}

/* $0412FC: its player bursting into sparkles ($30C) */
static void board_burst(void)
{
    uint16_t p = rd16(0x80D8), a = 0;
    for (int k = 0; k < 16; k++, a = (uint16_t)(a + 0x20)) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x30C);
        wr32(slot + 0x48, 0x05668C);
        wr16(slot + O_FLAGS, 0x8C00);
        wr16(slot + 0x4E, 0x30);
        wr16(slot + O_X, rd16(p + O_X));
        wr16(slot + O_Y, rd16(p + O_Y));
        uint32_t vy = (uint32_t)(int32_t)(int16_t)rom16(0x02977C + a) << 4;
        uint32_t vx = (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + a) << 4;
        wr32(slot + 0x1C, vy);
        wr32(slot + 0x18, vx);
        wr32(slot + 0x5C, ~(uint32_t)((int32_t)vy >> 6));
        wr32(slot + 0x58, ~(uint32_t)((int32_t)vx >> 6));
    }
}

/* $0412CA: leaving: the player (alive, from a room) bursting, the players to the board */
static void board_leave(uint16_t obj)
{
    if (rd16(obj + O_STATE) == 0x48) {
        int16_t v = (int16_t)rd16(rd16(0x80D8) + O_VITALITY);
        if (v <= 0) return;
    }
    board_burst();
    PlayersPlace(0, 0, 0x70);
}

/* $041A34: the room's sound */
static void board_room_sound(uint16_t obj)
{
    uint8_t s = rom8(0x041A58 + (uint32_t)(uint16_t)((int16_t)rd16(obj + 0x06) >> 1));
    if (rd16(obj + 0x06) != 0x0A) QueueSound(s);
}

/* $04194A: the room's message */
static void board_room_message(uint16_t obj)
{
    wr16(0x80C2, rd16(obj + 0x06) == 0x0A ? 0x4C : 8);
    uint32_t t = bit8(0xFF26, 7) ? 0x0419C4 : 0x041984;
    wr32(0x80C8, rom32(t + (uint32_t)(int16_t)(uint16_t)(rd16(obj + 0x06) << 1)));
}

/* $0413D0: a player's arrival sparkle ($2F8) at x, y */
static void board_arrival(uint16_t x, uint16_t y)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x2F8);
    wr16(slot + 0x48, x);
    wr16(slot + 0x4A, y);
    wr16(slot + 0x52, 2);
    wr16(slot + 0x54, 3);
}

/* $0413BA */
static void board_arrival_room(uint16_t obj)
{
    uint32_t t = 0x04128A + (uint32_t)(int16_t)(uint16_t)(rd16(obj + 0x06) << 1);
    board_arrival(rom16(t), rom16(t + 2));
}

/* $04137A: on the board: a sparkle for each player in the game, alternate frames */
static void board_arrival_board(void)
{
    if (bit8(0xA200, 0) && bit8(v_gameFrames + 1, 0)) { board_arrival(0xB0, 0x138); return; }
    if (bit8(0xA200, 1) && !bit8(v_gameFrames + 1, 0)) board_arrival(0x190, 0x138);
}

/* $04120E: the players at the board's start */
static void board_players_start(void)
{
    if (bit8(0xA200, 0)) Player1Place(0xB0, 0x138, 0);
    if (bit8(0xA200, 1)) Player2Place(0x190, 0x138, 0);
}

static void board_fade(int16_t by)
{
    for (uint16_t a = 0x8014; a <= 0x801A; a = (uint16_t)(a + 2)) wr16(a, (uint16_t)(rd16(a) + by));
}

/* $040B2E */
void DiceBoardStart(uint16_t obj)
{
    DiceRoomClear();
    board_init(obj);
    board_room_colours(obj);
    board_player_place(obj);
    board_warp_off();
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(0x80C2, 8);
    wr32(0x80C8, 0x0399C4);
}

/* $040B5C */
void DiceBoardShow(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    board_ripple();
}

/* $040B66: the message gone: the pieces and the die */
void DiceBoardBegin(uint16_t obj)
{
    if (!rd16(0x80C2)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        board_pieces();
    }
    board_cycle();
    board_ripple();
}

/* $040B80: waiting for the die's count (+$8044 bit 15) */
void DiceBoardWait(uint16_t obj)
{
    if (bit8(0x8044, 7)) wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    board_apart();
    board_cycle();
    board_ripple();
}

/* $040B9A: the count taken: the piece hopping */
void DiceBoardRolled(uint16_t obj)
{
    board_cycle();
    board_ripple();
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x48, 0x20);
    wr8(0x8044, (uint8_t)(rd8(0x8044) & ~0x80));
    uint16_t p = rd16(0x8046);
    wr32(p + 0x08, 0x0E2228);
    wr16(p + 0x0C, 0);
    wr16(0x8048, rd16(0x8044));
}

/* $040BCC: a pause, then a square on */
void DiceBoardPause(uint16_t obj)
{
    board_cycle();
    board_ripple();
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) >= 0) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    board_step();
}

/* $040C7A: landed on a room: its message ($6: the room) */
static void board_landed(uint16_t obj, uint16_t piece, uint16_t kind)
{
    wr16(0x80C2, 0x28);
    wr16(0x80C6, 0x80);
    wr16(0x80C4, 8);
    wr16(obj + 0x06, (uint16_t)(kind << 1));
    wr16(obj + O_STATE, 0x34);
    piece = rd16(0x8046);
    wr32(piece + 0x08, 0x0E21B4);
    wr16(piece + 0x0C, 0);
    wr8(0xA4C0, (uint8_t)(rd8(0xA4C0) | 0x80));
    wr8(0xA6C0, (uint8_t)(rd8(0xA6C0) | 0x80));
    wr16(piece == 0xDE80 ? 0x8040 : 0x8042, rd16(piece + 0x48));
}

/* $040BEA: moving: there, the count used up (or the goal): the square's room */
void DiceBoardMove(uint16_t obj)
{
    board_cycle();
    board_ripple();
    board_other_slide();
    uint16_t p = rd16(0x8046);
    if (board_move()) return;
    QueueSound(0xD0);
    uint16_t sq = board_square(p), kind = sq;
    if ((sq & 0x1F) == 5) {
        kind &= 0x1F;
        wr32(0x80C8, 0x03A41D);
        board_landed(obj, p, kind);
        return;
    }
    wr16(0x8048, (uint16_t)(rd16(0x8048) - 2));
    if ((int16_t)rd16(0x8048) >= 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
        wr16(obj + 0x48, 0x20);
        return;
    }
    if (kind & 0x20) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(p + 0x08, 0x0E21B4);
        wr16(p + 0x0C, 0);
        if (p == 0xDE80) {
            wr16(0x8040, rd16(p + 0x48));
            wr16(0x804E, 5);
        } else {
            wr16(0x8042, rd16(p + 0x48));
            wr16(0x804E, 6);
        }
        return;
    }
    if (kind & 0x40) {
        wr16(obj + O_STATE, 0x24);
        wr32(0x80C8, 0x03A411);
        wr16(0x80C2, 0x28);
        wr16(0x80C6, 0x80);
        wr16(0x80C4, 8);
        wr32(p + 0x08, 0x0E21E4);
        wr16(p + 0x0C, 0);
        wr16(p == 0xDE80 ? 0x8040 : 0x8042, rd16(p + 0x48));
        return;
    }
    kind &= 0x1F;
    wr32(0x80C8, kind == 6 ? 0x03A3FF : 0x03A3EF);
    board_landed(obj, p, kind);
}

/* $040D32: a turn again (the die back) */
void DiceBoardAgain(uint16_t obj)
{
    board_cycle();
    board_ripple();
    wr16(obj + O_STATE, 0x0C);
    wr16(0x8044, 0);
    wr16(0x8048, 0);
    board_die();
}

/* $040D52: back on the board: the next turn */
void DiceBoardReturn(uint16_t obj)
{
    board_warp_off();
    wr16(obj + O_STATE, 0x0C);
    wr16(0x8044, 0);
    wr16(0x8048, 0);
    board_turn();
    board_pieces();
    wr8(0xA4C0, (uint8_t)(rd8(0xA4C0) & ~0x80));
    wr8(0xA6C0, (uint8_t)(rd8(0xA6C0) & ~0x80));
}

/* $040D7E: a fall: the screen to black, the other piece hidden */
void DiceBoardFall(uint16_t obj)
{
    board_ripple();
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x48, 0x80);
    for (uint16_t a = 0x8014; a <= 0x801A; a = (uint16_t)(a + 2)) wr16(a, 0);
    wr16(0xDF42, 0x1000);
    wr16(rd16(0x8046) == 0xDE80 ? 0xDEF4 : 0xDE94, 0);
}

/* $040DC4: fading two colours down; then back to the start (its square marked) */
void DiceBoardFallen(uint16_t obj)
{
    board_ripple();
    if (!(rd16(v_gameFrames) & 1) && (int16_t)(rd16(0x8016) - 0xFFF9) >= 0) {
        wr16(0x8014, (uint16_t)(rd16(0x8014) - 1));
        wr16(0x8016, (uint16_t)(rd16(0x8016) - 1));
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        DiceRoomMark();
        board_room_view(obj);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(0x804A, 0xB0);
        wr16(0x804C, 0xD0);
        uint16_t p = rd16(0x8046);
        wr32(p + 0x08, 0x0E224E);
        wr16(p + 0x0C, 0);
    }
    AdjustPalettes();
    wr16(0xE30A, 0xEEE);
    wr16(0xE38A, 0xEEE);
}

/* $040E2E: the piece to the start */
void DiceBoardToStart(uint16_t obj)
{
    uint16_t p = rd16(0x8046);
    if (board_move()) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(p + 0x08, 0x0E21B4);
    wr16(p + 0x0C, 0);
}

/* $040E4E: the colours back; then the next turn */
void DiceBoardFadeUp(uint16_t obj)
{
    board_ripple();
    if (!(rd16(v_gameFrames) & 1)) {
        wr16(0x8014, (uint16_t)(rd16(0x8014) + 1));
        wr16(0x8016, (uint16_t)(rd16(0x8016) + 1));
        if (!(rd16(0x8016) & 0x8000)) {
            wr16(0x8044, 0);
            wr16(0x8048, 0);
            wr16(obj + O_STATE, 0x0C);
            board_turn();
            wr16(rd16(0x8046) == 0xDE80 ? 0x8040 : 0x8042, 0);
            board_pieces();
        }
    }
    AdjustPalettes();
}

/* $040E8E: into a room (or out of one): the warp begins */
void DiceBoardWarp(uint16_t obj)
{
    wr16(0x8080, 0);
    EffectsClear();
    board_leave(obj);
    wr16(obj + O_STATE, 0x38);
    wr16(obj + 0x4E, 0x80);
    for (uint16_t a = 0x8014; a <= 0x801A; a = (uint16_t)(a + 2)) wr16(a, 0);
    wr16(0x800A, 0xE000);
    wr16(0x8002, 0xF000);
    board_warp_on();
    QueueSound(0xBB);
}

/* $040EDC: warping, fading out after $80 frames */
void DiceBoardWarping(uint16_t obj)
{
    wr16(0x801C, (uint16_t)(rd16(0x801C) + 8));
    wr16(0x801E, (uint16_t)(rd16(0x801E) + 0x10));
    board_warp();
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) < 0 && !(rd16(v_gameFrames) & 3)) {
        board_fade(1);
        if ((int16_t)(rd16(0x8014) - 8) >= 0) wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    }
    AdjustPalettes();
}

/* $040F26: dark: the objects gone, the room's graphics loaded */
void DiceBoardLoad(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x4E, 0x40);
    ObjectsVanishFrom(0xC9E0, 0, 0);
    board_room_load(obj);
    AdjustPalettes();
}

/* $040F48: after $40: its sound and message, cleared to become the room */
void DiceBoardEnter(uint16_t obj)
{
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) >= 0) return;
    board_room_sound(obj);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x08, 0);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x0E, 0);
    wr32(obj + O_X, 0);
    wr32(obj + O_Y, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_VITALITY, 0);
    wr16(obj + 0x26, 0);
    for (uint16_t o = 0x48; o <= 0x5C; o = (uint16_t)(o + 4)) wr32(obj + o, 0);
    board_room_message(obj);
    wr16(0x801C, (uint16_t)(rd16(0x801C) + 8));
    wr16(0x801E, (uint16_t)(rd16(0x801E) + 0x10));
    board_warp();
    AdjustPalettes();
}

/* $040FBC: fading in, the warp settling: then the room (its type, $41A14) or the board */
void DiceBoardArrive(uint16_t obj)
{
    if (rd16(0x8014) && !(rd16(v_gameFrames) & 3)) {
        board_fade(-1);
        AdjustPalettes();
    }
    if (rd16(obj + 0x06)) board_arrival_room(obj);
    else board_arrival_board();
    wr16(0x801C, (uint16_t)(rd16(0x801C) - 8));
    wr16(0x801E, (uint16_t)(rd16(0x801E) - 0x10));
    board_warp();
    if (rd16(0x801C)) return;
    DiceRoomClear();
    wr8(0xF7E7, 0);
    wr16(obj + O_TYPE, rom16(0x041A14 + (uint32_t)(int16_t)rd16(obj + 0x06)));
    if (!rd16(obj + 0x06)) {
        wr16(obj + O_STATE, 0x20);
        board_players_start();
        return;
    }
    wr16(obj + O_STATE, 0);
    board_player_place(obj);
}

/* $040AD8 */
void DiceBoard(uint16_t obj)
{
    call_object(rom32(0x040AE2 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

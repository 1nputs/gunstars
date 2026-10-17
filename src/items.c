/* Types $58-$6C: the items (the weapons and vitality the players pick up). One falls to the
 * floor and rests there until a player touches it (+$23 bit 3 cleared); then it is removed, but
 * $68 becomes $568. In the mine (v_stage4 4) it drifts with the cart; in space ($14, but not from
 * a checkpoint) it only waits. */
#include "game.h"

#define I_TYPE_SEEN 0x48         /* the type its look was set for */

/* $01A5E4. Its mappings and sprite by its type (once), the palette at $FF808A. */
void ItemLook(uint16_t obj)
{
    uint16_t t = rd16(obj + O_TYPE);
    if (t != rd16(obj + I_TYPE_SEEN)) {
        wr16(obj + I_TYPE_SEEN, t);
        uint32_t a = rom32(0x01A5B8 + (uint32_t)(int16_t)t);
        wr32(obj + 0x08, bus_rd32(a));
        wr16(obj + 0x0E, bus_rd16(a + 4));
    }
    wr16(obj + 0x0E, (uint16_t)((rd16(obj + 0x0E) & 0x7FFF) | rd16(0x808A)));
}

/* collected: removed, or $68 turned into $568 */
static void collected(uint16_t obj)
{
    if (rd16(obj + O_TYPE) == 0x68) {
        wr16(obj + O_TYPE, 0x568);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        return;
    }
    set8(obj + O_FLAGS, 4);
}

/* a floor (an attribute from $20) or the bounds' bottom 4 or 12 below: the item put on it (the
 * snapped y less 8); 0 when there is neither */
static int land(uint16_t obj)
{
    uint16_t x = rd16(obj + O_X), y = (uint16_t)(rd16(obj + O_Y) + 4), a;
    for (int k = 0; k < 2; k++) {
        a = MapAttrHigh(x, y);
        if (a) {
            MapSnap(&x, &y, &a);
            break;
        }
        if (MapBoundsY(x, &y)) break;
        if (k == 1) return 0;
        y = (uint16_t)(y + 8);
    }
    wr16(obj + O_Y, (uint16_t)(y - 8));
    wr16(obj + O_Y + 2, 0);
    return 1;
}

/* resting on something that carries it (+$22 bit 3, +$47 bit 1) */
static int carried(uint16_t obj) { return bit8(obj + 0x22, 3) && bit8(obj + 0x47, 1); }

/* falling, faster to 7 a frame; 1 when it has landed */
static int fall(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if ((int32_t)rd32(obj + 0x1C) >= 0x70000) wr32(obj + 0x1C, 0x70000);
    if (rd16(obj + 0x1C) & 0x8000) return 0;
    if (!carried(obj)) {
        if (!land(obj)) return 0;
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
    }
    return 1;
}

static void touchable(uint16_t obj, uint8_t size)
{
    wr8(obj + 0x21, size);
    set8(obj + 0x23, 3);
    wr32(obj + 0x28, 0xF808F808);
    wr32(obj + 0x2C, 0xF808F808);
    wr16(obj + 0x26, 0);
    wr16(obj + O_STATE, 4);
}

/* $01A2D0. Step 0: touchable, then falling. */
void ItemStart(uint16_t obj)
{
    touchable(obj, 8);
    ItemFall(obj);
}

/* $01A2F6 */
void ItemFall(uint16_t obj)
{
    if (!fall(obj)) return;
    wr8(obj + 0x21, 0x18);
    wr16(obj + O_FLAGS, 0x8F00);
    wr16(obj + O_STATE, 8);
    ItemRest(obj);
}

/* $01A392. Resting; the floor gone from under it, falling again. */
void ItemRest(uint16_t obj)
{
    if (!bit8(obj + 0x23, 3)) { collected(obj); return; }
    if (carried(obj)) return;
    if (!land(obj)) { ItemStart(obj); return; }
    wr32(obj + 0x1C, 0);
}

/* $01A5C0. Its x speed toward the camera's (by $800 a frame). */
void ItemDrift(uint16_t obj)
{
    int32_t cam = (int32_t)rd32(v_camSpeedX), v = (int32_t)rd32(obj + 0x18);
    if (cam == v) return;
    if (cam < v) wr32(obj + 0x18, (uint32_t)v + 0x800);
    else wr32(obj + 0x18, (uint32_t)v - 0x800);
}

/* $01A41C. The mine's: the same, drifting. */
void ItemMineStart(uint16_t obj)
{
    touchable(obj, 8);
    ItemMineFall(obj);
}

/* $01A442 */
void ItemMineFall(uint16_t obj)
{
    if (!fall(obj)) return;
    wr8(obj + 0x21, 0x18);
    wr16(obj + O_STATE, 8);
    ItemMineRest(obj);
}

/* $01A4D8 */
void ItemMineRest(uint16_t obj)
{
    if (!bit8(obj + 0x23, 3)) { collected(obj); return; }
    ItemDrift(obj);
    if (carried(obj)) return;
    if (!land(obj)) { ItemMineStart(obj); return; }
    wr32(obj + 0x1C, 0);
}

/* $01A56A. Space's: shown, touchable, waiting. */
void ItemSpaceStart(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8F00);
    touchable(obj, 0x10);
    ItemSpaceRest(obj);
}

/* $01A596 */
void ItemSpaceRest(uint16_t obj)
{
    if (!bit8(obj + 0x23, 3)) collected(obj);
}

/* $01A2A2. Types $58-$6C. */
void Item(uint16_t obj)
{
    uint16_t s = rd16(v_stage4);
    uint32_t table = 0x01A2C4;
    if (s == 4) table = 0x01A410;
    else if (s == 0x14 && !rd16(v_checkpoint)) table = 0x01A562;
    ItemLook(obj);
    call_object(rom32(table + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $144: an item carrier. It flies across, bobbing; a player under it (or its touch,
 * by player) makes it drop an item (+$4C: $68, else one of $58-$64 at random) and pause, a hit
 * from below throws it ahead first; shot down it falls and becomes type $70, dropping one. ---- */

#define C_DROPPED 0x4A           /* bits 0, 1: a drop already made for player 1, 2 */
#define C_WAIT    0x48
#define C_BOB     0x50           /* long: the bob's sum (its sign turns it) */
#define C_FROM_X  0x4E

/* $01A8CE. Moving right at 1 (flipped), or left by +6's sign. */
void CarrierFace(uint16_t obj)
{
    if (rd16(obj + 6) & 0x8000) {
        wr32(obj + 0x18, 0xFFFF0000);
        clr8(obj + 0x0E, 3);
        return;
    }
    wr32(obj + 0x18, 0x10000);
    set8(obj + 0x0E, 3);
}

/* $01A8F6. The item it carries, into `slot` at it (4 to the right), resting. */
void CarrierDrop(uint16_t obj, uint16_t slot)
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
    wr32(slot + 0x18, 0);
    wr32(slot + 0x1C, 0);
    wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + 4));
    wr16(slot + O_FLAGS, 0x8F00);
    wr16(slot + 6, 0);
}

/* $01A836. The bob: its speed turned back toward the middle by $6000 a frame. */
void CarrierBob(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C);
    wr32(obj + C_BOB, rd32(obj + C_BOB) + v);
    if (rd32(obj + C_BOB) & 0x80000000u) wr32(obj + 0x1C, v + 0x6000);
    else wr32(obj + 0x1C, v - 0x6000);
}

/* $01A6C8: flying again (its first mappings), then the flying frame */
static void carrier_again(uint16_t obj)
{
    wr32(obj + 0x08, 0x0E1000);
    wr16(obj + 0x0C, 0);
    CarrierFace(obj);
    wr8(obj + 0x22, 0);
    wr16(obj + O_STATE, 4);
    CarrierFly(obj);
}

/* $01A806: a drop, stopped for $10 frames (no free slot: flying on) */
static void drop_still(uint16_t obj)
{
    wr32(obj + 0x08, 0x0E108C);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    uint16_t slot;
    if (!SlotFreeLow(&slot)) { carrier_again(obj); return; }
    CarrierDrop(obj, slot);
    wr16(obj + C_WAIT, 0x10);
    wr16(obj + O_STATE, 0xC);
    CarrierWait(obj);
}

/* $01A854: thrown ahead at 4 by a hit from below, a drop */
static void drop_thrown(uint16_t obj)
{
    if (bit8(obj + 0x23, 7)) set8(obj + C_DROPPED, 0);
    if (bit8(obj + 0x23, 6)) set8(obj + C_DROPPED, 1);
    wr32(obj + 0x08, 0x0E108C);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0x40000);
    wr16(obj + C_FROM_X, rd16(obj + O_X));
    uint16_t slot;
    if (!SlotFreeLow(&slot)) { carrier_again(obj); return; }
    CarrierDrop(obj, slot);
    wr16(obj + C_WAIT, 0x10);
    wr16(obj + O_STATE, 0x10);
    CarrierThrown(obj);
}

/* a player within 8 of its x, once each */
static int under(uint16_t obj, uint16_t player, int bit)
{
    uint16_t d = (uint16_t)(rd16(player + O_X) - rd16(obj + O_X) + 8);
    if (d >= 0x10) return 0;
    uint8_t b = rd8(obj + C_DROPPED);
    wr8(obj + C_DROPPED, (uint8_t)(b | (1u << bit)));
    return !(b & (1u << bit));
}

/* $01A6E2. Flying: shot down (+$22 bit 7) it falls; touched (bit 5) by a player who has not had
 * a drop (or hit from below, +$23 bit 4), or with a player under it, it drops. */
void CarrierFly(uint16_t obj)
{
    uint8_t h = rd8(obj + 0x22);
    wr8(obj + 0x22, (uint8_t)(h & 0x7F));
    if (h & 0x80) {
        wr16(obj + O_STATE, 8);
        CarrierFall(obj);
        return;
    }
    h = rd8(obj + 0x22);
    wr8(obj + 0x22, (uint8_t)(h & ~0x20));
    if (h & 0x20) {
        if (bit8(obj + 0x23, 4)) { drop_thrown(obj); return; }
        uint8_t b;
        if (bit8(obj + 0x23, 7)) {
            b = rd8(obj + C_DROPPED);
            wr8(obj + C_DROPPED, (uint8_t)(b | 1));
            if (!(b & 1)) { drop_still(obj); return; }
        }
        if (bit8(obj + 0x23, 6)) {
            b = rd8(obj + C_DROPPED);
            wr8(obj + C_DROPPED, (uint8_t)(b | 2));
            if (!(b & 2)) { drop_still(obj); return; }
        }
    }
    if (bit8(v_players, 0) && under(obj, v_player1, 0)) { drop_still(obj); return; }
    if (bit8(v_players, 1) && under(obj, v_player2, 1)) { drop_still(obj); return; }
    CarrierBob(obj);
}

/* $01A77E. Falling, faster to 7; on anything (a cell, the bounds' bottom, 8 lower too) it becomes
 * type $70 and drops its item. */
void CarrierFall(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if ((int32_t)rd32(obj + 0x1C) >= 0x70000) wr32(obj + 0x1C, 0x70000);
    if (!(bit8(obj + 0x22, 3) && bit8(obj + 0x47, 1))) {
        uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
        int landed = 0;
        for (int k = 0; k < 2 && !landed; k++) {
            if (MapAttr(x, y) || MapBoundsY(x, &y)) landed = 1;
            else y = (uint16_t)(y + 8);
        }
        if (!landed) return;
    }
    wr16(obj + O_TYPE, 0x70);
    wr16(obj + O_STATE, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x5E, 0);
    uint16_t slot;
    if (SlotFreeLow(&slot)) CarrierDrop(obj, slot);
}

/* $01A82E. Stopped after a drop, bobbing. */
void CarrierWait(uint16_t obj)
{
    wr16(obj + C_WAIT, (uint16_t)(rd16(obj + C_WAIT) - 1));
    if (!rd16(obj + C_WAIT)) { carrier_again(obj); return; }
    CarrierBob(obj);
}

/* $01A8A6. Thrown: swinging back toward where it was hit (by 2 a frame). */
void CarrierThrown(uint16_t obj)
{
    wr16(obj + C_WAIT, (uint16_t)(rd16(obj + C_WAIT) - 1));
    if (!rd16(obj + C_WAIT)) { carrier_again(obj); return; }
    CarrierBob(obj);
    if (rd16(obj + O_X) < rd16(obj + C_FROM_X)) wr32(obj + 0x18, rd32(obj + 0x18) + 0x20000);
    else wr32(obj + 0x18, rd32(obj + 0x18) - 0x20000);
}

/* $01A66A. Step 0: its item (+6 bit 2: $68), at x $80 unless +6 is negative, touchable (20),
 * flying. */
void CarrierStart(uint16_t obj)
{
    wr16(obj + 0x4C, rd16(obj + 6) & 4);
    wr16(obj + 6, rd16(obj + 6) & 0x8000);
    if (!rd16(obj + 6)) wr16(obj + O_X, 0x80);
    wr16(obj + O_FLAGS, 0xEF00);
    wr8(obj + 0x21, 0xAC);
    wr32(obj + 0x28, 0xF808F808);
    wr32(obj + 0x2C, 0xF808F010);
    wr16(obj + O_VITALITY, 0x14);
    wr16(obj + 0x0E, 0x8480);
    CarrierFace(obj);
    wr32(obj + 0x1C, 0x10000);
    wr16(obj + C_DROPPED, 0);
    wr32(obj + C_BOB, 0);
    wr8(obj + 0x46, 1);
    carrier_again(obj);
}

/* $01A64C. Type $144. */
void Carrier(uint16_t obj)
{
    call_object(rom32(0x01A656 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

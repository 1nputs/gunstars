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

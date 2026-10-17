/* Objects of the stages (the types from $38464 to $3F774): the figures next to type $BC, the
 * timers' digits, the shaking ceiling's machine, Black's dice palace (type $280), the debris
 * and pieces of the bosses, the seven-form boss (type $284), and the small types after it. */
#include "game.h"

/* ---- shared helpers ---- */

/* loc_03D2BC: the slot made a puff (type $100, its list $565F6), thrown in a random one of 16
 * directions (the sine tables at $2977C, $297FC, times 8) */
static void puff_thrown(uint16_t slot)
{
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0565F6);
    uint16_t k = rd16(v_randomSeed) & 0x1E0;
    int32_t vy = (int16_t)rom16(0x02977C + k), vx = (int16_t)rom16(0x0297FC + k);
    wr32(slot + 0x1C, (uint32_t)vy << 3);
    wr32(slot + 0x18, (uint32_t)vx << 3);
}

/* $03D2A6. A puff (type $100) at the object, thrown in a random direction. */
void DebrisPuff(uint16_t obj)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    puff_thrown(slot);
}

/* $03D342. Every 8th frame a spark (type $1DC, its list $566BC) at a random place up to 8 from
 * (x, y). */
void SparksAt(uint16_t x, uint16_t y)
{
    if (rd16(v_gameFrames) & 7) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1DC);
    wr32(slot + 0x48, 0x0566BC);
    wr16(slot + O_X, (uint16_t)((rd8(v_randomSeed) & 0xF) + (uint16_t)(x - 8)));
    wr16(slot + O_Y, (uint16_t)((rd8(0xFF09) & 0xF) + (uint16_t)(y - 8)));
}

/* $03D33A. The same at the object in slot $DE80. */
void SparksAtDE80(void)
{
    SparksAt(rd16(0xDE90), rd16(0xDE94));
}

/* $043906. A group of count+1 objects from this one placed by their offsets (+$48, +$4C) from
 * the leader's (+$50) place; the leader stays. */
void GroupPlace(uint16_t obj, uint16_t count)
{
    uint16_t lead = rd16(obj + 0x50);
    uint16_t x = (uint16_t)(rd16(lead + O_X) - rd16(lead + 0x48));
    uint16_t y = (uint16_t)(rd16(lead + O_Y) - rd16(lead + 0x4C));
    uint16_t a = obj;
    for (;;) {
        if (a != rd16(obj + 0x50)) {
            wr16(a + O_X, (uint16_t)(x + rd16(a + 0x48)));
            wr16(a + O_Y, (uint16_t)(y + rd16(a + 0x4C)));
        }
        a = (uint16_t)(a + 0x60);
        if (count-- == 0) break;
    }
}

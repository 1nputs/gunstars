/* Object types from $1567C to $16B9E, by the players' code: the shield parts held by a
 * carrier, the debris settling. */
#include "game.h"

/* ---- type $574: a part held at its carrier (+$4A): its place and look by +$48 (two sets, by
 * +$4C); the carrier down or let go (its +$48 0): gone ---- */

/* $016ABE */
void HeldPart(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0xE000);
        wr8(obj + 0x21, 0x40);
        wr32(obj + 0x2C, 0xF808F808);
        wr16(obj + 0x26, 2);
        wr32(obj + 0x08, 0x016B8A);
        wr16(obj + 0x0C, 0);
        wr16(obj + O_STATE, 4);
    }
    uint16_t c = rd16(obj + 0x4A);
    int16_t v = (int16_t)rd16(c + O_VITALITY);
    if (v <= 0 || !rd16(c + 0x48)) {
        wr8(obj + O_FLAGS, (uint8_t)(rd8(obj + O_FLAGS) | 0x10));
        return;
    }
    uint32_t i = (uint32_t)(int16_t)rd16(obj + 0x48);
    uint32_t tx = rd16(obj + 0x4C) ? 0x016B7A : 0x016B3E, ty = rd16(obj + 0x4C) ? 0x016B82 : 0x016B46;
    wr16(obj + O_X, (uint16_t)(rd16(c + O_X) + rom16(tx + i)));
    wr16(obj + O_Y, (uint16_t)(rd16(c + O_Y) + rom16(ty + i)));
    wr16(obj + 0x0E, rom16(0x016B4E + i));
}

/* ---- type $5B8: debris, its rise or fall eased to a stop by $1000 a frame ---- */

/* $016B9E */
void Settling(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0xCE00);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x08, 0x016BF0);
        wr16(obj + 0x0C, 0);
        wr16(obj + 0x0E, 0x845C);
    }
    uint32_t v = rd32(obj + 0x1C);
    if (!v) return;
    if (v & 0x80000000u) {
        v += 0x1000;
        if (!(v & 0x80000000u)) { wr32(obj + 0x1C, 0); return; }
    } else {
        v -= 0x1000;
        if (v & 0x80000000u) { wr32(obj + 0x1C, 0); return; }
    }
    wr32(obj + 0x1C, v);
}

/* Small objects and a boss's parts: a falling animation, a hopping figure, and the segments that
 * orbit a centre object (type $28, set up by $5D8: the craft's lower half in the story). */
#include "game.h"

/* The segments' common values are fields of the first, the $5D8 object, which must sit in slot
 * $C980: its +$56 the angle all turn by, +$50 a change to every radius, +$54 a tilt. */
#define ORBIT_TURN   0xC9D6
#define ORBIT_RADIUS 0xC9D0
#define ORBIT_TILT   0xC9D4
#define PART_SPRITE  0x8040      /* the segments' sprite word, and 1: their radius starts full */
#define PART_FULL    0x8042

/* $0561D8. Types $FC, $104: an animation list played falling. */
void AnimListFall(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    AnimList(obj);
}

/* ---- type $200: a figure that hops up after $10 frames of turning ---- */

/* $037D54 */
void HopperStart(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    wr8(obj + 0x21, 0);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x08, 0x0E0760);
    wr16(obj + O_FLAGS, 0xE780);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x48, 0x10);
}

/* $037D82 */
void HopperDone(uint16_t obj)
{
    (void)obj;
}

/* $037D84. Turning (by +$49 bit 1), then the hop. */
void HopperWait(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(obj + 0x1C, 0xFFFA0000);
        wr32(obj + 0x08, 0x0E06E4);
        wr16(obj + 0x0C, 0);
        return;
    }
    if (bit8(obj + 0x49, 1)) clr8(obj + 0x0E, 3);
    else set8(obj + 0x0E, 3);
}

/* $037D3E. Type $200. */
void Hopper(uint16_t obj)
{
    call_object(rom32(0x037D48 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- the orbiting segments ---- */

/* $0277BE. Six colours (from `colours`): three into palette line 2's $52 on, three into line
 * 3's $72 on, each into the fade's target too. */
uint32_t BossColours(uint32_t colours)
{
    for (uint16_t k = 0; k < 3; k++, colours += 2) {
        wr16((uint16_t)(v_palette + 0x52 + 2 * k), bus_rd16(colours));
        wr16((uint16_t)(v_paletteTarget + 0x52 + 2 * k), bus_rd16(colours));
    }
    for (uint16_t k = 0; k < 3; k++, colours += 2) {
        wr16((uint16_t)(v_palette + 0x72 + 2 * k), bus_rd16(colours));
        wr16((uint16_t)(v_paletteTarget + 0x72 + 2 * k), bus_rd16(colours));
    }
    return colours;
}

/* $0272E0. count+1 segments (type $28) into the slots from `slot` on, from three lists: maps (a
 * long each: a mappings table, its top nibble the segment's angle offset / 32 and bit 27 a flip;
 * bit 0 set: a fixed look's three words instead), radii (bytes) and parents (words: the parent's
 * slot and its box size). Their parent's flags get bits 10, 11. a6 is left past the last fixed
 * look read. */
uint16_t OrbiterSetup(uint16_t parent, uint32_t maps, uint32_t radii, uint32_t links, uint16_t count, uint16_t slot,
                      uint32_t *look)
{
    uint16_t m = 0, l = 0, r = 0;
    for (uint16_t k = count;; k--) {
        uint32_t d4 = bus_rd32(maps + m);
        if (d4) {
            *look = d4;         /* a zero entry keeps the last one's palette bits (a3) */
            uint32_t d5 = d4 & 0xF0000000u, d6 = d4 & 0x8000000u;
            d4 &= 0x3FFFFFF;
            if (!(d4 & 1)) {
                wr16(slot + O_FLAGS, 0xC080);
                wr16(slot + 0x0E, rd16(PART_SPRITE));
                wr32(slot + 0x08, 0x0E1462);
                wr32(slot + 0x4C, d4);
                uint16_t a = (uint16_t)(((d5 >> 28) << 5) | (d6 >> 16));
                wr16(slot + 0x50, a);
            } else {
                wr16(slot + O_FLAGS, 0x8080);
                d4 &= ~1u;
                wr16(slot + 0x0E, bus_rd16(d4));
                wr16(slot + 0x08, bus_rd16(d4 + 2));
                wr16(slot + 0x0A, bus_rd16(d4 + 4));
                reg_a6 = d4 + 4;
                wr32(slot + 0x4C, 0);
            }
        }
        uint16_t rad = bus_rd8(radii + r);
        wr16(slot + 0x52, rad);
        wr16(slot + 0x54, 0);
        if (rd16(PART_FULL)) wr16(slot + 0x54, rad);
        wr16(slot + 0x56, 0);
        uint16_t w = bus_rd16(links + l);
        wr32(slot + 0x48, 0xFFFF0000u | (uint16_t)((w & 0xFFE0) | 0xC000));
        uint16_t pri = (uint16_t)((int16_t)(w & 0xC000) >> 1);
        wr8(slot + 0x20, (uint8_t)((w & 0x1F) << 2));
        uint16_t pal = (uint16_t)(((*look & 0x3000000) >> 16) << 5);
        wr16(slot + 0x0E, rd16(slot + 0x0E) | pri);
        wr16(slot + 0x0E, rd16(slot + 0x0E) | pal);
        wr16(slot + O_TYPE, 0x28);
        m = (uint16_t)(m + 4);
        l = (uint16_t)(l + 2);
        r = (uint16_t)(r + 1);
        slot = (uint16_t)(slot + 0x60);
        if (k == 0) break;
    }
    wr16(parent + O_FLAGS, rd16(parent + O_FLAGS) | 0xC00);
    return slot;
}

/* $027144. Type $28: a segment on its parent's orbit: at its angle (+$56 plus the common one),
 * its radius (+$54, growing to +$52, plus the common change); its look by the angle (+$4C's
 * table, flipped past half a turn, +$50 bit 11 another flip). */
void Orbiter(uint16_t obj)
{
    uint16_t a = (uint16_t)((rd16(obj + 0x56) + rd16(ORBIT_TURN)) & 0x1FE);
    uint32_t maps = rd32(obj + 0x4C);
    if (maps) {
        uint16_t flip = rd16(obj + 0x50) & 0x800;
        uint16_t e = rd16(obj + 0x0E);
        wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xE000);
        uint16_t look = (uint16_t)((rd16(obj + 0x50) + a) & 0x1FE);
        if (look >= 0x100) e |= 0x1800;
        else e &= 0xE7FF;
        if ((int16_t)(rd16(ORBIT_TILT) - 0x100) >= 0) e ^= 0x800;
        e ^= flip;
        wr16(obj + 0x0E, rd16(obj + 0x0E) | e);
        wr32(obj + 0x08, bus_rd32(maps + ((look >> 3) & 0x1C)));
    }
    int16_t rad = (int16_t)rd16(obj + 0x54);
    if ((int16_t)(rad - (int16_t)rd16(obj + 0x52)) < 0) wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 2));
    rad = (int16_t)(rad + (int16_t)rd16(ORBIT_RADIUS));
    if (rad < 0) rad = 0;
    uint16_t b = (uint16_t)((a + rd16(ORBIT_TILT)) & 0x1FE);
    int32_t dy = (int32_t)(int16_t)rom16(0x02977C + a) * rad, dx = (int32_t)(int16_t)rom16(0x0297FC + b) * rad;
    uint16_t parent = rd16(obj + 0x4A);
    wr32(obj + O_Y, ((uint32_t)dy << 2) + rd32(parent + O_Y));
    wr32(obj + O_X, ((uint32_t)dx << 2) + rd32(parent + O_X));
}

/* $0277E8. Type $5D8: its colours, nine segments from itself on (it stays $5D8, the first), its
 * sprite word, the angles of the eight after it. */
void OrbitersStart(uint16_t obj)
{
    if (rd16(obj + O_STATE)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    BossColours(0x027FC2);
    uint32_t look = 0xFFFFE3F8;     /* where BossColours leaves a3 */
    wr16(PART_SPRITE, 0x300);
    wr16(PART_FULL, 1);
    OrbiterSetup(obj, 0x027F82, 0x027FA6, 0x027FB0, 8, obj, &look);
    wr16(obj + O_TYPE, 0x5D8);
    wr16(obj + 0x0E, 0x6360);
    for (uint16_t k = 0; k < 8; k++) wr16((uint16_t)(0xCA36 + 0x60 * k), rom16(0x02678A + 2 * k));
}

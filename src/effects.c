/* The small effect objects ($56128-$58200): sparks, chips, flashes, clouds, animation lists
 * played once or looped, things that follow another object. */
#include "game.h"

#define PALETTE_BITS 0x808A      /* the palette the animation lists' frames take */

/* one frame of a list: its time (+$4C), its sprite word (with the palette), the two words at +8 */
static uint32_t frame_set(uint16_t obj, uint32_t a)
{
    wr16(obj + 0x0E, bus_rd16(a));
    wr16(obj + 0x08, bus_rd16(a + 2));
    wr16(obj + 0x0A, bus_rd16(a + 4));
    a += 6;
    wr32(obj + 0x48, a);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(PALETTE_BITS));
    return a;
}

/* $056130. Types $50, $D8: a glint, gone when +$C turns negative; with two players it flickers,
 * alternate slots on alternate frames. */
void Glint(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + O_FLAGS, 0xED00);
        wr8(obj + 0x20, 0xB0);
        uint16_t e = (uint16_t)(0x480 | rd16(PALETTE_BITS));
        wr16(obj + 0x0E, e);
        wr16(obj + 0x0C, e);
        return;
    }
    if (rd16(obj + 0x0C) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    if (rd8(v_players) != 3) return;
    uint16_t odd = rd16(v_gameFrames) & 1;
    if ((obj & 0x20) ? !odd : odd) wr16(obj + O_FLAGS, 0x2D00);
    else wr16(obj + O_FLAGS, 0xED00);
}

/* $056128. Type $80: a glint falling. */
void GlintFall(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    Glint(obj);
}

/* $0561B2. Type $F4: +$50 frames, then an animation list (type $100). */
void AnimListLater(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xD00);
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if (!(rd16(obj + 0x50) & 0x8000)) return;
    wr16(obj + O_STATE, 0);
    wr16(obj + O_TYPE, 0x100);
}

/* $0561CC. Type $1DC: an animation list behind the others. */
void AnimListBehind(uint16_t obj)
{
    AnimList(obj);
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0xFEFF);
}

/* $0561E4. Type $2CC: an animation list sped up by +$58, +$5C each frame. */
void AnimListSpeeding(uint16_t obj)
{
    wr32(obj + 0x18, rd32(obj + 0x18) + rd32(obj + 0x58));
    wr32(obj + 0x1C, rd32(obj + 0x1C) + rd32(obj + 0x5C));
    AnimList(obj);
}

/* $056264. Types $108, $10C: an animation list played over and over (from +$50). */
void AnimListRepeat(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + O_FLAGS, 0x8D00);
        wr8(obj + 0x20, 0xB0);
        uint32_t a = rd32(obj + 0x48);
        wr32(obj + 0x50, a);
        wr16(obj + 0x4C, bus_rd16(a));
        frame_set(obj, a + 2);
        return;
    }
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if (rd16(obj + 0x4C)) return;
    uint32_t a = rd32(obj + 0x48);
    wr16(obj + 0x4C, bus_rd16(a));
    if (rd16(obj + 0x4C) & 0x8000) {
        wr32(obj + 0x48, rd32(obj + 0x50));
        a = rd32(obj + 0x48);
        wr16(obj + 0x4C, bus_rd16(a));
    }
    frame_set(obj, a + 2);
}

/* $0562E6. Type $1E4: an animation list, its flags +$54 and its sprite low (priority off). */
void AnimListLow(uint16_t obj)
{
    AnimList(obj);
    if (rd16(obj + O_FLAGS) == 0x1000) return;
    wr16(obj + O_FLAGS, rd16(obj + 0x54));
    wr16(obj + 0x0E, rd16(obj + 0x0E) & 0x7FFF);
}

/* $056360. A looping list of frames (+$48, +$4C the frames left): a negative time ends the list,
 * the long after it where it goes on. */
void AnimLoop(uint16_t obj)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if (!(rd16(obj + 0x4C) & 0x8000)) return;
    uint32_t a = rd32(obj + 0x48);
    wr16(obj + 0x4C, bus_rd16(a));
    a += 2;
    if (rd16(obj + 0x4C) & 0x8000) {
        a = bus_rd32(a);
        wr16(obj + 0x4C, bus_rd16(a));
        a += 2;
    }
    frame_set(obj, a);
}

/* $05631E. Type $220: a looping list for +$4E frames. */
void AnimLoopFor(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr8(obj + 0x20, 0xB0);
        wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x8000);
        uint32_t a = rd32(obj + 0x48);
        wr16(obj + 0x4C, bus_rd16(a));
        frame_set(obj, a + 2);
        return;
    }
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if (rd16(obj + 0x4E) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    AnimLoop(obj);
}

/* $056302. Type $224: the same, falling. */
void AnimLoopForFall(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    AnimLoopFor(obj);
}

/* $05630E. Type $30C: the same, sped up by +$58, +$5C each frame. */
void AnimLoopForSpeeding(uint16_t obj)
{
    wr32(obj + 0x18, rd32(obj + 0x18) + rd32(obj + 0x58));
    wr32(obj + 0x1C, rd32(obj + 0x1C) + rd32(obj + 0x5C));
    AnimLoopFor(obj);
}

/* $056824. Type $250: shown on the frames +$48 masks out, for +$4A frames. */
void Blinker(uint16_t obj)
{
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (rd16(obj + 0x4A) & 0x8000) wr16(obj + O_FLAGS, 0x1000);
    if (rd16(v_gameFrames) & rd16(obj + 0x48)) wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0x7FFF);
    else wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x8000);
}

/* $05684E. Types $A4, $A8: a reach held at (+$4A, +$4C) from its owner (+$58, which holds it
 * back) for +$48 frames; when it touches something, a burst (type $100). */
void Reach(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + O_FLAGS, 0xD00);
        wr8(obj + 0x23, 0x20);
        return;
    }
    uint16_t owner = rd16(obj + 0x58);
    if (rd16(owner + 0x58) != obj) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) {
        wr16(owner + 0x58, 0);
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr16(obj + O_X, (uint16_t)(rd16(owner + O_X) + rd16(obj + 0x4A)));
    wr16(obj + O_Y, (uint16_t)(rd16(owner + O_Y) + rd16(obj + 0x4C)));
    if (!bit8(obj + 0x22, 4)) return;
    wr16(obj + O_STATE, 1);
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0565D4);
    wr8(obj + 0x20, 0x60);
    AnimListStart(obj);
}

/* the sparks' first frame */
static void spark_start(uint16_t obj)
{
    wr16(obj + O_STATE, 1);
    wr16(obj + O_FLAGS, 0xEF00);
    wr8(obj + 0x20, 0xB0);
    wr16(obj + 0x0E, 0x480);
    wr16(obj + 0x0C, 0);
}

/* the effects' end: after +$48 frames */
static void effect_tick(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) wr16(obj + O_FLAGS, 0x1000);
}

/* $0568CA. Type $CC: a spark falling, for +$48 frames. */
void SparkFall(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        spark_start(obj);
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    effect_tick(obj);
}

/* $0568DE. Type $C8: a spark. */
void Spark(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        spark_start(obj);
        return;
    }
    effect_tick(obj);
}

/* $056922. Type $1B0: a flash for +$48 frames. */
void Flash(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + O_FLAGS, 0xCF00);
        return;
    }
    effect_tick(obj);
}

/* $056912. Type $1B4: falling. */
void FlashFall(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    Flash(obj);
}

/* $056946. Type $B8: a chip falling for +$48 frames, shown on the frames +$4A masks out. */
void Chip(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr8(obj + 0x20, 0x80);
        wr16(obj + 0x0C, 0);
        return;
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (rd16(v_gameFrames) & rd16(obj + 0x4A)) wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0x7FFF);
    else wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x8000);
}

/* $0569A2. Type $57C: gone after +$48 frames (its other flags kept). */
void Lasting(uint16_t obj)
{
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) set8(obj + O_FLAGS, 4);
}

/* $056992. Type $600: the same, sped up by +$4C, +$50 each frame. */
void LastingSpeeding(uint16_t obj)
{
    wr32(obj + 0x18, rd32(obj + 0x18) + rd32(obj + 0x4C));
    wr32(obj + 0x1C, rd32(obj + 0x1C) + rd32(obj + 0x50));
    Lasting(obj);
}

/* $0569B2. Type $1FC: for +$48 frames, shown (on the frames +$4A masks out) while its owner (+$58)
 * is at step +$4C. */
void BlinkWhile(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr8(obj + 0x20, 0x80);
        wr16(obj + 0x0C, 0);
        return;
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    uint16_t owner = rd16(obj + 0x58);
    if (rd16(owner + O_STATE) == rd16(obj + 0x4C) && !(rd16(v_gameFrames) & rd16(obj + 0x4A)))
        wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x8000);
    else
        wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0x7FFF);
}

/* $056A06. Type $D0: circling its owner (+$58, which holds it back) for +$48 frames: its angle
 * +$56 turning by +$54, its radii +$4E (y), +$50 (x), its centre offset (+$4A, +$4C). */
void Circler(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + 0x0E, 0x480);
        wr16(obj + O_FLAGS, 0xED00);
        wr8(obj + 0x20, 0x80);
        wr16(obj + 0x0C, 0);
        return;
    }
    uint16_t a = (uint16_t)((rd16(obj + 0x56) + rd16(obj + 0x54)) & 0x1FE);
    wr16(obj + 0x56, a);
    uint32_t dy = (uint32_t)((int32_t)(int16_t)rom16(0x02977C + a) * (int16_t)rd16(obj + 0x4E)) << 2;
    uint32_t dx = (uint32_t)((int32_t)(int16_t)rom16(0x0297FC + a) * (int16_t)rd16(obj + 0x50)) << 2;
    uint16_t owner = rd16(obj + 0x58);
    if (rd16(owner + 0x58) == obj) {
        wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
        if (!(rd16(obj + 0x48) & 0x8000)) {
            uint16_t ox = rd16(obj + 0x4A), oy = rd16(obj + 0x4C);
            dx += ((uint32_t)ox << 16) | ((ox & 0x8000) ? 0xFFFF : 0);
            wr32(obj + O_X, dx + rd32(owner + O_X));
            dy += ((uint32_t)oy << 16) | ((oy & 0x8000) ? 0xFFFF : 0);
            wr32(obj + O_Y, dy + rd32(owner + O_Y));
            return;
        }
    }
    wr16(owner + 0x58, 0);
    wr16(obj + O_FLAGS, 0x1000);
}

/* $056AA8. Type $84: falling +$48 frames, then dying as a soldier does. */
void FallThenDie(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (!(rd16(obj + 0x48) & 0x8000)) return;
    wr32(obj + 0x1C, 0);
    SoldierDying(obj);
}

/* $056100. The first frame of a list at `list` (its time in +$4C; a negative one: hidden). */
void FrameFrom(uint16_t obj, uint32_t list)
{
    wr16(obj + 0x4C, bus_rd16(list));
    if (rd16(obj + 0x4C) & 0x8000) {
        wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0x7FFF);
        return;
    }
    uint16_t e = bus_rd16(list + 2);
    wr16(obj + 0x08, bus_rd16(list + 4));
    wr16(obj + 0x0A, bus_rd16(list + 6));
    wr32(obj + 0x48, list + 8);
    wr16(obj + 0x0E, (uint16_t)(e | rd16(PALETTE_BITS)));
}

/* ---- the pellets: a few frames' glow where they start, then flying in direction +$48 (a speed
 * table's offset), their look by it; touchable on alternate frames (alternate slots taking
 * turns) ---- */

/* their touch (+$21) on this frame */
static int pellet_touchable(uint16_t obj)
{
    return ((obj >> 5) ^ rd8(v_gameFrames + 1)) & 1;
}

/* their look by the direction, and their sound */
static void pellet_look(uint16_t obj, uint16_t dir, uint16_t power)
{
    FrameFrom(obj, 0x05647A + (uint32_t)((dir & 0x3C) << 1));
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(PALETTE_BITS));
    wr16(obj + 0x26, power);
    wr32(obj + 0x2C, 0xFE01FE02);
    QueueSound(0xB3);
}

/* their palette flashing every 4 frames */
static void pellet_flash(uint16_t obj, uint16_t keep)
{
    if (bit8(v_gameFrames + 1, 2)) wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x6000);
    else wr16(obj + 0x0E, rd16(obj + 0x0E) & keep);
}

/* $056B36. Type $90. Into a wall's cell (bit 3) or a hit, a burst (type $100) bouncing back. */
void Pellet(uint16_t obj)
{
    uint16_t st = rd16(obj + O_STATE);
    if (st == 2) {
        pellet_flash(obj, 0x9FFF);
        if (!bit8(obj + 0x22, 4)) {
            if (!pellet_touchable(obj)) {
                wr8(obj + 0x21, 0);
                return;
            }
            wr8(obj + 0x21, 0x10);
            if (!(MapAttr(rd16(obj + O_X), rd16(obj + O_Y)) & 8)) return;
        }
        wr16(obj + O_STATE, 0);
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x0565D4);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, (uint32_t)((int32_t)~rd32(obj + 0x18) >> 1));
        wr32(obj + 0x1C, (uint32_t)((int32_t)~rd32(obj + 0x1C) >> 1));
        return;
    }
    if (st != 1) {
        wr16(obj + O_STATE, (uint16_t)(st + 1));
        wr16(obj + 0x4A, 7);
        wr16(obj + O_FLAGS, 0x8F00);
        wr8(obj + 0x20, 0x80);
        wr16(obj + 0x0E, 0x4B5);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
        wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(PALETTE_BITS));
        return;
    }
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (!(rd16(obj + 0x4A) & 0x8000)) {
        if ((int16_t)(rd16(obj + 0x4A) - 3) >= 0) return;
        wr16(obj + 0x0E, (uint16_t)(0x546 | rd16(PALETTE_BITS)));
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 1));
    uint16_t dir = rd16(obj + 0x48);
    wr32(obj + 0x1C, rom32(0x05818C + (uint32_t)(int16_t)dir));
    wr32(obj + 0x18, rom32(0x0581AC + (uint32_t)(int16_t)dir));
    pellet_look(obj, dir, 3);
}

/* $056B20. Type $438: untouchable. */
void PelletHarmless(uint16_t obj)
{
    Pellet(obj);
    wr8(obj + 0x20, 0);
}

/* $056B2A. Type $1A0: behind the others. */
void PelletBehind(uint16_t obj)
{
    Pellet(obj);
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0xFEFF);
}

/* the glow before they fly: four looks by the frames left */
static void glow(uint16_t obj)
{
    wr16(obj + 0x0E, rom16(0x0461CA + (uint32_t)((rd16(obj + 0x4A) << 1) & 6)));
}

/* $056C92. Types $490, $498 ($498 twice as fast above the lowest difficulty): gone off the
 * screen's area; a hit, a burst (type $100). */
void PelletAimed(uint16_t obj)
{
    uint16_t st = rd16(obj + O_STATE);
    if (st == 2) {
        int16_t x = (int16_t)rd16(obj + O_X), y = (int16_t)rd16(obj + O_Y);
        if (x - 0x1C0 >= 0 || x - 0x80 < 0 || y - 0x98 < 0 || y - 0x160 >= 0) {
            wr16(obj + O_FLAGS, 0x1000);
            return;
        }
        pellet_flash(obj, 0xDFFF);
        if (bit8(obj + 0x22, 4)) {
            wr16(obj + O_TYPE, 0x100);
            wr32(obj + 0x48, 0x0565F6);
            wr16(obj + O_STATE, 0);
            wr8(obj + 0x21, 0);
            wr32(obj + 0x18, 0);
            wr32(obj + 0x1C, 0);
            QueueSound(rom8(0x0000AC));
            return;
        }
        wr8(obj + 0x21, pellet_touchable(obj) ? 0x10 : 0);
        return;
    }
    if (st != 1) {
        wr16(obj + O_STATE, (uint16_t)(st + 1));
        wr16(obj + 0x4A, 4);
        wr16(obj + O_FLAGS, 0x8100);
        wr8(obj + 0x20, 0x80);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
        wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(PALETTE_BITS));
    }
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (!(rd16(obj + 0x4A) & 0x8000)) {
        glow(obj);
        wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x6000);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 1));
    wr16(obj + O_FLAGS, 0x8C00);
    uint16_t dir = rd16(obj + 0x48);
    uint32_t vy = rom32(0x05818C + (uint32_t)(int16_t)dir), vx = rom32(0x0581AC + (uint32_t)(int16_t)dir);
    if (rd16(obj + O_TYPE) == 0x498 && rd16(v_difficulty)) {
        vx <<= 1;
        vy <<= 1;
    }
    wr32(obj + 0x18, vx);
    wr32(obj + 0x1C, vy);
    pellet_look(obj, dir, 4);
}

/* $056E10. Type $52C: a bolt (its speed set by who made it; half at the lowest difficulty),
 * gone off the screen's sides; a hit, a burst (type $100). */
void Bolt(uint16_t obj)
{
    uint16_t st = rd16(obj + O_STATE);
    if (st == 2) {
        int16_t x = (int16_t)rd16(obj + O_X);
        if (x - 0x1C0 >= 0 || x - 0x80 < 0) {
            wr16(obj + O_FLAGS, 0x1000);
            return;
        }
        if (!bit8(obj + 0x22, 4)) return;
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x056450);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        QueueSound(rom8(0x0000AC));
        return;
    }
    if (st != 1) {
        wr16(obj + O_STATE, (uint16_t)(st + 1));
        wr16(obj + 0x4A, 4);
        wr16(obj + O_FLAGS, 0x8100);
        wr8(obj + 0x20, 0x78);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
        wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(PALETTE_BITS));
    }
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (!(rd16(obj + 0x4A) & 0x8000)) {
        glow(obj);
        wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xBFFF);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 1));
    wr16(obj + O_FLAGS, 0x8C00);
    if (!rd16(v_difficulty)) {
        wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
        wr32(obj + 0x1C, (uint32_t)((int32_t)rd32(obj + 0x1C) >> 1));
    }
    wr16(obj + 0x0E, (uint16_t)(0x48A | rd16(PALETTE_BITS)));
    wr16(obj + 0x08, 0x800);
    wr16(obj + 0x0A, 0xF6FE);
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xFF01FC04);
    wr16(obj + 0x26, 5);
    QueueSound(0xB3);
}

/* $056F1E. Type $DC: an ember thrown out in direction +$48, rising; gone when +$C turns negative
 * or in a wall's cell. Touchable from the odd slots. */
void Ember(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        uint16_t dir = rd16(obj + 0x48) & 0x7C;
        wr32(obj + 0x1C, rom32(0x05818C + dir));
        wr32(obj + 0x18, rom32(0x0581AC + dir));
        wr32(obj + 0x08, 0x0E085E);
        wr16(obj + O_FLAGS, 0xEF00);
        wr8(obj + 0x20, 0x80);
        wr16(obj + 0x0E, 0x480);
        wr16(obj + 0x0C, 0);
        if (obj & 0x20) {
            wr16(obj + 0x26, 6);
            wr8(obj + 0x21, 0x10);
            wr32(obj + 0x2C, 0xFC03FC04);
        }
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x1800);
    if ((rd16(obj + 0x0C) & 0x8000) || (MapAttr(rd16(obj + O_X), rd16(obj + O_Y)) & 8))
        wr16(obj + O_FLAGS, 0x1000);
}

/* ---- types $138, $1F4: a seeker that turns toward its player for +$4A frames, trailing smoke,
 * until it hits something or a wall's cell; gone when the boss's timer runs ---- */

/* $056FD4 */
void SeekerStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, rd16(obj + 0x4E) ? 0x8E00 : 0x8F00);
    wr8(obj + 0x20, 0x80);
    wr16(obj + 0x0E, 0x66A0);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(PALETTE_BITS));
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr16(obj + O_VITALITY, 0xA);
    wr16(obj + 0x26, 0xA);
    wr16(obj + 0x4C, PlayerBySlot(obj));
    wr8(obj + 0x21, 0x30);
    wr32(obj + 0x2C, 0xFE01FE02);
    wr32(obj + 0x28, 0xF807F808);
}

/* loc_0570AE: a burst (type $100), bouncing */
static void seeker_burst(uint16_t obj)
{
    wr16(obj + O_STATE, 0);
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x05663A);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x1C, ~rd32(obj + 0x1C));
    QueueSound(0xCE);
}

/* $05703E. Smoke every 8th frame; shot down, 2 points. Its direction +$48 (a sine table's
 * offset) turns toward the player by the difficulty's rate. */
void SeekerFly(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 7)) {
        uint16_t slot, t;
        if (SlotFreeHigh(&slot, &t)) {
            wr16(slot + O_TYPE, rd16(obj + 0x4E) ? 0x1DC : 0x100);
            wr32(slot + 0x48, 0x056450);
            wr16(slot + O_X, rd16(obj + O_X));
            wr16(slot + O_Y, rd16(obj + O_Y));
        }
    }
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 2);
        seeker_burst(obj);
        return;
    }
    if (bit8(obj + 0x22, 4) || (MapAttr(rd16(obj + O_X), rd16(obj + O_Y)) & 8)) {
        seeker_burst(obj);
        return;
    }
    if (rd16(obj + 0x4A) & 0x8000) return;
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    uint16_t dir = rd16(obj + 0x48);
    wr32(obj + 0x1C, (uint32_t)(int32_t)(int16_t)rom16(0x02977C + (uint32_t)(int16_t)dir) << 3);
    wr32(obj + 0x18, (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + (uint32_t)(int16_t)dir) << 3);
    uint16_t target = rd16(obj + 0x4C);
    uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t d2 = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
    uint16_t rate = rom16(0x05716C + (uint32_t)(int16_t)rd16(0xA20C));
    int16_t turn = (int16_t)((((int16_t)d2 >> 7) & 0x1FE) - rd16(obj + 0x48));
    int left = turn < 0 ? turn - (int16_t)0xFF00 < 0 : turn - 0x100 < 0;
    if (left) wr16(obj + 0x48, (uint16_t)((rd16(obj + 0x48) + rate) & 0x1FE));
    else wr16(obj + 0x48, (uint16_t)((rd16(obj + 0x48) - rate) & 0x1FE));
}

/* $056FBA. Types $138, $1F4. */
void Seeker(uint16_t obj)
{
    if (!(rd16(v_bossTimer) & 0x8000)) {
        seeker_burst(obj);
        return;
    }
    call_object(rom32(0x056FCC + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- types $98, $354: a bomb that falls until it hits something or the floor, then bursts into
 * four fires (type $F8) circling it for $21 frames ---- */

/* $0571F2 */
void BombStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x8F00);
    wr8(obj + 0x20, 0x80);
    wr16(obj + 0x0E, 0x65BC);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(PALETTE_BITS));
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr32(obj + 0x28, 0xF808F808);
    wr8(obj + 0x21, 2);
}

/* $057282. The burst. */
void BombBurst(uint16_t obj)
{
    wr16(obj + 0x26, 0x11);
    QueueSound(0xC5);
    wr16(obj + O_X, rd16(obj + O_X) & 0xFFF8);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x700);
    wr32(obj + 0x1C, 0xFFFF0000);
    wr8(obj + 0x21, 0x50);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x2C, 0xF00FE41C);
    wr32(obj + 0x28, 0xF00FE41C);
    wr16(obj + 0x48, 0);
    wr16(obj + 0x4A, 0);
    wr16(obj + 0x4C, 0);
    wr16(obj + 0x4E, 0);
    uint16_t list = (uint16_t)(obj + 0x48);
    for (uint16_t k = 3;; k--) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) {
            if (k == 3) {
                wr16(obj + O_FLAGS, 0x1000);
                return;
            }
            break;
        }
        wr16(slot + O_TYPE, 0xF8);
        wr32(slot + 0x48, 0x05663A);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        wr16(list, slot);
        list = (uint16_t)(list + 2);
        if (k == 0) break;
    }
    wr16(v_shakeSprites, 0x10);
    wr16(obj + 0x58, 0x21);
}

/* $05722C. Falling: thrown back by a player (+$23 bit 4) it hurts more; it bursts on a hit
 * (+$22 bit 6), or in the floor unless rising through a thin floor ($30-$37). */
void BombFall(uint16_t obj)
{
    if (bit8(obj + 0x23, 4)) {
        wr8(obj + 0x21, 0x40);
        wr32(obj + 0x2C, 0xF010F010);
    }
    if (bit8(obj + 0x22, 6)) {
        BombBurst(obj);
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2000);
    uint16_t a = MapAttr(rd16(obj + O_X), rd16(obj + O_Y));
    if (!a) return;
    if ((int16_t)(a - 0x30) < 0 || (int16_t)(a - 0x38) >= 0 || !(rd16(obj + 0x1C) & 0x8000)) BombBurst(obj);
}

/* $057336. The fires: two at 8 times a speed's length around it, turning with the frames, two
 * the other way at 4 times; it goes with them (or with any of them gone). */
void BombFire(uint16_t obj)
{
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 1));
    if (rd16(obj + 0x58) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    uint16_t list = (uint16_t)(obj + 0x48);
    uint16_t a = (uint16_t)((rd16(v_gameFrames) << 3) & 0x7C);
    for (int ring = 0; ring < 2; ring++) {
        for (int k = 0; k < 2; k++) {
            uint16_t fire = rd16(list);
            list = (uint16_t)(list + 2);
            if (!fire) return;
            if (!rd16(fire + O_TYPE)) {
                wr16(obj + O_FLAGS, 0x1000);
                return;
            }
            uint32_t dy = rom32(0x05818C + a), dx = rom32(0x0581AC + a);
            dy <<= ring ? 2 : 3;
            dx <<= ring ? 2 : 3;
            wr32(fire + O_Y, dy + rd32(obj + O_Y));
            wr32(fire + O_X, dx + rd32(obj + O_X));
            wr8(fire + 0x20, ring ? 0x90 : 0x94);
            a = (uint16_t)((ring ? a - 0x40 : a + 0x40) & 0x7C);
        }
        if (!ring) a = (uint16_t)(~a & 0x7C);
    }
}

/* $05732C. Its fires hurt more. */
void BombFireStart(uint16_t obj)
{
    wr8(obj + 0x21, 0x40);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    BombFire(obj);
}

/* $0571C2. Type $354: bursting at once. */
void BombNow(uint16_t obj)
{
    call_object(rom32(0x0571CC + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $0571D8. Type $98. */
void Bomb(uint16_t obj)
{
    call_object(rom32(0x0571E2 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $134: a cinder falling until the floor, then a fire there ---- */

/* $0573FA */
void CinderStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x8F00);
    wr8(obj + 0x20, 0x80);
    wr16(obj + 0x0E, 0x65BC);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(PALETTE_BITS));
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr16(obj + 0x26, 0xA);
    wr32(obj + 0x28, 0xF808F808);
    wr8(obj + 0x21, 2);
}

/* $05743A. In the floor (but rising through a thin one): a fire (type $FC, falling). */
void CinderFall(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x2000);
    uint16_t a = MapAttr(rd16(obj + O_X), rd16(obj + O_Y));
    if (!a) return;
    if ((int16_t)(a - 0x30) >= 0 && (int16_t)(a - 0x38) < 0 && (rd16(obj + 0x1C) & 0x8000)) return;
    wr16(obj + O_X, rd16(obj + O_X) & 0xFFF8);
    wr16(obj + O_STATE, 0);
    wr16(obj + O_TYPE, 0xFC);
    wr32(obj + 0x48, 0x05663A);
    wr16(obj + O_FLAGS, 0xD00);
    wr32(obj + 0x1C, 0xFFFF0000);
    wr32(obj + 0x18, 0);
    wr8(obj + 0x21, 0x50);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x2C, 0xF40BF40C);
    wr32(obj + 0x28, 0xF40BF40C);
}

/* $0573E8. Type $134. */
void Cinder(uint16_t obj)
{
    call_object(rom32(0x0573F2 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $21C: a shell thrown down (+$48 zero) or up, bursting (type $220) past a height ---- */

/* $0574D6 */
void ShellStart(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8C00);
    wr8(obj + 0x20, 0x80);
    wr16(obj + 0x0E, 0xE5BC);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr16(obj + 0x26, 9);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + (rd16(obj + 0x48) ? 8 : 4)));
}

/* loc_05753E */
static void shell_burst(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x220);
    wr32(obj + 0x48, 0x0566DE);
    wr16(obj + 0x4E, 0x20);
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xF808F808);
    wr16(obj + O_STATE, 0);
    QueueSound(0xCE);
}

/* $05750E. Falling, bursting below $13F. */
void ShellDown(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x3800);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if ((int16_t)(rd16(obj + O_Y) - 0x13F) >= 0) shell_burst(obj);
}

/* $057526. Rising, bursting above $B1. */
void ShellUp(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x3800);
    if (!(rd32(obj + 0x1C) & 0x80000000u)) return;
    if ((int16_t)(rd16(obj + O_Y) - 0xB1) < 0) shell_burst(obj);
}

/* $0574C0. Type $21C. */
void Shell(uint16_t obj)
{
    call_object(rom32(0x0574CA + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $0560F4. The next frame of a list when this one's time (+$4C) is up. */
void FrameNext(uint16_t obj)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if (rd16(obj + 0x4C)) return;
    FrameFrom(obj, rd32(obj + 0x48));
}

/* a puff (type $100, animation list `list`) at a random place within `mask` of the object, by
 * the random bytes at $FF08 (x) and $FF09 (y), $FF0A's and $FF0B's signs the side */
static uint16_t puff_near(uint16_t obj, uint16_t slot, uint16_t mask, uint32_t list)
{
    uint16_t dx = rd8(0xFF08) & mask, dy = rd8(0xFF09) & mask;
    if (rd8(0xFF0A) & 0x80) dx = (uint16_t)~dx;
    if (!(rd8(0xFF0B) & 0x80)) dy = (uint16_t)~dy;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, list);
    wr16(slot + O_X, (uint16_t)(dx + rd16(obj + O_X)));
    wr16(slot + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
    return slot;
}

/* $057574. Type $54: a blast: its own animation, and five puffs around it every 4th frame. */
void Blast(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + O_FLAGS, 0x8700);
        wr32(obj + 0x48, 0x0563EE);
        wr16(obj + 0x4C, 1);
        wr16(obj + 0x4E, 4);
        wr16(obj + 0x50, 3);
        wr8(obj + 0x21, 0);
        wr8(obj + 0x20, 0xAC);
        QueueSound(0xCE);
    }
    if (!(rd16(obj + 0x4C) & 0x8000)) FrameNext(obj);
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 1));
    if (!(rd16(obj + 0x50) & 0x8000)) return;
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if (rd16(obj + 0x4E) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr16(obj + 0x50, 3);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    puff_near(obj, slot, 0x1C, 0x05639C);
    wr32(slot + 0x1C, rd32(obj + 0x1C));
}

/* $057632. Type $70: a spiral of $D puffs every 4th frame, 8 speeds' length out, turning either
 * way by the frame it started. */
void Spiral(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + 0x48, 0xC);
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4C, rd16(v_randomSeed));
        wr16(obj + 0x4E, (rd16(v_gameFrames) & 1) ? 0xFFF4 : 0xC);
        wr16(obj + O_FLAGS, 0xF00);
        wr8(obj + 0x21, 0);
        QueueSound(0xCF);
        return;
    }
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (!(rd16(obj + 0x4A) & 0x8000)) return;
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr16(obj + 0x4A, 3);
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) + rd16(obj + 0x4E)));
    wr16(obj + 0x4C, rd16(obj + 0x4C) & 0x7C);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    uint16_t a = rd16(obj + 0x4C);
    uint32_t dy = rom32(0x05818C + a) << 3, dx = rom32(0x0581AC + a) << 3;
    dx += rd32(obj + O_X);
    dy += rd32(obj + O_Y);
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0563EE);
    wr32(slot + O_X, dx);
    wr32(slot + O_Y, dy);
}

/* loc_057702 */
static void ring_start(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 1));
    wr16(obj + O_FLAGS, 0xF00);
    wr16(obj + 0x48, 6);
    wr16(obj + 0x4A, 0);
    wr8(obj + 0x21, 0);
    QueueSound(0xCA);
}

/* $0576F4. Type $74: seven puffs thrown out every 3rd frame, each an eighth of a turn on. */
void Ring(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + 0x4C, rd16(v_randomSeed));
        ring_start(obj);
        return;
    }
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (!(rd16(obj + 0x4A) & 0x8000)) return;
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr16(obj + 0x4A, 2);
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) + 8));
    wr16(obj + 0x4C, rd16(obj + 0x4C) & 0x7C);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    uint16_t a = rd16(obj + 0x4C);
    wr32(slot + 0x1C, rom32(0x05818C + a));
    wr32(slot + 0x18, rom32(0x0581AC + a));
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x05639C);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* $0576EC. Type $308: a ring, turned to type $74 (its direction where it was). */
void RingKept(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x74);
    ring_start(obj);
}

/* $057790. Type $78: a debris emitter (see DebrisEmit) for +$48 frames. */
void Emitter(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + 0x5A, 0);
        wr16(obj + O_FLAGS, 0);
        wr8(obj + 0x21, 0);
        return;
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    DebrisEmit(obj);
}

/* $05790E. Four pieces (type $104) thrown up from it, from right to left. */
void Pieces(uint16_t obj)
{
    uint16_t vx = 3;
    for (uint16_t k = 3;; k--) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + 0x18, vx);
        vx = (uint16_t)(vx - 2);
        wr16(slot + 0x1C, 0xFFFD);
        wr16(slot + O_TYPE, 0x104);
        wr32(slot + 0x48, 0x056450);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        if (k == 0) return;
    }
}

/* $057872. Type $7C: a body falling (thrown aside when it was not moving, mostly), trailing
 * smoke; on the floor it crashes into pieces. */
void Crash(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + O_FLAGS, 0xF00);
        wr8(obj + 0x21, 0);
        if (!rd32(obj + 0x18) && (rd16(v_randomSeed) & 7))
            wr32(obj + 0x18, (int16_t)(rd16(obj + O_X) - 0x120) < 0 ? 0x28000 : 0xFFFD8000);
        QueueSound(0xC6);
        return;
    }
    int16_t vy = (int16_t)rd16(obj + 0x1C);
    if (vy < 0 || vy - 6 < 0) wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x3800);
    if (vy >= 0 && MapAttr(rd16(obj + O_X), (uint16_t)(rd16(obj + O_Y) + 8))) {
        QueueSound(0xC5);
        wr16(v_shakeSprites, 0x10);
        wr16(obj + O_FLAGS, 0x1000);
        Pieces(obj);
        return;
    }
    if (rd16(v_gameFrames) & 3) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + 0x18, 0);
    wr16(slot + 0x1C, 0xFFFE);
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0563F6);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* a piece of the scatter: in direction `a`, up a little more */
static void scatter_piece(uint16_t obj, uint16_t slot, uint16_t a)
{
    uint32_t vy = rom32(0x05818C + (uint32_t)(int16_t)a), vx = rom32(0x0581AC + (uint32_t)(int16_t)a);
    vy += 0xFFFE0000;
    wr32(slot + 0x18, vx);
    wr32(slot + 0x1C, vy);
    wr16(slot + O_TYPE, 0x104);
    wr32(slot + 0x48, 0x05639C);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* $05798C. Type $8C: +$4A+1 pieces fanned out from direction +$48, +$4C apart, alternately
 * either side of it; then gone. */
void Scatter(uint16_t obj)
{
    QueueSound(0xCE);
    uint16_t n = rd16(obj + 0x4A), right = rd16(obj + 0x48), step = rd16(obj + 0x4C), left = right;
    for (;;) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) break;
        scatter_piece(obj, slot, right);
        right = (uint16_t)((right - step) & 0x7C);
        if (n-- == 0) break;
        if (!SlotFreeHigh(&slot, &t)) break;
        left = (uint16_t)((left + step) & 0x7C);
        scatter_piece(obj, slot, left);
        if (n-- == 0) break;
    }
    wr16(obj + O_FLAGS, 0x1000);
}

/* $057A4C. Type $D4: four puffs (type $F8) wheeling out from it for $20 frames, in two pairs on
 * a line through it (one at the radius, one at twice), the line turning by +$4C each frame. */
void Wheel(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + O_FLAGS, 0xF00);
        wr8(obj + 0x21, 0);
        wr16(obj + 0x4A, 0x1F);
        wr16(obj + 0x4E, 0x80);
        for (uint16_t k = 0; k < 5; k++) wr16((uint16_t)(obj + 0x50 + 2 * k), 0);
        uint16_t turn = rd16(v_randomSeed) & 0xE;
        if (rd8(v_gameFrames) & 1) turn = (uint16_t)~turn;
        wr16(obj + 0x4C, turn);
        QueueSound(0xCA);
        uint16_t list = (uint16_t)(obj + 0x50);
        for (uint16_t k = 3;; k--) {
            uint16_t slot, t;
            if (!SlotFreeHigh(&slot, &t)) return;
            wr16(list, slot);
            list = (uint16_t)(list + 2);
            wr16(slot + O_TYPE, 0xF8);
            wr32(slot + 0x48, 0x0563EE);
            wr16(slot + O_X, rd16(obj + O_X));
            wr16(slot + O_Y, rd16(obj + O_Y));
            if (k == 0) return;
        }
    }
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (rd16(obj + 0x4A) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) + 1));
    uint16_t a = (uint16_t)((rd16(obj + 0x4E) + rd16(obj + 0x4C)) & 0x1FE);
    wr16(obj + 0x4E, a);
    int16_t r = (int16_t)rd16(obj + 0x58);
    uint16_t dy = (uint16_t)(((uint32_t)((int32_t)(int16_t)rom16(0x02977C + a) * r) << 2) >> 16);
    uint16_t dx = (uint16_t)(((uint32_t)((int32_t)(int16_t)rom16(0x0297FC + a) * r) << 2) >> 16);
    uint16_t y = 0, x = 0, p;
    if ((p = rd16(obj + 0x50))) {
        y = (uint16_t)(rd16(obj + O_Y) + dy);
        x = (uint16_t)(rd16(obj + O_X) + dx);
        wr16(p + O_Y, y);
        wr16(p + O_X, x);
    }
    if ((p = rd16(obj + 0x52))) {
        y = (uint16_t)(y + (uint16_t)(dy << 1));
        x = (uint16_t)(x + (uint16_t)(dx << 1));
        wr16(p + O_Y, y);
        wr16(p + O_X, x);
    }
    if ((p = rd16(obj + 0x54))) {
        y = (uint16_t)(rd16(obj + O_Y) - dy);
        x = (uint16_t)(rd16(obj + O_X) - dx);
        wr16(p + O_Y, y);
        wr16(p + O_X, x);
    }
    if ((p = rd16(obj + 0x56))) {
        y = (uint16_t)(y - (uint16_t)(dy << 1));
        x = (uint16_t)(x - (uint16_t)(dx << 1));
        wr16(p + O_Y, y);
        wr16(p + O_X, x);
    }
}

/* $057B96. Type $E0: a debris burst (see DebrisBurst) for +$48 frames. */
void Burster(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + 0x5A, 0);
        wr8(obj + 0x21, 0);
        wr16(obj + O_FLAGS, 0xD00);
        return;
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    DebrisBurst(obj);
}

/* $057C88. A spark (type $F4, its list +$4C, its time +$50) thrown out at random. */
int SparkThrow(uint16_t obj, uint16_t *slot)
{
    uint16_t t;
    if (!SlotFreeHigh(slot, &t)) return 0;
    uint16_t a = rd16(v_randomSeed) & 0x7C;
    wr32(*slot + 0x1C, rom32(0x05818C + a));
    wr32(*slot + 0x18, rom32(0x0581AC + a));
    wr16(*slot + O_TYPE, 0xF4);
    wr32(*slot + 0x48, rd32(obj + 0x4C));
    wr16(*slot + 0x50, rd16(obj + 0x50));
    wr16(*slot + O_X, rd16(obj + O_X));
    wr16(*slot + O_Y, rd16(obj + O_Y));
    return 1;
}

/* $057C4C. Type $F0: a spark thrown out each frame for +$48 frames. */
void SparkThrower(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 1);
        wr16(obj + O_FLAGS, 0xF00);
        wr8(obj + 0x21, 0);
        wr16(obj + 0x5A, 0);
        return;
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48) & 0x8000) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    uint16_t slot;
    SparkThrow(obj, &slot);
}

/* $057CD0. Type $1D8: seven puffs (type $1DC) fanned over a quarter turn about direction +$48,
 * at random speeds and looks; then gone. */
void Fan(uint16_t obj)
{
    QueueSound(0xCE);
    uint16_t a = (uint16_t)((rd16(obj + 0x48) - 0x40) & 0x1FE);
    for (uint16_t k = 6;; k--) {
        Random();
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) break;
        int sh = (rd8(0xFF09) & 1) + 4;
        uint32_t vy = (uint32_t)(int32_t)(int16_t)rom16(0x02977C + a) << sh;
        uint32_t vx = (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + a) << sh;
        wr32(slot + 0x18, vx);
        wr32(slot + 0x1C, vy);
        wr16(slot + O_TYPE, 0x1DC);
        wr32(slot + 0x48, bit8(0xFF09, 2) ? 0x0563EE : 0x05639C);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        a = (uint16_t)((a + 0x18) & 0x1FE);
        if (k == 0) break;
    }
    wr16(obj + O_FLAGS, 0x1000);
}

/* $057D7E. Type $1E0: a splash: itself and six more (type +$4A) thrown out over a half turn about
 * direction +$48, the middle ones fastest. */
void Splash(uint16_t obj)
{
    QueueSound(0xC5);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + O_STATE, 0);
    uint16_t type = rd16(obj + 0x4A);
    uint16_t a = (uint16_t)((rd16(obj + 0x48) - 0x18) & 0x7C);
    uint16_t slot = obj, t;
    for (uint16_t k = 0;; k++) {
        uint32_t vy = rom32(0x05818C + a), vx = rom32(0x0581AC + a);
        unsigned sh = rom16(0x057E0A + 2 * k) & 63;
        vy = sh >= 32 ? 0 : vy << sh;
        vx = sh >= 32 ? 0 : vx << sh;
        wr32(slot + 0x18, (uint32_t)((int32_t)vx >> 2));
        wr32(slot + 0x1C, (uint32_t)((int32_t)vy >> 2));
        wr16(slot + O_TYPE, type);
        wr32(slot + 0x48, 0x05639C);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        a = (uint16_t)((a + 8) & 0x7C);
        if (k == 6) return;
        if (!SlotFreeHigh(&slot, &t)) return;
    }
}

/* $057D68. Type $2F4: pieces, and a splash of $100s straight up, a little higher. */
void SplashUp(uint16_t obj)
{
    Pieces(obj);
    wr16(obj + 0x48, 0x60);
    wr16(obj + 0x4A, 0x100);
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10));
    Splash(obj);
}

/* $057E46. count+1 puffs (type $F8, animation list `list`) from the object, at angles from
 * `angle` on by `step` (masked by `mask`), speeds from the tables at `sines` (y, by ry) and
 * `cosines` (x, by rx); a slot taken from a puff if need be (which loses the angle and the sine
 * table: the 68k then reads its speeds from the RAM after that slot). */
void RadialBurst(uint16_t obj, uint32_t sines, uint32_t cosines, uint16_t angle, uint16_t step, uint16_t rx,
                 uint16_t ry, uint32_t list, uint16_t mask, uint16_t count)
{
    uint16_t slot, t;
    if (!SlotFreeOrPuff(&slot, &t, &angle, &sines)) return;
    for (;;) {
        wr16(slot + O_TYPE, 0xF8);
        wr32(slot + 0x48, list);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        wr32(slot + 0x1C, (uint32_t)((int32_t)(int16_t)bus_rd16(sines + (uint32_t)(int16_t)angle) * (int16_t)ry));
        wr32(slot + 0x18, (uint32_t)((int32_t)(int16_t)bus_rd16(cosines + (uint32_t)(int16_t)angle) * (int16_t)rx));
        angle = (uint16_t)((angle + step) & mask);
        if (count-- == 0) return;
        if (!SlotFreeOrPuff(&slot, &t, &angle, &sines)) return;
    }
}

/* $057E18. Type $214: a radial burst from its own fields, itself the first puff. */
void RadialBurster(uint16_t obj)
{
    uint16_t angle = rd16(obj + 0x48), step = rd16(obj + 0x4A), rx = rd16(obj + 0x54), ry = rd16(obj + 0x56);
    uint32_t list = rd32(obj + 0x50), sines = 0x02977C;
    uint16_t count = rd16(obj + 0x4C), slot = obj, t;
    for (;;) {
        wr16(slot + O_TYPE, 0xF8);
        wr32(slot + 0x48, list);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        wr32(slot + 0x1C, (uint32_t)((int32_t)(int16_t)bus_rd16(sines + (uint32_t)(int16_t)angle) * (int16_t)ry));
        wr32(slot + 0x18, (uint32_t)((int32_t)(int16_t)rom16(0x0297FC + (uint32_t)(int16_t)angle) * (int16_t)rx));
        angle = (uint16_t)((angle + step) & 0x1FE);
        if (count-- == 0) return;
        if (!SlotFreeOrPuff(&slot, &t, &angle, &sines)) return;
    }
}

/* $057E80. Type $11C: four bursts (type $100) moving off diagonally, itself the first. */
void Burst4(uint16_t obj)
{
    static const uint16_t vy[3] = { 0xFFFF, 1, 1 }, vx[3] = { 0xFFFF, 1, 0xFFFF };
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0565D4);
    wr16(obj + 0x1C, 0xFFFF);
    wr16(obj + 0x18, 1);
    for (int k = 0; k < 3; k++) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x100);
        wr32(slot + 0x48, 0x0565D4);
        wr16(slot + 0x1C, vy[k]);
        wr16(slot + 0x18, vx[k]);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
    }
}

/* $057F10. Type $1A4: a burning scrap falling (at most 4 a frame), slowing sideways, trailing
 * smoke every 4th frame; gone in anything solid. */
void Scrap(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xF00);
    if (MapAttr(rd16(obj + O_X), rd16(obj + O_Y))) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    if ((int16_t)(rd16(obj + 0x1C) - 4) < 0) wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if (rd8(obj + 0x18) & 0x80) wr32(obj + 0x18, rd32(obj + 0x18) + 0x400);
    else wr32(obj + 0x18, rd32(obj + 0x18) - 0x400);
    if (rd16(v_gameFrames) & 3) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x056580);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* $01A246. Type $430: a player's sparkle: an animation played once, over the player as its
 * effects are (EffectPriority). */
void Sparkle(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0x8100);
        wr8(obj + 0x21, 0);
        wr8(obj + 0x09, 0);
        wr16(obj + 0x0C, 0);
        wr8(obj + 0x20, 0);
        wr16(obj + O_STATE, 4);
    }
    SelectAnimation(obj, 0x01A284);
    EffectPriority(obj);
    if (rd8(obj + 0x09) & 0x80) wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x1000);
}

/* The Treasure logo (mode $48): a shape of a few polygons, turned and seen in perspective, comes
 * out of the distance turning, unfolds, settles, then morphs into the logo's flat polygons while
 * its colours change; then the colours cycle until the fade, and the title follows (Start skips
 * to it at once).
 *
 * Each frame the polygons are filled into a screen of tiles in work RAM: a name table at $FF8000
 * (64 x 28 cells) and the tiles at $FF0000. A cell's entry below 16 is one of the 16 solid tiles
 * (TreasureSolidTiles), colour n; from 16 up it is a tile of its own, made as the polygons cover
 * the cell partly. Both are DMAed to VRAM in the vertical blank. Polygons drawn later are ORed
 * over the earlier ones (the colours are chosen for it).
 *
 * The fill runs in work RAM, copied there from ROM ($062F7A to $FF2000, TreasureCopyFill): a
 * polygon is filled a band of 8 lines at a time, each edge stepped down the band (Bresenham)
 * through a table of masks, one tile line at a time. The walkers write their lines with an
 * instruction they rewrite as they go (at $FF2320: move.l for a tile the band has just made,
 * and.l for one it made already, where the other edge has been); that word is in RAM, so the C
 * writes it too and reads it back to choose. The vertices are in a list on the 68k's stack, and
 * the walkers keep their places in it in RAM, so the C puts the list where the 68k's stack would
 * have it (reg_sp). */
#include "game.h"

#define MODE_TABLE 0x0628AEu     /* the mode's steps: word offsets from the table */
#define ANGLE    0x9200          /* the shape's turn about the vertical axis (1024 to the turn) */
#define TILT     0x9202          /* and about the horizontal */
#define DISTANCE 0x920A          /* the shape's distance; 9204-9208 its place (read as longs) */
#define MODEL    0x920C          /* long: the vertices (a count - 1, then x, z, y words each) */
#define POLYS    0x9210          /* long: the polygons */
#define TIMER    0x921A          /* the colour cycle's frames */
#define UNFOLD   0x921C          /* the unfolding's angle */
#define MORPH    0x921E          /* the morph's progress: 0 to $4000 */
#define SINES    0x3000          /* the sine table TreasureSineTable makes: 1024 words */
#define PROJECTED 0x9400         /* the vertices seen: a count, then z, x, y, 0 each ($FFFF: unseen) */
#define SNAPSHOT 0x9600          /* the projected vertices the morph starts from (also a model) */
#define NAME_TABLE 0x8000
#define TILES    0xFFFF0000u     /* the tiles' start: tile 16 (a6 is the end of those made) */
#define TILES_FULL 0xFFFF2000u   /* the fill's code is above */

/* the fill's state */
#define TILE_NEXT 0x9000         /* the next tile's number */
#define COLOURS  0x9002          /* bytes: the front face's colour (in both nibbles), the back's
                                    (0: the back is not drawn); as a word, the face's two pixels */
#define LEFT     0x9100          /* the edges' walkers: */
#define RIGHT    0x9110
#define W_CELL   0               /*   the band's cell the edge is in (ROW_TILES) */
#define W_MASK   2               /*   its mask table's entry (sign-extended: 0 or 4 when done) */
#define W_VERTEX 4               /*   the vertex the edge left (the left edge goes backwards) */
#define W_STEP   6               /*   2 or -2: the edge's way across the cells */
#define W_TABLE  8               /*   its mask table: rightwards at +4, leftwards at +$30 */
#define ROW      0x9120          /* the name table's row the band is in */
#define BAND_MIN 0x9122          /* the cells the band's edges made tiles in */
#define BAND_MAX 0x9124
#define COLOUR   0x9126          /* the face's colour */
#define TILE_END 0x912A          /* long: the end of the tiles made */
#define ROW_TILES 0x8F00         /* the band's tiles: a word per column of cells (0: none) */
#define MASKS_L  0x8E00          /* the masks: pixels from the edge rightwards */
#define MASKS_R  0x8E58          /* and leftwards */
#define OPCODE   0x2320          /* the walkers' write */
#define OP_MOVE  0x2CC2          /* move.l d2,(a6)+ */
#define OP_AND   0xC59E          /* and.l d2,(a6)+ */
#define FILL_CODE 0x062F7Au      /* the fill's code, for $FF2000 */

static uint32_t muls(uint16_t a, uint16_t b) { return (uint32_t)((int32_t)(int16_t)a * (int16_t)b); }
static uint32_t sext(uint16_t v) { return (uint32_t)(int32_t)(int16_t)v; }

/* divs.w: the quotient in the low word, the remainder in the high; unchanged on overflow */
static uint32_t divs(uint32_t n, int16_t d)
{
    int32_t q = (int32_t)n / d, r = (int32_t)n % d;
    return q > 32767 || q < -32768 ? n : (uint32_t)(uint16_t)r << 16 | (uint16_t)q;
}

/* the display off, to another mode */
static void leave(uint16_t mode)
{
    clr8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0);
    wr16(v_mode, mode);
    wr16(v_modeState, 0);
}

/* $062892. Start goes to the title screen (mode 8). */
void Mode48(void)
{
    if ((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0x80) {
        leave(8);
        return;
    }
    uint32_t at = MODE_TABLE + (uint32_t)(int16_t)rd16(v_modeState);
    call68k(at + (uint32_t)(int16_t)rom16(at));
}

/* $0633B0. The solid tiles, the sine table, the fill's code in RAM; the display on. */
void Mode48Start(void)
{
    ScreenResetAlt();
    RunCommands(0x0633DE);
    TreasureSolidTiles();
    TreasureSineTable();
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 2));
    TreasureCopyFill();
}

/* $0628BC. The shape, its place, the palette. */
void Mode48Init(void)
{
    wr32(MODEL, 0x062B62);
    wr32(POLYS, 0x062BA0);
    for (uint16_t i = 0; i < 12; i += 2) wr16((uint16_t)(ANGLE + i), rom16(0x0628F6 + i));
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 2));
    for (uint16_t i = 0; i < 0x80; i += 4) wr32((uint16_t)(v_palette + i), rom32(0x062CF8 + i));
}

/* $062902. Turning, nearer each frame; at $2C0 other colours, at $340 the unfolding. */
void Mode48Approach(void)
{
    wr16(ANGLE, (uint16_t)(rd16(ANGLE) - 0x10));
    wr16(TILT, (uint16_t)(rd16(TILT) + 3));
    wr16(DISTANCE, (uint16_t)(rd16(DISTANCE) + 0x10));
    if (rd16(DISTANCE) < 0x2C0) { TreasureDraw(); return; }
    wr32(POLYS, 0x062BC6);
    if (rd16(DISTANCE) < 0x340) { TreasureDraw(); return; }
    wr16(UNFOLD, 0);
    wr32(POLYS, 0x062BEC);
    for (uint16_t i = 0; i < 0x40; i += 4) wr32((uint16_t)(SNAPSHOT + i), rom32(0x062B62 + i));
    wr32(MODEL, 0xFFFF0000u | SNAPSHOT);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 2));
    Mode48Unfold();
}

/* $062958. Two of the model's vertices (in its RAM copy) swing out, a step of $A a frame. */
void Mode48Unfold(void)
{
    wr16(ANGLE, (uint16_t)(rd16(ANGLE) - 0x10));
    wr16(UNFOLD, (uint16_t)(rd16(UNFOLD) + 0xA));
    int16_t s;
    int16_t c = TreasureSinCos(rd16(UNFOLD), &s);
    uint16_t a = (uint16_t)((c >> 8) - 0x40);
    uint16_t b = (uint16_t)(-(s >> 8) - 0x40);
    wr16(SNAPSHOT + 0x3C, b);
    wr16(SNAPSHOT + 0x34, a);
    wr16(SNAPSHOT + 0x36, b);
    wr16(SNAPSHOT + 0x3A, a);
    if (rd16(UNFOLD) >= 0x40) wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 2));
    TreasureDraw();
}

/* $06299C. A last turn, and the vertices seen kept for the morph. */
void Mode48Settle(void)
{
    wr16(ANGLE, (uint16_t)(rd16(ANGLE) - 0x10));
    WITH_SP(4, TreasureDraw());
    TreasureSnapshot();
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 2));
}

/* $0629B0. A frame of the shape: the tiles cleared, the colours by the turn, the vertices seen,
 * the polygons filled, the DMAs queued. */
void TreasureDraw(void)
{
    TreasureClear();
    TreasurePalette();
    TreasureTransform(PROJECTED + 2, rd32(MODEL));
    WITH_SP(4, TreasureDrawPolys(rd32(POLYS), PROJECTED + 2));
    TreasureQueue();
}

/* $0629D4. Eleven of the palette's colours, each between two by the cosine of the turn (and an
 * angle of its own). */
void TreasurePalette(void)
{
    uint32_t list = 0x062C12;
    uint16_t turn = (uint16_t)(rd16(ANGLE) + 0x200);
    for (int n = 0; n < 11; n++) {
        int16_t s;
        uint16_t c = (uint16_t)(TreasureSinCos((uint16_t)(turn + rom16(list)), &s) + 0x7FFF) >> 8;
        list += 2;
        TreasureMixEntry((uint16_t)((c + 6) & 0x1FF), v_palette, &list);
    }
}

/* $062A10. The vertices seen, and some again (those the logo's polygons use twice), as the
 * morph's start; the palette as the colours' start. */
void TreasureSnapshot(void)
{
    wr16(MORPH, 0);
    uint16_t to = SNAPSHOT;
    uint16_t n = (uint16_t)(rd16(PROJECTED) - 1);
    for (uint16_t from = PROJECTED + 2;; from += 8, to += 8) {
        wr32(to, rd32(from));
        wr32((uint16_t)(to + 4), rd32((uint16_t)(from + 4)));
        if (n-- == 0) break;
    }
    to += 8;
    for (uint32_t list = 0x062A60;; list += 2) {
        int16_t at = (int16_t)rom16(list);
        if (at < 0) break;
        wr16(PROJECTED, (uint16_t)(rd16(PROJECTED) + 1));
        uint16_t from = (uint16_t)(PROJECTED + 2 + at);
        wr32(to, rd32(from));
        wr32((uint16_t)(to + 4), rd32((uint16_t)(from + 4)));
        to += 8;
    }
    wr32(MODEL, 0x062CB8);
    for (uint16_t i = 0; i < 0x20; i += 4) wr32((uint16_t)(v_paletteTarget + i), rd32((uint16_t)(v_palette + i)));
}

/* $062A74. The morph: the vertices from the snapshot to the logo's, $300 a frame, the colours
 * with them; at the end the sprites (the logo's lettering). */
void Mode48Morph(void)
{
    wr16(MORPH, (uint16_t)(rd16(MORPH) + 0x300));
    if (rd16(MORPH) > 0x4000) {
        wr16(MORPH, 0x4000);
        wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 2));
        wr16(TIMER, 0);
        TreasureClear();
        TreasureQueue();
        TreasureSprites();
        Mode48Hold();
        return;
    }
    TreasureClear();
    TreasureBlend(rd16(MORPH), SNAPSHOT, 0x062C6A, PROJECTED + 2);
    WITH_SP(4, TreasureDrawPolys(rd32(MODEL), PROJECTED + 2));
    TreasureFadeList((uint16_t)(rd8(MORPH) << 2), 0x062CE2, v_palette, v_paletteTarget);
    TreasureQueue();
}

/* $062AE0. Two palette lines from one colour to their own, over 32 frames; after 128 the fade,
 * and when it has gone out the title (mode $18). */
void Mode48Hold(void)
{
    if (!rd16(v_fadeSpeed)) {
        if (rd16(v_fadeLevel)) { leave(0x18); return; }
        wr16(TIMER, (uint16_t)(rd16(TIMER) + 1));
        if (rd16(TIMER) == 0x80) {
            wr16(v_fadeSpeed, 0x100);
            wr16(v_fadeLevel, 0);
        }
    }
    uint16_t t = (uint16_t)(rd16(TIMER) << 3);
    if (t >= 0x100) t = 0x100;
    uint32_t list = 0x062D38;
    TreasureMixLine(0x804, t, v_palette + 0x40, &list);
    TreasureMixLine(0x0AE, t, v_palette + 0x60, &list);
}

/* $063392. The cosine (returned) and the sine of an angle (1024 to the turn), from the table. */
int16_t TreasureSinCos(uint16_t angle, int16_t *sin)
{
    uint16_t i = (uint16_t)(angle << 1) & 0x7FE;
    *sin = (int16_t)rd16((uint16_t)(SINES + i));
    return (int16_t)rd16((uint16_t)(SINES + ((i + 0x200) & 0x7FE)));
}

/* $063366. The sine table: the ROM's quarter-resolution one ($4000 for 1) doubled to $7FFF,
 * odd, each entry 4 times. */
void TreasureSineTable(void)
{
    uint16_t to = SINES;
    for (uint32_t from = 0x02977C; from < 0x02977C + 0x200; from += 2) {
        uint16_t v = rom16(from);
        uint16_t d = (uint16_t)(v << 1);
        if ((v ^ d) & 0x8000) d--;                       /* overflowed */
        d |= 1;
        for (int k = 0; k < 4; k++, to += 2) wr16(to, d);
    }
}

/* $063408. The 16 solid tiles at $FF0000 (colour n's pixels all n), and their DMA to VRAM 0. */
void TreasureSolidTiles(void)
{
    TreasureQueueDma(0x063458);
    wr32(COLOURS, 0);
    for (uint16_t at = 0; at < 0x200; at += 0x20) {
        for (uint16_t i = 0; i < 0x20; i += 4) wr32((uint16_t)(at + i), rd32(COLOURS));
        wr32(COLOURS, rd32(COLOURS) + 0x11111111u);
    }
}

/* $06343E. A DMA from work RAM ($FF0000 up) into the queue: three longs of its registers from
 * the template, the source's top register between. */
void TreasureQueueDma(uint32_t tmpl)
{
    uint16_t q = (uint16_t)(rd16(v_dmaQueue) - 16);
    wr16(v_dmaQueue, q);
    wr32(q, rom32(tmpl));
    wr32((uint16_t)(q + 4), 0x8F02977Fu);
    wr32((uint16_t)(q + 8), rom32(tmpl + 4));
    wr32((uint16_t)(q + 12), rom32(tmpl + 8));
}

/* $06347C. The name table cleared, no tiles made. */
void TreasureClear(void)
{
    for (uint16_t i = 0; i < 0xE00; i += 4) wr32((uint16_t)(NAME_TABLE + i), 0);
    reg_a6 = TILES;
    wr16(TILE_NEXT, 0x10);
}

/* $06349A. The name table's DMA, and the tiles' (as many as were made). */
void TreasureQueue(void)
{
    TreasureQueueDma(0x063464);
    uint16_t bytes = (uint16_t)reg_a6;
    if (!bytes) return;
    TreasureQueueDma(0x063470);
    uint16_t words = bytes >> 1;
    wr32(rd16(v_dmaQueue), 0x93009400u | (uint32_t)(words & 0xFF) << 16 | words >> 8);
}

/* $0634C8. The 26 sprites of the logo's lettering. */
void TreasureSprites(void)
{
    uint32_t from = 0x063500;
    uint16_t to = v_sprites;
    for (uint8_t link = 1; link <= 26; link++, from += 6, to += 8) {
        wr16(to, (uint16_t)((int8_t)rom8(from + 4) + 0xDA));
        wr16((uint16_t)(to + 2), (uint16_t)((rom16(from + 2) & 0xFF00) | link));
        wr16((uint16_t)(to + 4), rom16(from));
        wr16((uint16_t)(to + 6), (uint16_t)((int8_t)rom8(from + 5) + 0x123));
    }
    wr8((uint16_t)(to - 5), 0);                          /* the last links to none */
}

/* $062F64. The fill's code into work RAM at $FF2000. */
void TreasureCopyFill(void)
{
    for (uint16_t i = 0; i < 0x3F0; i += 4) wr32((uint16_t)(0x2000 + i), rom32(FILL_CODE + i));
}

/* $062F40. The vertices between the snapshot's and the logo's (x and y; t of $4000 is all the
 * way). */
void TreasureBlend(uint16_t t, uint16_t from, uint32_t to, uint16_t dst)
{
    uint16_t n = bus_rd16(to);
    to += 2;
    do {
        wr16(dst, rd16(from)); dst += 2; from += 2;
        TreasureLerp(t, &from, &to, &dst);
        TreasureLerp(t, &from, &to, &dst);
        wr16(dst, rd16(from)); dst += 2; from += 2;
    } while (n-- != 0);
}

/* $062F50 */
void TreasureLerp(uint16_t t, uint16_t *from, uint32_t *to, uint16_t *dst)
{
    uint16_t a = rd16(*from);
    uint16_t b = bus_rd16(*to);
    *from += 2;
    *to += 2;
    uint32_t d = muls(t, (uint16_t)(b - a)) << 2;
    wr16(*dst, (uint16_t)((d >> 16) + a));
    *dst += 2;
}

/* $062D78. The colours a list names (palette offsets, each with the colour to go to), from the
 * ones at `from` toward theirs by t / 256. */
void TreasureFadeList(uint16_t t, uint32_t list, uint16_t dst, uint16_t from)
{
    t &= 0x1FF;
    for (;;) {
        int16_t at = (int16_t)bus_rd16(list);
        list += 2;
        if (at < 0) return;
        TreasureMix(rd16((uint16_t)(from + at)), t, (uint16_t)at, dst, &list);
    }
}

/* $062D8A. A colour from a list entry: its offset, the colour from, then (TreasureMix) the one to. */
void TreasureMixEntry(uint16_t t, uint16_t dst, uint32_t *list)
{
    uint16_t at = bus_rd16(*list);
    uint16_t from = bus_rd16(*list + 2);
    *list += 4;
    TreasureMix(from, t, at, dst, list);
}

/* $062D8E. Colour `from` toward the list's next by t / 256, each component apart, into dst + at. */
void TreasureMix(uint16_t from, uint16_t t, uint16_t at, uint16_t dst, uint32_t *list)
{
    uint16_t to = bus_rd16(*list);
    *list += 2;
    uint16_t c = TreasureMixComponent(0x00F, from, t, to);
    c |= TreasureMixComponent(0x0F0, from, t, to);
    c |= TreasureMixComponent(0xF00, from, t, to);
    wr16((uint16_t)(dst + (int16_t)at), c);
}

/* $062DAE */
uint16_t TreasureMixComponent(uint16_t mask, uint16_t from, uint16_t t, uint16_t to)
{
    uint16_t a = from & mask;
    uint32_t d = muls((uint16_t)((to & mask) - a), t) >> 8;
    return (uint16_t)((uint16_t)(d + a) & mask);
}

/* $062DC2. A palette line of 16 colours, all from one toward the list's own. */
void TreasureMixLine(uint16_t from, uint16_t t, uint16_t dst, uint32_t *list)
{
    t &= 0x1FF;
    for (uint16_t at = 0; at < 0x20; at += 2) TreasureMix(from, t, at, dst, list);
}

/* $062DD4. The model's vertices turned (by ANGLE about the vertical, then by TILT), moved and
 * seen in perspective into dst. A negative count is a model of angles and distances instead (no
 * model the game has). */
void TreasureTransform(uint16_t dst, uint32_t model)
{
    int16_t s1, s2;
    int16_t c1 = TreasureSinCos(rd16(ANGLE), &s1);
    int16_t c2 = TreasureSinCos(rd16(TILT), &s2);
    uint32_t turn2 = (uint32_t)(uint16_t)c2 << 16 | (uint16_t)s2;
    uint16_t n = bus_rd16(model);
    model += 2;
    if (!(n & 0x8000)) {
        wr16(PROJECTED, (uint16_t)(n + 1));
        do {
            uint16_t x = bus_rd16(model), z = bus_rd16(model + 2);
            model += 4;
            uint32_t px = (muls(x, (uint16_t)s1) << 1) + (muls(z, (uint16_t)c1) << 1);
            uint32_t pz = (muls(x, (uint16_t)c1) << 1) - (muls(z, (uint16_t)s1) << 1);
            TreasureProject(px, pz, turn2, &dst, &model);
        } while (n-- != 0);
    } else {
        n = bus_rd16(model);
        model += 2;
        wr16(PROJECTED, (uint16_t)(n + 1));
        int16_t c, s;
        c = TreasureSinCos(rd16(TILT), &s);
        turn2 = (turn2 & 0xFFFF0000u) | (uint16_t)s;
        do {
            c = TreasureSinCos((uint16_t)(bus_rd16(model) - rd16(ANGLE)), &s);
            uint16_t r = bus_rd16(model + 2);
            model += 4;
            TreasureProject(muls(r, (uint16_t)c), muls((uint16_t)s, r), turn2, &dst, &model);
        } while (n-- != 0);
    }
    wr32(dst, 0);
    wr32((uint16_t)(dst + 4), 0);
}

/* $062E68. One vertex: turned by TILT (y from the model), moved, divided by its depth: z, x, y,
 * 0 into dst; $FFFF, 0, 0, 0 when it is behind the eye or off the screen. */
void TreasureProject(uint32_t px, uint32_t pz, uint32_t turn, uint16_t *dst, uint32_t *model)
{
    uint16_t sin = (uint16_t)turn, cos = (uint16_t)(turn >> 16);
    uint16_t h = (uint16_t)(pz >> 16);
    uint16_t y = bus_rd16(*model);
    *model += 2;
    uint32_t py = (muls(y, cos) << 1) - (muls(h, sin) << 1);
    uint16_t z = (uint16_t)(((muls(h, cos) << 1) + muls(y, sin)) >> 16);
    z = (uint16_t)(z + rd16(DISTANCE));
    uint16_t at = *dst;
    if ((int16_t)z > 0) {
        uint16_t sx = (uint16_t)(divs((uint32_t)((int32_t)(px + rd32(0x9206)) >> 8), (int16_t)z) + 0xA0);
        if (sx < 0x200) {
            uint16_t sy = (uint16_t)(divs((uint32_t)((int32_t)(py + rd32(0x9208)) >> 8), (int16_t)z) + 0x58);
            if (sy < 0xE0) {
                wr16(at, z);
                wr16((uint16_t)(at + 2), sx);
                wr16((uint16_t)(at + 4), sy);
                wr16((uint16_t)(at + 6), 0);
                *dst = (uint16_t)(at + 8);
                return;
            }
        }
    }
    wr16(at, 0xFFFF);
    wr32((uint16_t)(at + 2), 0);
    wr16((uint16_t)(at + 6), 0);
    *dst = (uint16_t)(at + 8);
}

/* $062ED2. The polygons, while there is room for tiles: each one's vertices (offsets into the
 * projected list) into a list on the stack, from the topmost (the last of the highest), and
 * filled; a polygon with a vertex unseen is skipped. */
void TreasureDrawPolys(uint32_t list, uint16_t verts)
{
    uint16_t buf = (uint16_t)(reg_sp - 0x80 + 4);
    uint16_t polys = bus_rd16(list);
    list += 2;
    do {
        if (reg_a6 > TILES_FULL) return;
        wr8(COLOURS, bus_rd8(list));
        wr8(COLOURS + 1, bus_rd8(list + 1));
        uint16_t last = bus_rd8(list + 2);
        list += 3;
        uint16_t end = buf, left = last;
        uint32_t top = 0xFFFFFFFFu;                      /* the topmost's count left, and y */
        int unseen = 0;
        for (;;) {
            uint16_t v = (uint16_t)(verts + bus_rd8(list++));
            if (rd16(v) & 0x8000) { list += (uint32_t)(int16_t)left; unseen = 1; break; }
            uint32_t xy = rd32((uint16_t)(v + 2));
            wr32(end, xy);
            end += 4;
            if ((uint16_t)xy <= (uint16_t)top) top = (uint32_t)left << 16 | (uint16_t)xy;
            if (left-- == 0) break;
        }
        if (unseen) continue;
        uint16_t first = buf;
        for (uint16_t k = (uint16_t)(last - (top >> 16)); k; k--, first += 4, end += 4)
            wr32(end, rd32(first));                      /* those before the topmost after */
        TreasureFillRam(first, end);
    } while (polys-- != 0);
}

/* $062FCC. The fill, in RAM. */
void TreasureFillRam(uint16_t first, uint16_t end)
{
    TreasureFill(first, end);
}

/* ---- the fill ($FF2000-$FF23EF) ---- */

/* the walkers' state, two of each register's halves on the 68k (swapped between the edges) */
typedef struct {
    uint16_t err;            /* d3 */
    uint16_t lines;          /* d4: the edge's lines left */
    uint16_t dx2, dy2;       /* d5, d6 */
} Edge;

static uint32_t tile_at(uint16_t n) { return 0xFFFEFE00u + sext((uint16_t)(n << 5)); }

/* $FF2000. The masks in the face's colour. MASKS_L's have the pixels from the edge rightwards (a
 * left edge's), MASKS_R's those up to it. Each table: from +4 the edge at pixel 0..7 going right,
 * then 0 (the cell's end) and the mask for the rest of the cell left behind (+$28); from +$30 the
 * edge at pixel 7..0 going left, 0, and the rest's (+$54); at +0 and +$2C the masks for the next
 * cell's lines above. */
static void make_masks(void)
{
    uint32_t p = (uint32_t)rd16(COLOURS) * 0x10001u;
    uint32_t m = p;
    wr32(MASKS_L, m);
    wr32(MASKS_L + 0x54, m);
    wr32(MASKS_L + 0x50, 0);
    for (uint16_t i = 0; i < 0x20; i += 4, m >>= 4) {
        wr32((uint16_t)(MASKS_L + 4 + i), m);
        wr32((uint16_t)(MASKS_L + 0x4C - i), m);
    }
    wr32(MASKS_L + 0x24, 0);
    wr32(MASKS_L + 0x28, 0);
    wr32(MASKS_L + 0x2C, 0);
    m = p;
    wr32(MASKS_R + 0x2C, m);
    wr32(MASKS_R + 0x28, m);
    wr32(MASKS_R + 0x24, 0);
    for (uint16_t i = 0; i < 0x20; i += 4, m <<= 4) {
        wr32((uint16_t)(MASKS_R + 0x30 + i), m);
        wr32((uint16_t)(MASKS_R + 0x20 - i), m);
    }
    wr32(MASKS_R + 0x50, 0);
    wr32(MASKS_R + 0x54, 0);
    wr32(MASKS_R, 0);
}

/* a tile line: moved into a tile the band has just made, ANDed into one it has */
static void put(uint32_t *line, uint32_t mask, int move)
{
    uint16_t a = (uint16_t)*line;
    wr32(a, move ? mask : rd32(a) & mask);
    *line += 4;
}

static void put_lines(uint32_t *line, uint32_t mask, uint16_t n, int move)
{
    while (n--) put(line, mask, move);
}

/* $FF221A, $FF2394: the edge into a cell: its tile, or a new one */
static void take_cell(uint16_t cell, uint32_t *line)
{
    uint16_t t = rd16(cell);
    if (t) {
        wr16(OPCODE, OP_AND);
        *line = tile_at(t);
        return;
    }
    wr16(OPCODE, OP_MOVE);
    wr16(cell, rd16(TILE_NEXT));
    wr16(TILE_NEXT, (uint16_t)(rd16(TILE_NEXT) + 1));
    *line = rd32(TILE_END);
    wr32(TILE_END, rd32(TILE_END) + 0x20);
    if (cell < rd16(BAND_MIN)) wr16(BAND_MIN, cell);
    if (cell > rd16(BAND_MAX)) wr16(BAND_MAX, cell);
}

/* $FF2348. Into the next cell: the old one's `rows` lines left with the mask past its table's
 * end (all or none), then the new one's lines above with the table's first. */
static void cross(uint16_t w, uint16_t *cell, uint32_t *at, uint16_t rows, uint32_t *line)
{
    uint32_t rest = bus_rd32(*at);
    *at -= 0x28;
    put_lines(line, rest, rows, rd16(OPCODE) == OP_MOVE);
    *cell = (uint16_t)(*cell + rd16((uint16_t)(w + W_STEP)));
    uint32_t mask = bus_rd32(*at);
    *at += 4;
    take_cell(*cell, line);
    put_lines(line, mask, (uint16_t)(8 - rows), rd16(OPCODE) == OP_MOVE);
}

/* $FF226A. The edge's next segment, from its vertex to the next (the left edge goes backwards
 * round the list, the right forwards): its step, its first mask; a level one crosses to the next
 * vertex's cell at once. 1 when the edge is done: the next vertex is higher (the polygon's
 * bottom), or the edges have crossed. */
static int next_segment(uint16_t w, Edge *e, uint16_t *cell, uint32_t *at, uint32_t *mask, uint16_t rows,
                        uint32_t *line)
{
    for (;;) {
        uint16_t v = rd16((uint16_t)(w + W_VERTEX));
        uint32_t from = rd32(v);
        v = w == RIGHT ? (uint16_t)(v + 4) : (uint16_t)(v - 4);
        uint32_t to = rd32(v);
        wr16((uint16_t)(w + W_VERTEX), v);
        *at = sext(rd16((uint16_t)(w + W_TABLE)));
        uint16_t y0 = (uint16_t)from, y1 = (uint16_t)to;
        if (y1 < y0) return 1;
        uint16_t dy = (uint16_t)(y1 - y0);
        uint16_t x = (uint16_t)(from >> 16), x1 = (uint16_t)(to >> 16);
        uint16_t dx = (uint16_t)(x1 - x);
        if (!(dx & 0x8000)) {
            wr16((uint16_t)(w + W_STEP), 2);
            *at += 4;
        } else {
            wr16((uint16_t)(w + W_STEP), 0xFFFE);
            x = (uint16_t)~x;
            dx = (uint16_t)-dx;
            *at += 0x30;
        }
        e->dx2 = dx;
        e->dy2 = dy;
        e->lines = dy;
        if (dy) {
            *at += (uint32_t)(x & 7) << 2;
            *mask = bus_rd32(*at);
            *at += 4;
            e->err = (uint16_t)-dy;
            e->dx2 = (uint16_t)(dx << 1);
            e->dy2 = (uint16_t)(dy << 1);
            e->lines = (uint16_t)(dy - 1);
            return 0;
        }
        uint16_t target = (uint16_t)(ROW_TILES + ((x1 >> 2) & 0xFFFE));
        while (target != *cell) {
            *at += 0x24;
            cross(w, cell, at, (uint16_t)(rows + 1), line);
            if (rd16(LEFT + W_VERTEX) < rd16(RIGHT + W_VERTEX)) return 1;
        }
    }
}

/* $FF2212. One edge down the band's 8 lines: a line of its tile a line, the mask moved along
 * as the edge moves (into the next cell past the table's end). */
static void walk(uint16_t w, Edge *e)
{
    uint16_t cell = rd16((uint16_t)(w + W_CELL));
    uint32_t at = sext(rd16((uint16_t)(w + W_MASK)));
    uint32_t mask = bus_rd32(at), line;
    at += 4;
    take_cell(cell, &line);
    uint16_t rows = 7;
    do {
        if (e->lines-- == 0 && next_segment(w, e, &cell, &at, &mask, rows, &line)) {
            mask = 0;                                    /* done: empty lines to the end */
            at = 4;
            e->dx2 = 0;
            e->dy2 = 0;
            e->err = 0xFFFF;
            e->lines = 0x7FFF;
        }
        put(&line, mask, rd16(OPCODE) == OP_MOVE);
        e->err = (uint16_t)(e->err + e->dx2);
        while (!(e->err & 0x8000)) {
            mask = bus_rd32(at);
            at += 4;
            if (!mask) {
                cross(w, &cell, &at, rows, &line);
                continue;
            }
            e->err = (uint16_t)(e->err - e->dy2);
        }
    } while (rows-- != 0);
    wr16((uint16_t)(w + W_MASK), (uint16_t)(at - 4));
    wr16((uint16_t)(w + W_CELL), cell);
}

/* the pattern of a colour (nibble) or a pair: all 8 pixels */
static uint32_t pattern(uint16_t c)
{
    uint8_t b = (uint8_t)(c | c << 4);
    return (uint32_t)b * 0x01010101u;
}

static void or_tile(uint32_t tile, uint32_t p)
{
    for (uint16_t i = 0; i < 0x20; i += 4) wr32((uint16_t)(tile + i), rd32((uint16_t)(tile + i)) | p);
}

/* $FF2148. The band's tiles into the name table's row, over what is there: a cell with no tile
 * of the band's was inside the face (its colour, or a tile there gets it); a tile of the band's
 * gets a solid colour there, or is ORed into a tile there (and given back when it was the last
 * made). */
static void merge_band(void)
{
    uint16_t at = rd16(BAND_MIN);
    uint16_t nt = (uint16_t)(rd16(ROW) + (uint16_t)(at - ROW_TILES));
    uint16_t n = (uint16_t)((int16_t)(rd16(BAND_MAX) - at) >> 1);
    do {
        uint16_t t = rd16(at), old = rd16(nt);
        if (!t) {
            if (old >= 16) {
                or_tile(tile_at(old), pattern(rd16(COLOURS)));
                t = old;
            } else {
                t = (uint16_t)(rd16(COLOUR) | old);
            }
        } else if (old && old < 16) {
            or_tile(tile_at(t), pattern(old));
        } else if (old) {
            uint32_t from = tile_at(t), to = tile_at(old);
            for (uint16_t i = 0; i < 0x20; i += 4)
                wr32((uint16_t)(to + i), rd32((uint16_t)(to + i)) | rd32((uint16_t)(from + i)));
            if (from + 0x20 == rd32(TILE_END)) {
                wr32(TILE_END, rd32(TILE_END) - 0x20);
                wr16(TILE_NEXT, (uint16_t)(rd16(TILE_NEXT) - 1));
            }
            t = old;
        }
        wr16(nt, t);
        nt += 2;
        wr16(at, 0);
        at += 2;
    } while (n-- != 0);
    wr16(ROW, (uint16_t)(rd16(ROW) + 0x80));
}

/* $FF2058. A polygon (its vertices from first, the topmost, to end): the face by the winding,
 * then bands of 8 lines from the top until the right edge is done. */
void TreasureFill(uint16_t first, uint16_t end)
{
    wr16(RIGHT + W_VERTEX, first);
    wr16(LEFT + W_VERTEX, end);
    uint32_t top = rd32(first), next = rd32((uint16_t)(first + 4));
    wr32(end, top);
    uint32_t prev = rd32((uint16_t)(end - 4));
    uint16_t x0 = (uint16_t)(top >> 16), y0 = (uint16_t)top;
    int32_t a = (int32_t)muls((uint16_t)((next >> 16) - x0), (uint16_t)((uint16_t)prev - y0));
    int32_t b = (int32_t)muls((uint16_t)((uint16_t)next - y0), (uint16_t)((prev >> 16) - x0));
    if (b > a) {                                         /* the back */
        if (!rd8(COLOURS + 1)) return;
        wr8(COLOURS, rd8(COLOURS + 1));
        wr16(RIGHT + W_TABLE, MASKS_L);
        wr16(LEFT + W_TABLE, MASKS_R);
    } else {
        wr8(COLOURS + 1, rd8(COLOURS));
        wr16(RIGHT + W_TABLE, MASKS_R);
        wr16(LEFT + W_TABLE, MASKS_L);
    }
    make_masks();
    wr16(COLOUR, rd16(COLOURS) & 0xF);
    top = rd32(rd16(RIGHT + W_VERTEX));
    uint16_t x = (uint16_t)(top >> 16), y = (uint16_t)top;
    Edge e[2];
    e[0].lines = e[1].lines = y & 7;                     /* the lines above the top: empty */
    wr16(ROW, (uint16_t)(((y << 4) & 0xFF80) + NAME_TABLE));
    uint16_t cell = (uint16_t)(ROW_TILES + ((x & 0xFFF8) >> 2));
    wr16(LEFT + W_CELL, cell);
    wr16(RIGHT + W_CELL, cell);
    wr16(LEFT + W_MASK, MASKS_L + 0x24);
    wr16(RIGHT + W_MASK, MASKS_L + 0x24);
    for (uint16_t i = 0; i < 0x80; i += 4) wr32((uint16_t)(ROW_TILES + i), 0);
    e[0].dx2 = e[1].dx2 = 0;
    e[0].dy2 = e[1].dy2 = 0;
    e[0].err = e[1].err = 0xFFFF;
    wr32(TILE_END, reg_a6);
    do {
        wr16(BAND_MAX, ROW_TILES);
        wr16(BAND_MIN, ROW_TILES + 0x80);
        walk(LEFT, &e[0]);
        walk(RIGHT, &e[1]);
        merge_band();
    } while (rd16(RIGHT + W_MASK));
    reg_a6 = rd32(TILE_END);
}

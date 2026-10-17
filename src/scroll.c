/* The planes' scrolling. A stage's map is in work RAM in three levels: a layout of blocks of 256
 * pixels (bytes, 32 to a row), each block 8 x 8 chunks of 32 pixels (bytes), each chunk 4 x 4
 * tiles (words). Plane A's are at $FF7000, $FF6000, $FF4000 and it is drawn into $FF0000 (64
 * tiles wide, 128 bytes a row); plane B's at $FF7400, $FF6800, $FF3000 into $FF2000. As a plane
 * moves (v_planes, v_planeSpeeds) the row or the column coming into view is drawn into its
 * buffer and queued for DMA (to VRAM $C000, $E000; a column through the buffer v_columnBuffer),
 * and the scroll tables get the planes' positions. v_planeFlagsA and B: bit 2 no new rows, bit 3
 * no new columns, bits 5 and 4 a plane twice as tall or wide, bits 7 and 6 which plane a
 * per-cell or per-line scroll table is for. */
#include "game.h"

#define LAYOUT_A 0x7000
#define BLOCKS_A 0x6000
#define CHUNKS_A 0x4000
#define MAP_A    0x0000
#define LAYOUT_B 0x7400
#define BLOCKS_B 0x6800
#define CHUNKS_B 0x3000
#define MAP_B    0x2000
#define SCROLL_MODE (VDP_REG(11) + 1)    /* register 11: bit 2 the vertical scroll per 2 cells, bit 1
                                            the horizontal per cell (bit 0: per line) */

static uint16_t ax(void) { return rd16(v_planes); }
static uint16_t ay(void) { return rd16(v_planes + 4); }
static uint16_t bx(void) { return rd16(v_planes + 8); }
static uint16_t by(void) { return rd16(v_planes + 0xC); }
static int a_up(void) { return (rd16(v_planeSpeeds + 4) & 0x8000) != 0; }
static int a_left(void) { return (rd16(v_planeSpeeds) & 0x8000) != 0; }
static int b_up(void) { return (rd16(v_planeSpeeds + 0xC) & 0x8000) != 0; }
static int b_left(void) { return (rd16(v_planeSpeeds + 8) & 0x8000) != 0; }

/* the block under a position (its index in the layout), the chunk in it, the chunk's tiles */
static uint16_t block_at(uint16_t x, uint16_t y) { return (uint16_t)(((y >> 3) & 0x3E0) + ((x >> 8) & 0x1F)); }

static uint16_t chunk_at(uint16_t layout, uint16_t x, uint16_t y, uint16_t b)
{
    return (uint16_t)(((y >> 2) & 0x38) + ((x >> 5) & 7) + (rd8((uint16_t)(layout + b)) << 6));
}

static uint16_t tiles_of(uint16_t blocks, uint16_t chunks, uint16_t c)
{
    return (uint16_t)(chunks + (rd8((uint16_t)(blocks + c)) << 5));
}

/* $009C02 / $009CA6: a row of chunk halves (two tile rows) across plane A from x, n of them */
static void row_a(uint16_t x, uint16_t y, uint16_t n)
{
    uint16_t chunks = (uint16_t)(CHUNKS_A + (y & 0x10));
    uint16_t left = (uint16_t)(n - 1);
    for (;;) {
        uint16_t b = block_at(x, y);
        do {
            uint16_t c = chunk_at(LAYOUT_A, x, y, b);
            uint16_t at = (uint16_t)(MAP_A + ((y << 4) & 0x1F00) + ((x >> 2) & 0x78));
            do {
                uint16_t src = tiles_of(BLOCKS_A, chunks, c);
                wr32((uint16_t)(at + 0x80), rd32((uint16_t)(src + 8)));
                wr32(at, rd32(src));
                wr32((uint16_t)(at + 0x84), rd32((uint16_t)(src + 0xC)));
                wr32((uint16_t)(at + 4), rd32((uint16_t)(src + 4)));
                at = (uint16_t)(at + 8);
                if (left-- == 0) return;
                x = (uint16_t)(x + 0x20);
                c++;
            } while (c & 7);
            b++;
        } while (b & 0x1F);
    }
}

/* $009E30: a column of tile pairs down plane A (16 chunks from y), the chunk's pair at x */
static void column_a(uint16_t x, uint16_t y)
{
    uint16_t chunks = (uint16_t)(CHUNKS_A + ((x >> 2) & 4));
    uint16_t left = 15;
    for (;;) {
        uint16_t b = block_at(x, y);
        do {
            uint16_t c = chunk_at(LAYOUT_A, x, y, b);
            uint16_t at = (uint16_t)(MAP_A + ((y << 4) & 0x1E00) + ((x >> 2) & 0x7C));
            do {
                uint16_t src = tiles_of(BLOCKS_A, chunks, c);
                for (uint16_t r = 0; r < 0x200; r += 0x80) {
                    wr16((uint16_t)(at + r), rd16((uint16_t)(src + (r >> 4))));
                    wr16((uint16_t)(at + r + 2), rd16((uint16_t)(src + (r >> 4) + 2)));
                }
                if (left-- == 0) return;
                y = (uint16_t)(y + 0x20);
                at = (uint16_t)(at + 0x200);
                c = (uint16_t)(c + 8);
            } while (c & 0x38);
            b = (uint16_t)(b + 0x20);
        } while (b & 0x3E0);
    }
}

/* $00A130: a row of chunk rows across plane B (16 chunks), the chunk's row at y */
static void row_b(uint16_t x, uint16_t y)
{
    uint16_t chunks = (uint16_t)(CHUNKS_B + (y & 0x18));
    uint16_t left = 15;
    for (;;) {
        uint16_t b = block_at(x, y);
        do {
            uint16_t c = chunk_at(LAYOUT_B, x, y, b);
            uint16_t at = (uint16_t)(MAP_B + ((y << 4) & 0xF80) + ((x >> 2) & 0x78));
            do {
                uint16_t src = tiles_of(BLOCKS_B, chunks, c);
                wr32(at, rd32(src));
                wr32((uint16_t)(at + 4), rd32((uint16_t)(src + 4)));
                at = (uint16_t)(at + 8);
                if (left-- == 0) return;
                x = (uint16_t)(x + 0x20);
                c++;
            } while (c & 7);
            b++;
        } while (b & 0x1F);
    }
}

/* $00A1F8: a column of single tiles down plane B (8 chunks), the chunk's column at x */
static void column_b(uint16_t x, uint16_t y)
{
    uint16_t chunks = (uint16_t)(CHUNKS_B + ((x >> 2) & 6));
    uint16_t left = 7;
    for (;;) {
        uint16_t b = block_at(x, y);
        do {
            uint16_t c = chunk_at(LAYOUT_B, x, y, b);
            uint16_t at = (uint16_t)(MAP_B + ((y << 4) & 0xE00) + ((x >> 2) & 0x7E));
            do {
                uint16_t src = tiles_of(BLOCKS_B, chunks, c);
                for (uint16_t r = 0; r < 0x200; r += 0x80) wr16((uint16_t)(at + r), rd16((uint16_t)(src + (r >> 4))));
                if (left-- == 0) return;
                y = (uint16_t)(y + 0x20);
                at = (uint16_t)(at + 0x200);
                c = (uint16_t)(c + 8);
            } while (c & 0x38);
            b = (uint16_t)(b + 0x20);
        } while (b & 0x3E0);
    }
}

/* $00976C. Plane A's row coming into view ($80 above, $160 below), 16 chunks from $60 left. */
void ScrollRowA(void) { row_a((uint16_t)(ax() - 0x60), (uint16_t)(ay() + (a_up() ? -0x80 : 0x160)), 16); }

/* $009790. The battleship's: 11 from $40 right. */
void ScrollRowAShort(void) { row_a((uint16_t)(ax() + 0x40), (uint16_t)(ay() + (a_up() ? -0x80 : 0x160)), 11); }

/* $0097B4. Plane A's column coming into view ($60 left, $180 right), from $80 above. */
void ScrollColumnA(void) { column_a((uint16_t)(ax() + (a_left() ? -0x60 : 0x180)), (uint16_t)(ay() - 0x80)); }

/* $0097D8. Plane B's row (at its y going up, $E0 below going down), from $60 left. */
void ScrollRowB(void) { row_b((uint16_t)(bx() - 0x60), (uint16_t)(by() + (b_up() ? 0 : 0xE0))); }

/* $0097F4. Plane B's column. */
void ScrollColumnB(void) { column_b((uint16_t)(bx() + (b_left() ? -0x60 : 0x180)), by()); }

/* ---- the DMA queue's entries: written downward from v_dmaQueue ---- */

/* $009DF0. The VRAM write command for an address (two words). */
uint16_t QueueVram(uint16_t q, uint16_t vram)
{
    q = (uint16_t)(q - 2);
    wr16(q, (uint16_t)(((vram & 0xC000) >> 14) | 0x80));
    q = (uint16_t)(q - 2);
    wr16(q, (uint16_t)((vram & 0x3FFF) | 0x4000));
    return q;
}

/* $009E0A. A source (a 68k address, halved) as registers 21-23. */
uint16_t QueueSource(uint16_t q, uint32_t src)
{
    src = (src & 0xFFFFFF) >> 1;
    q = (uint16_t)(q - 2);
    wr16(q, (uint16_t)(0x9500 | (src & 0xFF)));
    q = (uint16_t)(q - 2);
    wr16(q, (uint16_t)(0x9600 | ((src >> 8) & 0xFF)));
    q = (uint16_t)(q - 2);
    wr16(q, (uint16_t)(0x9700 | ((src >> 16) & 0xFF)));
    return q;
}

/* the auto-increment and the length registers */
static uint16_t queue_length(uint16_t q, uint16_t inc, uint32_t length)
{
    q = (uint16_t)(q - 2);
    wr16(q, inc);
    q = (uint16_t)(q - 4);
    wr32(q, length);
    return q;
}

/* a row of a plane's buffer to VRAM: y's row (two rows when tall) */
static void dma_row(uint16_t y, uint16_t vmask, uint16_t vram, uint16_t rmask, uint32_t buffer, uint32_t length)
{
    uint16_t d = (uint16_t)(y << 4);
    uint16_t q = QueueVram(rd16(v_dmaQueue), (uint16_t)((d & vmask) + vram));
    q = QueueSource(q, buffer + (uint16_t)(d & rmask));
    wr16(v_dmaQueue, queue_length(q, 0x8F02, length));
}

/* $009814 (from $00A37A): plane A's new row ($E0 below going down) */
static void dma_row_a(void)
{
    uint16_t y = (uint16_t)(ay() + (a_up() ? 0 : 0xE0));
    if (bit8(v_planeFlagsA, 5)) dma_row(y, 0xF00, 0xC000, 0x1F00, 0xFFFF0000u, 0x94009380u);
    else dma_row(y, 0xF80, 0xC000, 0x1F80, 0xFFFF0000u, 0x94009340u);
}

/* $009830: the battleship's (half a row, from the 8th cell) */
static void dma_row_a_short(void)
{
    uint16_t y = (uint16_t)(ay() + (a_up() ? 0 : 0xE0));
    dma_row(y, 0xF80, 0xC010, 0x1F80, 0xFFFF0010u, 0x94009320u);
}

/* $00A1C8 (from $009870): plane B's new row */
static void dma_row_b(void)
{
    uint16_t y = (uint16_t)(by() + (b_up() ? 0 : 0xE0));
    dma_row(y, 0xF80, 0xE000, 0xF80, 0xFFFF2000u, 0x94009340u);
}

/* $009870 */
void ScrollRowBDma(void) { dma_row_b(); }

/* one column of four rows, a group of the gather */
static void gather4(uint16_t *to, uint32_t from)
{
    for (uint16_t r = 0; r < 0x200; r += 0x80) {
        wr16(*to, rd16((uint16_t)(from + r)));
        *to = (uint16_t)(*to + 2);
    }
}

/* $009F8E. Plane A's column at byte `col` of a row, 32 rows from y's (y here $80 above the
 * view) into the buffer `to`, wrapping round the plane at the half it starts in. Where the wrap
 * comes it copies a group more than the 8 (the 68k's dbeq). */
uint16_t ColumnGather(uint16_t col, uint16_t y, uint16_t to)
{
    uint16_t d1 = (uint16_t)((uint16_t)((uint16_t)(y + 0x80) << 4) & 0x1E00);
    uint32_t from, stop;
    int32_t back;
    if (d1 && d1 <= 0x1000) {
        from = 0xFFFF1000u + (uint32_t)(int16_t)col;
        stop = 0xFFFF1000u + (uint32_t)(int16_t)(uint16_t)(d1 + col);
        back = -0x1000;
    } else {
        from = 0xFFFF0000u + (uint32_t)(int16_t)col;
        stop = 0xFFFEF000u + (uint32_t)(int16_t)(uint16_t)(d1 + col);
        back = 0x1000;
    }
    int16_t n = 7;
    for (;;) {
        gather4(&to, from);
        from += 0x200;
        if (from == stop) break;
        if (--n < 0) return to;
    }
    from += (uint32_t)back;
    do {
        gather4(&to, from);
        from += 0x200;
    } while (--n >= 0);
    return to;
}

/* one group of two columns: the even one to `even`, the odd to `odd` */
static void gather4x2(uint16_t *even, uint16_t *odd, uint32_t from)
{
    for (uint16_t r = 0; r < 0x200; r += 0x80) {
        wr16(*even, rd16((uint16_t)(from + r)));
        *even = (uint16_t)(*even + 2);
        wr16(*odd, rd16((uint16_t)(from + r + 2)));
        *odd = (uint16_t)(*odd + 2);
    }
}

/* $00A040. Two columns (a plane twice as wide): 8 groups, wrapping the same way. */
void ColumnGather2(uint16_t col, uint16_t y, uint16_t *even, uint16_t *odd)
{
    uint16_t d1 = (uint16_t)((uint16_t)((uint16_t)(y + 0x80) << 4) & 0x1E00);
    uint32_t from, stop;
    int32_t back;
    if (d1 && d1 <= 0x1000) {
        from = 0xFFFF1000u + (uint32_t)(int16_t)col;
        stop = 0xFFFF1000u + (uint32_t)(int16_t)(uint16_t)(d1 + col);
        back = -0x1000;
    } else {
        from = 0xFFFF0000u + (uint32_t)(int16_t)col;
        stop = 0xFFFEF000u + (uint32_t)(int16_t)(uint16_t)(d1 + col);
        back = 0x1000;
    }
    int16_t n = 7;
    for (;;) {
        gather4x2(even, odd, from);
        from += 0x200;
        if (--n < 0) return;
        if (from == stop) break;
    }
    from += (uint32_t)back;
    do {
        gather4x2(even, odd, from);
        from += 0x200;
    } while (--n >= 0);
}

/* $00984C (from $00A3B8): plane A's new column to VRAM, through the column buffer (one, or with
 * bit 4 of v_planeFlagsA two) */
static void dma_column_a(void)
{
    uint16_t x = (uint16_t)(ax() + (a_left() ? -0x60 : 0x180)), y = (uint16_t)(ay() - 0x80);
    uint16_t q = rd16(v_dmaQueue), buf = rd16(v_columnBuffer);
    if (!bit8(v_planeFlagsA, 4)) {
        uint16_t col = (uint16_t)((x >> 2) & 0x7E);
        q = QueueVram(q, (uint16_t)(col + 0xC000));
        q = QueueSource(q, (uint32_t)(int16_t)buf);
        buf = ColumnGather(col, y, buf);
        q = queue_length(q, 0x8F80, 0x94009320u);
    } else {
        uint16_t col = (uint16_t)((x >> 2) & 0x7C);
        q = QueueVram(q, (uint16_t)(col + 0xC000));
        q = QueueSource(q, (uint32_t)(int16_t)buf);
        q = queue_length(q, 0x8F80, 0x94009320u);
        q = (uint16_t)(q - 4);
        wr32(q, rd32((uint16_t)(q + 0x10)));
        wr16(q, (uint16_t)(rd16(q) + 2));
        uint16_t even = buf;
        buf = (uint16_t)(buf + 0x40);
        q = QueueSource(q, (uint32_t)(int16_t)buf);
        q = queue_length(q, 0x8F80, 0x94009320u);
        ColumnGather2(col, y, &even, &buf);
    }
    wr16(v_dmaQueue, q);
    wr16(v_columnBuffer, buf);
}

/* $00A2AA (from $00988C): plane B's new column */
static void dma_column_b(void)
{
    uint16_t x = (uint16_t)(bx() + (b_left() ? -0x60 : 0x180));
    uint16_t q = rd16(v_dmaQueue), buf = rd16(v_columnBuffer);
    uint16_t col = (uint16_t)((x >> 2) & 0x7E);
    q = QueueVram(q, (uint16_t)(col + 0xE000));
    q = QueueSource(q, (uint32_t)(int16_t)buf);
    for (uint16_t from = (uint16_t)(MAP_B + col), k = 0; k < 8; k++, from = (uint16_t)(from + 0x200)) gather4(&buf, from);
    q = queue_length(q, 0x8F80, 0x94009320u);
    wr16(v_dmaQueue, q);
    wr16(v_columnBuffer, buf);
}

/* $00988C */
void ScrollColumnBDma(void) { dma_column_b(); }

/* ---- the scroll tables ---- */

/* $00A434. The horizontal scroll table: the whole screen's (planes A and B), or by cell or by
 * line one plane's (bits 7 of the planes' flags: B's unless it is A's), at the camera. */
void HScrollTable(void)
{
    uint16_t t = (uint16_t)rd32(v_hscrollSrc);
    uint8_t mode = rd8(SCROLL_MODE);
    if (!(mode & 2)) {
        wr16(t, (uint16_t)-rd16(v_camX));
        wr16((uint16_t)(t + 2), (uint16_t)-rd16(v_camX + 8));
        return;
    }
    uint16_t v;
    if (!(rd8(v_planeFlagsB) & 0x80)) {
        t = (uint16_t)(t + 2);
        v = rd16(v_camX + 8);
    } else {
        if (rd8(v_planeFlagsA) & 0x80) return;
        v = rd16(v_camX);
    }
    v = (uint16_t)-v;
    if (!(mode & 1)) {
        for (uint16_t i = 0; i < 0x380; i += 0x20) wr16((uint16_t)(t + i), v);
    } else {
        for (uint16_t i = 0; i < 0x380; i += 4) wr16((uint16_t)(t + i), v);
    }
}

/* $00A53E. The vertical scroll table: the whole screen's, or one plane's for every 2 cells
 * (bits 6), at the camera less the shakes. */
void VScrollTable(void)
{
    uint16_t t = (uint16_t)rd32(v_vscrollSrc);
    if (!(rd8(SCROLL_MODE) & 4)) {
        wr16(t, (uint16_t)(rd16(v_camY) - rd16(v_cameraY)));
        wr16((uint16_t)(t + 2), (uint16_t)(rd16(v_camY + 8) - rd16(v_planeShake)));
        return;
    }
    uint16_t v;
    if (!bit8(v_planeFlagsB, 6)) {
        t = (uint16_t)(t + 2);
        v = (uint16_t)(rd16(v_camY + 8) - rd16(v_planeShake));
    } else {
        if (bit8(v_planeFlagsA, 6)) return;
        v = (uint16_t)(rd16(v_camY) - rd16(v_cameraY));
    }
    for (uint16_t i = 0; i < 0xA0; i += 4) wr16((uint16_t)(t + i), v);
}

/* ---- a whole plane at once (a stage's jump to another part of its map) ---- */

/* a plane's buffer filled with its chunks: `rows` rows of 16 (32 pixels square) from x, y.
 * Across a block the chunks follow on in the buffer; each new block starts its address again
 * from x (and ymask is where the buffer wraps). */
static void fill(uint16_t layout, uint16_t blocks, uint16_t chunks, uint16_t map, uint16_t x0, uint16_t y,
                 uint16_t rows, uint16_t ymask)
{
    for (uint16_t r = 0; r < rows; r++, y = (uint16_t)(y + 0x20)) {
        uint16_t x = x0, left = 15;
        for (;;) {
            uint16_t b = block_at(x, y);
            uint16_t c, at;
            for (;;) {
                c = chunk_at(layout, x, y, b);
                at = (uint16_t)(map + ((y << 4) & ymask) + ((x >> 2) & 0x78));
                for (;;) {
                    uint16_t src = tiles_of(blocks, chunks, c);
                    for (uint16_t k = 0; k < 4; k++) {
                        wr32((uint16_t)(at + 0x80 * k), rd32((uint16_t)(src + 8 * k)));
                        wr32((uint16_t)(at + 0x80 * k + 4), rd32((uint16_t)(src + 8 * k + 4)));
                    }
                    at = (uint16_t)(at + 8);
                    if (left-- == 0) goto next_row;
                    x = (uint16_t)(x + 0x20);
                    c++;
                    if (c & 7) continue;
                    break;
                }
                b++;
                if (b & 0x1F) continue;
                break;
            }
        }
    next_row:;
    }
}

/* $0098AC. Plane A's buffer, all 16 x 16 chunks around the plane's position. */
void PlaneAFill(void)
{
    fill(LAYOUT_A, BLOCKS_A, CHUNKS_A, MAP_A, (uint16_t)(ax() - 0x80), (uint16_t)(ay() - 0x80), 16, 0x1E00);
}

/* $009AD0. Plane B's buffer, 8 rows of 16 chunks. */
void PlaneBFill(void)
{
    fill(LAYOUT_B, BLOCKS_B, CHUNKS_B, MAP_B, (uint16_t)(bx() - 0x80), by(), 8, 0xE00);
}

/* a DMA's length and source registers written through their words in RAM ($FFF7F6 on: each
 * register's $9r byte kept there, the value's byte put after it), then the command */
static void dma_regs(uint16_t bytes, uint32_t src, uint16_t vram)
{
    uint16_t words = (uint16_t)(bytes >> 1);
    wr8(0xF7F7, (uint8_t)words);
    vdp_ctrl16(rd16(0xF7F6));
    wr8(0xF7F9, (uint8_t)(words >> 8));
    vdp_ctrl16(rd16(0xF7F8));
    src >>= 1;
    wr8(0xF7FB, (uint8_t)src);
    vdp_ctrl16(rd16(0xF7FA));
    wr8(0xF7FD, (uint8_t)(src >> 8));
    vdp_ctrl16(rd16(0xF7FC));
    wr8(0xF7FF, (uint8_t)(src >> 16) & 0x7F);
    vdp_ctrl16(rd16(0xF7FE));
    vdp_ctrl16((uint16_t)((vram & 0x3FFF) | 0x4000));
    vdp_ctrl16((uint16_t)((vram & 0xC000) >> 14 | 0x80));
}

/* $0099D2. Plane A's buffer is 64 rows of tiles, VRAM's plane 32: the 32 from `top` (bytes into
 * the buffer) go to VRAM's rows top mod 32 on. This is the part from top to the buffer's middle,
 * or (top past it) the part wrapped round from the buffer's start. */
void PlaneADmaFirst(uint16_t top)
{
    if (top == 0x1000) return;
    if (top < 0x1000) dma_regs((uint16_t)(0x1000 - top), RAM_BASE + top, (uint16_t)(0xC000 + top));
    else dma_regs((uint16_t)(top - 0x1000), RAM_BASE, 0xC000);
}

/* $009A4E. And the rest: the buffer's second half's rows before top, or the rows from top to the
 * buffer's end. */
void PlaneADmaSecond(uint16_t top)
{
    if (top == 0) return;
    if (top < 0x1000) dma_regs(top, RAM_BASE + 0x1000, 0xC000);
    else dma_regs((uint16_t)(0x2000 - top), RAM_BASE + top, (uint16_t)(0xC000 + top - 0x1000));
}

/* $009974. Plane A's buffer to VRAM $C000 by DMA now (interrupts masked, the Z80 off the bus). */
void PlaneADma(void)
{
    uint16_t sr = cpu_get_sr();
    cpu_set_sr(0x2700);
    z80_busreq(0x100);
    while (z80_busack() & 1) {}
    vdp_ctrl16(rd16(VDP_REG(1)) | 0x10);
    vdp_ctrl16(0x8F02);
    uint16_t top = (uint16_t)((ay() << 4) & 0x1F80);
    PlaneADmaFirst(top);
    PlaneADmaSecond(top);
    vdp_ctrl16(rd16(VDP_REG(1)) & (uint16_t)~0x10);
    z80_busreq(0);
    while (!(z80_busack() & 1)) {}
    cpu_set_sr(sr);
}

/* $009B94. Plane B's buffer ($1000 bytes) to VRAM $E000 by DMA now. */
void PlaneBDma(void)
{
    uint16_t sr = cpu_get_sr();
    cpu_set_sr(0x2700);
    z80_busreq(0x100);
    while (z80_busack() & 1) {}
    vdp_ctrl16(rd16(VDP_REG(1)) | 0x10);
    vdp_ctrl16(0x8F02);
    vdp_ctrl(0x93009408);
    vdp_ctrl16(0x9500);
    vdp_ctrl16(0x9690);
    vdp_ctrl16(0x977F);
    wr32(v_dmaCommand, 0x60000083);
    vdp_ctrl16(rd16(v_dmaCommand));
    vdp_ctrl16(rd16(v_dmaCommand + 2));
    vdp_ctrl16(rd16(VDP_REG(1)) & (uint16_t)~0x10);
    z80_busreq(0);
    while (!(z80_busack() & 1)) {}
    cpu_set_sr(sr);
}

/* $00A692. Plane A drawn again where it is now: its buffer, the row and column due, all to VRAM. */
void PlaneARedraw(void)
{
    PlaneAFill();
    ScrollRowA();
    ScrollColumnA();
    PlaneADma();
}

/* $00A6A2. Plane B the same. */
void PlaneBRedraw(void)
{
    PlaneBFill();
    ScrollRowB();
    ScrollColumnB();
    PlaneBDma();
}

/* ---- the frame ---- */

/* $00A418. Plane B's column, when it moves across. */
void ScrollPlaneBColumns(void)
{
    if (bit8(v_planeFlagsB, 3) || !rd32(v_planeSpeeds + 8)) return;
    ScrollColumnB();
    ScrollColumnBDma();
}

/* $00A3F6. Plane B: its column, its row. */
void ScrollPlaneB(void)
{
    ScrollPlaneBColumns();
    if (bit8(v_planeFlagsB, 2) || !rd32(v_planeSpeeds + 0xC)) return;
    ScrollRowB();
    ScrollRowBDma();
}

/* $00A3B8. Plane A's column. */
void ScrollPlaneAColumns(void)
{
    if (bit8(v_planeFlagsA, 3) || !rd32(v_planeSpeeds)) return;
    ScrollColumnA();
    dma_column_a();
}

/* $00A37A. The scroll tables, plane B's new edges, then plane A's (the battleship's, while
 * v_battleship, half rows). */
void ScrollFrame(void)
{
    HScrollTable();
    VScrollTable();
    ScrollPlaneB();
    if (rd16(v_stage4) == 8 && rd16(v_battleship)) {
        if (bit8(v_planeFlagsA, 2) || !rd32(v_planeSpeeds + 4)) return;
        ScrollRowAShort();
        dma_row_a_short();
        return;
    }
    ScrollPlaneAColumns();
    if (bit8(v_planeFlagsA, 2) || !rd32(v_planeSpeeds + 4)) return;
    ScrollRowA();
    dma_row_a();
}

/* $00970C. Plane A from the top left, still: drawn and sent. */
void PlaneAReset(void)
{
    wr32(v_planes, 0x800000);
    wr32(0xA924, 0);
    wr32(v_camX, 0);
    wr32(v_camY, 0);
    wr32(v_planeSpeeds, 0);
    wr32(0xA934, 0);
    wr32(v_camSpeedX, 0);
    wr32(v_camSpeedY, 0);
    PlaneAFill();
    PlaneADma();
}

/* $00973C. Plane B the same. */
void PlaneBReset(void)
{
    wr32(0xA928, 0x800000);
    wr32(0xA92C, 0);
    wr32(0xA908, 0);
    wr32(0xA90C, 0);
    wr32(0xA938, 0);
    wr32(0xA93C, 0);
    wr32(0xA918, 0);
    wr32(0xA91C, 0);
    PlaneBFill();
    PlaneBDma();
}

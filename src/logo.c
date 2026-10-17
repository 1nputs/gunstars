/* The SEGA logo (mode 4). The logo's tiles shrink in from full size: each step copies their
 * pixels into a buffer at $FF0000 at a smaller scale (a nibble at a time, the source's columns and
 * rows stepped by a fraction) and writes the buffer to VRAM; then the logo stretches and
 * squashes, an H-interrupt effect setting each line's scroll from the tables at $FF9000, while
 * its palette brightens and dims. Start (after the first few seconds) goes to the title, else
 * the Treasure logo follows. */
#include "game.h"

#define STEP     0x807C          /* the shrink's step, then the animation's (times 2) */
#define PHASE    0x807E          /* the animation's phase (times 4) */
#define LOGO_TILES 0x0D6050u
#define LINES    0x9000          /* the H-interrupt's line table */

/* $01B45E */
void Mode4(void)
{
    call68k(rom32(0x01B468 + (uint32_t)(int16_t)rd16(v_modeState)));
}

/* $01B478. The logo's tiles to VRAM $0800, its palette (white text on overseas consoles). */
void Mode4Start(void)
{
    wr16(v_fadeColor, 0);
    ScreenReset();
    QueueSound(0xF9);
    LoadPalettes(0x01B5A0);
    if (rd8(v_consoleVersion) & 0x80) {
        wr16(v_palette + 0x1E, 0x0EEE);
        wr16(v_paletteTarget + 0x1E, 0x0EEE);
    }
    cpu_set_sr(0x2700);
    vdp_ctrl16(0x8F02);
    vdp_ctrl(0x48000000);
    for (uint32_t i = 0; i < 0x180; i++) vdp_data32(rom32(LOGO_TILES + 4 * i));
    cpu_set_sr(0x2300);
    wr16(v_modeTimer, 0xC0);
    wr16(0xA01A, 0x48);
    wr8(VDP_REG(7) + 1, 0);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
}

/* $01B4F2. The shrink, a step a frame (when no job runs), 13 steps. */
void Mode4Shrink(void)
{
    LogoShrinkStep();
    if ((int16_t)rd16(STEP) < 0x1A) return;
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
}

/* $01B508. The display on, with H-interrupt effect 4. */
void Mode4Show(void)
{
    LogoAnimStart();
    wr8(VDP_REG(11) + 1, 0);
    wr16(v_hintEffect, 4);
    wr16(v_hintState, 0);
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
}

static void leave(uint16_t mode)
{
    wr16(v_hintEffect, 0);
    wr16(v_hintState, 0);
    LogoAnimEnd();
    clr8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0);
    wr16(v_mode, mode);
    wr16(v_modeState, 0);
}

/* $01B52E. The animation; Start once the timer is down to $40 goes to the title (mode 8), the
 * animation's end to the Treasure logo (mode $48). */
void Mode4Animate(void)
{
    if (rd16(PHASE) == 0x14) {
        leave(0x48);
        return;
    }
    if ((int16_t)rd16(v_modeTimer) <= 0x40 && ((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0x80)) {
        leave(8);
        return;
    }
    ClearLineTable();
    call68k(rom32(0x020298 + (uint32_t)(int16_t)rd16(PHASE)));
}

/* $020256 */
void LogoAnimStart(void)
{
    wr16(0x807A, 2);
    wr16(STEP, 0);
    wr16(PHASE, 0);
    wr16(0xEC02, 0x70);
}

/* $020270. Register 0 back to H-interrupts off, register 2 (plane A's address) back. */
void LogoAnimEnd(void)
{
    wr8(VDP_REG(0) + 1, 4);
    wr8(VDP_REG(2) + 1, 0x30);
    wr16(0x807A, 0);
    wr16(0xEC02, 0);
}

/* $020360. $FF9000-$FF9143 cleared. */
void ClearLineTable(void)
{
    for (int i = 0; i <= 0x50; i++) wr32((uint16_t)(LINES + 4 * i), 0);
}

/* $0205A4. The step's parameters: the fraction the rows and columns move by (d6), the buffer's
 * row stride (d4), the tiles to make less one (d7); the accumulator (d5) starts at -fraction. */
void LogoStepParams(uint16_t *acc, uint16_t *frac, uint16_t *stride, uint16_t *count)
{
    uint16_t s = rd16(STEP);
    *frac = rom16(0x0205D6 + s);
    *acc = (uint16_t)(0 - *frac);
    *stride = rom16(0x0205F2 + s);
    *count = rom16(0x02060E + s);
}

/* $02044A. The accumulator moved on by the fraction: its whole part. */
uint16_t LogoNext(uint16_t *acc, uint16_t frac)
{
    *acc = (uint16_t)(*acc + frac);
    return (uint16_t)((int16_t)*acc >> 8);
}

/* $02043C. The next two source positions. */
void LogoCoords(uint16_t *acc, uint16_t frac, uint16_t *x, uint16_t *y)
{
    *x = LogoNext(acc, frac);
    *y = LogoNext(acc, frac);
}

/* a source position (in half-bytes) as an address in the logo's tiles: within a 4-byte row of
 * a tile, then the tile */
static uint32_t logo_byte(uint16_t pos)
{
    return LOGO_TILES + (uint16_t)((pos & 3) | ((uint16_t)(pos << 3) & 0xFFE0));
}

/* $020460. The two sources (in half-bytes) as addresses, and which halves: bit 2 the first is
 * an odd half, bit 3 the second an even one. */
uint16_t LogoSources(uint16_t x, uint16_t y, uint32_t *a, uint32_t *b)
{
    uint16_t which = (x & 1) ? 4 : 0;
    *a = logo_byte((uint16_t)((int16_t)x >> 1));
    if (!(y & 1)) which += 8;
    *b = logo_byte((uint16_t)((int16_t)y >> 1));
    return which;
}

/* Four rows of a tile column: each byte from a half of a's byte and a half of b's. */
static void logo_copy(int kind, uint16_t rows, uint16_t stride, uint32_t *a, uint32_t *b, uint16_t *dst)
{
    do {
        for (int i = 0; i < 8; i++) {
            uint8_t hi = bus_rd8(*a), lo = bus_rd8(*b);
            hi = (kind & 1) ? (uint8_t)(hi << 4) : hi & 0xF0;
            lo = (kind & 2) ? (lo >> 4) & 0x0F : lo & 0x0F;
            wr8(*dst, hi | lo);
            *a += 4; *b += 4; *dst += 4;
        }
        *a += 0x160;
        *b += 0x160;
        *dst = (uint16_t)(*dst + stride);
    } while (rows-- != 0);
}

/* $0204C4, $0204FC, $020532, $02056C: the four ways of pairing the halves */
void LogoCopyHL(uint16_t rows, uint16_t stride, uint32_t *a, uint32_t *b, uint16_t *dst) { logo_copy(0, rows, stride, a, b, dst); }
void LogoCopyLL(uint16_t rows, uint16_t stride, uint32_t *a, uint32_t *b, uint16_t *dst) { logo_copy(1, rows, stride, a, b, dst); }
void LogoCopyHH(uint16_t rows, uint16_t stride, uint32_t *a, uint32_t *b, uint16_t *dst) { logo_copy(2, rows, stride, a, b, dst); }
void LogoCopyLH(uint16_t rows, uint16_t stride, uint32_t *a, uint32_t *b, uint16_t *dst) { logo_copy(3, rows, stride, a, b, dst); }

/* $0204A8. A column of four rows, the halves as `which` says. */
void LogoCopy(uint16_t which, uint16_t stride, uint32_t a, uint32_t b, uint16_t dst)
{
    switch (rom32(0x0204B4 + (uint32_t)(int16_t)which)) {
    case SYM_LogoCopyHL: LogoCopyHL(3, stride, &a, &b, &dst); break;
    case SYM_LogoCopyLL: LogoCopyLL(3, stride, &a, &b, &dst); break;
    case SYM_LogoCopyHH: LogoCopyHH(3, stride, &a, &b, &dst); break;
    case SYM_LogoCopyLH: LogoCopyLH(3, stride, &a, &b, &dst); break;
    default: call68k_missing(rom32(0x0204B4 + (uint32_t)(int16_t)which)); break;
    }
}

/* $0201C2. A shrink step: the logo's tiles at the step's scale into the buffer at $FF0000 (a
 * byte column at a time), then the buffer to VRAM at the step's address. */
void LogoShrinkStep(void)
{
    if (rd16(v_jobKind)) return;
    uint16_t acc, frac, stride, count;
    LogoStepParams(&acc, &frac, &stride, &count);
    uint16_t dst = 0x0000, col = 0;
    do {
        uint16_t x, y;
        uint32_t a, b;
        LogoCoords(&acc, frac, &x, &y);
        uint16_t which = LogoSources(x, y, &a, &b);
        LogoCopy(which, stride, a, b, dst);
        col++;
        dst = (uint16_t)((col & 3) | ((uint16_t)(col << 3) & 0xFFE0));
    } while (count-- != 0);
    uint16_t vram = rom16(0x02062A + rd16(STEP));
    uint32_t command = (uint32_t)((vram & 0x3FFF) | 0x4000) << 16 | (uint16_t)(vram >> 14);
    cpu_set_sr(0x2700);
    vdp_ctrl16(0x8F02);
    vdp_ctrl(command);
    for (uint32_t i = 0; i <= 0x1400; i++) vdp_data32(rd32((uint16_t)(4 * i)));
    cpu_set_sr(0x2300);
    wr16(STEP, (uint16_t)(rd16(STEP) + 2));
}

/* $020374. The stretch: H-interrupts every line; two scroll ramps from $FF9700 out (the step's
 * slope, up and down), and the logo's lines' entries in the table at $FF9000. */
void LogoStretch(void)
{
    wr8(VDP_REG(0) + 1, 0x14);
    wr8(VDP_REG(10) + 1, 0);
    wr8(VDP_REG(2) + 1, 0x38);
    uint16_t slope = rom16(0x02041E + rd16(STEP)), up = 0, down = 0;
    uint16_t lo = 0x9700, hi = 0x9700;
    for (int i = 0; i < 0x80; i++) {
        lo -= 2;
        up = (uint16_t)(up + slope);
        wr16(lo, up >> 8);
        hi += 2;
        down = (uint16_t)(down - slope);
        wr16(hi, down >> 8);
    }
    uint16_t s = rd16(STEP), v = rom16(0x020400 + s);
    uint16_t row = (uint16_t)(LINES + s), at = row;
    uint16_t n = (uint16_t)(0x1C - s + 0xB);
    for (int r = 0; r < 4; r++) {
        for (uint16_t k = n;; k--) {
            wr16(at, v);
            at += 2;
            v++;
            if (k == 0) break;
        }
        row += 0x50;
        at = row;
    }
}

/* $020646. Palette line 0 brightened by the step's amount (all three components). */
void LogoBrighten(void)
{
    wr16(0x8014, rom16(0x020668 + rd16(STEP)));
    wr16(0x8002, 0x8000);
    wr16(0x800A, 0xE000);
    AdjustPalettes();
}

/* $0202B0 ... $020340: the animation's phases. a6 (hint_data) picks the H-interrupt's table. */
void LogoGrow(void)              /* stretching, brightening, to step $1C, then a hold of 120 */
{
    hint_data = 0xFFFF9640;
    LogoStretch();
    LogoBrighten();
    wr16(STEP, (uint16_t)(rd16(STEP) + 2));
    if (rd16(STEP) != 0x1C) return;
    wr16(PHASE, (uint16_t)(rd16(PHASE) + 4));
    wr16(0x8040, 0x78);
}

void LogoHold(void)
{
    hint_data = 0xFFFF9640;
    LogoStretch();
    wr16(0x8040, (uint16_t)(rd16(0x8040) - 1));
    if (rd16(0x8040)) return;
    wr16(PHASE, (uint16_t)(rd16(PHASE) + 4));
    wr16(0x8040, 1);
}

void LogoShrinkBack(void)        /* back down to step 6 */
{
    hint_data = 0xFFFF9680;
    LogoStretch();
    wr16(STEP, (uint16_t)(rd16(STEP) - 2));
    if (rd16(STEP) != 6) return;
    wr16(PHASE, (uint16_t)(rd16(PHASE) + 4));
}

void LogoBounce(void)            /* up to step $1C again, the counter's times, else back */
{
    hint_data = 0xFFFF9600;
    LogoStretch();
    wr16(STEP, (uint16_t)(rd16(STEP) + 2));
    if (rd16(STEP) != 0x1C) return;
    wr16(0x8040, (uint16_t)(rd16(0x8040) - 1));
    if ((int16_t)rd16(0x8040) >= 0) wr16(PHASE, (uint16_t)(rd16(PHASE) - 4));
    else wr16(PHASE, (uint16_t)(rd16(PHASE) + 4));
}

void LogoFade(void)              /* dimming, down to step 0 */
{
    hint_data = 0xFFFF9600;
    LogoStretch();
    LogoBrighten();
    wr16(STEP, (uint16_t)(rd16(STEP) - 2));
    if (rd16(STEP)) return;
    wr16(PHASE, (uint16_t)(rd16(PHASE) + 4));
}

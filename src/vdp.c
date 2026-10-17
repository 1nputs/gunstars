/* The VDP side of the vertical blank: the registers from their shadow (v_vdpRegs), the DMAs
 * (the sprite table, the palette, the scroll tables queued during the frame, two blocks of
 * tiles), and the palette fade. */
#include "game.h"

#define REG(n) VDP_REG(n)
#define DISPLAY_ENABLE 0x40              /* register 1 */
#define DMA_ENABLE 0x10                  /* register 1 */
#define HINT_ENABLE 0x10                 /* register 0 */
#define QUEUE_END 0xF400                 /* the DMA queue grows down from here */
#define PALETTE_COLOURS 64

/* $000F9C. Register 0, its H-interrupt bit cleared while the display is off. */
void VdpWriteReg0(void)
{
    uint16_t r0 = rd16(REG(0));
    if (!rd8(v_displayOn)) r0 &= (uint16_t)~HINT_ENABLE;
    vdp_ctrl16(r0);
}

/* $000F4C. Registers 1-18 as the frame left them. */
void VdpWriteRegs(void)
{
    for (int n = 1; n <= 18; n++) vdp_ctrl16(rd16(REG(n)));
}

/* A DMA from the 68k's bus: auto-increment 2, the length in words (registers 19 and 20 in one
 * write), the source (21-23), then the command, which the game writes from v_dmaCommand as two
 * words. */
static void dma(uint32_t src, uint16_t words, uint32_t command)
{
    src = (src & 0xFFFFFF) >> 1;
    vdp_ctrl16(0x8F02);
    vdp_ctrl(0x93000000u | (uint32_t)(words & 0xFF) << 16 | 0x9400u | words >> 8);
    vdp_ctrl16((uint16_t)(0x9500 | (src & 0xFF)));
    vdp_ctrl16((uint16_t)(0x9600 | (src >> 8 & 0xFF)));
    vdp_ctrl16((uint16_t)(0x9700 | (src >> 16 & 0x7F)));
    wr32(v_dmaCommand, command);
    vdp_ctrl16(rd16(v_dmaCommand));
    vdp_ctrl16(rd16(v_dmaCommand + 2));
}

/* $000D36. The vertical blank's DMAs, with interrupts masked and the Z80 off the bus: the
 * sprite table to VRAM $F400; the palette to CRAM (or, with the display off, CRAM filled with
 * v_fadeColor); the queued DMAs (QueueHScroll, QueueVScroll); three blocks of 80 bytes from
 * $FFB200 to VRAM $D000 when either player's flag at +$81 asks; four from $FF9000 to VRAM $E500
 * while the word at $FF807A is set. A job's tiles waiting in its buffer are taken too
 * (v_jobBufferFull cleared). */
void VBlankDma(void)
{
    uint16_t sr = cpu_get_sr();
    cpu_set_sr(0x2700);
    z80_busreq(0x100);
    while (z80_busack() & 1) {}
    vdp_ctrl16(rd16(REG(1)) | DMA_ENABLE);
    dma(RAM_BASE | v_sprites, 0x140, 0x74000083);                /* VRAM $F400 */
    if (!rd8(v_displayOn)) {
        vdp_ctrl16(0x8F02);
        vdp_ctrl(0xC0000000);                                    /* CRAM 0 */
        uint32_t fill = rd16(v_fadeColor) * 0x10001u;
        for (int i = 0; i < PALETTE_COLOURS / 2; i++) vdp_data32(fill);
    } else {
        dma(RAM_BASE | v_palette, PALETTE_COLOURS, 0xC0000080);  /* CRAM 0 */
    }
    uint16_t q = rd16(v_dmaQueue);
    if (q != QUEUE_END) {
        for (; q != QUEUE_END; q += 16) {
            vdp_ctrl(rd32(q));
            vdp_ctrl(rd32((uint16_t)(q + 4)));
            vdp_ctrl(rd32((uint16_t)(q + 8)));
            vdp_ctrl16(rd16((uint16_t)(q + 12)));
            vdp_ctrl16(rd16((uint16_t)(q + 14)));
        }
        wr16(v_dmaQueue, q);
        wr16(v_dmaQueue + 2, q);
    }
    if (rd8(v_player1 + 0x81) || rd8(v_player2 + 0x81)) {
        dma(0xFFB200, 0x28, 0x50000083);                         /* VRAM $D000 */
        dma(0xFFB250, 0x28, 0x50800083);                         /* $D080 */
        dma(0xFFB2A0, 0x28, 0x51000083);                         /* $D100 */
        wr8(v_player1 + 0x81, 0);
        wr8(v_player2 + 0x81, 0);
    }
    if (rd16(0x807A)) {
        dma(0xFF9000, 0x28, 0x65000083);                         /* VRAM $E500 */
        dma(0xFF9050, 0x28, 0x65800083);                         /* $E580 */
        dma(0xFF90A0, 0x28, 0x66000083);                         /* $E600 */
        dma(0xFF90F0, 0x28, 0x66800083);                         /* $E680 */
    }
    vdp_ctrl16(rd16(REG(1)) & (uint16_t)~DMA_ENABLE);
    z80_busreq(0);
    while (!(z80_busack() & 1)) {}
    cpu_set_sr(sr);
    wr8(v_jobBufferFull, 0);
}

/* A DMA into the queue, as VBlankDma writes it out: the length registers (a long), register
 * 15, the source registers 23, 22, 21, the command. */
void QueueDma(uint32_t command, uint32_t src, uint32_t length_regs)
{
    uint16_t q = rd16(v_dmaQueue);
    src = (src & 0xFFFFFF) >> 1;
    q -= 4; wr32(q, command);
    q -= 2; wr16(q, (uint16_t)(0x9500 | (src & 0xFF)));
    q -= 2; wr16(q, (uint16_t)(0x9600 | (src >> 8 & 0xFF)));
    q -= 2; wr16(q, (uint16_t)(0x9700 | (src >> 16 & 0xFF)));
    q -= 2; wr16(q, 0x8F02);
    q -= 4; wr32(q, length_regs);
    wr16(v_dmaQueue, q);
}

/* $0012F4. The horizontal scroll table to VRAM $F000: a line per line in line scroll mode
 * (register 11 bit 1), else the one pair. */
void QueueHScroll(void)
{
    QueueDma(0x70000083, rd32(v_hscrollSrc), rd8(REG(11) + 1) & 2 ? 0x94029300 : 0x94009302);
}

/* $00134A. The vertical scroll table to VSRAM: 20 columns' pairs in column mode (register 11
 * bit 2), else the one pair. */
void QueueVScroll(void)
{
    QueueDma(0x40000090, rd32(v_vscrollSrc), rd8(REG(11) + 1) & 4 ? 0x94009328 : 0x94009302);
}

/* One component of a colour (in place, masked) less by step, 0 at the least; 0 stays 0. */
static uint16_t component_down(uint16_t c, uint16_t step)
{
    if (!c) return 0;
    uint16_t v = (uint16_t)(c - step);
    return (int16_t)v < 0 ? 0 : v;
}

/* One component more by step, at most max. */
static uint16_t component_up(uint16_t c, uint16_t step, uint16_t max)
{
    uint16_t v = (uint16_t)(c + step);
    return v > max ? max : v;
}

/* A colour faded by level (v_fadeLevel's high byte) toward black, or toward white. */
static uint16_t fade_colour(uint16_t c, uint8_t level, int white)
{
    uint16_t r = level, g = (uint16_t)(r << 4), b = (uint16_t)(g << 4);
    if (white)
        return component_up(c & 0x00F, r, 0x00F) | component_up(c & 0x0F0, g, 0x0F0) | component_up(c & 0xF00, b, 0xF00);
    return component_down(c & 0x00F, r) | component_down(c & 0x0F0, g) | component_down(c & 0xF00, b);
}

/* $000FB4. The fade, unless paused: v_fadeSpeed below 0 brings v_palette back to
 * v_paletteTarget (v_fadeLevel down to 0, then the fade stops); above 0 takes it away, toward
 * black or toward v_fadeColor's white. Fading out starts by keeping the palette as the target;
 * at level $1000 the display goes off and the fade stops. */
void FadePalette(void)
{
    if (rd8(v_pauseFlags) & 0x80) return;
    int white = rd16(v_fadeColor) != 0;
    int16_t speed = (int16_t)rd16(v_fadeSpeed);
    if (!speed) return;
    if (speed < 0) {
        int16_t level = (int16_t)(rd16(v_fadeLevel) + speed);
        wr16(v_fadeLevel, (uint16_t)(level < 0 ? 0 : level));
        uint8_t l = rd8(v_fadeLevel);
        for (int i = 0; i < PALETTE_COLOURS; i++)
            wr16((uint16_t)(v_palette + 2 * i), fade_colour(rd16((uint16_t)(v_paletteTarget + 2 * i)), l, white));
        if (!rd16(v_fadeLevel)) wr16(v_fadeSpeed, 0);
        return;
    }
    int first = rd16(v_fadeLevel) == 0;
    wr16(v_fadeLevel, (uint16_t)(rd16(v_fadeLevel) + speed));
    uint8_t l = rd8(v_fadeLevel);
    for (int i = 0; i < PALETTE_COLOURS; i++) {
        uint16_t pal = (uint16_t)(v_palette + 2 * i), target = (uint16_t)(v_paletteTarget + 2 * i);
        if (first) {
            uint16_t c = fade_colour(rd16(pal), l, white);
            wr16(pal, c);
            wr16(target, c);
        } else {
            wr16(pal, fade_colour(rd16(target), l, white));
        }
    }
    if ((int16_t)rd16(v_fadeLevel) >= 0x1000) {
        wr16(v_fadeLevel, 0x1000);
        wr16(v_fadeSpeed, 0);
        clr8(REG(1) + 1, 6);                                     /* DISPLAY_ENABLE */
        wr8(v_displayOn, 0);
    }
}

/* $003630. The next random number: the seed times 41, its halves added into the low word. d0
 * keeps the old seed's high word over it (the callers use the low word). */
uint32_t Random(void)
{
    uint32_t seed = rd32(v_randomSeed);
    if (!seed) seed = 0x2A6D365A;
    uint32_t m = seed * 41;
    uint16_t sum = (uint16_t)(m + (m >> 16));
    wr32(v_randomSeed, (uint32_t)sum << 16 | (uint16_t)m);
    return (seed & 0xFFFF0000) | sum;
}

/* $001BE4. Four palette lines from a list of four pointers (0 leaves the line), into the
 * palette and the fade's target. */
void LoadPalettes(uint32_t list)
{
    uint16_t pal = v_palette, target = v_paletteTarget;
    for (int i = 0; i < 4; i++) LoadPaletteLine(&list, &pal, &target);
}

/* $001BFE. One line (16 colours) of the list's next pointer. */
void LoadPaletteLine(uint32_t *list, uint16_t *pal, uint16_t *target)
{
    uint32_t src = bus_rd32(*list);
    *list += 4;
    if (!src) {
        *pal += 0x20;
        *target += 0x20;
        return;
    }
    for (int i = 0; i < 8; i++, src += 4) {
        wr32(*pal, bus_rd32(src));
        wr32(*target, bus_rd32(src));
        *pal += 4;
        *target += 4;
    }
}

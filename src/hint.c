/* H-interrupt effects. Each frame HintEffect runs the effect v_hintEffect names (an index times
 * 4). The first time (v_hintState 0) an effect sets the H-interrupt counter (register 10), runs
 * a command list that copies its handler into work RAM at v_hintCode (the H-interrupt vector),
 * and turns H-interrupts on (register 0 bit 4); each frame it writes those registers again and
 * points a6 (reg_a6) at the table its handler reads, line by line. */
#include "game.h"

#define RTE 0x4E73
#define HINT_ENABLE 0x10                 /* register 0 */
#define EFFECT_TABLE 0x001420u
#define HANDLER_1 0x00149Cu              /* command lists: a handler copied to v_hintCode */
#define HANDLER_2 0x0015BEu
#define HANDLER_3 0x0014E0u
#define HANDLER_5 0x001610u
#define HANDLER_6 0x001654u

uint32_t reg_a6;

/* $001416 */
void HintEffect(void)
{
    uint32_t effect = rom32(EFFECT_TABLE + (uint32_t)(int16_t)rd16(v_hintEffect));
    switch (effect) {
    case SYM_HintNone: HintNone(); break;
    case SYM_HintEffect1: HintEffect1(); break;
    case SYM_HintEffect2: HintEffect2(); break;
    case SYM_HintEffect3: HintEffect3(); break;
    case SYM_HintEffect4: HintEffect4(); break;
    case SYM_HintEffect5: HintEffect5(); break;
    case SYM_HintEffect6: HintEffect6(); break;
    case SYM_HintEffect7: HintEffect7(); break;
    default: call68k_missing(effect); break;
    }
}

/* the first frame of an effect: v_hintState moves on */
static int first(void)
{
    if (rd16(v_hintState)) return 0;
    wr16(v_hintState, (uint16_t)(rd16(v_hintState) + 4));
    return 1;
}

static void hint_on(void)
{
    wr8(VDP_REG(0) + 1, rd8(VDP_REG(0) + 1) | HINT_ENABLE);
}

/* $001440. Effect 0: H-interrupts off (register 0's bit 4), and a bare rte for a handler. */
void HintNone(void)
{
    if (!first()) return;
    wr8(VDP_REG(0) + 1, rd8(VDP_REG(0) + 1) & (uint8_t)~HINT_ENABLE);
    vdp_ctrl16(rd16(VDP_REG(0)));
    wr16(v_hintCode, RTE);
}

/* $001462. Effect 1: an H-interrupt every line, the table at $FF9600. */
void HintEffect1(void)
{
    if (first()) {
        wr8(VDP_REG(10) + 1, 0);
        RunCommands(HANDLER_1);
        hint_on();
    }
    vdp_ctrl16(rd16(VDP_REG(10)));
    vdp_ctrl16(rd16(VDP_REG(0)));
    reg_a6 = 0xFFFF9600;
}

/* $001534. Effect 2: the first frame copies the handler, the second turns H-interrupts on; then
 * each frame: every third line, the table at $FF984E (its first two longs moved on 8 bytes),
 * and the first line's horizontal and vertical scroll written at once. */
void HintEffect2(void)
{
    int16_t state = (int16_t)(rd16(v_hintState) - 4);
    if (state < 0) {
        wr16(v_hintState, (uint16_t)(rd16(v_hintState) + 4));
        wr8(VDP_REG(10) + 1, 2);
        vdp_ctrl16(rd16(VDP_REG(10)));
        RunCommands(HANDLER_2);
        return;
    }
    if (state == 0) {
        wr16(v_hintState, (uint16_t)(rd16(v_hintState) + 4));
        hint_on();
        vdp_ctrl16(rd16(VDP_REG(0)));
    }
    wr8(VDP_REG(10) + 1, 2);
    vdp_ctrl16(0x8A02);
    reg_a6 = 0xFFFF984E;
    wr16(v_hintLine, 0);
    HintShiftLines(0x984E);
    vdp_ctrl(0x70000083);                                        /* VRAM $F000: the scroll table */
    vdp_data(rd16(0xE400));
    vdp_ctrl(0x40000010);                                        /* VSRAM 0 */
    wr16(0xEC00, rd16(0xEC50));
    vdp_data(rd16(0xEC00));
}

/* $00176E. The longs from $FF9846 moved on 8 bytes, up to `end` (from the start). */
void HintShiftLines(uint16_t end)
{
    uint16_t from = 0x9846, to = 0x984E;
    do {
        wr32(to, rd32(from));
        from += 4; to += 4;
    } while ((int16_t)(from - end) < 0);
}

/* $0014A6. Effect 3 (no recorded run used it): as effect 1, the registers written once. */
void HintEffect3(void)
{
    if (first()) {
        wr8(VDP_REG(10) + 1, 0);
        vdp_ctrl16(rd16(VDP_REG(10)));
        RunCommands(HANDLER_3);
        hint_on();
        vdp_ctrl16(rd16(VDP_REG(0)));
    }
    reg_a6 = 0xFFFF9600;
}

/* $0014EA. Effect 4 (no recorded run used it): every fourth line; each frame the table at
 * $FF9600 is copied from $FF9400 (112 bytes). */
void HintEffect4(void)
{
    if (first()) {
        wr8(VDP_REG(10) + 1, 3);
        vdp_ctrl16(rd16(VDP_REG(10)));
        RunCommands(HANDLER_3);
        hint_on();
        vdp_ctrl16(rd16(VDP_REG(0)));
    }
    for (int i = 0; i < 28; i++) wr32((uint16_t)(0x9600 + 4 * i), rd32((uint16_t)(0x9400 + 4 * i)));
    reg_a6 = 0xFFFF9600;
}

/* $0015C8. Effect 5: H-interrupts on with its handler, and each frame the display off, the
 * backdrop colour 0 and the line count back to 0. */
void HintEffect5(void)
{
    if (first()) {
        RunCommands(HANDLER_5);
        wr8(VDP_REG(10) + 1, 0);
        set8(VDP_REG(0) + 1, 4);
        wr16(0xF766, 0);
    }
    vdp_ctrl16(rd16(VDP_REG(0)));
    vdp_ctrl16(rd16(VDP_REG(10)));
    clr8(VDP_REG(1) + 1, 6);
    wr8(VDP_REG(7) + 1, 0);
    wr16(v_hintLine, 0);
}

/* $00161A. Effect 6 (no recorded run used it): every fourth line, the table at $FF9800. */
void HintEffect6(void)
{
    if (first()) {
        wr8(VDP_REG(10) + 1, 3);
        RunCommands(HANDLER_6);
        hint_on();
    }
    vdp_ctrl16(rd16(VDP_REG(10)));
    vdp_ctrl16(rd16(VDP_REG(0)));
    reg_a6 = 0xFFFF9800;
}

/* $00165E. Effect 7 (no recorded run used it): effect 1's handler every eighth line. */
void HintEffect7(void)
{
    if (first()) {
        wr8(VDP_REG(10) + 1, 7);
        RunCommands(HANDLER_1);
        hint_on();
    }
    vdp_ctrl16(rd16(VDP_REG(10)));
    vdp_ctrl16(rd16(VDP_REG(0)));
    reg_a6 = 0xFFFF9600;
}

/* ---- the handlers: the code the effects copy to v_hintCode ($FFEE00), which the 68k runs at
 * each H-interrupt. Natively the runtime calls HintHandler (native/hal_native.c), which runs the
 * one whose bytes are there; the tests replay the 68k's (an interrupt is a recorded block). Their
 * waits (dbf, to reach the line's blank) are cpu_spin. ---- */

/* $001698 (effects 1, 7): plane A's vertical scroll for this line, from the table at a6 */
static void handler_vscroll_a(void)
{
    vdp_ctrl(0x40000010);
    vdp_data(rd16((uint16_t)reg_a6));
    reg_a6 += 2;
}

/* $0016B0 (effects 3, 4): plane B's */
static void handler_vscroll_b(void)
{
    vdp_ctrl(0x40020010);
    vdp_data(rd16((uint16_t)reg_a6));
    reg_a6 += 2;
}

/* $0016C8 (effect 6): colour 5 for this line */
static void handler_colour(void)
{
    vdp_ctrl(0xC00A0000);
    vdp_data(rd16((uint16_t)reg_a6));
    reg_a6 += 2;
}

/* $001704 (effect 2): two register writes on the first two H-interrupts ($FF801C, $FF801E), the
 * counter set to every third line on the third, then each time this line's horizontal and
 * vertical scroll */
static void handler_scroll(void)
{
    uint16_t n = (uint16_t)(rd16(v_hintLine) + 1);
    wr16(v_hintLine, n);
    if (n == 1) {
        vdp_ctrl16(rd16(0x801C));
        return;
    }
    if (n == 2) {
        vdp_ctrl16(rd16(0x801E));
        return;
    }
    if (n == 3) vdp_ctrl16(0x8A80);
    vdp_ctrl(0x70000083);
    vdp_data(rd16((uint16_t)reg_a6));
    reg_a6 += 2;
    vdp_ctrl(0x40000010);
    vdp_data(rd16((uint16_t)reg_a6));
    reg_a6 += 2;
}

/* $001782 (effect 5): the story's letterbox: 32 lines down the next interrupt, 144 further the
 * picture's top (the display on, the backdrop colour $10), then its bottom: the display off, the
 * text box's planes and scroll ($FF7766 less $68), and the display on again unless $FFF765 */
static void handler_letterbox(void)
{
    uint16_t s = rd16(v_hintLine);
    if (s == 0) {
        vdp_ctrl16(0x8A1F);
        wr16(v_hintLine, 4);
        return;
    }
    if (s == 4) {
        vdp_ctrl16(0x8A8F);
        wr16(v_hintLine, 8);
        return;
    }
    if (s == 8) {
        wr16(v_hintLine, 0xC);
        cpu_spin(0x1F);
        vdp_ctrl16((uint16_t)(rd16(VDP_REG(1)) | 0x40));
        vdp_ctrl16(0x8710);
        return;
    }
    cpu_spin(0x21);
    vdp_ctrl16((uint16_t)(rd16(VDP_REG(1)) & ~0x40));
    vdp_ctrl16(0x8700);
    vdp_ctrl(0x82008400);
    vdp_ctrl16(0x8B00);
    vdp_ctrl16(0x8F02);
    vdp_ctrl(0x70000003);
    vdp_data32(0);
    vdp_ctrl(0x40000010);
    uint16_t v = (uint16_t)(rd16(0xF766) - 0x68);
    vdp_data(v);
    vdp_data(v);
    if (rd8(0xF765)) return;
    cpu_spin(0x11);
    vdp_ctrl16((uint16_t)(rd16(VDP_REG(1)) | 0x40));
}

/* the H-interrupt: the handler whose code was copied to v_hintCode (an rte there: none) */
void HintHandler(void)
{
    static const struct { uint32_t rom; void (*run)(void); } handlers[] = {
        { 0x001698, handler_vscroll_a }, { 0x0016B0, handler_vscroll_b }, { 0x0016C8, handler_colour },
        { 0x001704, handler_scroll }, { 0x001782, handler_letterbox },
    };
    for (unsigned k = 0; k < sizeof handlers / sizeof *handlers; k++) {
        int same = 1;
        for (uint32_t i = 0; i < 16 && same; i++) same = rd8((uint16_t)(v_hintCode + i)) == rom8(handlers[k].rom + i);
        if (same) {
            handlers[k].run();
            return;
        }
    }
}

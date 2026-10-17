/* Type $4F8: the text box. Asked for a message (+$48, an offset into the language's table), it
 * types it out a character at a time straight into VRAM (plane B's rows at $EA00 on), waiting
 * between characters; a button held speeds it up, and at a pause it blinks until one is pressed.
 * A message is a list of words: a character (a tile from $87B4, or from the box's own tiles), or
 * a command: $FFFF the end, $FFFE a new VRAM address, $FFFD a wait, $FFFC a routine to call,
 * $FFFB the next line, $FFFA a new address and the line counter cleared. */
#include "game.h"

#define T_ASK     0x48           /* the message asked for (an offset; below 0: none, closed) */
#define T_RUNNING 0x4A           /* the message being typed (0: none) */
#define T_LINGER  0x4C           /* frames the box stays after a message */
#define T_VRAM    0x4E           /* where the next character goes */
#define T_SPEED   0x50           /* the frames between characters */
#define T_WAIT    0x52           /* frames until the next (below 0: a pause until a press) */
#define T_SCRIPT  0x54           /* long: the message's next word */
#define T_SOUND   0x58           /* byte: the typing's sound (0: none) */
#define T_BUTTONS 0x59           /* byte: bit 7 the players' buttons count */

#define BUTTONS_ACB 0x70

/* $00C6D6. The players' buttons held, if the box listens. */
uint8_t TextButtonsHeld(uint16_t obj)
{
    uint8_t b = 0;
    if (!(rd8(obj + T_BUTTONS) & 0x80)) return b;
    if (bit8(v_players, 0)) b |= rd8(v_padHeld);
    if (bit8(v_players, 1)) b |= rd8(v_padHeld + 1);
    return b;
}

/* $00C6FE. And pressed this frame. */
uint8_t TextButtonsPressed(uint16_t obj)
{
    uint8_t b = 0;
    if (!(rd8(obj + T_BUTTONS) & 0x80)) return b;
    if (bit8(v_players, 0)) b |= rd8(v_padPressed);
    if (bit8(v_players, 1)) b |= rd8(v_padPressed + 1);
    return b;
}

/* $00C6A8. The typing's sound. */
void TextSound(uint16_t obj)
{
    uint8_t s = rd8(obj + T_SOUND);
    if (s) QueueSound(s);
}

/* $00C6BC. The VRAM write address set to the box's next character's. */
void TextVramAddress(uint16_t obj)
{
    uint16_t v = rd16(obj + T_VRAM);
    vdp_ctrl16((uint16_t)((v & 0x3FFF) | 0x4000));
    vdp_ctrl16((uint16_t)(v >> 14));
}

/* rows of plane B's tiles (40 words each, from VRAM $EA00) cleared, each with interrupts masked */
static void clear_rows(int rows)
{
    for (int r = 0; r < rows; r++) {
        uint16_t sr = cpu_get_sr();
        cpu_set_sr(0x2700);
        vdp_ctrl16(0x8F02);
        vdp_ctrl(0x6A000003u + 0x800000u * (uint32_t)r);
        for (int k = 0; k < 40; k++) vdp_data(0);
        cpu_set_sr(sr);
    }
}

/* $00C726. A message's routine: the box's eight rows cleared. */
void TextClear8(void)
{
    clear_rows(8);
}

/* $00C8A0. Its first two. */
void TextClear2(void)
{
    clear_rows(2);
}

/* a character's tile written, the address on */
static void put(uint16_t obj, uint16_t tile)
{
    TextVramAddress(obj);
    vdp_data(tile);
    wr16(obj + T_VRAM, (uint16_t)(rd16(obj + T_VRAM) + 2));
}

/* $00C580. The message's next character (its commands up to it carried out). */
void TextBoxPrint(uint16_t obj)
{
    clr8(obj + O_FLAGS, 7);
    wr16(obj + T_WAIT, rd16(obj + T_SPEED));
    uint32_t a = rd32(obj + T_SCRIPT);
    for (;;) {
        uint16_t w = bus_rd16(a);
        a += 2;
        if (!(w & 0x8000)) {
            TextSound(obj);
            wr32(obj + T_SCRIPT, a);
            put(obj, (uint16_t)((w & 0xFF) + 0x87B4));
            return;
        }
        switch (w) {
        case 0xFFFF:
            wr16(obj + T_RUNNING, 0);
            return;
        case 0xFFFE:
            wr16(obj + T_VRAM, bus_rd16(a));
            a += 2;
            continue;
        case 0xFFFD:
            wr16(obj + T_WAIT, bus_rd16(a));
            a += 2;
            wr32(obj + T_SCRIPT, a);
            return;
        case 0xFFFC: {
            uint32_t fn = bus_rd32(a);
            a += 4;
            call68k(fn);
            continue;
        }
        case 0xFFFB:
            wr16(v_textLine, (uint16_t)(rd16(v_textLine) + 8));
            wr32(obj + T_SCRIPT, a);
            return;
        case 0xFFFA:
            wr16(v_textLine, 0);
            wr16(obj + T_VRAM, bus_rd16(a));
            a += 2;
            continue;
        default:
            break;
        }
        TextSound(obj);
        wr32(obj + T_SCRIPT, a);
        uint16_t tile = (uint16_t)((w & 0xFF) + rd16(obj + 0x0E));
        if (!(w & 0x300)) {
            put(obj, tile);
            return;
        }
        /* bit 8 or 9: a mark ($2E or $2F of the box's tiles) on the row above */
        TextVramAddress(obj);
        vdp_data(tile);
        wr16(obj + T_VRAM, (uint16_t)(rd16(obj + T_VRAM) - 0x80));
        TextVramAddress(obj);
        vdp_data((uint16_t)(((w & 0x100) ? 0x2E : 0x2F) + rd16(obj + 0x0E)));
        wr16(obj + T_VRAM, (uint16_t)(rd16(obj + T_VRAM) + 0x82));
        return;
    }
}

/* the next character, with interrupts masked */
static void print_masked(uint16_t obj)
{
    cpu_set_sr(0x2700);
    TextBoxPrint(obj);
    cpu_set_sr(0x2300);
}

/* $00C488. Not while fading: a new message started (by the console's language), the one running
 * typed on, or the box lingering. */
void TextBoxRun(uint16_t obj)
{
    if (rd16(v_fadeSpeed)) return;
    uint16_t ask = rd16(obj + T_ASK);
    if (ask && !(ask & 0x8000)) {
        wr16(obj + T_ASK, 0);
        wr16(obj + T_RUNNING, ask);
        uint32_t table = (rd8(v_consoleVersion) & 0x80) ? 0x00D73E : 0x00C96C;
        wr32(obj + T_SCRIPT, rom32(table + (uint32_t)(int16_t)ask));
        wr16(obj + T_WAIT, 0);
        wr16(obj + T_LINGER, 0x100);
        print_masked(obj);
        return;
    }
    if (ask & 0x8000) {
        wr16(obj + T_ASK, 0);
        wr16(obj + T_RUNNING, 0);
        wr16(obj + T_LINGER, 0);
    } else if (rd16(obj + T_RUNNING)) {
        uint16_t wait = rd16(obj + T_WAIT);
        if (!wait) { print_masked(obj); return; }
        if (wait & 0x8000) {
            if (TextButtonsPressed(obj) & BUTTONS_ACB) { print_masked(obj); return; }
            if (bit8(v_gameFrames + 1, 4)) set8(obj + O_FLAGS, 7);
            else clr8(obj + O_FLAGS, 7);
            return;
        }
        if ((int16_t)wait > (int16_t)rd16(obj + T_SPEED) && (TextButtonsHeld(obj) & BUTTONS_ACB)) {
            if (TextButtonsPressed(obj) & BUTTONS_ACB) print_masked(obj);
            return;
        }
        wr16(obj + T_WAIT, (uint16_t)(wait - 1));
        if (TextButtonsHeld(obj) & BUTTONS_ACB) print_masked(obj);
        return;
    }
    if (TextButtonsHeld(obj) & BUTTONS_ACB) return;
    uint16_t t = rd16(obj + T_LINGER);
    if (t) wr16(obj + T_LINGER, (uint16_t)(t - 1));
}

/* $00C46C. Step 0: shown, its mappings, the line counter cleared. */
void TextBoxStart(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + T_LINGER, 0);
    wr16(obj + O_FLAGS, 0x4000);
    wr32(obj + 0x08, 0x00C56C);
    wr16(v_textLine, 0);
    TextBoxRun(obj);
}

/* $00C45A. Type $4F8. */
void TextBox(uint16_t obj)
{
    call_object(rom32(0x00C464 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

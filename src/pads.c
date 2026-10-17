/* The pads. Each vertical blank ReadPads reads both 3-button pads through the I/O chip, the Z80
 * held off its bus meanwhile, and keeps for each the buttons held, newly pressed and newly
 * released (v_padHeld, v_padPressed, v_padReleased: pad 1 at the address, pad 2 at the next).
 * Bits: 0 up, 1 down, 2 left, 3 right, 4 B, 5 C, 6 A, 7 Start. In a game the options can give
 * A, B and C to other buttons (v_buttonMap). */
#include "game.h"

#define TH 0x40                 /* the select line of a pad's port */
#define PAD_3BUTTON 0x0D        /* the device ID of a 3-button pad */
#define BUT_B 4
#define BUT_C 5
#define BUT_A 6

/* $003066. Both pads, with the Z80 off the bus (the I/O chip is behind it). */
void ReadPads(void)
{
    z80_busreq(0x100);
    while (z80_busack() & 1) {}
    ReadPad1();
    ReadPad2();
    z80_busreq(0);
    while (!(z80_busack() & 1)) {}
}

/* One pad: the ID its port answers, and with a 3-button pad its buttons (remapped), the ones
 * pressed since the last frame and the ones released; anything else reads as no buttons. */
static void read_pad(int pad, uint32_t port)
{
    uint8_t id = PadId(port);
    wr8(v_padId + pad, id);
    if (id != PAD_3BUTTON) {
        wr8(v_padHeld + pad, 0);
        wr8(v_padPressed + pad, 0);
        wr8(v_padReleased + pad, 0);
        return;
    }
    io_write8(port, 0);
    uint8_t buttons = (uint8_t)(io_read8(port) << 2) & 0xC0;    /* Start and A */
    io_write8(port, TH);
    buttons |= io_read8(port) & 0x3F;                            /* C, B and the d-pad */
    buttons = RemapButtons((uint8_t)~buttons, (uint16_t)(v_buttonMap + pad));
    uint8_t old = rd8(v_padHeld + pad), changed = old ^ buttons;
    wr8(v_padHeld + pad, buttons);
    wr8(v_padReleased + pad, old & changed);
    wr8(v_padPressed + pad, buttons & changed);
}

/* $003094 */
void ReadPad1(void) { read_pad(0, PAD1_DATA); }

/* $0030B6 */
void ReadPad2(void) { read_pad(1, PAD2_DATA); }

/* $00311E. In a game (v_pauseFlags bit 6), A, B and C each take the raw button whose bit the
 * map names: map[0] for A, map[2] for B, map[4] for C. */
uint8_t RemapButtons(uint8_t buttons, uint16_t map)
{
    if (!(rd8(v_pauseFlags) & 0x40)) return buttons;
    uint8_t raw = buttons;
    buttons = CopyBit(buttons, raw, BUT_A, rd8(map));
    buttons = CopyBit(buttons, raw, BUT_B, rd8((uint16_t)(map + 2)));
    return CopyBit(buttons, raw, BUT_C, rd8((uint16_t)(map + 4)));
}

/* $003148. Bit `to` of buttons set to bit `from` of raw. */
uint8_t CopyBit(uint8_t buttons, uint8_t raw, uint8_t to, uint8_t from)
{
    if ((raw >> (from & 31)) & 1) return (uint8_t)(buttons | 1u << (to & 31));
    return (uint8_t)(buttons & ~(1u << (to & 31)));
}

/* $003156. The device on a port, Sega's way: two bits from the d-pad lines with TH high, two
 * with it low. */
uint8_t PadId(uint32_t port)
{
    io_write8(port, TH);
    uint8_t high = io_read8(port);
    high |= (uint8_t)(high << 1);
    uint8_t id = (high & 8) | ((uint8_t)(high << 1) & 4);
    io_write8(port, 0);
    uint8_t low = io_read8(port);
    low |= low >> 1;
    return id | ((low >> 1) & 2) | (low & 1);
}

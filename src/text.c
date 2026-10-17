/* Text: lines of characters written as tile words into a buffer that grows from $FFF400 up
 * (v_dmaQueue + 2 keeps its end), each line sent to VRAM by a DMA in the queue (which grows
 * from $FFF400 down). */
#include "game.h"

/* A text list: lines of [an attribute word (DrawText only)][the VRAM command, a long][the
 * characters], a line ending at $FE (then on to an even address) and the list at $FF. Drawing
 * writes attribute | character; erasing writes 0. */
static void text(uint32_t list, int with_attr, int draw, uint16_t word)
{
    uint16_t q = rd16(v_dmaQueue), buf = rd16(v_dmaQueue + 2);
    for (;;) {
        if (with_attr) {
            word = bus_rd16(list);
            list += 2;
        }
        q -= 4; wr32(q, bus_rd32(list));
        list += 4;
        uint32_t src = (RAM_BASE | buf) >> 1;
        q -= 2; wr16(q, (uint16_t)(0x9500 | (src & 0xFF)));
        q -= 2; wr16(q, (uint16_t)(0x9600 | (src >> 8 & 0xFF)));
        q -= 2; wr16(q, (uint16_t)(0x9700 | (src >> 16 & 0xFF)));
        q -= 2; wr16(q, 0x8F02);
        uint32_t length = 0x94009300;
        for (;;) {
            uint8_t c = bus_rd8(list++);
            if (c == 0xFE || c == 0xFF) {
                q -= 4; wr32(q, length);
                if (c == 0xFF) {
                    wr16(v_dmaQueue, q);
                    wr16(v_dmaQueue + 2, buf);
                    return;
                }
                if (list & 1) list++;
                break;
            }
            word = (uint16_t)((word & 0xFF00) | c);
            wr16(buf, draw ? word : 0);
            buf += 2;
            length = (length & 0xFFFFFF00) | (uint8_t)(length + 1);
        }
    }
}

/* $00187C (and $001878: the same text erased, never called) */
void DrawText(uint32_t list) { text(list, 1, 1, 0); }

/* $0018F8. Lines without an attribute word: the tile words' high byte is the caller's (its d0). */
void DrawTextPlain(uint32_t list, uint16_t attr) { text(list, 0, 1, attr); }

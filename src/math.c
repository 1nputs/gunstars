/* Arithmetic the game shares: the angle of a vector, the score's packed decimal. */
#include "game.h"

#define ATAN_TABLE 0x003326u     /* 256 words: the angle of each slope up to 1 (a 45th of 2^13) */

/* the slope's angle from the table, and what the 68k leaves in the register it divided:
 * the remainder over the table index (times 2) */
static uint16_t atan_of(uint16_t small, uint16_t big, uint32_t *reg)
{
    uint32_t n = (uint32_t)small << 16;
    uint16_t slope = (uint16_t)(n / big), index = (uint16_t)((slope >> 8) * 2);
    if (reg) *reg = (n % big) << 16 | index;
    return rom16(ATAN_TABLE + index);
}

/* $003202. The angle of (dx, dy) (dy the low word of *d1): 0 along +x, $4000 along +y, a turn
 * $10000. *d1 is left as the 68k leaves its d1 (a caller passes it on). */
uint16_t Atan2(int16_t dx, uint32_t *d1)
{
    int16_t dy = (int16_t)*d1;
    if (dx == 0) return dy < 0 ? 0xC000 : dy > 0 ? 0x4000 : 0;
    if (dy == 0) return dx < 0 ? 0x8000 : 0;
    uint16_t x = (uint16_t)(dx < 0 ? -dx : dx), y = (uint16_t)(dy < 0 ? -dy : dy);
    if (dy < 0) *d1 = (*d1 & 0xFFFF0000) | y;
    if (dx > 0 && dy > 0) {
        if (y < x) return atan_of(y, x, d1);
        if (y == x) return 0x2000;
        return (uint16_t)(0x4000 - atan_of(x, y, 0));
    }
    if (dx > 0) {
        if (y < x) return (uint16_t)(0 - atan_of(y, x, d1));
        if (y == x) return 0xE000;
        return (uint16_t)(0xC000 + atan_of(x, y, 0));
    }
    if (dy > 0) {
        if (y < x) return (uint16_t)(0x8000 - atan_of(y, x, d1));
        if (y == x) return 0x6000;
        return (uint16_t)(0x4000 + atan_of(x, y, 0));
    }
    if (y < x) return (uint16_t)(0x8000 + atan_of(y, x, d1));
    if (y == x) return 0xA000;
    return (uint16_t)(0xC000 - atan_of(x, y, 0));
}

/* the 68000's abcd (as the oracle's core has it): two BCD bytes and X */
static uint8_t abcd(uint8_t a, uint8_t b, int *x)
{
    unsigned r = (a & 0x0F) + (b & 0x0F) + (unsigned)*x;
    if (r > 9) r += 6;
    r += (a & 0xF0) + (b & 0xF0);
    *x = r > 0x99;
    if (*x) r -= 0xA0;
    return (uint8_t)r;
}

/* $0035D6. Points (BCD) to the players the object credits (+$23 bit 7 player 1, bit 6 player 2),
 * their scores marked to be shown again. */
void AddScore(uint16_t obj, uint32_t points)
{
    if (rd8((uint16_t)(obj + 0x23)) & 0x80) {
        set8(v_player1 + 0x81, 1);
        AddBcd(v_player1 + 0x69, points);
    }
    if (rd8((uint16_t)(obj + 0x23)) & 0x40) {
        set8(v_player2 + 0x81, 1);
        AddBcd(v_player2 + 0x69, points);
    }
}

/* $003602. The 5-byte BCD number ending at `end`, plus points; 9,999,999,999 at most. */
void AddBcd(uint16_t end, uint32_t points)
{
    wr8(0xA005, 0);
    wr32(0xA006, points);
    int x = 0;
    uint16_t s = 0xA00A, d = end;
    for (int i = 0; i < 5; i++) {
        s--; d--;
        wr8(d, abcd(rd8(d), rd8(s), &x));
    }
    if (rd8(d) >= 0x10) {
        wr32(d, 0x09999999);
        wr8((uint16_t)(d + 4), 0x99);
    }
}

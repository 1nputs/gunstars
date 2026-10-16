/* Work RAM, the cartridge, and the 68k's bus for addresses the game computes (mem.h). */
#include <stdio.h>
#include <stdlib.h>
#include "mem.h"

uint8_t ram[RAM_SIZE];
const uint8_t *rom_image;

/* addresses outside RAM and ROM are hardware, which the port reaches through hal.h only: a bus
 * access there is a porting mistake */
static void bad_bus(uint32_t a)
{
    fprintf(stderr, "bus access to %06X: not RAM or ROM\n", a & 0xFFFFFFu);
    abort();
}

uint8_t bus_rd8(uint32_t a)
{
    if (is_ram(a)) return ram[(uint16_t)a];
    if ((a & 0xFFFFFFu) < ROM_SIZE) return rom8(a);
    bad_bus(a);
    return 0;
}

uint16_t bus_rd16(uint32_t a)
{
    if (is_ram(a)) return rd16((uint16_t)a);
    if ((a & 0xFFFFFFu) < ROM_SIZE) return rom16(a);
    bad_bus(a);
    return 0;
}

uint32_t bus_rd32(uint32_t a)
{
    return (uint32_t)bus_rd16(a) << 16 | bus_rd16(a + 2);
}

void bus_wr8(uint32_t a, uint8_t v)
{
    if (!is_ram(a)) bad_bus(a);
    ram[(uint16_t)a] = v;
}

void bus_wr16(uint32_t a, uint16_t v)
{
    if (!is_ram(a)) bad_bus(a);
    wr16((uint16_t)a, v);
}

void bus_wr32(uint32_t a, uint32_t v)
{
    bus_wr16(a, (uint16_t)(v >> 16));
    bus_wr16(a + 2, (uint16_t)v);
}

static uint32_t crc32(const uint8_t *p, long n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (long i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

#define ROM_CRC32 0xB813CF0Du

int rom_check(const uint8_t *data, long size)
{
    return size == (long)ROM_SIZE && crc32(data, size) == ROM_CRC32;
}

int rom_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t *buf = malloc(ROM_SIZE + 1);
    long n = buf ? (long)fread(buf, 1, ROM_SIZE + 1, f) : 0;
    fclose(f);
    if (!buf || !rom_check(buf, n)) { free(buf); return 0; }
    rom_image = buf;
    return 1;
}

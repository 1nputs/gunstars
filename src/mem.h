/* The 68k's memory as the port sees it: work RAM as a byte array in the 68k's byte order (so the
 * tests compare it with the oracle's by memcmp, and a state is these bytes), the cartridge read
 * at run time. Every compiler builds this: no storage-order attributes.
 *
 * rd8/rd16/rd32 and wr8/wr16/wr32 take a work RAM offset: the low word of $FFxxxx, which is what
 * an absolute short address names ($FFFFA204).w -> 0xA204. bus_rd* / bus_wr* take a 68k address
 * the game keeps in a register or in RAM, which can be ROM or work RAM. A word or long access
 * at an odd address is an address error on the 68k; the game makes none, so nothing checks. */
#pragma once
#include <stdint.h>

#define RAM_SIZE 0x10000u
#define ROM_SIZE 0x100000u
#define RAM_BASE 0xFF0000u
/* the 68k's stack: from $FFFF00 down, never below $FFFDD4 in the recorded sessions. Not compared
 * by the tests (the C port's stack is C's) */
#define STACK_LO 0xFD00u
#define STACK_HI 0xFF00u

extern uint8_t ram[RAM_SIZE];
extern const uint8_t *rom_image;

static inline uint8_t rd8(uint16_t a) { return ram[a]; }
static inline uint16_t rd16(uint16_t a) { return (uint16_t)(ram[a] << 8 | ram[(uint16_t)(a + 1)]); }
static inline uint32_t rd32(uint16_t a) { return (uint32_t)rd16(a) << 16 | rd16((uint16_t)(a + 2)); }
static inline void wr8(uint16_t a, uint8_t v) { ram[a] = v; }
static inline void wr16(uint16_t a, uint16_t v) { ram[a] = (uint8_t)(v >> 8); ram[(uint16_t)(a + 1)] = (uint8_t)v; }
static inline void wr32(uint16_t a, uint32_t v) { wr16(a, (uint16_t)(v >> 16)); wr16((uint16_t)(a + 2), (uint16_t)v); }

/* the bits of a byte, as btst / bset / bclr on memory name them */
static inline int bit8(uint16_t a, int n) { return (ram[a] >> n) & 1; }
static inline void set8(uint16_t a, int n) { ram[a] |= (uint8_t)(1u << n); }
static inline void clr8(uint16_t a, int n) { ram[a] &= (uint8_t)~(1u << n); }

/* a 68k address: work RAM ($E00000-$FFFFFF, mirrored every 64 KB) or the cartridge */
static inline int is_ram(uint32_t a) { return (a & 0xE00000u) == 0xE00000u; }
uint8_t bus_rd8(uint32_t a);
uint16_t bus_rd16(uint32_t a);
uint32_t bus_rd32(uint32_t a);
void bus_wr8(uint32_t a, uint8_t v);
void bus_wr16(uint32_t a, uint16_t v);
void bus_wr32(uint32_t a, uint32_t v);

/* the cartridge's bytes by 68k address */
static inline uint8_t rom8(uint32_t a) { return rom_image[a & (ROM_SIZE - 1)]; }
static inline uint16_t rom16(uint32_t a) { return (uint16_t)(rom8(a) << 8 | rom8(a + 1)); }
static inline uint32_t rom32(uint32_t a) { return (uint32_t)rom16(a) << 16 | rom16(a + 2); }

/* load the ROM (1 MB, the retail image's CRC32); 0 on failure */
int rom_load(const char *path);
int rom_check(const uint8_t *data, long size);

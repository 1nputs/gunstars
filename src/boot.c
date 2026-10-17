/* The reset and the main loop. The reset is Sega's start-up sequence (the TMSS, the VDP's
 * registers and memory, the Z80 parked on a small loop, the PSG quiet), skipped when the I/O
 * chip says the console was reset rather than powered on; then the game's own: the ROM checksum,
 * a warm start kept when 'TREASURE' is in RAM (with the reset button), RAM and the VDP set up,
 * the sound driver started, and the main loop, which runs the background jobs. */
#include "game.h"

#define BOOT_TABLE 0x00028Eu     /* Sega's start-up data: counts, ports, VDP registers, Z80 code */
#define BOOT_REGS (BOOT_TABLE + 26)
#define BOOT_DMA_FILL (BOOT_REGS + 24)
#define BOOT_Z80 (BOOT_DMA_FILL + 4)
#define BOOT_Z80_SIZE 38
#define BOOT_VDP (BOOT_Z80 + BOOT_Z80_SIZE)
#define BOOT_PSG (BOOT_VDP + 12)
#define VDP_INIT 0x002AFAu       /* the game's 24 VDP registers */
#define HEADER_CHECKSUM 0x00018Eu
#define HEADER_ROM_END 0x0001A4u
#define SEGA 0x53454741u
#define TREA 0x54524541u
#define SURE 0x53555245u

int checksum_check = 1;          /* the ROM checksum: off for a patched image (QOL.md) */

/* Sega's start-up, on a power on (port C's control and the expansion port both 0). */
static void console_start(void)
{
    if (io_read32(0xA10008)) return;
    if (io_read16(0xA1000C)) return;
    uint16_t reg = rom16(BOOT_TABLE), step = rom16(BOOT_TABLE + 4);
    if (io_read8(IO_VERSION) & 0x0F) tmss_write(SEGA);
    vdp_status();
    for (int i = 0; i < 24; i++) {
        reg = (uint16_t)((reg & 0xFF00) | rom8(BOOT_REGS + (uint32_t)i));
        vdp_ctrl16(reg);
        reg += step;
    }
    vdp_ctrl(rom32(BOOT_DMA_FILL));                              /* VRAM filled with 0 */
    vdp_data(0);
    z80_busreq(0x100);
    z80_reset(0x100);
    while (z80_busack() & 1) {}
    for (int i = 0; i < BOOT_Z80_SIZE; i++) z80_ram_write8((uint16_t)i, rom8(BOOT_Z80 + (uint32_t)i));
    z80_reset(0);
    z80_busreq(0);
    z80_reset(0x100);
    for (uint32_t a = 0; a < RAM_SIZE; a++) ram[a] = 0;
    vdp_ctrl(rom32(BOOT_VDP));                                   /* auto-increment 2 */
    vdp_ctrl(rom32(BOOT_VDP + 4));                               /* CRAM 0 */
    for (int i = 0; i < 32; i++) vdp_data32(0);
    vdp_ctrl(rom32(BOOT_VDP + 8));                               /* VSRAM 0 */
    for (int i = 0; i < 20; i++) vdp_data32(0);
    for (int i = 0; i < 4; i++) psg_write(rom8(BOOT_PSG + (uint32_t)i));
    z80_reset(0);
    cpu_set_sr(0x2700);
}

/* the checksum's failure: a red screen, for ever */
static void red_screen(void)
{
    VdpInitRegs();
    vdp_ctrl(0xC0000000);
    for (int i = 0; i < 64; i++) vdp_data(0x000E);
    for (;;) hal_wait_vblank();
}

/* $000200 (to MainLoop). */
void Start(void)
{
    console_start();
    vdp_status();
    cpu_set_sr(0x2700);
    z80_reset(0);
    z80_reset(0x100);
    uint8_t version = io_read8(IO_VERSION);
    if (version & 0x0F) tmss_write(SEGA);
    int warm = (io_read8(0xA10007) & 0x40) && rd32(0xFF10) == TREA && rd32(0xFF14) == SURE;
    if (!warm) {
        uint16_t sum = 0;
        uint32_t end = rom32(HEADER_ROM_END);
        for (uint32_t a = 0x200; a <= end; a += 2) sum = (uint16_t)(sum + rom16(a));
        if (checksum_check && sum != rom16(HEADER_CHECKSUM)) red_screen();
        for (uint16_t a = 0xFF00; a; a++) wr8(a, 0);
        wr32(0xFF10, TREA);
        wr32(0xFF14, SURE);
        wr8(v_buttonMap, 6);                     /* A, B, C as the pad has them */
        wr8(v_buttonMap + 1, 6);
        wr8(v_buttonMap + 2, 4);
        wr8(v_buttonMap + 3, 4);
        wr8(v_buttonMap + 4, 5);
        wr8(v_buttonMap + 5, 5);
        wr8(v_consoleVersion, io_read8(IO_VERSION));
        wr16(0xFF0E, 2);
    }
    while (vdp_status() & 2) {}                  /* a DMA still running */
    for (uint32_t a = 0; a < 0xFF00; a++) ram[a] = 0;
    VdpInitRegs();
    IoInit();
    FrameInit();
    z80_busreq(0x100);
    while (z80_busack() & 1) {}
    for (uint16_t a = 0; a < 0x2000; a += 4) z80_ram_write32(a, 0);
    SoundInit();
    wr8(v_soundQueue, 0xF9);                     /* everything stopped */
    SoundDriver();
    wr8(v_frameReady, 1);
    cpu_set_sr(0x2300);
}

/* $000434. The main loop: the background jobs, for ever (the vertical blank does the rest). In
 * C a pass with no job waits for the vertical blank instead of spinning. */
void MainLoop(void)
{
    for (;;) {
        cpu_set_sr(0x2300);
        RunJobs();
        hal_wait_vblank();
    }
}

/* $002AB4. The VDP's 24 registers, and their shadow v_vdpRegs. */
void VdpInitRegs(void)
{
    uint16_t reg = 0x8000;
    for (int i = 0; i < 24; i++) {
        reg = (uint16_t)((reg & 0xFF00) | rom8(VDP_INIT + (uint32_t)i));
        vdp_ctrl16(reg);
        wr16((uint16_t)(v_vdpRegs + 2 * i), reg);
        reg += 0x100;
    }
}

/* $002B2A. The I/O chip: the serial ports off, the pads' TH lines outputs (and high). */
void IoInit(void)
{
    io_write8(0xA10013, 0);
    io_write8(0xA10019, 0);
    io_write8(0xA1001F, 0);
    io_write8(PAD1_CTRL, 0x40);
    io_write8(PAD2_CTRL, 0x40);
    io_write8(0xA1000D, 0);
    io_write8(PAD1_DATA, 0x40);
    wr32(0xFF00, 0);
}

/* $002A7C. The frame's state: the DMA area, the sprite and scroll areas cleared, the pads
 * reset, the DMA queue empty, the scroll tables' addresses. */
void FrameInit(void)
{
    ClearDmaArea();
    ClearRamF000();
    ClearRamB800();
    ResetPads();
    wr16(v_dmaQueue, 0xF400);
    wr16(v_dmaQueue + 2, 0xF400);
    wr32(v_hscrollSrc, 0xFFFFE400);
    wr32(v_vscrollSrc, 0xFFFFEC00);
}

static void clear(uint16_t from, uint16_t bytes)
{
    for (uint16_t i = 0; i < bytes; i++) wr8((uint16_t)(from + i), 0);
}

/* $002DE0. $FFF700-$FFF7CF. */
void ClearDmaArea(void) { clear(v_dmaCommand, 13 * 16); }

/* $002DC8. $FFF000-$FFF6FF. */
void ClearRamF000(void) { clear(0xF000, 0x70 * 16); }

/* $002D00. $FFB800-$FFB8BF. */
void ClearRamB800(void) { clear(0xB800, 12 * 16); }

/* $003050. No pause, the pads as all held (so nothing reads as newly pressed), then read. */
void ResetPads(void)
{
    wr8(v_pauseFlags, 0);
    wr16(v_padHeld, 0xFFFF);
    wr16(v_padPressed, 0);
    wr16(v_padReleased, 0);
    ReadPads();
}

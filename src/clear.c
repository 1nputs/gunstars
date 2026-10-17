/* Clearing: work RAM's areas and VRAM's (the planes, the scroll tables, the sprite table), and
 * the screen resets that a mode starts with. The 68k clears RAM 16 bytes at a time and VRAM a
 * word at a time through the data port, interrupts masked. */
#include "game.h"

#define VDP_INIT_ALT 0x002B12u           /* the other register set (ScreenResetAlt) */

static void clear_ram(uint16_t from, uint32_t bytes)
{
    for (uint32_t i = 0; i < bytes; i++) wr8((uint16_t)(from + i), 0);
}

/* `words` words of VRAM from the address the command names */
static void clear_vram(uint32_t command, uint32_t words)
{
    uint16_t sr = cpu_get_sr();
    cpu_set_sr(0x2700);
    vdp_ctrl16(0x8F02);
    vdp_ctrl(command);
    for (uint32_t i = 0; i < words; i++) vdp_data(0);
    cpu_set_sr(sr);
}

/* the 24 VDP registers' shadow from a table (written out by the next vertical blank) */
static void vdp_shadow(uint32_t table)
{
    uint16_t reg = 0x8000;
    for (int i = 0; i < 24; i++) {
        reg = (uint16_t)((reg & 0xFF00) | rom8(table + (uint32_t)i));
        wr16((uint16_t)(v_vdpRegs + 2 * i), reg);
        reg += 0x100;
    }
}

/* $0029EA. A mode's start: the registers' shadow, VRAM, the objects, the palettes, the job
 * buffers, the frame's state, the game's state, the players. */
void ScreenReset(void)
{
    VdpShadowRegs();
    ClearVideo();
    ClearObjects();
    ClearPalettes();
    ClearBuffers();
    FrameInit();
    ClearRamA200();
    ClearRamA000();
    ClearRamFF8000();
    clear_ram(0xA400, 0x400);
}

/* $002A12. The same with the other register set, the players kept. */
void ScreenResetAlt(void)
{
    VdpShadowRegsAlt();
    ClearVideo();
    ClearObjects();
    ClearPalettes();
    ClearBuffers();
    FrameInit();
    ClearRamA000();
    ClearRamFF8000();
}

/* $002A32. VRAM's planes and scroll tables, RAM from $FF0000, the scroll tables in RAM, VSRAM. */
void ClearVideo(void)
{
    ClearVramC000();
    ClearVramE000();
    ClearVramD000();
    ClearRamFF0000();
    ClearRamA900();
    ClearHScroll();
    ClearVramF000();
    ClearVScroll();
    cpu_set_sr(0x2700);
    vdp_ctrl(0x40000010);                                        /* VSRAM 0 */
    for (int i = 0; i < 20; i++) vdp_data32(0);
    cpu_set_sr(0x2300);
}

/* $002A56 */
void ClearObjects(void)
{
    ClearObjectRam();
    ClearRamA800();
    ClearRamBE00();
    ClearSprites();
    ClearVramF400();
}

/* $002A6C. The palettes, then the palette list at $1BA36. */
void ClearPalettes(void)
{
    ClearPaletteRam();
    LoadPalettes(0x01BA36);
}

/* $002AAA */
void ClearBuffers(void)
{
    ClearTileBuffer();
    ClearVram();
}

/* $002AD6, $002ADE */
void VdpShadowRegs(void) { vdp_shadow(0x002AFA); }
void VdpShadowRegsAlt(void) { vdp_shadow(VDP_INIT_ALT); }

/* $002B68 ... : RAM */
void ClearRamFF0000(void) { clear_ram(0x0000, 0x8000); }
void ClearRamFF8000(void) { clear_ram(0x8000, 0x2000); }
void ClearRamA000(void) { clear_ram(0xA000, 0x100); clear_ram(0xA100, 0x100); }
void ClearRamA200(void) { clear_ram(0xA200, 0x80); }
void ClearPlayers(void) { clear_ram(0xA400, 0x60); clear_ram(0xA600, 0x60); }
void ClearRamA800(void) { clear_ram(0xA800, 0x100); }
void ClearRamA800Head(void) { clear_ram(0xA800, 0x60); }
void ClearRamA900(void) { clear_ram(0xA900, 0x100); }
void ClearTileBuffer(void) { clear_ram(v_tilePixels, 0x400); }
void ClearRamBE00(void) { clear_ram(0xBE00, 0x1C0); }
void ClearSprites(void) { clear_ram(v_sprites, 0x270); }
void ClearPaletteRam(void) { clear_ram(v_palette, 0x100); }
void ClearHScroll(void) { clear_ram(0xE400, 0x800); }
void ClearVScroll(void) { clear_ram(0xEC00, 0xA0); }

/* $002D30. The players' heads, the objects' area's, and $FFBFC0-$FFDFFF. */
void ClearObjectRam(void)
{
    ClearPlayers();
    ClearRamA800Head();
    clear_ram(0xBFC0, 0x2040);
}

/* $002E28 ... : VRAM */
void ClearVram(void) { clear_vram(0x40000000, 0x6000); }        /* VRAM $0000-$BFFF */
void ClearVramC000(void) { clear_vram(0x40000003, 0x800); }
void ClearVramD000(void) { clear_vram(0x50000003, 0x800); }
void ClearVramE000(void) { clear_vram(0x60000003, 0x800); }
void ClearVramF000(void) { clear_vram(0x70000003, 0x200); }     /* the scroll table */
void ClearVramF400(void) { clear_vram(0x74000003, 0x140); }     /* the sprite table */

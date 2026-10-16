/* H-interrupt effects: HintEffect ($001416) runs the one v_hintEffect names each frame; each
 * copies its handler to v_hintCode the first time (v_hintState 0) and sets its registers. */
#include "game.h"

#define RTE 0x4E73
#define VDP_REG0 0                         /* v_vdpRegs + 2 n: register n's word */

/* $001440. Effect 0: H-interrupts off (register 0's bit 4), and a bare rte for a handler. */
void HintNone(void)
{
    if (rd16(v_hintState)) return;
    wr16(v_hintState, (uint16_t)(rd16(v_hintState) + 4));
    wr8(v_vdpRegs + VDP_REG0 + 1, rd8(v_vdpRegs + VDP_REG0 + 1) & 0xEF);
    vdp_ctrl16(rd16(v_vdpRegs + VDP_REG0));
    wr16(v_hintCode, RTE);
}

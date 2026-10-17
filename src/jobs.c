/* Graphics in the background. The main loop runs the job list (RunJobs): each job decodes or
 * copies a block of data from the cartridge to RAM, or to VRAM through v_jobBuffer and the
 * vertical blank's DMA queue, waiting for the vertical blank whenever the buffer is full. Command
 * lists (RunCommands) do the same at once, writing VRAM through the data port. Two formats:
 * tiles (DecodeTile: a run-length code with vertical marks over a bit stream) and RLE (RleRun). */
#include "game.h"

#define JOB_TABLE 0x00252Cu      /* RunJobs: a routine per job kind */
#define CMD_TABLE 0x0022F0u      /* RunCommands: a routine per command */
#define TILE_BYTES 32
#define TILE_PIXELS 64
#define RLE_CHUNK 0x400          /* bytes an RLE chunk fills of v_tilePixels */

/* The VDP command that writes VRAM at addr. */
static uint32_t vram_write(uint32_t addr)
{
    return ((addr & 0x3FFF) | 0x4000) << 16 | (uint16_t)((addr << 2) >> 16);
}

static void wait_buffer(void)
{
    while (rd8(v_jobBufferFull)) hal_wait_vblank();
}

/* ---- command lists -------------------------------------------------------------------------- */

/* $0022DE. Each command: a word (its kind; negative ends the list) and its arguments. */
void RunCommands(uint32_t list)
{
    for (;;) {
        uint16_t kind = bus_rd16(list);
        list += 2;
        if (kind & 0x8000) return;
        uint32_t cmd = rom32(CMD_TABLE + (uint32_t)(int16_t)(kind << 2));
        switch (cmd) {
        case SYM_CmdCopyToRam: list = CmdCopyToRam(list); break;
        case SYM_CmdCopyToVram: list = CmdCopyToVram(list); break;
        case SYM_CmdDecodeToRam: list = CmdDecodeToRam(list); break;
        case SYM_CmdDecodeToVram: list = CmdDecodeToVram(list); break;
        case SYM_CmdRleToRam: list = CmdRleToRam(list); break;
        case SYM_CmdRleToVram: list = CmdRleToVram(list); break;
        default: call68k_missing(cmd); return;
        }
    }
}

/* $002310. Source (long), RAM destination (word): the source's first word is its length in
 * bytes, copied in longs. */
uint32_t CmdCopyToRam(uint32_t list)
{
    uint32_t src = bus_rd32(list), dst = 0xFFFF0000u | bus_rd16(list + 4);
    uint16_t n = (uint16_t)((bus_rd16(src) >> 2) - 1);
    src += 2;
    do {
        bus_wr32(dst, bus_rd32(src));
        dst += 4; src += 4;
    } while (n-- != 0);
    return list + 6;
}

/* $002326. Source (long), VRAM destination (word): the source's length word, then the data,
 * written in chunks of $200 bytes, each from the destination again (the 68k never moves it on). */
uint32_t CmdCopyToVram(uint32_t list)
{
    uint32_t src = bus_rd32(list);
    uint16_t vram = bus_rd16(list + 4);
    int16_t left = (int16_t)bus_rd16(src);
    src += 2;
    do {
        int16_t chunk = left >= 0x200 ? 0x200 : left;
        left = (int16_t)(left - chunk);
        uint16_t sr = cpu_get_sr();
        cpu_set_sr(0x2700);
        vdp_ctrl16(0x8F02);
        vdp_ctrl(vram_write(vram));
        uint16_t n = (uint16_t)(((uint16_t)chunk >> 2) - 1);
        do {
            vdp_data32(bus_rd32(src));
            src += 4;
        } while (n-- != 0);
        cpu_set_sr(sr);
    } while (left);
    return list + 6;
}

/* Tiles from a source whose first byte is their number less one, the bit stream starting with
 * its second byte (DecodeTile). */
static uint32_t tiles_start(uint32_t src, uint16_t *count)
{
    *count = bus_rd8(src);
    uint16_t first = bus_rd16(src);
    wr16(v_bitsLeft, 8);
    wr16(v_bitBuffer, (uint16_t)(first << 8 | first >> 8));
    ClearTilePixels();
    return src + 2;
}

/* $002374. Source (long), RAM destination (word): tiles, packed into RAM. */
uint32_t CmdDecodeToRam(uint32_t list)
{
    uint32_t dst = 0xFFFF0000u | bus_rd16(list + 4);
    uint16_t n;
    uint32_t src = tiles_start(bus_rd32(list), &n);
    do {
        src = DecodeTile(src);
        dst = PackTile(dst);
    } while (n-- != 0);
    return list + 6;
}

/* $0023A0. Source (long), VRAM destination (word): tiles, written to VRAM. */
uint32_t CmdDecodeToVram(uint32_t list)
{
    uint32_t vram = 0xFFFF0000u | bus_rd16(list + 4);
    uint16_t n;
    uint32_t src = tiles_start(bus_rd32(list), &n);
    do {
        src = DecodeTile(src);
        vram = WriteTileVram(vram);
    } while (n-- != 0);
    return list + 6;
}

/* $0023CC. Source (long: its length word, then the RLE data), RAM destination (word). */
uint32_t CmdRleToRam(uint32_t list)
{
    uint32_t src = bus_rd32(list), dst = 0xFFFF0000u | bus_rd16(list + 4);
    uint32_t end = src + 2 + bus_rd16(src);
    src += 2;
    do RleChunk(end, &src, &dst); while (src < end);
    return list + 6;
}

/* $0023E6. Source (long), VRAM destination (word): RLE through v_tilePixels, $400 bytes at a
 * time through the data port; the last chunk's words and one more. */
uint32_t CmdRleToVram(uint32_t list)
{
    uint32_t src = bus_rd32(list), vram = bus_rd16(list + 4);
    uint32_t end = src + 2 + bus_rd16(src);
    src += 2;
    for (;;) {
        uint32_t buf = RAM_BASE | v_tilePixels;
        RleChunk(end, &src, &buf);
        uint16_t sr;
        if (src >= end) {
            uint16_t n = (uint16_t)((uint16_t)(buf - v_tilePixels) >> 1);
            uint16_t at = v_tilePixels;
            sr = cpu_get_sr();
            cpu_set_sr(0x2700);
            vdp_ctrl16(0x8F02);
            vdp_ctrl(vram_write(vram));
            do {
                vdp_data(rd16(at));
                at += 2;
            } while (n-- != 0);
            cpu_set_sr(sr);
            return list + 6;
        }
        sr = cpu_get_sr();
        cpu_set_sr(0x2700);
        vdp_ctrl16(0x8F02);
        vdp_ctrl(vram_write(vram));
        for (uint16_t i = 0; i < RLE_CHUNK / 4; i++) vdp_data32(rd32((uint16_t)(v_tilePixels + 4 * i)));
        cpu_set_sr(sr);
        vram += RLE_CHUNK;
    }
}

/* ---- the job list --------------------------------------------------------------------------- */

/* $002506. The jobs from v_jobList, each by its kind, until the list ends. */
void RunJobs(void)
{
    uint16_t kind = rd16(v_jobKind);
    if (!kind) return;
    uint32_t list = rd32(v_jobList);
    do {
        uint32_t job = rom32(JOB_TABLE + (uint32_t)(int16_t)(kind << 2));
        switch (job) {
        case SYM_JobCopyToRam: JobCopyToRam(); break;
        case SYM_JobCopyToVram: JobCopyToVram(); break;
        case SYM_JobDecodeToRam: JobDecodeToRam(); break;
        case SYM_JobDecodeToVram: JobDecodeToVram(); break;
        case SYM_JobRleToRam: JobRleToRam(); break;
        case SYM_JobRleToVram: JobRleToVram(); break;
        default: call68k_missing(job); break;
        }
        kind = NextJob(&list);
    } while (kind != 0xFFFF);
    wr16(v_jobKind, 0);
}

/* $002478. The next job: its kind (negative: the list's end, returned as it is), the source,
 * the destination (a VRAM address with bit 0 of the kind, else RAM), into v_job*. Bit 1 of the
 * kind: compressed, as tiles, or as RLE with bit 2. */
uint16_t NextJob(uint32_t *list)
{
    uint16_t kind = bus_rd16(*list);
    *list += 2;
    if (kind & 0x8000) return kind;
    kind |= 0x8000;
    wr16(v_jobKind, kind);
    uint32_t src = bus_rd32(*list);
    uint16_t dst = bus_rd16(*list + 4);
    *list += 6;
    wr32(v_jobDst, kind & 1 ? dst : 0xFFFF0000u | dst);
    wr32(v_jobList, *list);
    if (!(kind & 2)) {
        wr16(v_jobLength, bus_rd16(src));
        wr32(v_jobSrc, src + 2);
    } else if (kind & 4) {
        uint16_t length = bus_rd16(src);
        wr32(v_jobSrc, src + 2);
        wr32(v_bitsLeft, src + 2 + length);
    } else {
        wr16(v_jobLength, (uint16_t)(bus_rd8(src) + 1));
        wr16(v_bitsLeft, 8);
        uint16_t first = bus_rd16(src);
        wr16(v_bitBuffer, (uint16_t)(first << 8 | first >> 8));
        wr32(v_jobSrc, src + 2);
        ClearTilePixels();
    }
    return kind;
}

/* $00254C. v_jobLength bytes copied, 8 at a time. */
void JobCopyToRam(void)
{
    uint32_t src = rd32(v_jobSrc), dst = rd32(v_jobDst);
    uint16_t left = rd16(v_jobLength), before;
    do {
        bus_wr32(dst, bus_rd32(src));
        bus_wr32(dst + 4, bus_rd32(src + 4));
        src += 8; dst += 8;
        before = left;
        left -= 8;
    } while (before > 8);
}

/* $002562. v_jobLength bytes by DMA, $200 at a time. The VRAM address is the high word of
 * v_jobDst (where a VRAM job has 0), not the low one. */
void JobCopyToVram(void)
{
    uint32_t src = rd32(v_jobSrc), vram = (uint32_t)(int16_t)rd16(v_jobDst);
    uint16_t left = rd16(v_jobLength);
    do {
        uint16_t chunk = (int16_t)left >= 0x200 ? 0x200 : left;
        QueueTileDma(chunk, &src, &vram);
        wait_buffer();
        left -= chunk;
    } while (left);
}

/* $00258A. v_jobLength tiles decoded into RAM. */
void JobDecodeToRam(void)
{
    uint32_t src = rd32(v_jobSrc), dst = rd32(v_jobDst);
    uint16_t n = (uint16_t)(rd16(v_jobLength) - 1);
    do {
        src = DecodeTile(src);
        dst = PackTile(dst);
    } while (n-- != 0);
}

/* $0025A6. v_jobLength tiles to VRAM: 16 at a time into v_jobBuffer, each lot DMAed in the
 * vertical blank before the next. */
void JobDecodeToVram(void)
{
    uint32_t src = rd32(v_jobSrc), vram = rd32(v_jobDst);
    uint16_t left = rd16(v_jobLength);
    for (;;) {
        uint32_t buf = RAM_BASE | v_jobBuffer;
        uint16_t tiles = (int16_t)left >= 16 ? 16 : left;
        uint16_t bytes = (uint16_t)(tiles * TILE_BYTES);
        left -= tiles;
        uint16_t n = (uint16_t)(tiles - 1);
        do {
            src = DecodeTile(src);
            buf = PackTile(buf);
        } while (n-- != 0);
        buf = RAM_BASE | v_jobBuffer;
        QueueTileDma(bytes, &buf, &vram);
        wait_buffer();
        if (!left) return;
    }
}

/* $0025FC. RLE into RAM to the source's end; the job list ends with it. */
void JobRleToRam(void)
{
    uint32_t src = rd32(v_jobSrc), dst = rd32(v_jobDst), end = rd32(v_bitsLeft);
    do RleChunk(end, &src, &dst); while (src < end);
    wr16(v_jobKind, 0);
}

/* $002616. RLE to VRAM by DMA, $400 bytes at a time. Only a chunk that overruns the source's
 * end is sent as it is, and the loop then goes on: no job of this kind ever ran. */
void JobRleToVram(void)
{
    uint32_t src = rd32(v_jobSrc), vram = rd32(v_jobDst), end = rd32(v_bitsLeft);
    for (;;) {
        uint32_t buf = RAM_BASE | v_tilePixels;
        RleChunk(end, &src, &buf);
        uint16_t bytes = end < src ? (uint16_t)(buf - v_tilePixels) : RLE_CHUNK;
        buf = RAM_BASE | v_tilePixels;
        QueueTileDma(bytes, &buf, &vram);
        if (bytes == RLE_CHUNK) wait_buffer();
    }
}

/* ---- RLE ------------------------------------------------------------------------------------ */

/* $002660. RLE commands until the source's end or $400 bytes out. */
void RleChunk(uint32_t end, uint32_t *src, uint32_t *dst)
{
    uint16_t limit = (uint16_t)(*dst + RLE_CHUNK);
    do RleRun(src, dst); while (*src < end && limit > (uint16_t)*dst);
}

/* $00267E. One command byte c, and its data:
 *   c & $80: copy ((c >> 2) & $1F) + 2 bytes from ((c & 3) << 8 | next) + 1 bytes back
 *   $20: (c & $1F) + 2 times the next byte       $60: (c & $1F) + 2 times the next byte, each
 *   $40: (c & $1F) + 2 times the next 2 bytes         followed by a literal byte
 *   $00: (c & $1F) + 1 literal bytes */
void RleRun(uint32_t *src, uint32_t *dst)
{
    uint32_t s = *src, d = *dst;
    uint8_t c = bus_rd8(s++);
    int n = (c & 0x1F) + 1;
    if (c & 0x80) {
        n = ((c >> 2) & 0x1F) + 1;
        uint16_t back = (uint16_t)(((c << 8 | bus_rd8(s++)) & 0x3FF) + 1);
        uint32_t from = d - back;
        for (int i = 0; i <= n; i++) bus_wr8(d++, bus_rd8(from++));
    } else if ((c & 0x60) == 0x20) {
        uint8_t v = bus_rd8(s++);
        for (int i = 0; i <= n; i++) bus_wr8(d++, v);
    } else if ((c & 0x60) == 0x60) {
        uint8_t v = bus_rd8(s++);
        for (int i = 0; i <= n; i++) { bus_wr8(d++, v); bus_wr8(d++, bus_rd8(s++)); }
    } else if (c & 0x40) {
        uint8_t v = bus_rd8(s++), w = bus_rd8(s++);
        for (int i = 0; i <= n; i++) { bus_wr8(d++, v); bus_wr8(d++, w); }
    } else {
        for (int i = 0; i < n; i++) bus_wr8(d++, bus_rd8(s++));
    }
    *src = s; *dst = d;
}

/* ---- tiles ---------------------------------------------------------------------------------- */

/* $002702 */
void ClearTilePixels(void)
{
    for (int i = 0; i < TILE_PIXELS; i++) wr16((uint16_t)(v_tilePixels + 2 * i), 0);
}

/* The tile decoder's bits: buf has the ones not read yet at its top, `left` of them. The 68k
 * reads them in four ways, which leave different bits at the bottom of buf (rotated round, or
 * shifted out); buf goes back into v_bitBuffer, so each way is kept. */
typedef struct { uint32_t src; uint16_t buf; int left; } Bits;

static uint16_t next_word(Bits *b)
{
    uint16_t w = bus_rd16(b->src);
    b->src += 2;
    return w;
}

/* n bits; with rotate, the ones read go round to the bottom of buf, else they shift out */
static uint16_t read_bits(Bits *b, int n, int rotate)
{
    if (b->left > n) {
        uint16_t v = b->buf >> (16 - n);
        b->buf = (uint16_t)(b->buf << n | (rotate ? v : 0));
        b->left -= n;
        return v;
    }
    if (b->left == n) {
        uint16_t v = b->buf >> (16 - n);
        b->buf = next_word(b);
        b->left = 16;
        return v;
    }
    int k = b->left, rest = n - k;
    uint16_t w = next_word(b);
    uint16_t v = (uint16_t)((b->buf >> (16 - k)) << rest | w >> (16 - rest));
    b->buf = (uint16_t)(w << rest);
    b->left = 16 - rest;
    return v;
}

/* 2 bits: as read_bits(2) with rotation, but with one bit left the 68k keeps that bit at the
 * bottom of the new word */
static uint16_t read2(Bits *b)
{
    if (b->left != 1) return read_bits(b, 2, 1);
    uint16_t old = b->buf >> 15, w = next_word(b);
    b->buf = (uint16_t)(w << 1 | old);
    b->left = 15;
    return (uint16_t)(old << 1 | w >> 15);
}

static int read_bit(Bits *b)
{
    int bit = b->buf >> 15;
    if (--b->left) b->buf = (uint16_t)(b->buf << 1);
    else { b->buf = next_word(b); b->left = 16; }
    return bit;
}

/* $002712. One tile into v_tilePixels from the bit stream at src (its state in v_bitsLeft and
 * v_bitBuffer); returns where the stream has got to. Runs, until the 64 pixels are done: a
 * colour (4 bits) and a flag bit; with the flag, marks of that colour in the rows below, each
 * from the last: 2 bits, 1-3 for 7, 8 or 9 pixels on, 0 then a bit: 0 for no more marks, 1 and
 * a bit for 6 or 10 on; then the run's length, a gamma code (n 1 bits, n bits of value v:
 * 2^n + v - 2 pixels after the first). The colour fills on, and a mark met turns it to the
 * mark's colour. */
uint32_t DecodeTile(uint32_t src)
{
    Bits b = { src, rd16(v_bitBuffer), (int16_t)rd16(v_bitsLeft) };
    uint16_t px = v_tilePixels;
    do {
        uint16_t c = read_bits(&b, 5, 1) & 0x1F;
        uint16_t fill = c >> 1, mark = fill | 0x8000;
        wr16(px, fill);
        px += 2;
        if (c & 1) {
            uint16_t at = 0;
            for (;;) {
                uint16_t step = read2(&b);
                if (step) at = (uint16_t)(at + (step + 6) * 2);
                else if (!read_bit(&b)) break;
                else at = (uint16_t)(at + (read_bit(&b) ? 20 : 12));
                wr16((uint16_t)(px + at - 2), mark);
            }
        }
        int n = 0;
        uint16_t length = 1;
        do { n++; length = (uint16_t)(length << 1); } while (read_bit(&b));
        length = (uint16_t)(length + read_bits(&b, n, 0));
        if (length >= 3) {
            uint16_t k = (uint16_t)(length - 3);
            do {
                uint16_t p = rd16(px);
                if (p & 0x8000) fill = (fill & 0xFF00) | (p & 0xFF);
                wr16(px, fill);
                px += 2;
            } while (k-- != 0);
        }
    } while (px < v_tilePixels + 2 * TILE_PIXELS);
    wr16(v_bitsLeft, (uint16_t)b.left);
    wr16(v_bitBuffer, b.buf);
    return b.src;
}

/* A row of v_tilePixels as 4bpp: eight pixels, the first in the top nibble. */
static uint32_t tile_row(int row)
{
    uint16_t at = (uint16_t)(v_tilePixels + 16 * row), hi = 0, lo = 0;
    for (int i = 0; i < 4; i++) hi = (uint16_t)((hi << 4) + rd16((uint16_t)(at + 2 * i)));
    for (int i = 4; i < 8; i++) lo = (uint16_t)((lo << 4) + rd16((uint16_t)(at + 2 * i)));
    return (uint32_t)hi << 16 | lo;
}

/* $002886 */
uint32_t PackTile(uint32_t dst)
{
    for (int row = 0; row < 8; row++, dst += 4) bus_wr32(dst, tile_row(row));
    return dst;
}

/* $0028B2. Through the data port at VRAM vram (its low word); returns the next tile's. */
uint32_t WriteTileVram(uint32_t vram)
{
    uint16_t sr = cpu_get_sr();
    cpu_set_sr(0x2700);
    vdp_ctrl16(0x8F02);
    vdp_ctrl(vram_write(vram & 0xFFFF));
    for (int row = 0; row < 8; row++) vdp_data32(tile_row(row));
    cpu_set_sr(sr);
    return vram + TILE_BYTES;
}

/* $00290C. A DMA of `bytes` from src to VRAM vram into the queue, both moved on past it; the
 * job waits for the vertical blank to take it (v_jobBufferFull). */
void QueueTileDma(uint32_t bytes, uint32_t *src, uint32_t *vram)
{
    uint16_t sr = cpu_get_sr();
    cpu_set_sr(0x2700);
    uint16_t words = (uint16_t)((uint16_t)bytes >> 1);
    uint32_t command = (uint32_t)((*vram & 0x3FFF) | 0x4000) << 16 | 0x80 | ((uint16_t)*vram >> 14);
    QueueDma(command, *src, (0x9400u | words >> 8) << 16 | 0x9300 | (words & 0xFF));
    *src += bytes;
    *vram += (uint32_t)(int16_t)bytes;
    wr8(v_jobBufferFull, 1);
    cpu_set_sr(sr);
}

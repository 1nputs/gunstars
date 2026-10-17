/* Types $1A8 and $1AC: the soldiers' spawner. Its kind (+6: 0, 8, $10 ...) picks its set-up and
 * its pattern; while the camera is not held it brings in soldiers (type $48) from the right and
 * the left and others ($88), each source on its own timer (reset from a table by the difficulty,
 * plus a random 0-15, and run down by the camera's speed); a pattern lasts $E0-$1DF frames.
 * When the camera holds (a fight's end) it becomes something else by its kind. */
#include "game.h"

#define SPAWN_FLAGS   0x8080     /* bit 1: the boss is coming */
#define TIMER_RIGHT   0x8280
#define TIMER_MIDDLE  0x8282
#define TIMER_LEFT    0x8284
#define PATTERN_ALT   0x8298     /* 0 or 4: which of a kind's two patterns */
#define PATTERN_ON    0x829A     /* 0: the pattern's timers not set yet */
#define PATTERN_TIME  0x829C

#define KIND 0x06
#define LEVEL 0xA20C             /* the timers' table index (a word per difficulty) */

/* $03210A. A timer reset: the table's entry for the difficulty plus a random 0-15. */
void SpawnTimer(uint32_t table, uint16_t timer)
{
    Random();
    uint16_t r = rd16(v_randomSeed) & 0xF;
    wr16(timer, (uint16_t)(r + rom16(table + (uint32_t)(int16_t)rd16(LEVEL))));
}

/* $032302. The three timers run down by the camera's speed (and one). */
void SpawnTimersTick(void)
{
    uint16_t d = (uint16_t)(rd16(v_planeSpeeds) + 1);
    wr16(TIMER_RIGHT, (uint16_t)(rd16(TIMER_RIGHT) - d));
    wr16(TIMER_MIDDLE, (uint16_t)(rd16(TIMER_MIDDLE) - d));
    wr16(TIMER_LEFT, (uint16_t)(rd16(TIMER_LEFT) - d));
}

/* a soldier at the right edge, its kind from the table (by the random) */
static void soldier_right(uint16_t slot, uint32_t kinds)
{
    wr16(slot + O_X, 0x1C0);
    wr16(slot + O_TYPE, 0x48);
    wr16(slot + KIND, rom16(kinds + (rd16(v_randomSeed) & 6)));
}

/* $032316. From the right, when its timer has run out: a soldier in a slot from $CB00. */
void SpawnRight(uint32_t kinds)
{
    if (!(rd16(TIMER_RIGHT) & 0x8000)) return;
    SpawnTimer(0x032126, TIMER_RIGHT);
    uint16_t slot, t;
    if (SpawnSlot(&slot, &t)) soldier_right(slot, kinds);
}

/* $032350. The same in a slot from $CC20. */
void SpawnRightLow(uint32_t kinds)
{
    if (!(rd16(TIMER_RIGHT) & 0x8000)) return;
    SpawnTimer(0x032126, TIMER_RIGHT);
    uint16_t slot = 0xCC20, t;
    if (SlotFreeBelowCE(&slot, &t)) soldier_right(slot, kinds);
}

/* $03229E. N: a player is left of x $B0 (player 1 looked at first). */
int PlayersLeftEdge(void)
{
    if (bit8(v_players, 0) && (int16_t)(rd16(v_player1 + O_X) - 0xB0) < 0) return 1;
    if (bit8(v_players, 1)) return (int16_t)(rd16(v_player2 + O_X) - 0xB0) < 0;
    return 0;
}

/* $032376. From the left (unless a player is at that edge): a soldier of `kind` from $CD40. */
void SpawnLeft(uint16_t kind)
{
    if (!(rd16(TIMER_LEFT) & 0x8000)) return;
    SpawnTimer(0x032146, TIMER_LEFT);
    if (PlayersLeftEdge()) return;
    uint16_t slot = 0xCD40, t;
    if (!SlotFreeBelowCE(&slot, &t)) return;
    wr16(slot + O_X, 0x80);
    wr16(slot + O_TYPE, 0x48);
    wr16(slot + KIND, kind);
}

/* $032282. A free slot from $CB00 below $CE00. */
int SpawnSlot(uint16_t *slot, uint16_t *type)
{
    *slot = 0xCB00;
    return SlotFreeBelowCE(slot, type);
}

/* $0323B2: the middle source: an object $88 of `kind` */
static void spawn_middle(uint16_t kind)
{
    if (!(rd16(TIMER_MIDDLE) & 0x8000)) return;
    SpawnTimer(0x032136, TIMER_MIDDLE);
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x88);
    wr16(slot + KIND, kind);
}

/* $0322C4: a pattern's start: its length, the three timers */
static void pattern_start(void)
{
    wr16(PATTERN_TIME, (uint16_t)((rd16(v_randomSeed) & 0xFF) + 0xE0));
    wr16(PATTERN_ON, (uint16_t)(rd16(PATTERN_ON) + 4));
    SpawnTimer(0x032126, TIMER_RIGHT);
    SpawnTimer(0x032136, TIMER_MIDDLE);
    SpawnTimer(0x032146, TIMER_LEFT);
}

/* a pattern's frame: started, run out (the spawner's next step), or its sources */
static int pattern(uint16_t obj)
{
    if (!rd16(PATTERN_ON)) { pattern_start(); return 0; }
    wr16(PATTERN_TIME, (uint16_t)(rd16(PATTERN_TIME) - 1));
    if (rd16(PATTERN_TIME) & 0x8000) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        return 0;
    }
    SpawnTimersTick();
    return 1;
}

/* the patterns: the right's kinds, the left's kind */
static void three(uint16_t obj, uint32_t kinds, uint16_t left)
{
    if (!pattern(obj)) return;
    SpawnRight(kinds);
    SpawnLeft(left);
    spawn_middle(0);
}

/* $031FF8 */
void SpawnPatternA(uint16_t obj) { three(obj, 0x0323DA, 8); }
/* $03205A */
void SpawnPatternB(uint16_t obj) { three(obj, 0x0323EA, 0x68); }
/* $032028 */
void SpawnPatternC(uint16_t obj) { three(obj, 0x0323E2, 0x50); }
/* $0320D4 */
void SpawnPatternD(uint16_t obj) { three(obj, 0x0323EA, 0x50); }
/* $0320CA */
void SpawnPatternE(uint16_t obj) { three(obj, 0x03240A, 0x50); }
/* $0320B6 */
void SpawnPatternF(uint16_t obj) { three(obj, 0x0323FA, 0x50); }
/* $0320C0 */
void SpawnPatternG(uint16_t obj) { three(obj, 0x0323F2, 0x50); }

/* $03208C. Low on the right, from the left: no middle. */
void SpawnPatternH(uint16_t obj)
{
    if (!pattern(obj)) return;
    SpawnRightLow(0x0323DA);
    SpawnLeft(8);
}

/* $03222C */
void SpawnerSetupNone(void)
{
}

/* $032238 */
void SpawnerSetupC(void) { StageColoursFor(0xC); }

/* $032240 */
void SpawnerSetup8(void) { StageColoursFor(8); }

/* $032248. The base's: an object $404 at $DB80, ten $400 after it and two at $CB00, $CB60,
 * line 3's colours, the next tiles. */
void SpawnerSetupBase(void)
{
    wr16(0xDB80 + O_TYPE, 0x404);
    wr16(0xDB80 + KIND, 0);
    for (uint16_t k = 1; k <= 10; k++) wr16((uint16_t)(0xDB80 + 0x60 * k), 0x400);
    wr16(0xCB00, 0x400);
    wr16(0xCB60, 0x400);
    StageColoursFor(4);
    uint32_t list = 0x03222E;
    NextJob(&list);
}

/* $031F5E. Step 0: the flags cleared, $40 frames' wait, the kind's set-up. */
void SpawnerStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(SPAWN_FLAGS, 0);
    wr16(TIMER_RIGHT, 0x40);
    uint16_t i = rom16(0x032206 + (uint32_t)(int16_t)((int16_t)rd16(obj + KIND) >> 3));
    call68k(rom32(0x0321F6 + (uint32_t)(int16_t)i));
}

/* $031F72. The wait; then a pattern (one of two by the random). */
void SpawnerWait(uint16_t obj)
{
    wr16(TIMER_RIGHT, (uint16_t)(rd16(TIMER_RIGHT) - 1));
    if (!(rd16(TIMER_RIGHT) & 0x8000)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(PATTERN_ON, 0);
    wr16(PATTERN_ALT, rd16(v_randomSeed) & 4);
}

/* $031F92. The kind's pattern. */
void SpawnerPattern(uint16_t obj)
{
    uint16_t i = (uint16_t)(rd16(PATTERN_ALT) + ((int16_t)rd16(obj + KIND) >> 1));
    call_object(rom32(0x031FA4 + (uint32_t)(int16_t)i), obj);
}

/* $031FEC. Back to waiting $40 frames. */
void SpawnerAgain(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 8));
    wr16(TIMER_RIGHT, 0x40);
}

/* $0321A0. The camera held: it becomes type $1F8. */
void SpawnerBecome1F8(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x1F8);
    wr16(obj + O_STATE, 0);
}

/* $0321AC */
void SpawnerEnd(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x1000);
}

/* $0321B4. Ends, the boss ($2A8) comes. */
void SpawnerBoss(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x1000);
    wr16(v_boss + O_TYPE, 0x2A8);
    set8(SPAWN_FLAGS, 1);
}

/* $0321C8. An object $3C4 in the last slots, then type $1F8. */
void Spawner3C4(uint16_t obj)
{
    uint16_t slot = 0xDE80, t;
    if (SlotFree4(&slot, &t)) wr16(slot + O_TYPE, 0x3C4);
    SpawnerBecome1F8(obj);
}

/* $031F3A. Types $1A8, $1AC. */
void Spawner(uint16_t obj)
{
    if (bit8(v_cameraHold, 7)) {
        uint16_t i = rom16(0x03218E + (uint32_t)(int16_t)((int16_t)rd16(obj + KIND) >> 3));
        call_object(rom32(0x03217E + (uint32_t)(int16_t)i), obj);
        return;
    }
    call_object(rom32(0x031F4E + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

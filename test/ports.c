/* One adapter per ported routine: the recorded call's registers into the C function's arguments,
 * the call, and its register results checked. */
#include "game.h"
#include "ports.h"

static void run_CheckPause(const Record *r) { (void)r; CheckPause(); }
static void run_HintNone(const Record *r) { (void)r; HintNone(); }

const Port ports[] = {
    { SYM_CheckPause, "CheckPause", run_CheckPause },
    { SYM_HintNone, "HintNone", run_HintNone },
    { 0, NULL, NULL },
};

const uint32_t inner_labels[] = { 0 };

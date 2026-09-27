/* crash.c - leave the machine usable after any crash (PRD G7, FR-DBG-7..9).
 *
 * The HAL port's hooks run on #GP, #PF, #UD and divide error, on normal
 * exit (atexit) and on Ctrl-Break; each resets the drawing engine and
 * restores text mode, and a fault also logs the exception and, in builds
 * with the register write trace, the last register writes. */
#include "dgl.h"
#include "mga/mmio.h"
#include "mga/sys.h"

static int installed, graphics;

static void log_line(const char *l) { DGL_ERR("%s", l); }

void dgl_teardown(void)
{
    if (!graphics)
        return;
    graphics = 0;
    if (mga_mmio && engine_sync(100000) != 0)
        engine_reset();
    vbe_set_text_mode();
    dgl_ctx.active = 0;
}

static void on_fault(int exc, uint32_t err, uint32_t eip)
{
    dgl_teardown();
    DGL_ERR("DGL-FAULT exc=%d err=%lx eip=%08lx", exc, (unsigned long)err, (unsigned long)eip);
#if defined(DGL_DEBUG) || defined(MGA_TRACE_MMIO)
    mga_trace_dump(log_line, 32);
#else
    (void)log_line;
#endif
}

static void on_exit_hook(void)
{
    dgl_teardown();
}

void dgl_crash_install(void)
{
    graphics = 1;                 /* dglInit is about to leave text mode */
    if (installed)
        return;
    if (sys_hook_faults(on_fault) < 0 || sys_hook_exit(on_exit_hook) < 0)
        DGL_WARN("DGL-WARN crash hooks unavailable in this port");
    installed = 1;
}

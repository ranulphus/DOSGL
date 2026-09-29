/* context.c - dglInit, dglShutdown, dglSwapBuffers (PRD §5.5, FR-HAL-3/4).
 *
 * VRAM layout: colour buffer A at 0, colour buffer B (double buffering),
 * the 16-bit depth buffer, then the texture heap; each 4 KB aligned. The
 * engine always draws into the hidden buffer; a swap shows it through VBE
 * 4F07h, after the vertical retrace when vsync is on. */
#include "dgl.h"
#include "../gl/gl_state.h"
#include "../gl/gl_tex.h"
#include "mga/mmio.h"
#include "mga/setup.h"
#include "mga/sys.h"
#include <stdlib.h>
#include <string.h>
#ifdef __DJGPP__
#include <sys/nearptr.h>
#endif

dgl_context dgl_ctx;
static DGLStats stats;
/* Test hooks (PRD §5.5, M5): DGL_EXIT_AFTER=n ends the program after n
 * swaps; DGL_STATS=1 logs frame and triangle rates once a second. */
static unsigned long exit_after;
static int exit_pending;           /* DGL-START printed, DGL-EXIT owed */
static int stats_on;                /* DGL_STATS: 1 = DGL-STAT, 2 = also DGL-PRIMS */
static uint32_t stats_t0, stats_tris0;
static unsigned long stats_swaps0;

#define ALIGN4K(x) (((x) + 4095u) & ~4095u)

/* The first GL errors, with the address inside the GL function that raised
 * them (look it up in the program's link map). */
static void log_gl_error(GLenum e, void *at)
{
    static int n;
    if (n < 16 && ++n)
        DGL_WARN("DGL-GLERR 0x%04x at %p%s", (unsigned)e, at, n == 16 ? " (no more logged)" : "");
}

uint32_t dgl_color_off(int front)
{
    if (!dgl_ctx.double_buffer)
        return dgl_ctx.front_off;
    return dgl_ctx.front_is_a == !!front ? dgl_ctx.front_off : dgl_ctx.back_off;
}

static void set_target(void)
{
    mga_target t;
    memset(&t, 0, sizeof t);
    t.color_off = dgl_color_off(dgl_ctx.draw_front);
    t.z_off = dgl_ctx.z_off;
    t.pitch_px = dgl_ctx.pitch_px;
    t.bpp = 16;
    t.zbits = dgl_ctx.z_off ? 16 : 0;
    engine_set_target(&t);
    engine_set_clip(0, 0, dgl_ctx.width, dgl_ctx.height);
}

int dglInit(const DGLConfig *cfg)
{
    static const DGLConfig zero;
    DGLConfig c;
    const mga_vbe_mode *m;
    int pitch = 0;
    uint32_t fb;
    if (dgl_ctx.active) {
        dgl_set_error("dglInit called twice");
        return -1;
    }
    if (!cfg || !memcmp(cfg, &zero, sizeof zero)) {
        memset(&c, 0, sizeof c);        /* PRD §8.1 defaults */
        c.width = 640; c.height = 480; c.color_bits = 16; c.depth_bits = 16;
        c.double_buffer = 1; c.vsync = 1;
    } else {
        c = *cfg;
        if (!c.width || !c.height) { c.width = 640; c.height = 480; }
    }
    if (c.color_bits && c.color_bits != 16) {
        dgl_set_error("only 16-bit colour is supported");
        return -1;
    }
    if (dgl_discover() != 0)
        return -1;
    m = dgl_vbe_mode_for(c.width, c.height);
    if (!m) {
        dgl_set_error("no usable %dx%dx16 mode on this card", c.width, c.height);
        return -1;
    }
    memset(&dgl_ctx, 0, sizeof dgl_ctx);
    dgl_ctx.width = c.width;
    dgl_ctx.height = c.height;
    dgl_ctx.pitch_px = dgl_pitch_for(c.width);
    dgl_ctx.double_buffer = c.double_buffer != 0;
    dgl_ctx.depth_bits = c.depth_bits ? 16 : 0;
    dgl_ctx.vsync = c.vsync != 0;
    {
        const char *e = getenv("DGL_VSYNC");     /* a test or user override */
        if (e && *e)
            dgl_ctx.vsync = *e != '0';
    }
    dgl_ctx.vram_bytes = mga.vram_bytes;
    fb = (uint32_t)dgl_ctx.pitch_px * (uint32_t)c.height * 2u;
    dgl_ctx.front_off = 0;
    dgl_ctx.back_off = dgl_ctx.double_buffer ? ALIGN4K(fb) : 0;
    dgl_ctx.z_off = dgl_ctx.depth_bits ? ALIGN4K((dgl_ctx.double_buffer ? dgl_ctx.back_off : 0) + fb) : 0;
    dgl_ctx.heap_off = ALIGN4K((dgl_ctx.z_off ? dgl_ctx.z_off : dgl_ctx.back_off) + fb);
    if (dgl_ctx.heap_off > dgl_ctx.vram_bytes) {
        dgl_set_error("%dx%d with these buffers needs %lu bytes of VRAM; the card has %lu", c.width, c.height,
                      (unsigned long)dgl_ctx.heap_off, (unsigned long)dgl_ctx.vram_bytes);
        return -1;
    }
    mga.fb_size = mga.vram_bytes;
    if (mga_map(&mga) != 0) {
        dgl_set_error("cannot map the card's apertures (DPMI 0800h)");
        return -1;
    }
    dgl_crash_install();
    if (vbe_set_mode(m, dgl_ctx.pitch_px, &pitch) != 0 || pitch != dgl_ctx.pitch_px) {
        dgl_teardown();
        dgl_set_error("VBE mode %03x with pitch %d failed (BIOS gave %d)", m->mode, dgl_ctx.pitch_px, pitch);
        return -1;
    }
    engine_init(dgl_ctx.pitch_px, 16);
    {
        /* 16-bit pixels index the DAC palette on these chips, and the BIOS
         * leaves it non-linear: load an identity ramp. */
        uint8_t ramp[256];
        int i;
        for (i = 0; i < 256; i++)
            ramp[i] = (uint8_t)i;
        dac_set_ramp(ramp);
    }
    dgl_ctx.front_is_a = 1;
    dgl_ctx.active = 1;
    /* Clear everything the context owns, then draw into the hidden buffer. */
    memset(&stats, 0, sizeof stats);
    dgl_ctx.front_is_a = 0; set_target(); engine_fill(0, 0, c.width, c.height, 0);
    dgl_ctx.front_is_a = 1; set_target(); engine_fill(0, 0, c.width, c.height, 0);
    if (dgl_ctx.z_off)
        engine_fill_depth(0, 0, c.width, c.height, 0xFFFF);
    engine_sync(500000);
    vbe_set_display_start(dgl_ctx.front_off, dgl_ctx.pitch_px * 2, 16);
    {
        const char *e = getenv("DGL_EXIT_AFTER");
        exit_after = e ? strtoul(e, NULL, 10) : 0;
        if (exit_after) {               /* the harness pairs each DGL-START with a DGL-EXIT */
            exit_pending = 1;
            DGL_ERR("DGL-START %dx%d exit_after=%lu", c.width, c.height, exit_after);
        }
        dgl_snap_init();
        e = getenv("DGL_STATS");
        stats_on = e ? atoi(e) : 0;
        stats_t0 = sys_time_us();
        stats_tris0 = setup_stats.tris;
    }
    dgl_gl_reset();
    dgl_gl_error_hook = log_gl_error;
    dgl_gl_set_window(c.width, c.height);
    {
        /* DGL_TEXHEAP_KB caps the texture heap (tests of eviction, or a
         * smaller card's memory on a bigger one). */
        const char *e = getenv("DGL_TEXHEAP_KB");
        uint32_t end = dgl_ctx.vram_bytes, kb = e ? (uint32_t)strtoul(e, NULL, 10) : 0;
        if (kb && dgl_ctx.heap_off + kb * 1024u < end)
            end = dgl_ctx.heap_off + kb * 1024u;
        dgl_textures_reset(dgl_ctx.heap_off, end);
    }
    dgl_emit_install();
    DGL_INFO("DGL-INIT %dx%d pitch=%d double=%d depth=%d heap=%lu..%lu", c.width, c.height, dgl_ctx.pitch_px,
             dgl_ctx.double_buffer, dgl_ctx.depth_bits, (unsigned long)dgl_ctx.heap_off,
             (unsigned long)dgl_ctx.vram_bytes);
    return 0;
}

void dglShutdown(void)
{
    dgl_teardown();
}

/* Every way out (dglShutdown, DGL_EXIT_AFTER, exit, a fault) ends here once. */
void dgl_note_exit(void)
{
    if (!exit_pending)
        return;
    exit_pending = 0;
    DGL_ERR("DGL-EXIT frames=%lu", stats.swaps);
}

void dglSetVSync(int enabled)
{
    dgl_ctx.vsync = enabled != 0;
}

void dglSwapBuffers(void)
{
    uint32_t show;
    if (!dgl_ctx.active)
        return;
#ifdef __DJGPP__
    __djgpp_nearptr_enable();           /* the consumer may have turned near pointers off */
#endif
    dgl_sync();                         /* retired texture blocks go back to the heap */
    stats.swaps++;
    dgl_snap_frame(stats.swaps);        /* DGL_SNAP: before the frame is shown */
    stats.frames++;
    stats.triangles = setup_stats.tris;
    stats.stub_calls = dgl_stub_calls;
    if (stats_on) {
        uint32_t now = sys_time_us(), dt = now - stats_t0;
        if (dt >= 1000000u) {
            DGL_ERR("DGL-STAT fps=%.1f tris/s=%.0f swaps=%lu tex_kb=%lu stubs=%lu",
                    (stats.swaps - stats_swaps0) * 1e6 / dt, (setup_stats.tris - stats_tris0) * 1e6 / dt, stats.swaps,
                    (unsigned long)(dgl_vram_used() >> 10), dgl_stub_calls);
            if (stats_on >= 2) {
                DGL_ERR("DGL-PRIMS begins=%lu skipped=%lu tris=%lu clipped=%lu zero=%lu culled=%lu",
                        dgl_prims.begins, dgl_prims.skipped, dgl_prims.tris_in, dgl_prims.clipped,
                        dgl_prims.zero_area, dgl_prims.culled);
                memset(&dgl_prims, 0, sizeof dgl_prims);
                DGL_ERR("DGL-TEX uploads=%lu kb=%lu sub_fast=%lu sub_full=%lu renames=%lu evictions=%lu syncs=%lu",
                        dgl_texc.uploads, dgl_texc.upload_bytes >> 10, dgl_texc.sub_fast, dgl_texc.sub_full,
                        dgl_texc.renames, dgl_texc.evictions, dgl_texc.syncs);
                memset(&dgl_texc, 0, sizeof dgl_texc);
            }
            stats_t0 = now;
            stats_tris0 = setup_stats.tris;
            stats_swaps0 = stats.swaps;
        }
    }
    if (exit_after && stats.swaps >= exit_after) {
        dglShutdown();
        exit(0);
    }
    if (!dgl_ctx.double_buffer)
        return;
    show = dgl_ctx.front_is_a ? dgl_ctx.back_off : dgl_ctx.front_off;
    if (dgl_ctx.vsync)
        engine_vsync_wait(50000);
    vbe_set_display_start(show, dgl_ctx.pitch_px * 2, 16);
    dgl_ctx.front_is_a = !dgl_ctx.front_is_a;
    set_target();
    dgl_gl.dirty |= DGL_DIRTY_TARGET | DGL_DIRTY_RASTER;    /* the target write reset MACCESS */
}

void dgl_retarget(void)
{
    if (dgl_ctx.active)
        set_target();                   /* through the FIFO, so queued draws keep their buffer */
}

const DGLStats *dglGetStats(void)
{
    stats.stub_calls = dgl_stub_calls;
    return &stats;
}

/* dgl.h - DOS-GL internals shared by src/dgl and src/gl. */
#ifndef DGL_INTERNAL_H
#define DGL_INTERNAL_H
#include <GL/dosgl.h>
#include "mga/hal.h"

/* Logging over COM1 through the HAL's unbuffered serial driver (FR-DBG-1/2).
 * Lines start "DGL-". Levels above DGL_LOG_LEVEL compile out (FR-DBG-3):
 * 0 errors (always), 1 warnings, 2 info, 3 debug. */
#ifndef DGL_LOG_LEVEL
#  ifdef DGL_DEBUG
#    define DGL_LOG_LEVEL 3
#  else
#    define DGL_LOG_LEVEL 1
#  endif
#endif
void dgl_logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern unsigned long dgl_stub_calls;      /* log.c */
#define DGL_ERR(...)  dgl_logf(__VA_ARGS__)
#define DGL_WARN(...) do { if (DGL_LOG_LEVEL >= 1) dgl_logf(__VA_ARGS__); } while (0)
#define DGL_INFO(...) do { if (DGL_LOG_LEVEL >= 2) dgl_logf(__VA_ARGS__); } while (0)
#define DGL_DBG(...)  do { if (DGL_LOG_LEVEL >= 3) dgl_logf(__VA_ARGS__); } while (0)

/* Errors reported by dglGetErrorString. */
void dgl_set_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* device.c: discovery (once) and the filtered mode list. */
int dgl_discover(void);                   /* 0 = a supported card was found */
void dgl_note_vram(uint32_t bytes);       /* the probed VRAM size, when larger than VBE's */
const mga_vbe_mode *dgl_vbe_mode_for(int width, int height);
int dgl_pitch_for(int width);             /* pixels, engine rules */

/* crash.c: fault/exit teardown (FR-DBG-7..9). */
void dgl_crash_install(void);
void dgl_teardown(void);                  /* reset engine, text mode; safe to call twice */
void dgl_note_exit(void);                 /* DGL-EXIT if DGL-START was printed */
void dgl_snap_init(void);                 /* snap.c: read DGL_SNAP, DGL_SNAPDIR */
void dgl_snap_frame(unsigned long swap);  /* capture if swap is listed */

/* emit.c: install the hardware sinks for the vertex stream. */
void dgl_emit_install(void);

/* Context state (context.c). */
typedef struct {
    int      active;
    int      width, height, pitch_px;
    int      double_buffer, depth_bits, vsync;
    uint32_t front_off, back_off, z_off, heap_off, vram_bytes;
    int      front_is_a;                  /* which of the two colour buffers is shown */
    int      draw_front, read_front;      /* glDrawBuffer/glReadBuffer chose GL_FRONT */
} dgl_context;
extern dgl_context dgl_ctx;

uint32_t dgl_color_off(int front);        /* VRAM offset of the shown (1) or hidden (0) buffer */
void dgl_retarget(void);                  /* point the engine at dgl_ctx.draw_front's buffer */

#endif

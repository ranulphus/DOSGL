/* dosgl.h - DOS-GL's context and display API (PRD §5.5, D7).
 *
 * Draft until M1, where it is frozen. Everything here is callable from C
 * (DJGPP) and warning-clean under -Wall -Wextra -Werror, as ClassiCube's
 * build requires. */
#ifndef DOSGL_H
#define DOSGL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width, height;      /* any enumerated mode; 0 = 640x480 */
    int color_bits;         /* 16 only in v1.0 (0 = 16) */
    int depth_bits;         /* 0 or 16 */
    int refresh_hz;         /* 0 = BIOS default; VBE 3.0 only */
    int double_buffer;      /* nonzero for double buffering */
    int vsync;              /* nonzero to sync swaps to retrace */
} DGLConfig;

typedef struct {
    int width, height;
    int color_bits;
    int max_depth_bits;     /* 16, or 0 if VRAM cannot fit a Z buffer */
    int can_double_buffer;
    int vbe_mode;           /* the BIOS mode number */
    int pitch_px;           /* the pitch DOS-GL will program */
} DGLMode;

typedef struct {
    const char   *chip_name;        /* e.g. "MGA-G450" */
    unsigned int  device_id, revision;
    unsigned long vram_bytes;
    unsigned long fb_phys, mmio_phys, iload_phys;
    unsigned int  fifo_depth;
    int           width, height;    /* current mode after dglInit */
    int           emulated;         /* nonzero under 86Box */
} DGLDeviceInfo;

typedef struct {
    unsigned long frames, triangles, swaps, texture_bytes, fifo_stalls;
    unsigned long stub_calls;       /* calls to GL functions DOS-GL only stubs (v1.1) */
} DGLStats;

/* The library's version string, e.g. "DOS-GL 0.1 (<build>)". */
const char *dglVersion(void);

/* Enumeration: callable before dglInit, after device discovery. Returns the
   number of usable modes and fills up to max_modes entries. */
int  dglEnumModes(DGLMode *modes, int max_modes);

int  dglInit(const DGLConfig *cfg);     /* 0 on success */
void dglShutdown(void);
void dglSwapBuffers(void);
void dglSetVSync(int enabled);
const char *dglGetErrorString(void);
const DGLDeviceInfo *dglGetDeviceInfo(void);
const DGLStats *dglGetStats(void);

/* The address of a GL function by name: every GL 1.1 function and the
   extension functions DOS-GL implements; NULL for anything else. Whether an
   extension may be used is still decided by glGetString(GL_EXTENSIONS). (v1.1) */
void *dglGetProcAddress(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* DOSGL_H */

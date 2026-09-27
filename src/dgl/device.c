/* device.c - card discovery and the mode list (PRD FR-HAL-1/3/8, §5.5).
 *
 * Discovery reads PCI configuration space and the VBE information block
 * only: nothing is mapped and VRAM is not touched, so dglEnumModes can run
 * in text mode before dglInit. */
#include "dgl.h"
#include <stdlib.h>
#include <string.h>

#define MAX_MODES 32
#define TEXTURE_RESERVE (512u * 1024u)   /* VRAM kept for textures when judging fit */

static int discovered;                   /* 0 not yet, 1 found, -1 none */
static DGLDeviceInfo info;
static mga_vbe_mode vbe_modes[MAX_MODES];
static DGLMode modes[MAX_MODES];
static int nmodes;

int dgl_pitch_for(int width)
{
    /* The drawing engine wants a pitch that is a multiple of 32 pixels, at
     * most 4096 (FR-HAL-8). DOS-GL programs it with VBE 4F06h rather than
     * trusting the BIOS's choice. */
    int p = (width + 31) & ~31;
    return p <= 4096 ? p : 0;
}

static void judge(DGLMode *d, uint32_t vram)
{
    uint32_t fb = (uint32_t)d->pitch_px * (uint32_t)d->height * 2u;
    d->can_double_buffer = 2u * fb + TEXTURE_RESERVE <= vram;
    d->max_depth_bits = ((d->can_double_buffer ? 3u : 2u) * fb + TEXTURE_RESERVE <= vram) ? 16 : 0;
}

static void mode_cb(const mga_vbe_mode *m, void *ctx)
{
    DGLMode d;
    int i;
    (void)ctx;
    /* Linear, direct colour (vbe.c already required both), 16 bpp RGB565. */
    if (m->bpp != 16 || m->red_size != 5 || m->green_size != 6 || m->blue_size != 5 || !m->lfb_phys)
        return;
    if (m->lfb_phys != mga.fb_phys)
        return;                          /* not this card's aperture */
    memset(&d, 0, sizeof d);
    d.width = m->width;
    d.height = m->height;
    d.color_bits = 16;
    d.vbe_mode = m->mode;
    d.pitch_px = dgl_pitch_for(m->width);
    if (!d.pitch_px)
        return;
    judge(&d, info.vram_bytes);
    if ((uint32_t)d.pitch_px * d.height * 2u > info.vram_bytes)
        return;
    for (i = 0; i < nmodes; i++)
        if (modes[i].width == d.width && modes[i].height == d.height)
            return;
    if (nmodes == MAX_MODES)
        return;
    vbe_modes[nmodes] = *m;
    modes[nmodes++] = d;
}

int dgl_discover(void)
{
    int i, j;
    if (discovered)
        return discovered > 0 ? 0 : -1;
    discovered = -1;
    if (mga_find(&mga) != 0) {
        dgl_set_error("no supported Matrox card found (G400, G450 or G200)");
        return -1;
    }
    memset(&info, 0, sizeof info);
    info.chip_name = mga.name;
    info.device_id = mga.device_id;
    info.revision = mga.revision;
    info.fb_phys = mga.fb_phys;
    info.mmio_phys = mga.mmio_phys;
    info.iload_phys = mga.iload_phys;
    info.fifo_depth = mga.fifo_depth;
    info.emulated = mga_detect_emulator();
    info.vram_bytes = vbe_total_memory();
    if (!info.vram_bytes) {
        dgl_set_error("the card's VBE BIOS did not answer (VBE 2.0 or later needed)");
        return -1;
    }
    mga.vram_bytes = info.vram_bytes;
    nmodes = 0;
    vbe_enumerate(mode_cb, NULL);
    /* Smallest first. */
    for (i = 1; i < nmodes; i++)
        for (j = i; j > 0 && modes[j].width * modes[j].height < modes[j - 1].width * modes[j - 1].height; j--) {
            DGLMode t = modes[j]; mga_vbe_mode v = vbe_modes[j];
            modes[j] = modes[j - 1]; vbe_modes[j] = vbe_modes[j - 1];
            modes[j - 1] = t; vbe_modes[j - 1] = v;
        }
    if (mga.family == MGA_FAMILY_G100)
        DGL_WARN("DGL-WARN %s is a development-only card: no GL blending (PRD D13)", mga.name);
    DGL_INFO("DGL-DEVICE %s id=%04x rev=%02x vram=%lu modes=%d", mga.name, mga.device_id, mga.revision,
             info.vram_bytes, nmodes);
    discovered = 1;
    return 0;
}

const mga_vbe_mode *dgl_vbe_mode_for(int width, int height)
{
    int i;
    for (i = 0; i < nmodes; i++)
        if (modes[i].width == width && modes[i].height == height)
            return &vbe_modes[i];
    return NULL;
}

int dglEnumModes(DGLMode *out, int max_modes)
{
    int i;
    if (dgl_discover() != 0)
        return 0;
    for (i = 0; i < nmodes && i < max_modes && out; i++)
        out[i] = modes[i];
    return nmodes;
}

const DGLDeviceInfo *dglGetDeviceInfo(void)
{
    if (dgl_discover() != 0)
        return NULL;
    info.width = dgl_ctx.active ? dgl_ctx.width : 0;
    info.height = dgl_ctx.active ? dgl_ctx.height : 0;
    return &info;
}

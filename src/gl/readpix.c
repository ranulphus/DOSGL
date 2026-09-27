/* readpix.c - glReadPixels (PRD §8.4): the buffer being drawn (GL's back
 * buffer when double-buffered), RGB565 expanded to 8 bits per channel by
 * bit replication, alpha 255, rows bottom-up as GL numbers them. */
#include "gl_state.h"
#include "../dgl/dgl.h"
#include <string.h>

void APIENTRY glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid *pixels)
{
    const volatile uint16_t *fb;
    unsigned char *row = (unsigned char *)pixels;
    int bpp, stride, i, j;
    if (w < 0 || h < 0) { dgl_gl_error(GL_INVALID_VALUE); return; }
    if (type != GL_UNSIGNED_BYTE) { dgl_gl_error(GL_INVALID_ENUM); return; }
    switch (format) {
    case GL_RGBA: case GL_BGRA_EXT: bpp = 4; break;
    case GL_RGB: case GL_BGR_EXT: bpp = 3; break;
    default: dgl_gl_error(GL_INVALID_ENUM); return;
    }
    if (!dgl_ctx.active)
        return;
    stride = w * bpp;
    stride = (stride + dgl_gl.pack_align - 1) / dgl_gl.pack_align * dgl_gl.pack_align;
    engine_sync(500000);                    /* the LFB is not ordered with queued draws */
    fb = (const volatile uint16_t *)(mga_fb + (dgl_ctx.double_buffer
                                                   ? (dgl_ctx.front_is_a ? dgl_ctx.back_off : dgl_ctx.front_off)
                                                   : dgl_ctx.front_off));
    for (j = 0; j < h; j++, row += stride) {
        int sy = dgl_ctx.height - 1 - (y + j);     /* GL row -> screen row */
        unsigned char *p = row;
        for (i = 0; i < w; i++, p += bpp) {
            int sx = x + i;
            unsigned r = 0, g = 0, b = 0;
            if (sx >= 0 && sx < dgl_ctx.width && sy >= 0 && sy < dgl_ctx.height) {
                uint16_t c = fb[sy * dgl_ctx.pitch_px + sx];
                r = (c >> 11) & 31; g = (c >> 5) & 63; b = c & 31;
                r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); b = (b << 3) | (b >> 2);
            }
            if (format == GL_RGBA || format == GL_RGB) { p[0] = (unsigned char)r; p[2] = (unsigned char)b; }
            else { p[0] = (unsigned char)b; p[2] = (unsigned char)r; }
            p[1] = (unsigned char)g;
            if (bpp == 4)
                p[3] = 255;
        }
    }
}

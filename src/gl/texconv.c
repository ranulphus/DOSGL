/* texconv.c - texel conversion (PRD §8.3). The consumer uploads 32-bit
 * RGBA or BGRA; each texture is stored in the hardware format its alpha
 * needs: all opaque -> RGB565 (TW16), only 0/255 -> ARGB1555 (TW15),
 * anything else -> ARGB4444 (TW12). Pure C, unit-tested on the host. */
#include "gl_tex.h"

int dgl_alpha_class(const unsigned char *rgba, long n)
{
    int cls = DGL_ALPHA_OPAQUE;
    long i;
    for (i = 0; i < n; i++) {
        unsigned a = rgba[i * 4 + 3];
        if (a == 255)
            continue;
        if (a != 0)
            return DGL_ALPHA_GRADIENT;
        cls = DGL_ALPHA_BINARY;
    }
    return cls;
}

int dgl_hwfmt_for_class(int cls)
{
    return cls == DGL_ALPHA_OPAQUE ? DGL_TW16 : cls == DGL_ALPHA_BINARY ? DGL_TW15 : DGL_TW12;
}

/* Rounded to the nearest representable value, as a GL implementation should. */
static unsigned q(unsigned v, int bits) { return (v * ((1u << bits) - 1) + 127) / 255; }

uint16_t dgl_pack_texel(int hwfmt, unsigned r, unsigned g, unsigned b, unsigned a)
{
    switch (hwfmt) {
    case DGL_TW16: return (uint16_t)((q(r, 5) << 11) | (q(g, 6) << 5) | q(b, 5));
    case DGL_TW15: return (uint16_t)(((a >= 128) << 15) | (q(r, 5) << 10) | (q(g, 5) << 5) | q(b, 5));
    default:       return (uint16_t)((q(a, 4) << 12) | (q(r, 4) << 8) | (q(g, 4) << 4) | q(b, 4));
    }
}

void dgl_convert_rgba(int hwfmt, const unsigned char *rgba, long n, uint16_t *out)
{
    long i;
    for (i = 0; i < n; i++, rgba += 4)
        out[i] = dgl_pack_texel(hwfmt, rgba[0], rgba[1], rgba[2], rgba[3]);
}

/* Any client format DOS-GL accepts -> RGBA8 (the shadow copy's format). */
int dgl_to_rgba(GLenum format, const unsigned char *src, long n, unsigned char *dst)
{
    long i;
    for (i = 0; i < n; i++, dst += 4) {
        switch (format) {
        case GL_RGBA: dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3]; src += 4; break;
        case GL_BGRA_EXT: dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = src[3]; src += 4; break;
        case GL_RGB: dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 255; src += 3; break;
        case GL_BGR_EXT: dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = 255; src += 3; break;
        case GL_LUMINANCE: dst[0] = dst[1] = dst[2] = src[0]; dst[3] = 255; src += 1; break;
        case GL_LUMINANCE_ALPHA: dst[0] = dst[1] = dst[2] = src[0]; dst[3] = src[1]; src += 2; break;
        case GL_ALPHA: dst[0] = dst[1] = dst[2] = 255; dst[3] = src[0]; src += 1; break;
        default: return -1;
        }
    }
    return 0;
}

/* GL 1.1 internal formats (§3.8.1): the base format decides which
 * components a texture keeps; the sized variants are hints DOS-GL maps to
 * its three 16-bit formats by alpha (dgl_hwfmt_for_class), except that
 * RGB5_A1 never takes ARGB4444. */
int dgl_ifmt_class(GLint f)
{
    switch (f) {
    case 4: case GL_RGBA: case GL_RGBA2: case GL_RGBA4: case GL_RGBA8: case GL_RGB10_A2: case GL_RGBA12:
    case GL_RGBA16:
        return DGL_IF_RGBA;
    case 3: case GL_RGB: case GL_R3_G3_B2: case GL_RGB4: case GL_RGB5: case GL_RGB8: case GL_RGB10: case GL_RGB12:
    case GL_RGB16: case GL_RGB2_EXT:
        return DGL_IF_RGB;
    case GL_RGB5_A1:
        return DGL_IF_RGB5_A1;
    case 1: case GL_LUMINANCE: case GL_LUMINANCE4: case GL_LUMINANCE8: case GL_LUMINANCE12: case GL_LUMINANCE16:
        return DGL_IF_LUMINANCE;
    case 2: case GL_LUMINANCE_ALPHA: case GL_LUMINANCE4_ALPHA4: case GL_LUMINANCE6_ALPHA2:
    case GL_LUMINANCE8_ALPHA8: case GL_LUMINANCE12_ALPHA4: case GL_LUMINANCE12_ALPHA12: case GL_LUMINANCE16_ALPHA16:
        return DGL_IF_LUMINANCE_ALPHA;
    case GL_ALPHA: case GL_ALPHA4: case GL_ALPHA8: case GL_ALPHA12: case GL_ALPHA16:
        return DGL_IF_ALPHA;
    case GL_INTENSITY: case GL_INTENSITY4: case GL_INTENSITY8: case GL_INTENSITY12: case GL_INTENSITY16:
        return DGL_IF_INTENSITY;
    default:
        return -1;
    }
}

/* The texel as the texture environment will see it: luminance and
 * intensity come from red (GL's conversion from RGBA), missing colour is
 * white and missing alpha is 1. */
void dgl_apply_ifmt(int ifc, unsigned char *p, long n)
{
    long i;
    for (i = 0; i < n; i++, p += 4)
        switch (ifc) {
        case DGL_IF_RGB: p[3] = 255; break;
        case DGL_IF_LUMINANCE: p[1] = p[2] = p[0]; p[3] = 255; break;
        case DGL_IF_LUMINANCE_ALPHA: p[1] = p[2] = p[0]; break;
        case DGL_IF_ALPHA: p[0] = p[1] = p[2] = 255; break;
        case DGL_IF_INTENSITY: p[1] = p[2] = p[3] = p[0]; break;
        default: break;
        }
}

/* Grey in RGB565 with green derived from the quantised red, so a ramp of
 * greys (a luminance lightmap) never alternates between green and magenta
 * tints. */
uint16_t dgl_pack_grey565(unsigned l)
{
    unsigned r = q(l, 5);
    return (uint16_t)((r << 11) | (((r << 1) | (r >> 4)) << 5) | r);
}

int dgl_format_bytes(GLenum format)
{
    switch (format) {
    case GL_RGBA: case GL_BGRA_EXT: return 4;
    case GL_RGB: case GL_BGR_EXT: return 3;
    case GL_LUMINANCE_ALPHA: return 2;
    case GL_LUMINANCE: case GL_ALPHA: return 1;
    default: return 0;
    }
}

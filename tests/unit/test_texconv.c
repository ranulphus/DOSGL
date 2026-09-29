/* test_texconv.c - texel formats, alpha classes and client format expansion. */
#include "unit.h"
#include "../../src/gl/gl_tex.h"
#include <string.h>

void unit_run(void)
{
    unsigned char opaque[8] = { 255, 0, 0, 255, 0, 255, 0, 255 };
    int i;
    unsigned char binary[8] = { 255, 0, 0, 0, 0, 255, 0, 255 };
    unsigned char grad[8] = { 255, 0, 0, 128, 0, 255, 0, 255 };
    unsigned char bgra[4] = { 10, 20, 30, 40 }, out[8];
    uint16_t t[2];
    CHECK(dgl_alpha_class(opaque, 2) == DGL_ALPHA_OPAQUE);
    CHECK(dgl_alpha_class(binary, 2) == DGL_ALPHA_BINARY);
    CHECK(dgl_alpha_class(grad, 2) == DGL_ALPHA_GRADIENT);
    CHECK(dgl_hwfmt_for_class(DGL_ALPHA_OPAQUE) == DGL_TW16);
    CHECK(dgl_hwfmt_for_class(DGL_ALPHA_BINARY) == DGL_TW15);
    CHECK(dgl_hwfmt_for_class(DGL_ALPHA_GRADIENT) == DGL_TW12);
    /* Exact extremes and rounding. */
    CHECK(dgl_pack_texel(DGL_TW16, 255, 255, 255, 255) == 0xFFFF);
    CHECK(dgl_pack_texel(DGL_TW16, 255, 0, 0, 255) == 0xF800);
    CHECK(dgl_pack_texel(DGL_TW16, 0, 255, 0, 255) == 0x07E0);
    CHECK(dgl_pack_texel(DGL_TW16, 132, 0, 0, 255) == (16u << 11));   /* 132*31/255 = 16.05 */
    CHECK(dgl_pack_texel(DGL_TW15, 255, 255, 255, 0) == 0x7FFF);
    CHECK(dgl_pack_texel(DGL_TW15, 0, 0, 0, 255) == 0x8000);
    CHECK(dgl_pack_texel(DGL_TW12, 255, 0, 0, 128) == 0x8F00);
    dgl_convert_rgba(DGL_TW15, binary, 2, t);
    CHECK(t[0] == 0x7C00 && t[1] == 0x83E0);
    /* BGRA and RGB expand to RGBA. */
    CHECK(dgl_to_rgba(GL_BGRA_EXT, bgra, 1, out) == 0);
    CHECK(out[0] == 30 && out[1] == 20 && out[2] == 10 && out[3] == 40);
    CHECK(dgl_to_rgba(GL_RGB, bgra, 1, out) == 0 && out[3] == 255 && out[0] == 10);
    CHECK(dgl_to_rgba(GL_LUMINANCE, bgra, 1, out) == 0 && out[1] == 10);
    CHECK(dgl_to_rgba(0x1234, bgra, 1, out) == -1);
    CHECK(dgl_format_bytes(GL_BGRA_EXT) == 4 && dgl_format_bytes(GL_RGB) == 3);
    /* Internal formats: the class, then what the texel keeps. */
    CHECK(dgl_ifmt_class(3) == DGL_IF_RGB && dgl_ifmt_class(GL_RGB5) == DGL_IF_RGB);
    CHECK(dgl_ifmt_class(4) == DGL_IF_RGBA && dgl_ifmt_class(GL_RGBA4) == DGL_IF_RGBA);
    CHECK(dgl_ifmt_class(GL_RGB5_A1) == DGL_IF_RGB5_A1 && dgl_ifmt_class(1) == DGL_IF_LUMINANCE);
    CHECK(dgl_ifmt_class(GL_INTENSITY8) == DGL_IF_INTENSITY && dgl_ifmt_class(GL_ALPHA8) == DGL_IF_ALPHA);
    CHECK(dgl_ifmt_class(GL_LUMINANCE8_ALPHA8) == DGL_IF_LUMINANCE_ALPHA && dgl_ifmt_class(5) == -1);
    {
        unsigned char px[4];
        memcpy(px, "\x50\x60\x70\x10", 4); dgl_apply_ifmt(DGL_IF_RGB, px, 1);
        CHECK(px[0] == 0x50 && px[2] == 0x70 && px[3] == 255);
        memcpy(px, "\x50\x60\x70\x10", 4); dgl_apply_ifmt(DGL_IF_LUMINANCE, px, 1);
        CHECK(px[1] == 0x50 && px[2] == 0x50 && px[3] == 255);
        memcpy(px, "\x50\x60\x70\x10", 4); dgl_apply_ifmt(DGL_IF_INTENSITY, px, 1);
        CHECK(px[1] == 0x50 && px[3] == 0x50);
        memcpy(px, "\x50\x60\x70\x10", 4); dgl_apply_ifmt(DGL_IF_ALPHA, px, 1);
        CHECK(px[0] == 255 && px[2] == 255 && px[3] == 0x10);
    }
    /* Greys in RGB565 keep green = red widened, for every level. */
    for (i = 0; i < 256; i++) {
        uint16_t g = dgl_pack_grey565((unsigned)i);
        unsigned r5 = g >> 11, g6 = (g >> 5) & 63, b5 = g & 31;
        if (r5 != b5 || g6 != ((r5 << 1) | (r5 >> 4)))
            break;
    }
    CHECK(i == 256);
}

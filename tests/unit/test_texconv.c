/* test_texconv.c - texel formats, alpha classes and client format expansion. */
#include "unit.h"
#include "../../src/gl/gl_tex.h"

void unit_run(void)
{
    unsigned char opaque[8] = { 255, 0, 0, 255, 0, 255, 0, 255 };
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
}

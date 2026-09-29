/* combine.c - GL texture environments as the G400's combiner words
 * (TDUALSTAGE0/1). The values are Mesa's G400 driver's, which ran on real
 * G400s, collected with their sources in MGA-Glide's
 * docs/g400-dual-texture.md; unit 1's GL_BLEND words are derived there
 * (Mesa used the diffuse colour where GL means the previous unit). Stage 0
 * works on texture 0 and the iterated colour, stage 1 on texture 1 and
 * stage 0's result. */
#include "gl_tex.h"

enum { BASE_RGB, BASE_RGBA, BASE_A, BASE_I };

/* The texture's base format as the environments see it. */
static int base_of(const dgl_texture *t)
{
    switch (t->level[0].ifc) {
    case DGL_IF_RGB: case DGL_IF_LUMINANCE: return BASE_RGB;
    case DGL_IF_ALPHA: return BASE_A;
    case DGL_IF_INTENSITY: return BASE_I;
    default: return BASE_RGBA;              /* RGBA, LA, RGB5_A1, colour index */
    }
}

/* Stage `stage`'s word for environment `env` on texture t; 0 (with *w
 * unset) when the combiner cannot do it in one stage: GL_BLEND with an
 * environment colour other than black. */
int dgl_stage_word(int stage, GLenum env, const dgl_texture *t, const GLfloat *env_color, uint32_t *w)
{
    static const uint32_t replace[2][4]  = { { 0x40000000u, 0x00000000u, 0x00200000u, 0x00000000u },
                                             { 0x43000000u, 0x00000000u, 0x00200003u, 0x00000000u } };
    static const uint32_t modulate[2][4] = { { 0x40600000u, 0xC0600000u, 0xC0200000u, 0xC0600000u },
                                             { 0x43600003u, 0xC3600003u, 0xC3200003u, 0xC3600003u } };
    static const uint32_t decal[2][4]    = { { 0x40000000u, 0x40526A08u, 0x40200000u, 0x40200000u },
                                             { 0x43000000u, 0x43526A0Bu, 0x43200003u, 0x43200003u } };
    static const uint32_t blend[2][4]    = { { 0x40600040u, 0xC0600040u, 0xC0200000u, 0xC0E00040u },
                                             { 0x43600043u, 0xC3600043u, 0xC3200003u, 0xC3E00043u } };
    int b = base_of(t), s = stage ? 1 : 0;
    switch (env) {
    case GL_REPLACE: *w = replace[s][b]; return 1;
    case GL_MODULATE: *w = modulate[s][b]; return 1;
    case GL_DECAL: *w = decal[s][b]; return 1;
    case GL_BLEND:
        /* Cf * (1 - Ct): exact only for a black environment colour (and, for
         * intensity, a zero alpha). */
        if (env_color[0] != 0 || env_color[1] != 0 || env_color[2] != 0 || (b == BASE_I && env_color[3] != 0))
            return 0;
        *w = blend[s][b];
        return 1;
    default:
        return 0;
    }
}

/* Does this environment need more than TEXCTL's modulate/replace with one
 * texture? GL_BLEND always; GL_DECAL on a texture with alpha (a blend by
 * texel alpha: the G400's combiner in its blend mode, which needs dual
 * texturing on, docs/g400-dual-texture.md §2; the G200's decalblend). */
int dgl_env_needs_combiner(GLenum env, const dgl_texture *t)
{
    return env == GL_BLEND || (env == GL_DECAL && base_of(t) != BASE_RGB);
}

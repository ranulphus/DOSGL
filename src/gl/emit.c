/* emit.c - from the vertex stream to the drawing engine (FR-TS-1..3).
 *
 * begin() validates dirty state into registers: DWGCTL (depth), ALPHACTRL
 * (blend, alpha test), MACCESS fog enable, FOGCOL, PLNWT (colour mask) and
 * the scissor. Each triangle is clipped (near/far and a guard band),
 * projected to 1/16-pixel screen coordinates, culled, and handed to the
 * HAL's setup_triangle. Lines and points become screen-space quads. */
#include "gl_state.h"
#include "gl_draw.h"
#include "gl_tex.h"
#include "../dgl/dgl.h"
#include "mga/tex.h"
#include "mga/mmio.h"
#include "mga/regs_mga.h"
#include "mga/setup.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int dgl_stage_word(int stage, const dgl_texenv *e, const dgl_texture *t, uint32_t *w);
int dgl_env_needs_combiner(GLenum env, const dgl_texture *t);

static mga_tri_ctx tctx;
static mga_texstate tstate, tstate1;  /* hardware maps 0 and 1 */
static int textured;                  /* 0 none, 1 the bound texture, 2 the white texture (fog) */
static int dual;                      /* G400: both maps in use (MGA_S_TEX2) */
static int map0_unit;                 /* the GL unit whose coordinates feed map 0 */
static int map1_copy;                 /* map 1 repeats map 0 (a one-texture combiner mode needing dualtex) */
static float tex_scale_s = 1, tex_scale_t = 1;   /* logical / stored size (small textures are widened to 8) */
static float tex_scale_s1 = 1, tex_scale_t1 = 1;
/* Silicon experiments (docs/silicon-experiments.md): DGL_COMBINER=1 draws
 * every environment with the G400's combiner, the legacy modulate off (as
 * Mesa does); DGL_TC2_EXTRA=hex ORs bits into every TEXCTL2 (bit 15, which
 * the Linux and X.org drivers always set on the G400). */
static int force_combiner;
static uint32_t tc2_extra;
/* DGL_GUARD_PX: how far from the viewport's centre screen coordinates may
 * reach before triangles are clipped (default 2000, the setup's range);
 * smaller clips more but keeps edge and texture gradients shorter. */
static float guard_px = 2000.0f;
static int skip_all;                  /* depth or alpha function NEVER, or no context */
static float tri_offset;              /* glPolygonOffset for the triangle being drawn (depth steps) */
static float guard_x, guard_y;

/* ---- State validation --------------------------------------------------- */
static uint32_t zmode_for(GLenum f)
{
    switch (f) {
    case GL_LESS:     return DWG_ZMODE_ZLT;
    case GL_EQUAL:    return DWG_ZMODE_ZE;
    case GL_LEQUAL:   return DWG_ZMODE_ZLTE;
    case GL_GREATER:  return DWG_ZMODE_ZGT;
    case GL_NOTEQUAL: return DWG_ZMODE_ZNE;
    case GL_GEQUAL:   return DWG_ZMODE_ZGTE;
    default:          return DWG_ZMODE_NOZCMP;       /* ALWAYS */
    }
}

static uint32_t src_factor(GLenum f)
{
    switch (f) {
    case GL_ZERO: return BLEND_ZERO;
    case GL_DST_COLOR: return BLEND_DST_COLOR;
    case GL_ONE_MINUS_DST_COLOR: return BLEND_ONE_MINUS_DST_COLOR;
    case GL_SRC_ALPHA: return BLEND_SRC_ALPHA;
    case GL_ONE_MINUS_SRC_ALPHA: return BLEND_ONE_MINUS_SRC_ALPHA;
    case GL_DST_ALPHA: return BLEND_DST_ALPHA;
    case GL_ONE_MINUS_DST_ALPHA: return BLEND_ONE_MINUS_DST_ALPHA;
    case GL_SRC_ALPHA_SATURATE: return BLEND_SRC_ALPHA_SATURATE;
    default: return BLEND_ONE;
    }
}

static uint32_t dst_factor(GLenum f)
{
    switch (f) {
    case GL_ONE: return BLEND_ONE;
    case GL_SRC_COLOR: return BLEND_SRC_COLOR;
    case GL_ONE_MINUS_SRC_COLOR: return BLEND_ONE_MINUS_SRC_COLOR;
    case GL_SRC_ALPHA: return BLEND_SRC_ALPHA;
    case GL_ONE_MINUS_SRC_ALPHA: return BLEND_ONE_MINUS_SRC_ALPHA;
    case GL_DST_ALPHA: return BLEND_DST_ALPHA;
    case GL_ONE_MINUS_DST_ALPHA: return BLEND_ONE_MINUS_DST_ALPHA;
    default: return BLEND_ZERO;
    }
}

static uint32_t atmode_for(GLenum f)
{
    static const uint8_t m[8] = { 0, 4, 2, 5, 6, 3, 7, 0 };   /* NEVER..ALWAYS -> ATMODE codes */
    return m[(f - GL_NEVER) & 7];
}

static uint32_t plnwt_for(void)
{
    uint32_t m = (dgl_gl.color_mask[0] ? 0xF800u : 0) | (dgl_gl.color_mask[1] ? 0x07E0u : 0) |
                 (dgl_gl.color_mask[2] ? 0x001Fu : 0);
    return m | (m << 16);
}

static void scissor_rows(int *x0, int *y0, int *x1, int *y1)
{
    *x0 = 0; *y0 = 0; *x1 = dgl_ctx.width; *y1 = dgl_ctx.height;
    if (dgl_gl.scissor_test) {
        int sx0 = dgl_gl.scissor[0], sx1 = dgl_gl.scissor[0] + dgl_gl.scissor[2];
        int sy0 = dgl_ctx.height - (dgl_gl.scissor[1] + dgl_gl.scissor[3]);     /* GL y is bottom-up */
        int sy1 = dgl_ctx.height - dgl_gl.scissor[1];
        if (sx0 > *x0) *x0 = sx0;
        if (sy0 > *y0) *y0 = sy0;
        if (sx1 < *x1) *x1 = sx1;
        if (sy1 < *y1) *y1 = sy1;
        if (*x1 < *x0) *x1 = *x0;
        if (*y1 < *y0) *y1 = *y0;
    }
}

/* A map's sampler from a texture object. */
static void sampler(mga_texstate *ts, const dgl_texture *tex, float *scale_s, float *scale_t)
{
    int k;
    memset(ts, 0, sizeof *ts);
    ts->org = tex->level_off[0];
    ts->mip_n = tex->hw_levels > 1 ? tex->hw_levels : 0;
    for (k = 0; k < tex->hw_levels && k < 5; k++)
        ts->mip_org[k] = tex->level_off[k];
    ts->w_log2 = tex->hw_w_log2;
    ts->h_log2 = tex->hw_h_log2;
    *scale_s = (float)tex->level[0].w / (float)(1 << tex->hw_w_log2);
    *scale_t = (float)tex->level[0].h / (float)(1 << tex->hw_h_log2);
    ts->pitch = 1 << tex->hw_w_log2;
    ts->hwfmt = (uint32_t)tex->hwfmt;
    ts->clamp_u = tex->wrap_s != GL_REPEAT;      /* CLAMP behaves as CLAMP_TO_EDGE */
    ts->clamp_v = tex->wrap_t != GL_REPEAT;
    ts->bilinear = tex->mag_filter == GL_LINEAR || tex->min_filter == GL_LINEAR ||
                   tex->min_filter == GL_LINEAR_MIPMAP_NEAREST || tex->min_filter == GL_LINEAR_MIPMAP_LINEAR;
    ts->trilinear = ts->mip_n > 1 && (tex->min_filter == GL_NEAREST_MIPMAP_LINEAR ||
                                      tex->min_filter == GL_LINEAR_MIPMAP_LINEAR);
}

/* A stage's combiner word for a GL unit's environment; one the combiner
 * cannot do in one stage (GL_BLEND with a colour, some GL_COMBINE_ARB
 * functions, combine.c) is drawn as GL_MODULATE, logged once. */
static uint32_t stage_word(int stage, int unit, const dgl_texture *t)
{
    static int warned;
    const dgl_texenv *e = dgl_tex_env(unit);
    dgl_texenv modulate = *e;
    uint32_t w = 0;
    if (!dgl_stage_word(stage, e, t, &w)) {
        if (!warned++)
            DGL_WARN("DGL-WARN env 0x%x (combine 0x%x/0x%x) is drawn as GL_MODULATE", (unsigned)e->mode,
                     (unsigned)e->combine_rgb, (unsigned)e->combine_alpha);
        modulate.mode = GL_MODULATE;
        dgl_stage_word(stage, &modulate, t, &w);
    }
    return w;
}

static void validate(void)
{
    int depth = dgl_gl.depth_test && dgl_ctx.z_off;
    uint32_t alphactrl, zmode = depth ? zmode_for(dgl_gl.depth_func) : DWG_ZMODE_NOZCMP, asel;
    dgl_texture *tex = dgl_gl.texture_2d ? dgl_unit_texture(0) : NULL;
    dgl_texture *tex1 = dgl_gl.texture_2d1 && dgl_texture_units() > 1 ? dgl_unit_texture(1) : NULL;
    int unit0 = 0;
    if (!tex && tex1) {
        tex = tex1;                     /* GL unit 1 alone drives map 0 */
        tex1 = NULL;
        unit0 = 1;
    }
    skip_all = !dgl_ctx.active || (depth && dgl_gl.depth_func == GL_NEVER) ||
               (dgl_gl.alpha_test && dgl_gl.alpha_func == GL_NEVER);
    if (dgl_gl.dirty & DGL_DIRTY_TARGET) {
        int x0, y0, x1, y1;
        scissor_rows(&x0, &y0, &x1, &y1);
        engine_set_clip(x0, y0, x1, y1);
        tctx.clip_y0 = y0;
        tctx.clip_y1 = y1;
    }
    /* An incomplete texture draws untextured (GL); one that cannot be made
     * resident skips the draw (out of memory, PRD §8.3). */
    if (tex && dgl_texture_ready(tex) != 0) {
        skip_all = 1;
        tex = NULL;
    } else if (tex) {
        dgl_texture_drawn(tex);        /* busy until the next completed sync */
        dgl_texture_lut(tex);          /* TW8: its palette in the LUT (queued before this draw) */
    }
    if (tex && tex1) {
        /* Busy, map 0's texture cannot be evicted to make room for map 1's
         * unless a sync frees everything; then make it resident again. */
        if (dgl_texture_ready(tex1) != 0 || dgl_texture_ready(tex) != 0) {
            skip_all = 1;
            tex = tex1 = NULL;
        } else {
            dgl_texture_drawn(tex1);
            dgl_texture_drawn(tex);
            dgl_texture_lut(tex1);     /* both maps share one LUT (the shared palette) */
        }
    }
    if ((dgl_gl.dirty & DGL_DIRTY_TEXTURE) || !tex != (textured != 1) || (tex1 != NULL) != (dual && !map1_copy) ||
        unit0 != map0_unit)
        dgl_gl.dirty |= DGL_DIRTY_RASTER | DGL_DIRTY_TEXTURE;
    if (dgl_gl.dirty & (DGL_DIRTY_RASTER | DGL_DIRTY_FOG | DGL_DIRTY_TARGET | DGL_DIRTY_TEXTURE)) {
        uint32_t white, w0 = 0, w1 = 0;
        memset(&tstate, 0, sizeof tstate);
        textured = 0;
        dual = map1_copy = 0;
        map0_unit = unit0;
        asel = ALPHASEL_DIFFUSE;
        if (tex) {
            GLenum env = dgl_tex_env_mode(unit0);
            textured = 1;
            sampler(&tstate, tex, &tex_scale_s, &tex_scale_t);
            if (mga.has_dual_tex && (tex1 || force_combiner || dgl_env_needs_combiner(env, tex))) {
                /* G400: the combiner does the environments (Mesa's words);
                 * the legacy modulate stays off. */
                w0 = stage_word(0, unit0, tex);
                if (tex1) {
                    sampler(&tstate1, tex1, &tex_scale_s1, &tex_scale_t1);
                    w1 = stage_word(1, 1, tex1);
                    dual = 1;
                } else if (w0 & (1u << 20)) {
                    /* The blend mode (GL_DECAL on alpha) needs dualtex: map 1
                     * repeats map 0 and stage 1 passes stage 0 through. */
                    tstate1 = tstate;
                    tex_scale_s1 = tex_scale_s;
                    tex_scale_t1 = tex_scale_t;
                    w1 = 0x43200003u;
                    dual = map1_copy = 1;
                } else
                    w1 = w0;                /* single texturing: stage 1 as stage 0 */
                asel = ALPHASEL_TEXTURE;    /* the combiner's alpha */
            } else {
                static int warned;
                if (env == GL_BLEND && !warned++)
                    DGL_WARN("DGL-WARN GL_BLEND is drawn as GL_MODULATE on this card");
                tstate.modulate = env == GL_MODULATE || env == GL_BLEND;
                if (env == GL_DECAL && dgl_env_needs_combiner(env, tex) && mga.has_decalblend)
                    tstate.texctl2 |= TEXCTL2_DECALBLEND;   /* G200: blend by texel alpha */
                asel = tstate.modulate ? ALPHASEL_MODULATED : ALPHASEL_TEXTURE;
            }
        } else if (dgl_gl.fog && dgl_white_texture(&white) == 0) {
            /* The engine fogs only textured trapezoids (in 86Box, and on the
             * G100): untextured fogged draws sample a white texel. */
            textured = 2;
            tex_scale_s = tex_scale_t = 1;
            tstate.org = white;
            tstate.w_log2 = tstate.h_log2 = 3;
            tstate.pitch = 8;
            tstate.hwfmt = DGL_TW16;
            tstate.modulate = 1;
        }
        tctx.dwgctl = (textured ? DWG_OPCOD_TEXTURE_TRAP : DWG_OPCOD_TRAP) | zmode | DWG_BOP_COPY |
                      ((depth && dgl_gl.depth_mask) ? DWG_ATYPE_ZI : DWG_ATYPE_I);
        tctx.flags = MGA_S_COLOR | (depth ? MGA_S_Z : 0) | (textured ? MGA_S_TEX : 0) | (dual ? MGA_S_TEX2 : 0);
        tctx.tex_tw = tstate.w_log2;
        tctx.tex_th = tstate.h_log2;
        tctx.tex_tw1 = tstate1.w_log2;
        tctx.tex_th1 = tstate1.h_log2;
        tstate.texctl2 |= tc2_extra;
        tstate1.texctl2 |= tc2_extra;
        tctx.texctl2_1 = tstate1.texctl2 | TEXCTL2_DUALTEX;
        if (textured) {
            if (dual)
                tex_emit_dual(&tstate, &tstate1);
            else
                tex_emit(&tstate);
            if (mga.has_dual_tex)
                tex_emit_combiner(w0, w1);  /* zero words pass the legacy result through */
        }
        if (mga.has_alpha_blend) {
            alphactrl = ALPHACTRL_ALPHASEL(asel);
            if (dgl_gl.blend)
                alphactrl |= ALPHACTRL_SRC(src_factor(dgl_gl.blend_src)) | ALPHACTRL_DST(dst_factor(dgl_gl.blend_dst));
            else
                alphactrl |= ALPHACTRL_SRC(BLEND_ONE) | ALPHACTRL_DST(BLEND_ZERO);
            if (dgl_gl.alpha_test && dgl_gl.alpha_func != GL_ALWAYS && mga.has_alpha_test)
                alphactrl |= ALPHACTRL_ATEN | ALPHACTRL_ATMODE(atmode_for(dgl_gl.alpha_func)) |
                             ALPHACTRL_ATREF((uint32_t)lrintf(dgl_gl.alpha_ref * 255.0f));
        } else {
            /* G100 (development only): stipple is its only blend (PRD D13). */
            alphactrl = ALPHACTRL_G100_FIXED | ALPHACTRL_ALPHASEL(asel) |
                        (dgl_gl.blend ? ALPHACTRL_ASTIPPLE : 0);
        }
        if (dgl_gl.blend || dgl_gl.alpha_test)
            tctx.flags |= MGA_S_ALPHA;
        if (dgl_gl.fog && textured)
            tctx.flags |= MGA_S_FOG;
        engine_set_maccess_flags(((tctx.flags & MGA_S_FOG) ? MACCESS_FOGEN : 0) |
                                 (dgl_gl.dither ? 0 : MACCESS_NODITHER));
        fifo_reserve(3);
        MGA_WR32(MGAREG_ALPHACTRL, alphactrl);
        MGA_WR32(MGAREG_PLNWT, plnwt_for());
        MGA_WR32(MGAREG_FOGCOL, ((uint32_t)lrintf(dgl_gl.fog_color[0] * 255.0f) << 16) |
                                ((uint32_t)lrintf(dgl_gl.fog_color[1] * 255.0f) << 8) |
                                (uint32_t)lrintf(dgl_gl.fog_color[2] * 255.0f));
    }
    dgl_gl.dirty &= ~(DGL_DIRTY_RASTER | DGL_DIRTY_FOG | DGL_DIRTY_TARGET | DGL_DIRTY_TEXTURE);
    /* Guard band: keep screen coordinates inside +-2048 (the setup's range). */
    {
        float hw = dgl_gl.viewport[2] * 0.5f, hh = dgl_gl.viewport[3] * 0.5f;
        guard_x = hw > 0 ? (guard_px - fabsf(dgl_gl.viewport[0] + hw)) / hw : 1.0f;
        guard_y = hh > 0 ? (guard_px - fabsf(dgl_gl.viewport[1] + hh)) / hh : 1.0f;
        if (guard_x < 1.0f) guard_x = 1.0f;
        if (guard_y < 1.0f) guard_y = 1.0f;
    }
}

static int emit_begin(void)
{
    validate();
    return !skip_all;
}

/* ---- Projection -------------------------------------------------------- */
static float fog_factor(float d)
{
    float f;
    switch (dgl_gl.fog_mode) {
    case GL_LINEAR:
        f = dgl_gl.fog_end != dgl_gl.fog_start ? (dgl_gl.fog_end - d) / (dgl_gl.fog_end - dgl_gl.fog_start) : 1.0f;
        break;
    case GL_EXP:
        f = expf(-dgl_gl.fog_density * d);
        break;
    default:
        f = expf(-(dgl_gl.fog_density * d) * (dgl_gl.fog_density * d));
        break;
    }
    return f < 0 ? 0 : f > 1 ? 1 : f;
}

typedef struct { double x, y; mga_svtx v; } proj;

/* Out of line, as it has always been compiled: its float temporaries stay
 * where they are. */
static __attribute__((noinline)) void project(const dgl_cvtx *c, proj *p)
{
    double iw = 1.0 / c->w;
    double xw = dgl_gl.viewport[0] + (c->x * iw + 1.0) * 0.5 * dgl_gl.viewport[2];
    double yw = dgl_gl.viewport[1] + (c->y * iw + 1.0) * 0.5 * dgl_gl.viewport[3];
    double zw = dgl_gl.depth_near + (c->z * iw + 1.0) * 0.5 * (dgl_gl.depth_far - dgl_gl.depth_near);
    p->x = xw;
    p->y = dgl_ctx.height - yw;                 /* screen rows go down */
    memset(&p->v, 0, sizeof p->v);
    p->v.X16 = mga_irint_nearest(p->x * 16.0);     /* lrint() inside DGL_FPU_ENTER, without the call */
    p->v.Y16 = mga_irint_nearest(p->y * 16.0);
    p->v.z = zw < 0 ? 0 : zw > 1 ? 65535.0 : zw * 65535.0;
    p->v.r = c->r * 255.0f; p->v.g = c->g * 255.0f; p->v.b = c->b * 255.0f; p->v.a = c->a * 255.0f;
    p->v.fog = dgl_gl.fog ? 255.0f * fog_factor(c->eye_d) : 255.0f;
    /* Texture coordinates for the setup: normalised s, t times q = 1/w. */
    p->v.q = (float)iw;
    if (map0_unit) {                            /* GL unit 1 alone feeds map 0 */
        p->v.s = (float)(c->s1 * tex_scale_s * iw);
        p->v.t = (float)(c->t1 * tex_scale_t * iw);
    } else {
        p->v.s = (float)(c->s * tex_scale_s * iw);
        p->v.t = (float)(c->t * tex_scale_t * iw);
    }
    if (dual) {
        p->v.s1 = (float)((map1_copy ? p->v.s / tex_scale_s * tex_scale_s1 : c->s1 * tex_scale_s1 * iw));
        p->v.t1 = (float)((map1_copy ? p->v.t / tex_scale_t * tex_scale_t1 : c->t1 * tex_scale_t1 * iw));
    }
}

static void clamp_colour(mga_svtx *v)
{
#define C(f) if (v->f < 0) v->f = 0; else if (v->f > 255) v->f = 255
    C(r); C(g); C(b); C(a);
#undef C
}

/* flat: the provoking vertex when its colour is to replace the projected
 * ones (GL_FLAT on the fast path: the same product project() makes). */
static void draw_projected(const proj *p0, const proj *p1, const proj *p2, const dgl_cvtx *flat)
{
    mga_svtx a = p0->v, b = p1->v, c = p2->v;
    if (flat) {
        a.r = flat->r * 255.0f; a.g = flat->g * 255.0f; a.b = flat->b * 255.0f; a.a = flat->a * 255.0f;
        b.r = a.r; b.g = a.g; b.b = a.b; b.a = a.a;
        c.r = a.r; c.g = a.g; c.b = a.b; c.a = a.a;
    }
    clamp_colour(&a); clamp_colour(&b); clamp_colour(&c);
    if (tri_offset != 0.0f) {
#define OFS(v) v.z += tri_offset; if (v.z < 0) v.z = 0; else if (v.z > 65535.0) v.z = 65535.0
        OFS(a); OFS(b); OFS(c);
#undef OFS
    }
    if (textured)
        tex_adjust_coords(&a, &b, &c, &tstate);
    if (dual)
        tex_adjust_coords1(&a, &b, &c, &tstate1);
    setup_triangle(&a, &b, &c, &tctx);
}

/* ---- Fog subdivision -------------------------------------------------------
 * The fog factor is iterated linearly across the screen, while GL's varies
 * with eye distance (perspective-correct, and exponentially for EXP/EXP2).
 * Where it changes by more than FOG_SPLIT levels over a sizeable triangle,
 * the triangle is split at its clip-space edge midpoints, which puts exact
 * per-vertex fog at the new corners. */
#define FOG_SPLIT 12.0f
#define FOG_SPLIT_DEPTH 4

static void mid(dgl_cvtx *o, const dgl_cvtx *a, const dgl_cvtx *b)
{
#define M(f) o->f = 0.5f * (a->f + b->f)
    M(x); M(y); M(z); M(w); M(r); M(g); M(b); M(a); M(s); M(t); M(s1); M(t1); M(eye_d);
#undef M
}

static void fog_split(const dgl_cvtx *c0, const dgl_cvtx *c1, const dgl_cvtx *c2,
                      const proj *p0, const proj *p1, const proj *p2, int depth, const dgl_cvtx *flat)
{
    float lo = p0->v.fog, hi = p0->v.fog;
    double area;
    dgl_cvtx m01, m12, m20;
    proj q01, q12, q20;
    if (p1->v.fog < lo) lo = p1->v.fog;
    if (p1->v.fog > hi) hi = p1->v.fog;
    if (p2->v.fog < lo) lo = p2->v.fog;
    if (p2->v.fog > hi) hi = p2->v.fog;
    area = fabs((p1->x - p0->x) * (p2->y - p0->y) - (p2->x - p0->x) * (p1->y - p0->y)) * 0.5;
    if (depth >= FOG_SPLIT_DEPTH || hi - lo <= FOG_SPLIT || area < 32.0) {
        draw_projected(p0, p1, p2, flat);
        return;
    }
    mid(&m01, c0, c1); mid(&m12, c1, c2); mid(&m20, c2, c0);
    project(&m01, &q01); project(&m12, &q12); project(&m20, &q20);
    fog_split(c0, &m01, &m20, p0, &q01, &q20, depth + 1, flat);
    fog_split(&m01, c1, &m12, &q01, p1, &q12, depth + 1, flat);
    fog_split(&m20, &m12, c2, &q20, &q12, p2, depth + 1, flat);
    fog_split(&m01, &m12, &m20, &q01, &q12, &q20, depth + 1, flat);
}

/* ---- Triangles ---------------------------------------------------------- */
/* Per-slot outcodes and projections of vertex.c's cache (gl_draw.h): a
 * vertex shared by several triangles of one dgl_assemble is projected
 * once. project() is out of line and deterministic, so a cached result is
 * the one a fresh call would give. */
enum { VS_OC = 1, VS_PROJ = 2 };
static struct { proj p; unsigned oc; } vs[DGL_VSLOTS];

static int slot_of(const dgl_cvtx *v)
{
    uintptr_t d = (uintptr_t)v - (uintptr_t)dgl_vslot;
    return d < sizeof dgl_vslot ? (int)(d / sizeof dgl_vslot[0]) : -1;
}

static unsigned slot_oc(int s, const dgl_cvtx *v)
{
    if (!(dgl_vslot_sink[s] & VS_OC)) {
        vs[s].oc = dgl_outcode(v, guard_x, guard_y);
        dgl_vslot_sink[s] |= VS_OC;
    }
    return vs[s].oc;
}

static const proj *slot_proj(int s, const dgl_cvtx *v)
{
    if (!(dgl_vslot_sink[s] & VS_PROJ)) {
        project(v, &vs[s].p);
        dgl_vslot_sink[s] |= VS_PROJ;
    }
    return &vs[s].p;
}

static void emit_triangle(const dgl_cvtx *a, const dgl_cvtx *b, const dgl_cvtx *c, const dgl_cvtx *prov)
{
    dgl_cvtx in[3], out[9];
    proj p[9];
    const proj *P[9];
    const dgl_cvtx *C[9], *flat = NULL;
    int n, i, sa = slot_of(a), sb = slot_of(b), sc = slot_of(c);
    int64_t area2;
    PROF_SCOPE(PROF_D_CLIP);
    dgl_prims.tris_in++;
    if (sa >= 0 && sb >= 0 && sc >= 0 && !(slot_oc(sa, a) | slot_oc(sb, b) | slot_oc(sc, c))) {
        /* Nothing to clip: the vertices as they are, projected once each;
         * flat colour replaced after projection instead of before. */
        PROF_SWITCH(PROF_D_PROJ);
        P[0] = slot_proj(sa, a); P[1] = slot_proj(sb, b); P[2] = slot_proj(sc, c);
        C[0] = a; C[1] = b; C[2] = c;
        n = 3;
        if (dgl_gl.shade_model == GL_FLAT)
            flat = prov;
    } else {
        in[0] = *a; in[1] = *b; in[2] = *c;
        if (dgl_gl.shade_model == GL_FLAT)
            for (i = 0; i < 3; i++) {
                in[i].r = prov->r; in[i].g = prov->g; in[i].b = prov->b; in[i].a = prov->a;
            }
        n = dgl_clip_polygon(in, 3, out, guard_x, guard_y);
        if (n < 3) {
            dgl_prims.clipped++;
            return;
        }
        PROF_SWITCH(PROF_D_PROJ);
        for (i = 0; i < n; i++) {
            project(&out[i], &p[i]);
            P[i] = &p[i];
            C[i] = &out[i];
        }
    }
    /* Cull on the (unclipped-equivalent) winding of the first three screen
     * vertices; y runs down on screen, so GL's counter-clockwise is area < 0. */
    area2 = (int64_t)(P[1]->v.X16 - P[0]->v.X16) * (P[2]->v.Y16 - P[0]->v.Y16) -
            (int64_t)(P[2]->v.X16 - P[0]->v.X16) * (P[1]->v.Y16 - P[0]->v.Y16);
    if (area2 == 0) {
        dgl_prims.zero_area++;
        return;
    }
    if (dgl_gl.cull_face) {
        int front = (dgl_gl.front_face == GL_CCW) ? area2 < 0 : area2 > 0;
        if (dgl_gl.cull_mode == GL_FRONT_AND_BACK || (dgl_gl.cull_mode == GL_BACK) != front) {
            dgl_prims.culled++;
            return;
        }
    }
    PROF_SWITCH(PROF_D_SETUP);
    for (i = 1; i + 1 < n; i++) {
        tri_offset = 0.0f;
        if (dgl_gl.offset_fill && (dgl_gl.offset_factor != 0.0f || dgl_gl.offset_units != 0.0f)) {
            /* GL: o = factor * max |dz/dx|, |dz/dy| + units * r, with z in
             * depth-buffer steps (r = one step). */
            double x1 = P[i]->x - P[0]->x, y1 = P[i]->y - P[0]->y, z1 = P[i]->v.z - P[0]->v.z;
            double x2 = P[i + 1]->x - P[0]->x, y2 = P[i + 1]->y - P[0]->y, z2 = P[i + 1]->v.z - P[0]->v.z;
            double det = x1 * y2 - x2 * y1, m = 0;
            if (det != 0) {
                double dzdx = fabs((z1 * y2 - z2 * y1) / det), dzdy = fabs((x1 * z2 - x2 * z1) / det);
                m = dzdx > dzdy ? dzdx : dzdy;
            }
            tri_offset = (float)(dgl_gl.offset_factor * m + dgl_gl.offset_units);
        }
        if (dgl_gl.fog)
            fog_split(C[0], C[i], C[i + 1], P[0], P[i], P[i + 1], 0, flat);
        else
            draw_projected(P[0], P[i], P[i + 1], flat);
    }
}

/* ---- Lines and points: one-pixel screen-space quads (PRD §7) ------------ */
static void quad(proj *a, proj *b, double ox, double oy)
{
    proj q[4];
    q[0] = *a; q[1] = *b; q[2] = *b; q[3] = *a;
    q[0].x -= ox; q[0].y -= oy; q[1].x -= ox; q[1].y -= oy;
    q[2].x += ox; q[2].y += oy; q[3].x += ox; q[3].y += oy;
    {
        int i;
        for (i = 0; i < 4; i++) {
            q[i].v.X16 = mga_irint_nearest(q[i].x * 16.0);
            q[i].v.Y16 = mga_irint_nearest(q[i].y * 16.0);
        }
    }
    draw_projected(&q[0], &q[1], &q[2], NULL);
    draw_projected(&q[0], &q[2], &q[3], NULL);
}

static void emit_line(const dgl_cvtx *a, const dgl_cvtx *b)
{
    dgl_cvtx in[3], out[9];
    proj p0, p1;
    int n;
    double dx, dy, hw;
    PROF_SCOPE(PROF_D_SETUP);
    /* Clip the segment as a degenerate triangle, keep the first two outputs. */
    in[0] = *a; in[1] = *b; in[2] = *b;
    if (dgl_gl.shade_model == GL_FLAT) {
        in[0].r = b->r; in[0].g = b->g; in[0].b = b->b; in[0].a = b->a;
    }
    n = dgl_clip_polygon(in, 3, out, guard_x, guard_y);
    if (n < 2)
        return;
    project(&out[0], &p0);
    project(&out[1], &p1);
    tri_offset = 0.0f;
    dx = p1.x - p0.x; dy = p1.y - p0.y;
    hw = 0.5 * (dgl_gl.line_width < 1.0f ? 1.0 : dgl_gl.line_width);
    if (fabs(dx) >= fabs(dy))
        quad(&p0, &p1, 0.0, hw);              /* x-major: widen vertically */
    else
        quad(&p0, &p1, hw, 0.0);
}

static void emit_point(const dgl_cvtx *a)
{
    proj p, q;
    double s = dgl_gl.point_size < 1.0f ? 1.0 : dgl_gl.point_size;   /* a square s pixels wide */
    PROF_SCOPE(PROF_D_SETUP);
    if (dgl_outcode(a, guard_x, guard_y))
        return;
    project(a, &p);
    tri_offset = 0.0f;
    p.x -= 0.5 * s;
    q = p;
    q.x += s;
    quad(&p, &q, 0.0, 0.5 * s);
}

static void emit_end(void) { }

void dgl_emit_install(void)
{
    const char *e = getenv("DGL_COMBINER");
    force_combiner = e && *e && *e != '0';
    e = getenv("DGL_TC2_EXTRA");
    tc2_extra = e && *e && mga.has_dual_tex ? (uint32_t)strtoul(e, NULL, 16) : 0;
    e = getenv("DGL_GUARD_PX");
    guard_px = e && atoi(e) > 0 && atoi(e) <= 2000 ? (float)atoi(e) : 2000.0f;
    dgl_sink.begin = emit_begin;
    dgl_sink.triangle = emit_triangle;
    dgl_sink.line = emit_line;
    dgl_sink.point = emit_point;
    dgl_sink.end = emit_end;
}

/* ---- Clears -------------------------------------------------------------- */
static void clear(GLbitfield mask)
{
    int x0, y0, x1, y1;
    PROF_SCOPE(PROF_D_CLEAR);
    DGL_FPU_ENTER();
    scissor_rows(&x0, &y0, &x1, &y1);
    engine_set_clip(x0, y0, x1, y1);
    /* Clears are engine fills: no blending or fog; the colour mask applies. */
    engine_set_maccess_flags(0);
    fifo_reserve(2);
    MGA_WR32(MGAREG_ALPHACTRL, mga.has_alpha_blend ? ALPHACTRL_SRC(BLEND_ONE) | ALPHACTRL_DST(BLEND_ZERO)
                                                   : ALPHACTRL_G100_FIXED);
    MGA_WR32(MGAREG_PLNWT, plnwt_for());
    if (mask & GL_COLOR_BUFFER_BIT) {
        uint32_t r = (uint32_t)lrintf(dgl_gl.clear_color[0] * 31.0f);
        uint32_t g = (uint32_t)lrintf(dgl_gl.clear_color[1] * 63.0f);
        uint32_t b = (uint32_t)lrintf(dgl_gl.clear_color[2] * 31.0f);
        engine_fill(x0, y0, x1 - x0, y1 - y0, (r << 11) | (g << 5) | b);
    }
    if ((mask & GL_DEPTH_BUFFER_BIT) && dgl_ctx.z_off && dgl_gl.depth_mask) {
        fifo_reserve(1);
        MGA_WR32(MGAREG_PLNWT, 0xFFFFFFFFu);
        engine_fill_depth(x0, y0, x1 - x0, y1 - y0, (uint32_t)lrint(dgl_gl.clear_depth * 65535.0));
    }
    dgl_gl.dirty |= DGL_DIRTY_RASTER | DGL_DIRTY_TARGET;
    DGL_FPU_LEAVE();
}

void APIENTRY glClear(GLbitfield mask)
{
    if (mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | 0x200u /* accum */)) {
        dgl_gl_error(GL_INVALID_VALUE);
        return;
    }
    if (!dgl_ctx.active)
        return;
    clear(mask);
}

/* In a scaled mode a single-buffered program, or one drawing to GL_FRONT,
 * sees its picture once it is scaled onto the display. */
void APIENTRY glFlush(void)
{
    if (dgl_ctx.active && dgl_ctx.scaled && (!dgl_ctx.double_buffer || dgl_ctx.draw_front))
        dgl_present_front();
}

void APIENTRY glFinish(void)
{
    if (!dgl_ctx.active)
        return;
    if (dgl_ctx.scaled && (!dgl_ctx.double_buffer || dgl_ctx.draw_front))
        dgl_present_front();
    else
        dgl_sync();
}

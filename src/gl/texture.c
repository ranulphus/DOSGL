/* texture.c - texture objects (PRD §6.1, §8.3, M4).
 *
 * Every level keeps an RGBA8 shadow copy, so a later upload can change the
 * hardware format (an opaque atlas gaining translucent texels moves from
 * RGB565 to ARGB4444) and the VRAM copy can be rebuilt after eviction. The
 * hardware copy holds level 0 and, for mipmapped filters, the following
 * levels while they are at least 8x8 (the G200-style window, up to 5
 * levels). Levels smaller than 8 texels are widened to 8 by repetition
 * (wrap) or edge extension (clamp). Uploads go through the LFB without
 * waiting for the engine: a texture drawn since the last engine sync (busy)
 * is re-uploaded into a new block (rename-on-write) while its old block is
 * retired until the next sync, and glTexSubImage2D writes just the rectangle
 * when the texture is not busy and the new texels fit its format. When VRAM
 * runs out, the least recently drawn textures are evicted (their shadow
 * copies bring them back). */
#include "gl_tex.h"
#include "../dgl/dgl.h"
#include <stdlib.h>
#include <string.h>

#define MAX_TEXTURES 4096
#define WINDOW_LEVELS 5

static dgl_texture *tex[MAX_TEXTURES];
static GLuint bound_u[2];                /* names bound to GL_TEXTURE_2D, per texture unit */
#define bound (bound_u[dgl_gl.active_unit])   /* the active unit's */
static uint32_t white_off;
static int white_ok;
/* The hardware texture LUT (G200): the palette last loaded into it, from a
 * 512-byte block of RGB565 entries. DGL_TLUT: unset uses it on the G200
 * only (the G400 specification says to expand 8-bit textures; untested on
 * silicon), 1 on any card that has one, 0 never. */
static int tlut_mode;
static uint32_t lut_off;
static int lut_ok;
static const dgl_palette *lut_pal;
static unsigned lut_gen;
static GLenum env_mode_u[2] = { GL_MODULATE, GL_MODULATE };
static GLfloat env_color_u[2][4];       /* GL_TEXTURE_ENV_COLOR */
#define env_mode (env_mode_u[dgl_gl.active_unit])
#define env_color (env_color_u[dgl_gl.active_unit])

uint32_t dgl_sync_epoch;
dgl_tex_counts dgl_texc;

GLenum dgl_tex_env_mode(int unit) { return env_mode_u[unit]; }
const GLfloat *dgl_tex_env_color(int unit) { return env_color_u[unit]; }

int dgl_sync(void)
{
    if (engine_sync(500000) != 0)
        return -1;
    dgl_sync_epoch++;
    dgl_vram_sync_done();
    return 0;
}
GLuint dgl_bound_name(void) { return bound; }

static dgl_texture *get(GLuint name, int create)
{
    dgl_texture *t;
    if (name >= MAX_TEXTURES)
        return NULL;
    t = tex[name];
    if (!t && create) {
        t = (dgl_texture *)calloc(1, sizeof *t);
        if (!t)
            return NULL;
        t->name = name;
        t->min_filter = GL_NEAREST_MIPMAP_LINEAR;         /* GL defaults */
        t->mag_filter = GL_LINEAR;
        t->wrap_s = t->wrap_t = GL_REPEAT;
        t->max_level = 1000;
        tex[name] = t;
    }
    return t;
}

static void release_vram(dgl_texture *t)
{
    if (!t->resident)
        return;
    if (!dgl_texture_busy(t))
        dgl_vram_free(t->vram_off);
    else if (dgl_vram_retire(t->vram_off) != 0) {   /* retire list full */
        dgl_texc.syncs++;
        dgl_sync();
        dgl_vram_free(t->vram_off);
    }
    t->resident = 0;
}

/* A block for t: from the free heap, then after a sync (retired blocks come
 * back, busy textures become evictable), then by evicting the least
 * recently drawn textures that are not busy. */
static int alloc_vram(const dgl_texture *self, uint32_t size, uint32_t *off)
{
    int synced = 0;
    for (;;) {
        dgl_texture *lru = NULL;
        GLuint i;
        if (dgl_vram_alloc(size, off) == 0)
            return 0;
        if (!synced && dgl_vram_retired()) {
            synced = 1;
            dgl_texc.syncs++;
            if (dgl_sync() == 0)
                continue;
        }
        for (i = 0; i < MAX_TEXTURES; i++) {
            dgl_texture *c = tex[i];
            if (c && c != self && c->resident && !dgl_texture_busy(c) && (!lru || c->drawn < lru->drawn))
                lru = c;
        }
        if (!lru) {
            if (synced)
                return -1;
            synced = 1;
            dgl_texc.syncs++;
            if (dgl_sync() != 0)
                return -1;
            continue;
        }
        release_vram(lru);
        lru->dirty = 1;
        dgl_texc.evictions++;
    }
}

static void destroy(GLuint name)
{
    dgl_texture *t = tex[name];
    int l;
    if (!t)
        return;
    release_vram(t);
    for (l = 0; l < DGL_MAX_LEVELS; l++) {
        free(t->level[l].rgba);
        free(t->level[l].idx);
    }
    free(t->own);
    free(t);
    tex[name] = NULL;
}

void dgl_textures_reset(uint32_t heap_start, uint32_t heap_end)
{
    GLuint i;
    for (i = 0; i < MAX_TEXTURES; i++)
        destroy(i);
    bound_u[0] = bound_u[1] = 0;
    dgl_palettes_reset();
    env_mode_u[0] = env_mode_u[1] = GL_MODULATE;
    memset(env_color_u, 0, sizeof env_color_u);
    dgl_vram_init(heap_start, heap_end);
    white_ok = dgl_vram_alloc(8 * 8 * 2, &white_off) == 0;
    lut_ok = dgl_vram_alloc(256 * 2, &lut_off) == 0;
    lut_pal = NULL;
    {
        const char *e = getenv("DGL_TLUT");
        tlut_mode = !mga.has_tlut ? 0 : e && *e ? (*e != '0') : !mga.has_dual_tex;
    }
    if (white_ok) {
        volatile uint16_t *p = (volatile uint16_t *)(mga_fb + white_off);
        for (i = 0; i < 64; i++)
            p[i] = 0xFFFF;
    }
}

int dgl_white_texture(uint32_t *off)
{
    *off = white_off;
    return white_ok ? 0 : -1;
}

dgl_texture *dgl_unit_texture(int unit)
{
    dgl_texture *t = get(bound_u[unit], 0);
    return t && DGL_LEVEL_DEFINED(&t->level[0]) ? t : NULL;
}

dgl_palette *dgl_bound_palette(void)
{
    dgl_texture *t = get(bound, 1);
    if (t && !t->own)
        t->own = (dgl_palette *)calloc(1, sizeof *t->own);
    return t ? t->own : NULL;
}

/* ---- Names -------------------------------------------------------------- */
void APIENTRY glGenTextures(GLsizei n, GLuint *names)
{
    GLuint next = 1;
    GLsizei i;
    if (n < 0) { dgl_gl_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) {
        while (next < MAX_TEXTURES && tex[next])
            next++;
        if (next == MAX_TEXTURES || !get(next, 1)) {
            dgl_gl_error(GL_OUT_OF_MEMORY);
            names[i] = 0;
            continue;
        }
        names[i] = next++;
    }
}

void APIENTRY glBindTexture(GLenum target, GLuint name)
{
    if (target != GL_TEXTURE_2D) { dgl_gl_error(GL_INVALID_ENUM); return; }
    if (name && !get(name, 1)) { dgl_gl_error(GL_OUT_OF_MEMORY); return; }
    bound = name;
    dgl_gl.dirty |= DGL_DIRTY_TEXTURE;
}

void APIENTRY glDeleteTextures(GLsizei n, const GLuint *names)
{
    GLsizei i;
    if (n < 0) { dgl_gl_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) {
        if (!names[i] || names[i] >= MAX_TEXTURES)
            continue;
        if (names[i] == bound_u[0] || names[i] == bound_u[1]) {
            if (names[i] == bound_u[0]) bound_u[0] = 0;
            if (names[i] == bound_u[1]) bound_u[1] = 0;
            dgl_gl.dirty |= DGL_DIRTY_TEXTURE;
        }
        destroy(names[i]);
    }
}

GLboolean APIENTRY glIsTexture(GLuint name)
{
    return (GLboolean)(name && name < MAX_TEXTURES && tex[name] != NULL);
}

/* ---- Parameters and environment ---------------------------------------- */
static void param(GLenum target, GLenum pname, GLint v)
{
    dgl_texture *t;
    if (target != GL_TEXTURE_2D) { dgl_gl_error(GL_INVALID_ENUM); return; }
    t = get(bound, 1);
    if (!t) { dgl_gl_error(GL_OUT_OF_MEMORY); return; }
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
        if (v != GL_NEAREST && v != GL_LINEAR && (v < GL_NEAREST_MIPMAP_NEAREST || v > GL_LINEAR_MIPMAP_LINEAR)) {
            dgl_gl_error(GL_INVALID_ENUM);
            return;
        }
        t->min_filter = (GLenum)v;
        t->dirty = 1;                   /* the mip window may change */
        break;
    case GL_TEXTURE_MAG_FILTER:
        if (v != GL_NEAREST && v != GL_LINEAR) { dgl_gl_error(GL_INVALID_ENUM); return; }
        t->mag_filter = (GLenum)v;
        break;
    case GL_TEXTURE_WRAP_S: case GL_TEXTURE_WRAP_T:
        if (v != GL_REPEAT && v != GL_CLAMP && v != GL_CLAMP_TO_EDGE) { dgl_gl_error(GL_INVALID_ENUM); return; }
        if (pname == GL_TEXTURE_WRAP_S) t->wrap_s = (GLenum)v; else t->wrap_t = (GLenum)v;
        t->dirty = 1;                   /* small levels are widened differently */
        break;
    case GL_TEXTURE_MAX_LEVEL:
        if (v < 0) { dgl_gl_error(GL_INVALID_VALUE); return; }
        t->max_level = v;
        t->dirty = 1;
        break;
    case GL_TEXTURE_PRIORITY: case GL_TEXTURE_MAX_ANISOTROPY_EXT:
        return;                         /* accepted, no effect */
    default:
        dgl_gl_error(GL_INVALID_ENUM);
        return;
    }
    dgl_gl.dirty |= DGL_DIRTY_TEXTURE;
}

void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint v) { param(target, pname, v); }
void APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat v) { param(target, pname, (GLint)v); }

void APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *v)
{
    if (pname == GL_TEXTURE_BORDER_COLOR) {         /* borders are not drawn (PRD §8.3) */
        if (target != GL_TEXTURE_2D) dgl_gl_error(GL_INVALID_ENUM);
        return;
    }
    param(target, pname, (GLint)v[0]);
}

void APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *v)
{
    if (pname == GL_TEXTURE_BORDER_COLOR) {
        if (target != GL_TEXTURE_2D) dgl_gl_error(GL_INVALID_ENUM);
        return;
    }
    param(target, pname, v[0]);
}

static int get_param(GLenum target, GLenum pname, GLfloat *v)
{
    dgl_texture *t = get(bound, 1);
    if (target != GL_TEXTURE_2D || !t) { dgl_gl_error(GL_INVALID_ENUM); return 0; }
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER: v[0] = (GLfloat)t->min_filter; return 1;
    case GL_TEXTURE_MAG_FILTER: v[0] = (GLfloat)t->mag_filter; return 1;
    case GL_TEXTURE_WRAP_S: v[0] = (GLfloat)t->wrap_s; return 1;
    case GL_TEXTURE_WRAP_T: v[0] = (GLfloat)t->wrap_t; return 1;
    case GL_TEXTURE_MAX_LEVEL: v[0] = (GLfloat)t->max_level; return 1;
    case GL_TEXTURE_PRIORITY: v[0] = 1.0f; return 1;
    case GL_TEXTURE_RESIDENT: v[0] = (GLfloat)t->resident; return 1;
    case GL_TEXTURE_BORDER_COLOR: v[0] = v[1] = v[2] = v[3] = 0; return 4;
    default: dgl_gl_error(GL_INVALID_ENUM); return 0;
    }
}

void APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *out)
{
    GLfloat v[4];
    int n = get_param(target, pname, v), i;
    for (i = 0; i < n; i++)
        out[i] = v[i];
}

void APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint *out)
{
    GLfloat v[4];
    int n = get_param(target, pname, v), i;
    for (i = 0; i < n; i++)
        out[i] = (GLint)v[i];
}

/* Level parameters of the bound texture, as uploaded (the internal format
 * is the base format DOS-GL stores). */
void APIENTRY glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *out)
{
    dgl_texture *t = get(bound, 0);
    const dgl_level *L;
    if (target != GL_TEXTURE_2D || level < 0 || level >= DGL_MAX_LEVELS) { dgl_gl_error(GL_INVALID_VALUE); return; }
    L = t ? &t->level[level] : NULL;
    switch (pname) {
    case GL_TEXTURE_WIDTH: out[0] = L ? L->w : 0; break;
    case GL_TEXTURE_HEIGHT: out[0] = L ? L->h : 0; break;
    case GL_TEXTURE_BORDER: out[0] = 0; break;
    case GL_TEXTURE_INTERNAL_FORMAT: out[0] = L && DGL_LEVEL_DEFINED(L) ? L->ifmt : 1; break;
    case GL_TEXTURE_INDEX_SIZE_EXT: out[0] = L && L->idx ? 8 : 0; break;
    case GL_TEXTURE_RED_SIZE: case GL_TEXTURE_BLUE_SIZE:
        out[0] = !t ? 0 : t->hwfmt == DGL_TW12 ? 4 : 5; break;
    case GL_TEXTURE_GREEN_SIZE: out[0] = !t ? 0 : t->hwfmt == DGL_TW12 ? 4 : t->hwfmt == DGL_TW15 ? 5 : 6; break;
    case GL_TEXTURE_ALPHA_SIZE: out[0] = !t ? 0 : t->hwfmt == DGL_TW12 ? 4 : t->hwfmt == DGL_TW15 ? 1 : 0; break;
    case GL_TEXTURE_LUMINANCE_SIZE: case GL_TEXTURE_INTENSITY_SIZE: out[0] = 0; break;
    default: dgl_gl_error(GL_INVALID_ENUM);
    }
}

void APIENTRY glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *out)
{
    GLint v = 0;
    glGetTexLevelParameteriv(target, level, pname, &v);
    out[0] = (GLfloat)v;
}

/* ---- Texture environment ------------------------------------------------- */
static void env(GLenum target, GLenum pname, const GLfloat *v)
{
    GLint mode;
    int i;
    if (target != GL_TEXTURE_ENV) { dgl_gl_error(GL_INVALID_ENUM); return; }
    switch (pname) {
    case GL_TEXTURE_ENV_MODE:
        mode = (GLint)v[0];
        /* GL_BLEND is drawn by the G400's combiner (with a black environment
         * colour; otherwise as GL_MODULATE, logged); GL_ADD is not GL 1.1. */
        if (mode != GL_MODULATE && mode != GL_REPLACE && mode != GL_DECAL && mode != GL_BLEND) {
            dgl_gl_error(GL_INVALID_ENUM);
            return;
        }
        env_mode = (GLenum)mode;
        break;
    case GL_TEXTURE_ENV_COLOR:
        for (i = 0; i < 4; i++)
            env_color[i] = v[i] < 0 ? 0 : v[i] > 1 ? 1 : v[i];
        break;
    default:
        dgl_gl_error(GL_INVALID_ENUM);
        return;
    }
    dgl_gl.dirty |= DGL_DIRTY_TEXTURE | DGL_DIRTY_RASTER;
}

void APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint p)
{
    GLfloat f = (GLfloat)p;
    if (pname == GL_TEXTURE_ENV_COLOR) { dgl_gl_error(GL_INVALID_ENUM); return; }
    env(target, pname, &f);
}

void APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat p)
{
    if (pname == GL_TEXTURE_ENV_COLOR) { dgl_gl_error(GL_INVALID_ENUM); return; }
    env(target, pname, &p);
}

void APIENTRY glTexEnvfv(GLenum target, GLenum pname, const GLfloat *v) { env(target, pname, v); }

void APIENTRY glTexEnviv(GLenum target, GLenum pname, const GLint *v)
{
    GLfloat f[4];
    int i;
    if (pname == GL_TEXTURE_ENV_COLOR)
        for (i = 0; i < 4; i++)          /* integer colours map [0, INT_MAX] to [0, 1] */
            f[i] = (GLfloat)((double)v[i] / 2147483647.0);
    else
        f[0] = (GLfloat)v[0];
    env(target, pname, f);
}

void APIENTRY glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *v)
{
    int i;
    if (target != GL_TEXTURE_ENV) { dgl_gl_error(GL_INVALID_ENUM); return; }
    if (pname == GL_TEXTURE_ENV_MODE) v[0] = (GLfloat)env_mode;
    else if (pname == GL_TEXTURE_ENV_COLOR) for (i = 0; i < 4; i++) v[i] = env_color[i];
    else dgl_gl_error(GL_INVALID_ENUM);
}

void APIENTRY glGetTexEnviv(GLenum target, GLenum pname, GLint *v)
{
    int i;
    if (target != GL_TEXTURE_ENV) { dgl_gl_error(GL_INVALID_ENUM); return; }
    if (pname == GL_TEXTURE_ENV_MODE) v[0] = (GLint)env_mode;
    else if (pname == GL_TEXTURE_ENV_COLOR) for (i = 0; i < 4; i++) v[i] = (GLint)(env_color[i] * 2147483647.0);
    else dgl_gl_error(GL_INVALID_ENUM);
}

/* ---- Uploads ------------------------------------------------------------- */
static int is_pow2(int v) { return v > 0 && !(v & (v - 1)); }

static int log2i(int v) { int l = 0; while ((1 << l) < v) l++; return l; }

static int check_upload(GLenum target, GLint level, GLenum format, GLenum type)
{
    if (target != GL_TEXTURE_2D || type != GL_UNSIGNED_BYTE || !dgl_format_bytes(format)) {
        dgl_gl_error(GL_INVALID_ENUM);
        return 0;
    }
    if (level < 0 || level >= DGL_MAX_LEVELS) {
        dgl_gl_error(GL_INVALID_VALUE);
        return 0;
    }
    return 1;
}

/* Copy client rows (with GL_UNPACK_ALIGNMENT) into RGBA8. */
static void unpack(GLenum format, const void *pixels, int w, int h, unsigned char *dst, int dst_stride, int ifc)
{
    int bytes = dgl_format_bytes(format), y;
    int stride = (w * bytes + dgl_gl.unpack_align - 1) / dgl_gl.unpack_align * dgl_gl.unpack_align;
    for (y = 0; y < h; y++) {
        unsigned char *d = dst + (size_t)y * dst_stride;
        dgl_to_rgba(format, (const unsigned char *)pixels + (size_t)y * stride, w, d);
        dgl_apply_ifmt(ifc, d, w);
    }
}

/* Copy client rows of 8-bit colour indices. */
static void unpack_idx(const void *pixels, int w, int h, unsigned char *dst, int dst_stride)
{
    int stride = (w + dgl_gl.unpack_align - 1) / dgl_gl.unpack_align * dgl_gl.unpack_align, y;
    for (y = 0; y < h; y++)
        memcpy(dst + (size_t)y * dst_stride, (const unsigned char *)pixels + (size_t)y * stride, (size_t)w);
}

/* Texel i of a level as RGBA8: its shadow, or its index looked up in pal. */
static const unsigned char *texel(const dgl_level *L, const dgl_palette *pal, size_t i)
{
    return L->idx ? dgl_palette_texel(pal, L->idx[i]) : L->rgba + i * 4;
}

/* The alpha class of n texels of a level from texel i. */
static int texels_class(const dgl_level *L, const dgl_palette *pal, size_t i, long n)
{
    int cls = DGL_ALPHA_OPAQUE;
    long k;
    if (!L->idx)
        return dgl_alpha_class(L->rgba + i * 4, n);
    for (k = 0; k < n; k++) {
        unsigned a = dgl_palette_texel(pal, L->idx[i + (size_t)k])[3];
        if (a == 255)
            continue;
        if (a != 0)
            return DGL_ALPHA_GRADIENT;
        cls = DGL_ALPHA_BINARY;
    }
    return cls;
}

void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei w, GLsizei h,
                           GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    dgl_texture *t;
    dgl_level *L;
    int max = mga.max_tex_size ? mga.max_tex_size : 1024, ifc = dgl_ifmt_class(internalformat);
    if (!check_upload(target, level, format, type))
        return;
    if (ifc < 0 || border != 0 || !is_pow2(w) || !is_pow2(h) || w > max || h > max) {
        dgl_gl_error(GL_INVALID_VALUE);
        return;
    }
    if ((ifc == DGL_IF_INDEX) != (format == GL_COLOR_INDEX)) {
        dgl_gl_error(GL_INVALID_OPERATION);          /* indices only into COLOR_INDEX textures (no pixel maps) */
        return;
    }
    t = get(bound, 1);
    if (!t) { dgl_gl_error(GL_OUT_OF_MEMORY); return; }
    L = &t->level[level];
    free(L->rgba);
    free(L->idx);
    L->rgba = L->idx = NULL;
    if (ifc == DGL_IF_INDEX)
        L->idx = (unsigned char *)calloc((size_t)w * h, 1);
    else
        L->rgba = (unsigned char *)calloc((size_t)w * h, 4);
    if (!DGL_LEVEL_DEFINED(L)) { L->w = L->h = 0; dgl_gl_error(GL_OUT_OF_MEMORY); return; }
    L->w = w; L->h = h;
    L->ifc = ifc;
    L->ifmt = internalformat;
    if (L->idx) {
        if (pixels)
            unpack_idx(pixels, w, h, L->idx, w);
    } else if (pixels) {
        unpack(format, pixels, w, h, L->rgba, w * 4, ifc);
    } else {
        dgl_apply_ifmt(ifc, L->rgba, (long)w * h);   /* undefined texels: at least the right class */
    }
    t->dirty = 1;
    dgl_gl.dirty |= DGL_DIRTY_TEXTURE;
}

static uint16_t pack(const dgl_texture *t, int hwfmt, const unsigned char *p)
{
    return t->grey && hwfmt == DGL_TW16 ? dgl_pack_grey565(p[0]) : dgl_pack_texel(hwfmt, p[0], p[1], p[2], p[3]);
}

/* Write a changed rectangle straight into the hardware copy: only when the
 * copy is current, not busy, holds this level at its own size (not widened)
 * and its format can represent the new texels' alpha. Returns 1 if done. */
static int sub_in_place(dgl_texture *t, int level, int x, int y, int w, int h)
{
    static const int fits[] = { [DGL_TW16] = DGL_ALPHA_OPAQUE, [DGL_TW15] = DGL_ALPHA_BINARY,
                                [DGL_TW12] = DGL_ALPHA_GRADIENT };
    const dgl_level *L = &t->level[level];
    const dgl_palette *pal = L->idx ? dgl_palette_for(t) : NULL;
    int hw_w, j, i, cls = DGL_ALPHA_OPAQUE;
    if (!t->resident || t->dirty || level >= t->hw_levels ||
        t->level[0].w < 8 || t->level[0].h < 8 || (pal && (t->pal_used != pal || t->pal_gen != pal->gen)))
        return 0;
    if (dgl_texture_busy(t)) {
        /* Queued draws may still read it. A small rectangle is cheaper to
         * write after waiting for the engine than the whole texture is to
         * re-upload elsewhere (GLQuake's multitexture path updates a
         * lightmap page between the surfaces that use it). Writing the
         * rectangle through the engine (ILOAD) would avoid both: plan Q6. */
        if ((long)w * h * 4 > (long)L->w * L->h)
            return 0;
        if (dgl_sync() != 0)
            return 0;
        dgl_texc.sub_sync++;
    }
    for (j = 0; j < h && cls <= fits[t->hwfmt]; j++) {
        int c = texels_class(L, pal, (size_t)(y + j) * L->w + x, w);
        if (c > cls)
            cls = c;
    }
    if (cls > fits[t->hwfmt])
        return 0;
    hw_w = L->w;                                  /* level >= 8x8: stored at its own size */
    if (t->hwfmt == DGL_TW8) {
        for (j = 0; j < h; j++) {
            volatile uint8_t *d8 = mga_fb + t->level_off[level] + (size_t)(y + j) * hw_w + x;
            for (i = 0; i < w; i++)
                d8[i] = L->idx[(size_t)(y + j) * L->w + x + i];
        }
        dgl_texc.sub_fast++;
        return 1;
    }
    for (j = 0; j < h; j++) {
        volatile uint16_t *dst = (volatile uint16_t *)(mga_fb + t->level_off[level]) + (size_t)(y + j) * hw_w + x;
        size_t row = (size_t)(y + j) * L->w + x;
        for (i = 0; i < w; i++)
            dst[i] = pack(t, t->hwfmt, texel(L, pal, row + (size_t)i));
    }
    dgl_texc.sub_fast++;
    return 1;
}

void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format,
                              GLenum type, const GLvoid *pixels)
{
    dgl_texture *t;
    dgl_level *L;
    if (!check_upload(target, level, format, type))
        return;
    t = get(bound, 0);
    L = t ? &t->level[level] : NULL;
    if (!L || !DGL_LEVEL_DEFINED(L) || (L->idx != NULL) != (format == GL_COLOR_INDEX)) {
        dgl_gl_error(GL_INVALID_OPERATION);
        return;
    }
    if (x < 0 || y < 0 || w < 0 || h < 0 || x + w > L->w || y + h > L->h) {
        dgl_gl_error(GL_INVALID_VALUE);
        return;
    }
    if (L->idx)
        unpack_idx(pixels, w, h, L->idx + (size_t)y * L->w + x, L->w);
    else
        unpack(format, pixels, w, h, L->rgba + ((size_t)y * L->w + x) * 4, L->w * 4, L->ifc);
    if (w && h && !sub_in_place(t, level, x, y, w, h)) {
        dgl_texc.sub_full++;
        t->dirty = 1;
        dgl_gl.dirty |= DGL_DIRTY_TEXTURE;
    }
}

/* ---- Residency ------------------------------------------------------------ */
static int mip_filter(GLenum f) { return f >= GL_NEAREST_MIPMAP_NEAREST && f <= GL_LINEAR_MIPMAP_LINEAR; }

/* Write one level into VRAM at off as hw_w x hw_h texels (>= the level). */
static void write_level(const dgl_texture *t, int l, int hwfmt, uint32_t off, int hw_w, int hw_h,
                        const dgl_palette *pal)
{
    const dgl_level *L = &t->level[l];
    volatile uint16_t *dst = (volatile uint16_t *)(mga_fb + off);
    volatile uint8_t *dst8 = mga_fb + off;
    int x, y;
    for (y = 0; y < hw_h; y++)
        for (x = 0; x < hw_w; x++) {
            int sx = x, sy = y;
            if (sx >= L->w) sx = t->wrap_s != GL_REPEAT ? L->w - 1 : sx % L->w;
            if (sy >= L->h) sy = t->wrap_t != GL_REPEAT ? L->h - 1 : sy % L->h;
            if (hwfmt == DGL_TW8)
                dst8[y * hw_w + x] = L->idx[(size_t)sy * L->w + sx];
            else
                dst[y * hw_w + x] = pack(t, hwfmt, texel(L, pal, (size_t)sy * L->w + sx));
        }
}

/* Can t (colour indices) be stored as TW8 and read through the LUT? One
 * LUT holds one palette: the shared one, and only while it is opaque. */
static int tlut_capable(const dgl_palette *pal)
{
    return tlut_mode && lut_ok && dgl_gl.shared_palette && pal && pal->width && pal->opaque;
}

void dgl_texture_lut(const dgl_texture *t)
{
    const dgl_palette *pal = t->pal_used;
    volatile uint16_t *dst = (volatile uint16_t *)(mga_fb + lut_off);
    int i;
    if (t->hwfmt != DGL_TW8 || !pal || (pal == lut_pal && pal->gen == lut_gen))
        return;
    dgl_sync();                                      /* the last load may still be reading the block */
    for (i = 0; i < 256; i++) {
        const unsigned char *p = dgl_palette_texel(pal, (unsigned)i);
        dst[i] = dgl_pack_texel(DGL_TW16, p[0], p[1], p[2], 255);
    }
    engine_tlut_load(lut_off, 0, 256);
    lut_pal = pal;
    lut_gen = pal->gen;
    dgl_texc.lut_loads++;
}

int dgl_texture_ready(dgl_texture *t)
{
    int levels = 1, l, cls = DGL_ALPHA_OPAQUE, hw_w, hw_h;
    uint32_t size = 0, off;
    const dgl_palette *pal = t->level[0].idx ? dgl_palette_for(t) : NULL;
    int bpt, tw8;
    if (!t->dirty && t->resident &&
        (!pal || (t->pal_used == pal && (t->hwfmt == DGL_TW8 ? tlut_capable(pal) : t->pal_gen == pal->gen))))
        return 0;                                    /* a TW8 texture follows its palette through the LUT */
    /* The levels that go to the hardware: level 0, then while the chain halves
     * correctly and stays >= 8x8, up to the window and GL_TEXTURE_MAX_LEVEL. */
    if (mip_filter(t->min_filter) && mga.max_mip_levels > 1) {
        int limit = mga.max_mip_levels < WINDOW_LEVELS ? mga.max_mip_levels : WINDOW_LEVELS;
        if (limit > t->max_level + 1)
            limit = t->max_level + 1;
        while (levels < limit && DGL_LEVEL_DEFINED(&t->level[levels]) &&
               (t->level[levels].idx != NULL) == (pal != NULL) && t->level[levels].w == t->level[levels - 1].w / 2 &&
               t->level[levels].h == t->level[levels - 1].h / 2 && t->level[levels].w >= 8 &&
               t->level[levels].h >= 8)
            levels++;
    }
    for (l = 0; l < levels; l++) {
        int c = texels_class(&t->level[l], pal, 0, (long)t->level[l].w * t->level[l].h);
        if (c > cls)
            cls = c;
    }
    if (cls == DGL_ALPHA_GRADIENT && t->level[0].ifc == DGL_IF_RGB5_A1)
        cls = DGL_ALPHA_BINARY;                      /* one alpha bit, as asked */
    t->grey = t->level[0].ifc == DGL_IF_LUMINANCE || t->level[0].ifc == DGL_IF_INTENSITY ||
              t->level[0].ifc == DGL_IF_LUMINANCE_ALPHA;
    hw_w = t->level[0].w < 8 ? 8 : t->level[0].w;
    hw_h = t->level[0].h < 8 ? 8 : t->level[0].h;
    tw8 = pal && tlut_capable(pal);                  /* one byte a texel */
    bpt = tw8 ? 1 : 2;
    for (l = 0; l < levels; l++)
        size += (((uint32_t)(hw_w >> l) * (uint32_t)(hw_h >> l) * (uint32_t)bpt) + 31u) & ~31u;
    if (t->resident && (t->vram_size != size || dgl_texture_busy(t))) {
        if (dgl_texture_busy(t))
            dgl_texc.renames++;                      /* queued draws keep reading the old block */
        release_vram(t);
    }
    if (!t->resident) {
        if (alloc_vram(t, size, &off) != 0) {
            static int logged;
            if (logged < 4 && ++logged)
                DGL_WARN("DGL-TEXOOM tex=%u %dx%d levels=%d bytes=%lu heap_used=%lu largest_free=%lu blocks=%d",
                         t->name, t->level[0].w, t->level[0].h, levels, (unsigned long)size,
                         (unsigned long)dgl_vram_used(), (unsigned long)dgl_vram_largest_free(), dgl_vram_blocks());
            dgl_gl_error(GL_OUT_OF_MEMORY);          /* the draw is skipped (PRD §8.3) */
            return -1;
        }
        t->vram_off = off;
        t->vram_size = size;
        t->resident = 1;
    }
    dgl_texc.uploads++;
    dgl_texc.upload_bytes += size;
    t->hwfmt = tw8 ? DGL_TW8 : dgl_hwfmt_for_class(cls);
    t->hw_levels = levels;
    t->hw_w_log2 = log2i(hw_w);
    t->hw_h_log2 = log2i(hw_h);
    off = t->vram_off;
    for (l = 0; l < levels; l++) {
        t->level_off[l] = off;
        write_level(t, l, t->hwfmt, off, hw_w >> l, hw_h >> l, pal);
        off += (((uint32_t)(hw_w >> l) * (uint32_t)(hw_h >> l) * (uint32_t)bpt) + 31u) & ~31u;
    }
    t->pal_used = pal;
    t->pal_gen = pal ? pal->gen : 0;
    t->dirty = 0;
    dgl_gl.dirty |= DGL_DIRTY_TEXTURE;               /* new VRAM offsets or format for the emitter */
    return 0;
}

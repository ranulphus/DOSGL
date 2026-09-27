/* gl_tex.h - textures: conversion, the VRAM heap and texture objects. */
#ifndef DGL_GL_TEX_H
#define DGL_GL_TEX_H
#include "gl_state.h"
#include <stdint.h>

/* Hardware texel formats (TEXCTL.texformat codes). */
enum { DGL_TW15 = 2, DGL_TW16 = 3, DGL_TW12 = 4 };
enum { DGL_ALPHA_OPAQUE, DGL_ALPHA_BINARY, DGL_ALPHA_GRADIENT };

/* texconv.c */
int      dgl_alpha_class(const unsigned char *rgba, long n);
int      dgl_hwfmt_for_class(int cls);
uint16_t dgl_pack_texel(int hwfmt, unsigned r, unsigned g, unsigned b, unsigned a);
void     dgl_convert_rgba(int hwfmt, const unsigned char *rgba, long n, uint16_t *out);
int      dgl_to_rgba(GLenum format, const unsigned char *src, long n, unsigned char *dst);
int      dgl_format_bytes(GLenum format);

/* vram.c */
void     dgl_vram_init(uint32_t start, uint32_t end);
int      dgl_vram_alloc(uint32_t size, uint32_t *off);     /* 0 ok, -1 out of memory */
void     dgl_vram_free(uint32_t off);
uint32_t dgl_vram_used(void);
int      dgl_vram_blocks(void);

/* texture.c */
#define DGL_MAX_LEVELS 12

typedef struct {
    int      w, h;                 /* as uploaded */
    unsigned char *rgba;           /* shadow copy, RGBA8 */
} dgl_level;

typedef struct dgl_texture {
    GLuint   name;
    int      used;
    dgl_level level[DGL_MAX_LEVELS];
    GLenum   min_filter, mag_filter, wrap_s, wrap_t;
    int      max_level;
    /* Hardware copy: every defined level from 0, in one format. */
    int      resident, dirty, hwfmt, hw_levels;
    uint32_t vram_off, level_off[DGL_MAX_LEVELS], vram_size;
    int      hw_w_log2, hw_h_log2;  /* level 0 as stored (>= 8 texels) */
} dgl_texture;

dgl_texture *dgl_bound_texture(void);           /* NULL when none or texture 0 */
int  dgl_texture_ready(dgl_texture *t);         /* upload if needed; 0 = drawable */
void dgl_textures_reset(uint32_t heap_start, uint32_t heap_end);
int  dgl_white_texture(uint32_t *off);          /* resident 8x8 white TW16 */

#endif

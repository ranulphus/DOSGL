/* gl_tex.h - textures: conversion, the VRAM heap and texture objects. */
#ifndef DGL_GL_TEX_H
#define DGL_GL_TEX_H
#include "gl_state.h"
#include <stdint.h>

/* Hardware texel formats (TEXCTL.texformat codes). */
enum { DGL_TW15 = 2, DGL_TW16 = 3, DGL_TW12 = 4 };
enum { DGL_ALPHA_OPAQUE, DGL_ALPHA_BINARY, DGL_ALPHA_GRADIENT };
/* Internal format classes (what the texture keeps of its texels). */
enum { DGL_IF_RGBA, DGL_IF_RGB, DGL_IF_RGB5_A1, DGL_IF_LUMINANCE, DGL_IF_LUMINANCE_ALPHA, DGL_IF_ALPHA,
       DGL_IF_INTENSITY };

/* texconv.c */
int      dgl_alpha_class(const unsigned char *rgba, long n);
int      dgl_hwfmt_for_class(int cls);
uint16_t dgl_pack_texel(int hwfmt, unsigned r, unsigned g, unsigned b, unsigned a);
void     dgl_convert_rgba(int hwfmt, const unsigned char *rgba, long n, uint16_t *out);
int      dgl_to_rgba(GLenum format, const unsigned char *src, long n, unsigned char *dst);
int      dgl_ifmt_class(GLint internalformat);                 /* DGL_IF_*, -1 if not a GL 1.1 format */
void     dgl_apply_ifmt(int ifc, unsigned char *rgba, long n);  /* keep what the class keeps */
uint16_t dgl_pack_grey565(unsigned l);                          /* RGB565 grey, green from red */
int      dgl_format_bytes(GLenum format);

/* vram.c */
void     dgl_vram_init(uint32_t start, uint32_t end);
int      dgl_vram_alloc(uint32_t size, uint32_t *off);     /* 0 ok, -1 out of memory */
void     dgl_vram_free(uint32_t off);
uint32_t dgl_vram_used(void);
int      dgl_vram_blocks(void);
int      dgl_vram_retire(uint32_t off);   /* free after the next sync; -1 = list full, sync first */
int      dgl_vram_retired(void);           /* blocks waiting for a sync */
void     dgl_vram_sync_done(void);         /* the engine is idle: free the retired blocks */

/* texture.c */
#define DGL_MAX_LEVELS 12

typedef struct {
    int      w, h;                 /* as uploaded */
    int      ifc;                  /* internal format class, DGL_IF_* */
    unsigned char *rgba;           /* shadow copy, RGBA8, already reduced to the class */
} dgl_level;

typedef struct dgl_texture {
    GLuint   name;
    int      used;
    dgl_level level[DGL_MAX_LEVELS];
    GLenum   min_filter, mag_filter, wrap_s, wrap_t;
    int      max_level;
    /* Hardware copy: every defined level from 0, in one format. */
    int      resident, dirty, hwfmt, hw_levels;
    int      grey;                  /* luminance or intensity: RGB565 greys stay grey */
    uint32_t vram_off, level_off[DGL_MAX_LEVELS], vram_size;
    int      hw_w_log2, hw_h_log2;  /* level 0 as stored (>= 8 texels) */
    uint32_t drawn;                 /* dgl_sync_epoch + 1 when last drawn, 0 = never */
} dgl_texture;

/* Engine syncs (texture.c): dgl_sync waits for the engine to go idle and
 * counts completed syncs in dgl_sync_epoch. A texture drawn since the last
 * completed sync is busy: queued draws may still read its VRAM copy. */
extern uint32_t dgl_sync_epoch;
int  dgl_sync(void);                            /* 0 = idle, -1 = timed out */
#define dgl_texture_busy(t) ((t)->drawn == dgl_sync_epoch + 1)
#define dgl_texture_drawn(t) ((t)->drawn = dgl_sync_epoch + 1)

/* Texture traffic, printed and reset by DGL_STATS=2 (DGL-TEX). */
typedef struct {
    unsigned long uploads, upload_bytes;    /* whole textures written to VRAM */
    unsigned long sub_fast, sub_full;       /* glTexSubImage2D: rectangle written / whole re-upload */
    unsigned long renames, evictions, syncs;
} dgl_tex_counts;
extern dgl_tex_counts dgl_texc;

dgl_texture *dgl_bound_texture(void);           /* NULL when none or texture 0 */
int  dgl_texture_ready(dgl_texture *t);         /* upload if needed; 0 = drawable */
void dgl_textures_reset(uint32_t heap_start, uint32_t heap_end);
int  dgl_white_texture(uint32_t *off);          /* resident 8x8 white TW16 */

#endif

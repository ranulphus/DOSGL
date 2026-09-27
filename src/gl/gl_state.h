/* gl_state.h - the GL state DOS-GL tracks (PRD §5.3, FR-ST-1..3). */
#ifndef DGL_GL_STATE_H
#define DGL_GL_STATE_H
#include <GL/gl.h>
#include <GL/glext.h>

/* Column-major 4x4, as GL stores matrices. */
typedef struct { GLfloat m[16]; } dgl_mat4;

enum { DGL_MV_DEPTH = 32, DGL_PROJ_DEPTH = 2, DGL_TEX_DEPTH = 2 };

typedef struct {
    dgl_mat4 *stack;
    int depth, max;
} dgl_mstack;

/* Dirty groups validated at draw time (FR-ST-1). */
enum {
    DGL_DIRTY_MVP     = 1u << 0,
    DGL_DIRTY_RASTER  = 1u << 1,     /* depth, blend, alpha test, fog, masks */
    DGL_DIRTY_TARGET  = 1u << 2,     /* scissor */
    DGL_DIRTY_TEXTURE = 1u << 3,
    DGL_DIRTY_FOG     = 1u << 4
};

/* Client-side array (glVertexPointer etc.). */
typedef struct {
    GLint size;
    GLenum type;
    GLsizei stride;
    const void *ptr;
    int enabled;
} dgl_array;

/* A vertex after the modelview-projection transform: clip coordinates plus
 * attributes (colour 0..1, texture coordinates, eye distance for fog). */
typedef struct {
    float x, y, z, w;
    float r, g, b, a;
    float s, t;
    float eye_d;
} dgl_cvtx;

typedef struct {
    /* Matrices */
    GLenum      matrix_mode;
    dgl_mstack  mv, proj, tex;
    dgl_mat4    mv_stack[DGL_MV_DEPTH], proj_stack[DGL_PROJ_DEPTH], tex_stack[DGL_TEX_DEPTH];
    dgl_mat4    mvp;                   /* proj * mv, valid unless DGL_DIRTY_MVP */
    int         tex_identity;          /* texture matrix is the identity */
    /* Enables */
    int depth_test, cull_face, blend, alpha_test, fog, scissor_test, texture_2d, dither;
    /* Raster state */
    GLenum  depth_func, blend_src, blend_dst, alpha_func, cull_mode, front_face, shade_model;
    GLenum  fog_mode, polygon_mode;
    int     depth_mask;
    GLboolean color_mask[4];
    GLfloat alpha_ref;
    GLint   viewport[4], scissor[4];
    GLfloat clear_color[4];
    GLdouble clear_depth, depth_near, depth_far;
    GLfloat fog_density, fog_start, fog_end, fog_color[4];
    GLenum  hint_perspective, hint_fog;
    GLint   pack_align, unpack_align;
    /* Current vertex attributes and arrays */
    GLfloat cur_color[4], cur_tex[2];
    dgl_array va, ca, ta;
    /* Errors */
    GLenum      error;
    unsigned    dirty;
} dgl_gl_state;

extern dgl_gl_state dgl_gl;

void dgl_gl_reset(void);                                  /* GL defaults for a w x h window */
void dgl_gl_set_window(int w, int h);
void dgl_gl_error(GLenum e);                              /* record the first error */

/* matrix.c */
void dgl_mat_identity(dgl_mat4 *r);
void dgl_mat_mul(dgl_mat4 *r, const dgl_mat4 *a, const dgl_mat4 *b);     /* r = a * b */
const dgl_mat4 *dgl_mvp(void);                            /* recomputed when dirty */
void dgl_matrix_reset(void);

/* clip.c: clip a convex polygon (in clip coordinates) against the near and
 * far planes and a guard band of +-g in x and y (NDC units); returns the
 * output vertex count (0 = rejected). out must hold n + 6 vertices. */
int dgl_clip_polygon(const dgl_cvtx *in, int n, dgl_cvtx *out, float gx, float gy);
/* Outcode of one vertex against the same planes (0 = inside all). */
unsigned dgl_outcode(const dgl_cvtx *v, float gx, float gy);

#endif

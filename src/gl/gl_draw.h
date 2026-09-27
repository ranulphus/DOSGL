/* gl_draw.h - the vertex stream between assembly (vertex.c) and its sinks. */
#ifndef DGL_GL_DRAW_H
#define DGL_GL_DRAW_H
#include "gl_state.h"

/* An input vertex: object position (x, y, z, w), colour 0..1, texcoord. */
typedef struct { float pos[4], col[4], tex[2]; } dgl_vin;

typedef struct {
    int  (*begin)(void);        /* validate state; 0 = draw nothing */
    void (*triangle)(const dgl_cvtx *a, const dgl_cvtx *b, const dgl_cvtx *c, const dgl_cvtx *provoking);
    void (*line)(const dgl_cvtx *a, const dgl_cvtx *b);
    void (*point)(const dgl_cvtx *a);
    void (*end)(void);
    /* Display lists (lists.c): return 1 when the call was captured. */
    int  (*record)(GLenum mode, GLsizei count, const GLint *idx, GLint first);
    int  (*record_imm)(GLenum mode, GLsizei count, const dgl_vin *v);
} dgl_sinks;

extern dgl_sinks dgl_sink;

void dgl_transform(const dgl_vin *in, dgl_cvtx *out);
/* Assemble a primitive from the current arrays (imm = NULL; idx or first)
 * or from an immediate-mode buffer (imm). */
void dgl_assemble(GLenum mode, GLsizei count, const GLint *idx, GLint first, const dgl_vin *imm);

#endif

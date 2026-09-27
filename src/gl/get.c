/* get.c - state queries and strings (PRD §6.1: glGetIntegerv, glGetString;
 * §6.2: glGetFloatv, glGetBooleanv). */
#include "gl_state.h"
#include "../dgl/dgl.h"
#include <stdio.h>
#include <string.h>

/* Every query as doubles; the typed entry points convert. Returns the
 * number of values, 0 for an unknown name. */
static int query(GLenum p, double *v)
{
    const dgl_mat4 *m = NULL;
    int i;
    switch (p) {
    case GL_MATRIX_MODE: v[0] = dgl_gl.matrix_mode; return 1;
    case GL_VIEWPORT: for (i = 0; i < 4; i++) v[i] = dgl_gl.viewport[i]; return 4;
    case GL_SCISSOR_BOX: for (i = 0; i < 4; i++) v[i] = dgl_gl.scissor[i]; return 4;
    case GL_MAX_TEXTURE_SIZE: v[0] = mga.max_tex_size ? mga.max_tex_size : 1024; return 1;
    case GL_MAX_MODELVIEW_STACK_DEPTH: v[0] = DGL_MV_DEPTH; return 1;
    case GL_MAX_PROJECTION_STACK_DEPTH: v[0] = DGL_PROJ_DEPTH; return 1;
    case GL_MAX_TEXTURE_STACK_DEPTH: v[0] = DGL_TEX_DEPTH; return 1;
    case GL_MAX_VIEWPORT_DIMS: v[0] = v[1] = 4096; return 2;
    case GL_RED_BITS: case GL_BLUE_BITS: v[0] = 5; return 1;
    case GL_GREEN_BITS: v[0] = 6; return 1;
    case GL_ALPHA_BITS: v[0] = 0; return 1;
    case GL_DEPTH_BITS: v[0] = dgl_ctx.z_off ? 16 : 0; return 1;
    case GL_DEPTH_FUNC: v[0] = dgl_gl.depth_func; return 1;
    case GL_DEPTH_WRITEMASK: v[0] = dgl_gl.depth_mask; return 1;
    case GL_DEPTH_CLEAR_VALUE: v[0] = dgl_gl.clear_depth; return 1;
    case GL_DEPTH_RANGE: v[0] = dgl_gl.depth_near; v[1] = dgl_gl.depth_far; return 2;
    case GL_COLOR_CLEAR_VALUE: for (i = 0; i < 4; i++) v[i] = dgl_gl.clear_color[i]; return 4;
    case GL_COLOR_WRITEMASK: for (i = 0; i < 4; i++) v[i] = dgl_gl.color_mask[i]; return 4;
    case GL_BLEND_SRC: v[0] = dgl_gl.blend_src; return 1;
    case GL_BLEND_DST: v[0] = dgl_gl.blend_dst; return 1;
    case GL_ALPHA_TEST_FUNC: v[0] = dgl_gl.alpha_func; return 1;
    case GL_ALPHA_TEST_REF: v[0] = dgl_gl.alpha_ref; return 1;
    case GL_CULL_FACE_MODE: v[0] = dgl_gl.cull_mode; return 1;
    case GL_FRONT_FACE: v[0] = dgl_gl.front_face; return 1;
    case GL_SHADE_MODEL: v[0] = dgl_gl.shade_model; return 1;
    case GL_FOG_MODE: v[0] = dgl_gl.fog_mode; return 1;
    case GL_FOG_DENSITY: v[0] = dgl_gl.fog_density; return 1;
    case GL_FOG_START: v[0] = dgl_gl.fog_start; return 1;
    case GL_FOG_END: v[0] = dgl_gl.fog_end; return 1;
    case GL_FOG_COLOR: for (i = 0; i < 4; i++) v[i] = dgl_gl.fog_color[i]; return 4;
    case GL_PERSPECTIVE_CORRECTION_HINT: v[0] = dgl_gl.hint_perspective; return 1;
    case GL_FOG_HINT: v[0] = dgl_gl.hint_fog; return 1;
    case GL_PACK_ALIGNMENT: v[0] = dgl_gl.pack_align; return 1;
    case GL_UNPACK_ALIGNMENT: v[0] = dgl_gl.unpack_align; return 1;
    case GL_MODELVIEW_MATRIX: m = &dgl_gl.mv.stack[dgl_gl.mv.depth - 1]; break;
    case GL_PROJECTION_MATRIX: m = &dgl_gl.proj.stack[dgl_gl.proj.depth - 1]; break;
    case GL_TEXTURE_MATRIX: m = &dgl_gl.tex.stack[dgl_gl.tex.depth - 1]; break;
    case GL_CULL_FACE: v[0] = dgl_gl.cull_face; return 1;
    case GL_DEPTH_TEST: v[0] = dgl_gl.depth_test; return 1;
    case GL_BLEND: v[0] = dgl_gl.blend; return 1;
    case GL_ALPHA_TEST: v[0] = dgl_gl.alpha_test; return 1;
    case GL_FOG: v[0] = dgl_gl.fog; return 1;
    case GL_SCISSOR_TEST: v[0] = dgl_gl.scissor_test; return 1;
    case GL_TEXTURE_2D: v[0] = dgl_gl.texture_2d; return 1;
    case GL_TEXTURE_BINDING_2D: { extern GLuint dgl_bound_name(void); v[0] = dgl_bound_name(); return 1; }
    default: return 0;
    }
    for (i = 0; i < 16; i++)
        v[i] = m->m[i];
    return 16;
}

void APIENTRY glGetIntegerv(GLenum p, GLint *out)
{
    double v[16];
    int n = query(p, v), i;
    if (!n) { dgl_gl_error(GL_INVALID_ENUM); return; }
    for (i = 0; i < n; i++)
        out[i] = (p == GL_COLOR_CLEAR_VALUE || p == GL_DEPTH_CLEAR_VALUE || p == GL_FOG_COLOR ||
                  p == GL_DEPTH_RANGE || p == GL_ALPHA_TEST_REF)
                 ? (GLint)(v[i] * 2147483647.0) : (GLint)(v[i] < 0 ? v[i] - 0.5 : v[i] + 0.5);
}

void APIENTRY glGetFloatv(GLenum p, GLfloat *out)
{
    double v[16];
    int n = query(p, v), i;
    if (!n) { dgl_gl_error(GL_INVALID_ENUM); return; }
    for (i = 0; i < n; i++)
        out[i] = (GLfloat)v[i];
}

void APIENTRY glGetBooleanv(GLenum p, GLboolean *out)
{
    double v[16];
    int n = query(p, v), i;
    if (!n) { dgl_gl_error(GL_INVALID_ENUM); return; }
    for (i = 0; i < n; i++)
        out[i] = v[i] != 0.0;
}

const GLubyte *APIENTRY glGetString(GLenum name)
{
    static char renderer[64];
    switch (name) {
    case GL_VENDOR: return (const GLubyte *)"DOS-GL";
    case GL_RENDERER:
        snprintf(renderer, sizeof renderer, "DOS-GL on %s", mga.name ? mga.name : "Matrox");
        return (const GLubyte *)renderer;
    case GL_VERSION: return (const GLubyte *)"1.1 DOS-GL 0.1";
    case GL_EXTENSIONS: return (const GLubyte *)"GL_EXT_bgra";
    default: dgl_gl_error(GL_INVALID_ENUM); return (const GLubyte *)"";
    }
}

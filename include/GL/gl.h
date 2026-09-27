/* gl.h - DOS-GL's OpenGL 1.1 subset (PRD §6).
 *
 * Written for DOS-GL from the OpenGL 1.1 specification's token values and
 * entry points; no Khronos or Mesa header text. Tier 1 is what ClassiCube's
 * GL 1.1 backend calls (the definition of done); Tier 2 is the common GL 1.1
 * core listed in PRD §6.2. Token spellings match ClassiCube's
 * misc/opengl/GLCommon.h where both define a token, so the two headers can
 * share a translation unit (tests/unit/test_gl_h_abi.c). */
#ifndef __gl_h_
#define __gl_h_

#ifndef GLAPI
#define GLAPI extern
#endif
#ifndef APIENTRY
#define APIENTRY
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLubyte;
typedef unsigned short GLushort;
typedef unsigned int   GLuint;
typedef float          GLfloat;
typedef float          GLclampf;
typedef double         GLdouble;
typedef double         GLclampd;
typedef void           GLvoid;

/* Booleans and errors */
#define GL_FALSE                 0
#define GL_TRUE                  1
#define GL_NO_ERROR              0
#define GL_INVALID_ENUM          0x0500
#define GL_INVALID_VALUE         0x0501
#define GL_INVALID_OPERATION     0x0502
#define GL_STACK_OVERFLOW        0x0503
#define GL_STACK_UNDERFLOW       0x0504
#define GL_OUT_OF_MEMORY         0x0505

/* Data types */
#define GL_BYTE                  0x1400
#define GL_UNSIGNED_BYTE         0x1401
#define GL_SHORT                 0x1402
#define GL_UNSIGNED_SHORT        0x1403
#define GL_INT                   0x1404
#define GL_UNSIGNED_INT          0x1405
#define GL_FLOAT                 0x1406
#define GL_DOUBLE                0x140A

/* Primitives */
#define GL_POINTS                0x0000
#define GL_LINES                 0x0001
#define GL_LINE_LOOP             0x0002
#define GL_LINE_STRIP            0x0003
#define GL_TRIANGLES             0x0004
#define GL_TRIANGLE_STRIP        0x0005
#define GL_TRIANGLE_FAN          0x0006
#define GL_QUADS                 0x0007
#define GL_QUAD_STRIP            0x0008
#define GL_POLYGON               0x0009

/* Comparison functions */
#define GL_NEVER                 0x0200
#define GL_LESS                  0x0201
#define GL_EQUAL                 0x0202
#define GL_LEQUAL                0x0203
#define GL_GREATER               0x0204
#define GL_NOTEQUAL              0x0205
#define GL_GEQUAL                0x0206
#define GL_ALWAYS                0x0207

/* Blend factors */
#define GL_ZERO                  0
#define GL_ONE                   1
#define GL_SRC_COLOR             0x0300
#define GL_ONE_MINUS_SRC_COLOR   0x0301
#define GL_SRC_ALPHA             0x0302
#define GL_ONE_MINUS_SRC_ALPHA   0x0303
#define GL_DST_ALPHA             0x0304
#define GL_ONE_MINUS_DST_ALPHA   0x0305
#define GL_DST_COLOR             0x0306
#define GL_ONE_MINUS_DST_COLOR   0x0307
#define GL_SRC_ALPHA_SATURATE    0x0308

/* Faces and winding */
#define GL_FRONT                 0x0404
#define GL_BACK                  0x0405
#define GL_FRONT_AND_BACK        0x0408
#define GL_CW                    0x0900
#define GL_CCW                   0x0901

/* Clear bits */
#define GL_DEPTH_BUFFER_BIT      0x00000100
#define GL_STENCIL_BUFFER_BIT    0x00000400
#define GL_COLOR_BUFFER_BIT      0x00004000

/* Capabilities */
#define GL_CULL_FACE             0x0B44
#define GL_LIGHTING              0x0B50
#define GL_FOG                   0x0B60
#define GL_DEPTH_TEST            0x0B71
#define GL_NORMALIZE             0x0BA1
#define GL_ALPHA_TEST            0x0BC0
#define GL_DITHER                0x0BD0
#define GL_BLEND                 0x0BE2
#define GL_SCISSOR_TEST          0x0C11
#define GL_TEXTURE_2D            0x0DE1

/* Fog */
#define GL_FOG_INDEX             0x0B61
#define GL_FOG_DENSITY           0x0B62
#define GL_FOG_START             0x0B63
#define GL_FOG_END               0x0B64
#define GL_FOG_MODE              0x0B65
#define GL_FOG_COLOR             0x0B66
#define GL_EXP                   0x0800
#define GL_EXP2                  0x0801

/* State queries */
#define GL_CULL_FACE_MODE        0x0B45
#define GL_FRONT_FACE            0x0B46
#define GL_SHADE_MODEL           0x0B54
#define GL_DEPTH_RANGE           0x0B70
#define GL_DEPTH_WRITEMASK       0x0B72
#define GL_DEPTH_CLEAR_VALUE     0x0B73
#define GL_DEPTH_FUNC            0x0B74
#define GL_MATRIX_MODE           0x0BA0
#define GL_VIEWPORT              0x0BA2
#define GL_MODELVIEW_MATRIX      0x0BA6
#define GL_PROJECTION_MATRIX     0x0BA7
#define GL_TEXTURE_MATRIX        0x0BA8
#define GL_ALPHA_TEST_FUNC       0x0BC1
#define GL_ALPHA_TEST_REF        0x0BC2
#define GL_BLEND_DST             0x0BE0
#define GL_BLEND_SRC             0x0BE1
#define GL_SCISSOR_BOX           0x0C10
#define GL_COLOR_CLEAR_VALUE     0x0C22
#define GL_COLOR_WRITEMASK       0x0C23
#define GL_UNPACK_ALIGNMENT      0x0CF5
#define GL_PACK_ALIGNMENT        0x0D05
#define GL_MAX_TEXTURE_SIZE      0x0D33
#define GL_MAX_MODELVIEW_STACK_DEPTH  0x0D36
#define GL_MAX_PROJECTION_STACK_DEPTH 0x0D38
#define GL_MAX_TEXTURE_STACK_DEPTH    0x0D39
#define GL_MAX_VIEWPORT_DIMS     0x0D3A
#define GL_RED_BITS              0x0D52
#define GL_GREEN_BITS            0x0D53
#define GL_BLUE_BITS             0x0D54
#define GL_ALPHA_BITS            0x0D55
#define GL_DEPTH_BITS            0x0D56
#define GL_TEXTURE_BINDING_2D    0x8069

/* Hints */
#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#define GL_POINT_SMOOTH_HINT     0x0C51
#define GL_LINE_SMOOTH_HINT      0x0C52
#define GL_POLYGON_SMOOTH_HINT   0x0C53
#define GL_FOG_HINT              0x0C54
#define GL_DONT_CARE             0x1100
#define GL_FASTEST               0x1101
#define GL_NICEST                0x1102

/* Shading and polygon mode */
#define GL_FLAT                  0x1D00
#define GL_SMOOTH                0x1D01
#define GL_POINT                 0x1B00
#define GL_LINE                  0x1B01
#define GL_FILL                  0x1B02

/* Display lists */
#define GL_COMPILE               0x1300
#define GL_COMPILE_AND_EXECUTE   0x1301

/* Matrices */
#define GL_MODELVIEW             0x1700
#define GL_PROJECTION            0x1701
#define GL_TEXTURE               0x1702

/* Strings */
#define GL_VENDOR                0x1F00
#define GL_RENDERER              0x1F01
#define GL_VERSION               0x1F02
#define GL_EXTENSIONS            0x1F03

/* Pixel formats */
#define GL_ALPHA                 0x1906
#define GL_RGB                   0x1907
#define GL_RGBA                  0x1908
#define GL_LUMINANCE             0x1909
#define GL_LUMINANCE_ALPHA       0x190A

/* Textures */
#define GL_MODULATE              0x2100
#define GL_DECAL                 0x2101
#define GL_TEXTURE_ENV_MODE      0x2200
#define GL_TEXTURE_ENV_COLOR     0x2201
#define GL_TEXTURE_ENV           0x2300
#define GL_REPLACE               0x1E01
#define GL_NEAREST               0x2600
#define GL_LINEAR                0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR  0x2703
#define GL_TEXTURE_MAG_FILTER    0x2800
#define GL_TEXTURE_MIN_FILTER    0x2801
#define GL_TEXTURE_WRAP_S        0x2802
#define GL_TEXTURE_WRAP_T        0x2803
#define GL_CLAMP                 0x2900
#define GL_REPEAT                0x2901
#define GL_TEXTURE_MAX_LEVEL     0x813D   /* GL 1.2 token accepted by glTexParameteri (ClassiCube) */

/* Vertex arrays */
#define GL_VERTEX_ARRAY          0x8074
#define GL_NORMAL_ARRAY          0x8075
#define GL_COLOR_ARRAY           0x8076
#define GL_INDEX_ARRAY           0x8077
#define GL_TEXTURE_COORD_ARRAY   0x8078

/* ---- Tier 1 (ClassiCube) ------------------------------------------------ */
GLAPI void APIENTRY glAlphaFunc(GLenum func, GLfloat ref);
GLAPI void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor);
GLAPI void APIENTRY glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
GLAPI void APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
GLAPI void APIENTRY glDepthFunc(GLenum func);
GLAPI void APIENTRY glDepthMask(GLboolean flag);
GLAPI void APIENTRY glDisable(GLenum cap);
GLAPI void APIENTRY glDisableClientState(GLenum array);
GLAPI void APIENTRY glEnable(GLenum cap);
GLAPI void APIENTRY glEnableClientState(GLenum array);
GLAPI void APIENTRY glFogf(GLenum pname, GLfloat param);
GLAPI void APIENTRY glFogfv(GLenum pname, const GLfloat *params);
GLAPI void APIENTRY glFogi(GLenum pname, GLint param);
GLAPI void APIENTRY glFogiv(GLenum pname, const GLint *params);
GLAPI void APIENTRY glLoadIdentity(void);
GLAPI void APIENTRY glLoadMatrixf(const GLfloat *m);
GLAPI void APIENTRY glMatrixMode(GLenum mode);
GLAPI void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count);
GLAPI void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
GLAPI void APIENTRY glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
GLAPI void APIENTRY glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
GLAPI void APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
GLAPI void APIENTRY glClear(GLbitfield mask);
GLAPI void APIENTRY glHint(GLenum target, GLenum mode);
GLAPI void APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                                 GLvoid *pixels);
GLAPI void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void APIENTRY glBindTexture(GLenum target, GLuint texture);
GLAPI void APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures);
GLAPI void APIENTRY glGenTextures(GLsizei n, GLuint *textures);
GLAPI void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                                 GLint border, GLenum format, GLenum type, const GLvoid *pixels);
GLAPI void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                    GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
GLAPI void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param);
GLAPI GLenum APIENTRY glGetError(void);
GLAPI void APIENTRY glGetFloatv(GLenum pname, GLfloat *params);
GLAPI void APIENTRY glGetIntegerv(GLenum pname, GLint *params);
GLAPI const GLubyte *APIENTRY glGetString(GLenum name);
GLAPI void APIENTRY glCallList(GLuint list);
GLAPI void APIENTRY glDeleteLists(GLuint list, GLsizei range);
GLAPI GLuint APIENTRY glGenLists(GLsizei range);
GLAPI void APIENTRY glNewList(GLuint list, GLenum mode);
GLAPI void APIENTRY glEndList(void);
GLAPI void APIENTRY glBegin(GLenum mode);
GLAPI void APIENTRY glEnd(void);
GLAPI void APIENTRY glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
GLAPI void APIENTRY glTexCoord2f(GLfloat s, GLfloat t);
GLAPI void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z);

/* ---- Tier 2 (common GL 1.1 core) ----------------------------------------- */
GLAPI void APIENTRY glPushMatrix(void);
GLAPI void APIENTRY glPopMatrix(void);
GLAPI void APIENTRY glMultMatrixf(const GLfloat *m);
GLAPI void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z);
GLAPI void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
GLAPI void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z);
GLAPI void APIENTRY glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear,
                            GLdouble zFar);
GLAPI void APIENTRY glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear,
                              GLdouble zFar);
GLAPI void APIENTRY glColor3f(GLfloat red, GLfloat green, GLfloat blue);
GLAPI void APIENTRY glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
GLAPI void APIENTRY glColor3ub(GLubyte red, GLubyte green, GLubyte blue);
GLAPI void APIENTRY glVertex2f(GLfloat x, GLfloat y);
GLAPI void APIENTRY glVertex3fv(const GLfloat *v);
GLAPI void APIENTRY glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz);
GLAPI void APIENTRY glShadeModel(GLenum mode);
GLAPI void APIENTRY glCullFace(GLenum mode);
GLAPI void APIENTRY glFrontFace(GLenum mode);
GLAPI void APIENTRY glPolygonMode(GLenum face, GLenum mode);
GLAPI void APIENTRY glFlush(void);
GLAPI void APIENTRY glFinish(void);
GLAPI void APIENTRY glGetBooleanv(GLenum pname, GLboolean *params);
GLAPI GLboolean APIENTRY glIsEnabled(GLenum cap);
GLAPI void APIENTRY glDepthRange(GLclampd zNear, GLclampd zFar);
GLAPI void APIENTRY glClearDepth(GLclampd depth);
GLAPI void APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint param);
GLAPI void APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param);
GLAPI void APIENTRY glPixelStorei(GLenum pname, GLint param);
GLAPI GLboolean APIENTRY glIsTexture(GLuint texture);

#ifdef __cplusplus
}
#endif

#endif /* __gl_h_ */

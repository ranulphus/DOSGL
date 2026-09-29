/* glext.h - extension and GL 1.2 tokens DOS-GL accepts or advertises (PRD §6.1).
 * Values from the extension specifications; each guarded, since the Quake
 * ports define some of them themselves. */
#ifndef __glext_h_
#define __glext_h_

/* GL_EXT_bgra: BGRA transfers for glTexImage2D/glTexSubImage2D/glReadPixels. */
#define GL_EXT_bgra 1
#define GL_BGR_EXT               0x80E0
#define GL_BGRA_EXT              0x80E1

/* GL_EXT_texture: the one internal format GL 1.1 did not keep (programs
 * list it among their texture modes). */
#ifndef GL_RGB2_EXT
#define GL_RGB2_EXT              0x804E
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif
#ifndef GL_ADD
#define GL_ADD 0x0104
#endif
#ifndef GL_TEXTURE0_ARB
#define GL_TEXTURE0_ARB 0x84C0
#endif
#ifndef GL_TEXTURE1_ARB
#define GL_TEXTURE1_ARB 0x84C1
#endif
#ifndef GL_ACTIVE_TEXTURE_ARB
#define GL_ACTIVE_TEXTURE_ARB 0x84E0
#endif
#ifndef GL_CLIENT_ACTIVE_TEXTURE_ARB
#define GL_CLIENT_ACTIVE_TEXTURE_ARB 0x84E1
#endif
#ifndef GL_MAX_TEXTURE_UNITS_ARB
#define GL_MAX_TEXTURE_UNITS_ARB 0x84E2
#endif
#ifndef GL_COLOR_INDEX1_EXT
#define GL_COLOR_INDEX1_EXT 0x80E2
#endif
#ifndef GL_COLOR_INDEX2_EXT
#define GL_COLOR_INDEX2_EXT 0x80E3
#endif
#ifndef GL_COLOR_INDEX4_EXT
#define GL_COLOR_INDEX4_EXT 0x80E4
#endif
#ifndef GL_COLOR_INDEX8_EXT
#define GL_COLOR_INDEX8_EXT 0x80E5
#endif
#ifndef GL_COLOR_INDEX12_EXT
#define GL_COLOR_INDEX12_EXT 0x80E6
#endif
#ifndef GL_COLOR_INDEX16_EXT
#define GL_COLOR_INDEX16_EXT 0x80E7
#endif
#ifndef GL_TEXTURE_INDEX_SIZE_EXT
#define GL_TEXTURE_INDEX_SIZE_EXT 0x80ED
#endif
#ifndef GL_COLOR_TABLE_FORMAT_EXT
#define GL_COLOR_TABLE_FORMAT_EXT 0x80D8
#endif
#ifndef GL_COLOR_TABLE_WIDTH_EXT
#define GL_COLOR_TABLE_WIDTH_EXT 0x80D9
#endif
#ifndef GL_SHARED_TEXTURE_PALETTE_EXT
#define GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB
#endif
#ifndef GL_POINT_SIZE_MIN_EXT
#define GL_POINT_SIZE_MIN_EXT 0x8126
#endif
#ifndef GL_POINT_SIZE_MAX_EXT
#define GL_POINT_SIZE_MAX_EXT 0x8127
#endif
#ifndef GL_POINT_FADE_THRESHOLD_SIZE_EXT
#define GL_POINT_FADE_THRESHOLD_SIZE_EXT 0x8128
#endif
#ifndef GL_DISTANCE_ATTENUATION_EXT
#define GL_DISTANCE_ATTENUATION_EXT 0x8129
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_COMPRESSED_RGB_FXT1_3DFX
#define GL_COMPRESSED_RGB_FXT1_3DFX 0x86B0
#endif
#ifndef GL_COMPRESSED_RGBA_FXT1_3DFX
#define GL_COMPRESSED_RGBA_FXT1_3DFX 0x86B1
#endif
#ifndef GL_INVALID_FRAMEBUFFER_OPERATION_EXT
#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT 0x0506
#endif
#ifndef GL_TABLE_TOO_LARGE
#define GL_TABLE_TOO_LARGE 0x8031
#endif
#ifndef GL_TEXTURE_COLOR_TABLE_SGI
#define GL_TEXTURE_COLOR_TABLE_SGI 0x80BC
#endif
#ifndef GL_COLOR_TABLE_FORMAT_EXT
#define GL_COLOR_TABLE_FORMAT_EXT 0x80D8
#endif
#ifndef GL_COLOR_TABLE_WIDTH_EXT
#define GL_COLOR_TABLE_WIDTH_EXT 0x80D9
#endif
/* GL_SGIS_multitexture (period GLQuake builds): texture units by these names. */
#ifndef GL_TEXTURE0_SGIS
#define GL_TEXTURE0_SGIS 0x835E
#endif
#ifndef GL_TEXTURE1_SGIS
#define GL_TEXTURE1_SGIS 0x835F
#endif

#endif /* __glext_h_ */

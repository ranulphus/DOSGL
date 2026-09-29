# Using DOS-GL

DOS-GL is a static library for 32-bit DOS programs built with DJGPP (gcc
12.2 cross compiler) and run under CWSDPMI. It implements the OpenGL 1.1
subset in PRD §6 on Matrox G400, G450 and G200 cards.

## Building and linking

```
#include <GL/gl.h>       /* OpenGL 1.1 (includes GL/glext.h)       */
#include <GL/glext.h>    /* extension and GL 1.2 tokens            */
#include <GL/dosgl.h>    /* dglInit, dglSwapBuffers, ...           */

i586-pc-msdosdjgpp-gcc ... -I<dosgl>/include -L<dosgl>/build/lib -lGL -lm
```

Ship `CWSDPMI.EXE` next to the program (or on the `PATH`).

## Context

```
DGLConfig cfg = { 0 };            /* all zero: 640x480x16, double-buffered, 16-bit Z, vsync */
if (dglInit(&cfg) != 0) { puts(dglGetErrorString()); return 1; }
... draw with GL ...
dglSwapBuffers();
dglShutdown();
```

- `dglEnumModes` lists the 16-bit modes the card's BIOS offers that the
  drawing engine can use and that fit in VRAM, before `dglInit`.
- A crash (#GP, #PF, #UD, divide error), `exit()` or Ctrl-Break resets the
  engine and restores text mode.
- DOS-GL maps the card with near pointers (`__djgpp_nearptr_enable`). A
  program that turns them off must not call GL until `dglSwapBuffers`
  (which turns them back on) or turn them back on itself.
- Games may leave the x87 FPU at 24-bit precision or change its rounding
  (Quake 2 does): drawing calls switch to their own control word and
  restore the caller's.
- `dglGetProcAddress(name)` returns any GL 1.1 function and the extension
  functions DOS-GL implements (for programs that bind GL at run time).
- Test hooks, over COM1:
  - `DGL_EXIT_AFTER=n` ends the program after n swaps. `dglInit` then logs
    `DGL-START` and every way out logs `DGL-EXIT frames=n`, which is how
    Loop A knows a program has finished.
  - `DGL_STATS=1` logs frame and triangle rates once a second (`DGL-STAT`);
    `DGL_STATS=2` adds where primitives went (`DGL-PRIMS`: skipped, clipped
    away, zero area, culled) and texture traffic (`DGL-TEX`: uploads,
    sub-image writes in place or as whole re-uploads, renames, evictions,
    syncs forced by texture memory).
  - `DGL_TEXHEAP_KB=n` caps the texture heap (to test eviction, or to
    behave like a card with less memory).
  - `DGL_SNAP=100,250` writes the frames shown by those swaps to
    `DGL_SNAPDIR` (default `C:\OUT`) as `F00100.PPM`...; a timedemo draws
    every frame, so these are the same pictures on every card and run.
    `dglSnapshot(path)` does the same on demand.
  - The first 16 GL errors are logged as `DGL-GLERR <code> at <address>`
    (the address is inside the GL function that raised it; look it up in
    the program's link map).

## What differs from full OpenGL 1.1

| Area | Behaviour |
|---|---|
| Colour | RGB565 only, dithered; `GL_ALPHA_BITS` is 0 (destination alpha reads as 1) |
| API | Every OpenGL 1.1 function links. Those DOS-GL does not implement (evaluators, feedback, selection, pixel maps, lighting state...) are stubs that log `DGL-STUB glName` once, count the call in `DGLStats.stub_calls` and return zero; the list is generated at build time (`build/gen/stubs.c`, from `src/gl/gl11.api`) |
| Textures | Power-of-two sizes; stored as RGB565, ARGB1555 or ARGB4444 chosen from the texels' alpha; internal format ignored. Textures smaller than 8 texels are widened to 8 |
| Texture memory | Uploads never wait for the engine: a texture drawn since the last swap is rewritten into a new block (the old one is freed at the next swap), and `glTexSubImage2D` writes just the rectangle when it can. When VRAM runs out, the least recently drawn textures are evicted and re-uploaded from DOS-GL's copies when next used; `GL_OUT_OF_MEMORY` only when one texture cannot fit at all |
| Texture environment | `GL_MODULATE`, `GL_REPLACE` and `GL_DECAL` (drawn as `GL_REPLACE`); `GL_BLEND` is refused with `GL_INVALID_ENUM`; `GL_TEXTURE_ENV_COLOR` is stored |
| `GL_CLAMP` | Clamps to the edge texel (as the hardware does), the same as `GL_CLAMP_TO_EDGE`; GL 1.1 would blend edge texels with the border colour under linear filtering. Border colours are stored, never drawn |
| Mipmaps | Up to 5 levels in a window starting at level 0 (levels at least 8x8); the chip picks the level per pixel and rounds it |
| Fog | Per-vertex factor, interpolated linearly across the screen; triangles whose fog varies a lot are subdivided |
| Lighting, accumulation | Not implemented (normals accepted and ignored) |
| Stencil | No stencil buffer (`GL_STENCIL_BITS` is 0): the stencil state is kept and the test always passes, as GL specifies for a zero-bit buffer |
| Colour buffers | `glDrawBuffer` and `glReadBuffer` choose the front or back buffer; `GL_FRONT_AND_BACK` draws into the back buffer only |
| Polygon offset | `GL_POLYGON_OFFSET_FILL` only (point and line offset are accepted and ignored) |
| Display lists | `GL_COMPILE` and `GL_COMPILE_AND_EXECUTE` capture draw calls and immediate-mode blocks only; other calls made while compiling take effect immediately; no nesting |
| Polygon mode | Fill only |
| Lines and points | Screen-space quads of the line width and point size |
| Extensions | `GL_EXT_bgra`, `GL_EXT_texture_edge_clamp`, `GL_SGIS_texture_edge_clamp` |

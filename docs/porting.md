# Using DOS-GL

DOS-GL is a static library for 32-bit DOS programs built with DJGPP (gcc
12.2 cross compiler) and run under CWSDPMI. It implements the OpenGL 1.1
subset in PRD §6 on Matrox G400, G450 and G200 cards.

## Building and linking

```
#include <GL/gl.h>       /* OpenGL 1.1 subset (Tier 1 and Tier 2)  */
#include <GL/glext.h>    /* GL_BGRA_EXT                            */
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
- Test hooks: `DGL_EXIT_AFTER=n` ends the program after n swaps;
  `DGL_STATS=1` logs frame and triangle rates over COM1 once a second.

## What differs from full OpenGL 1.1

| Area | Behaviour |
|---|---|
| Colour | RGB565 only, dithered; `GL_ALPHA_BITS` is 0 (destination alpha reads as 1) |
| Textures | Power-of-two sizes; stored as RGB565, ARGB1555 or ARGB4444 chosen from the texels' alpha; internal format ignored. Textures smaller than 8 texels are widened to 8 |
| `GL_CLAMP` | Clamps to the edge texel (as the hardware does); GL 1.1 would blend edge texels with the border colour under linear filtering |
| Mipmaps | Up to 5 levels in a window starting at level 0 (levels at least 8x8); the chip picks the level per pixel and rounds it |
| Fog | Per-vertex factor, interpolated linearly across the screen; triangles whose fog varies a lot are subdivided |
| Lighting, stencil, accumulation | Not implemented (`glNormal3f` accepted and ignored) |
| Display lists | `GL_COMPILE` and `GL_COMPILE_AND_EXECUTE` capture draw calls and immediate-mode blocks only; other calls made while compiling take effect immediately; no nesting |
| Polygon mode | Fill only |
| Lines and points | One-pixel quads |
| Extensions | `GL_EXT_bgra` |

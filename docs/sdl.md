# SDL3 on DOS-GL

SDL3 has had a DOS (DJGPP) port since April 2026: VESA/VGA video with
software rendering, an IRQ 1 keyboard, the INT 33h mouse, a gameport
joystick, Sound Blaster 16/Pro/2.0 audio and cooperative threads. It has no
OpenGL. DOS-GL carries a pinned SDL3 (`third_party/sdl`) and a patch series
(`tools/sdl/patches/`) that adds OpenGL windows through DOS-GL, so a game
can use SDL for input, sound, timing and files and DOS-GL for drawing, and
build the same source for Linux with desktop OpenGL.

## Building

```
make sdl                  # build/sdl/dos/{include/SDL3,lib/libSDL3.a}, patches applied
make sdl-host             # the same SDL for Linux: build/sdl/host (desktop GL builds)
make sdl-examples         # SDLINFO, SDLKEYS, SDLBEEP, SDLGL, SDLCRASH
make loopa-sdl CARD=g450  # the SDL checks in 86Box (tools/sdl/loopa.sh)
```

A program links SDL before DOS-GL:

```
i586-pc-msdosdjgpp-gcc ... -I<dosgl>/include -I<dosgl>/build/sdl/dos/include \
    -L<dosgl>/build/sdl/dos/lib -L<dosgl>/build/lib -lSDL3 -lGL -lm
```

It includes `<SDL3/SDL_main.h>` in the file with `main` (SDL's DOS startup
locks memory for its interrupt handlers and turns near pointers on, which
DOS-GL also uses), and `<GL/gl.h>` from DOS-GL, not SDL's `SDL_opengl.h`.
Ship `CWSDPMI.EXE` beside it. SDL's audio needs `BLASTER` (with `H` for an
SB16); without a Sound Blaster, `SDL_AUDIO_DRIVER=dummy` keeps
`SDL_Init(SDL_INIT_AUDIO)` working.

## OpenGL windows

```
SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
SDL_Window *w = SDL_CreateWindow("game", 640, 480, SDL_WINDOW_OPENGL);
SDL_GLContext c = SDL_GL_CreateContext(w);      /* dglInit: the mode is set here */
...
SDL_GL_SwapWindow(w);                           /* dglSwapBuffers */
...
SDL_GL_DestroyContext(c);                       /* dglShutdown: text mode */
SDL_DestroyWindow(w);
```

- The window's size picks the mode: any of DOS-GL's, including 320x200,
  320x240, 400x300, 512x384 and 640x512, which it draws scaled (or zoomed)
  into a larger BIOS mode. `SDL_GetFullscreenDisplayModes` lists them as
  RGB565 modes; the ones only DOS-GL has are for OpenGL windows (a
  software window asking for one gets an error). A size DOS-GL lacks is
  rounded up to the smallest of its modes that holds it.
- One OpenGL window and one context at a time. The size is fixed for the
  context's life: to change resolution, destroy the context and window
  and create them again (textures and display lists go with the context).
- `SDL_GL_SetSwapInterval(0)` turns vsync off (DOS-GL's default is on).
- `SDL_GL_GetProcAddress` returns every GL 1.1 function and DOS-GL's
  extensions. The limits are DOS-GL's (`docs/porting.md`): RGB565, no GL
  lighting, no stencil, power-of-two textures.
- SDL's OpenGL `SDL_Renderer` is not built (it needs GL functions DOS-GL
  lacks); `SDL_Renderer` on DOS stays software, in software windows.
- DOS-GL's environment switches (`DGL_STATS`, `DGL_SNAP`, `DGL_EXIT_AFTER`,
  ...) work as for any DOS-GL program.

## Threads, events and audio

SDL on DOS switches threads only when the running one yields: at
`SDL_PumpEvents`/`SDL_PollEvent`, `SDL_Delay`, a contended mutex or
semaphore, and the audio thread's own wait. The Sound Blaster driver's ring
holds about 45 ms, and only the audio thread refills it, so a game must not
go 45 ms without a yield:

- pump events at the start of each frame, and again inside long stretches
  of work (every 10 ms or so: `SDL_PumpEvents` only queues events);
- `SDL_GL_SwapWindow` yields before the swap, and with DOS-GL 0.3 or later
  the bridge also yields while the swap waits for the drawing engine
  (`dglSetWaitHook`: about once a millisecond, and once before the retrace
  wait; `DGL-STAT hooks=`).

A frame of 30 ms of work with a pump at its start and the swap's yields
keeps the ring full; a 60 ms frame with no pump in it does not.

## Leaving the machine usable

`SDL_Quit` restores the keyboard, stops the Sound Blaster and puts the
video mode back. For programs that end any other way, patch 0003 makes SDL
stop the Sound Blaster (it would otherwise loop its DMA buffer until the
machine is reset) and unhook IRQ 1 on `exit()` and on a fatal signal. DOS-GL
restores text mode on the same paths; its fault handler runs first and
chains to SDL's.

## The patches

| Patch | What | Upstream |
|---|---|---|
| `0001-dos-opengl-through-dos-gl` | `SDL_DOS_DOSGL=ON` with `DOSGL_DIR`: OpenGL windows through DOS-GL (`src/video/dos/SDL_dosopengl.c`); DOS-GL's modes join the display's; SDL leaves the hardware alone while DOS-GL owns the screen | opt-in backend; offer on approval |
| `0002-dos-tls-init-once` | `SDL_SetTLS` cleared every thread's storage each call on DOS (so `SDL_GL_GetCurrentWindow` lost the window and every GL swap failed); thread 16 had none | bug fix |
| `0003-dos-cleanup-on-exit-and-crash` | stop the Sound Blaster and unhook IRQ 1 on `exit()` and fatal signals | bug fix |
| `0004-dos-joystick-four-axes` | read the gameport's third and fourth axes when present (4-axis sticks, wheels with pedals); calibrate around the rest position (the first move to an end was lost and set the centre there) | bug fix + feature |

`tools/sdl/build.sh` applies them to a copy of the pinned source; the
submodule itself stays untouched. To move the pin: update the submodule,
rebuild, run `make loopa-sdl` on g200, g400 and g450.

## Tests

`tools/sdl/loopa.sh CARD [CHECK...]` (all by default):

| Check | What passes |
|---|---|
| `info` | SDL starts with video, audio and joystick; modes listed |
| `keys` | keys typed in 86Box arrive through SDL; afterwards KEYWAIT reads one through the BIOS and VECCHK finds the interrupt vectors unchanged |
| `beep` | a tone through SDL's SB16 driver is in the recording (`--wav`) |
| `gl` | SDLGL's last frame equals TEXCUBE's (the same scene, DOS-GL directly), with SDL audio playing; the wait hook ran |
| `modes` | an OpenGL window, context, frames and snapshot in every mode SDL lists, destroyed and recreated in one run |
| `crash` | after a #UD with SDL's keyboard hooked and its SB playing: text mode, SBCHK finds the DMA stopped, VECCHK and KEYWAIT pass |
| `exit` | the same after DOS-GL's `DGL_EXIT_AFTER` calls `exit()` inside a swap |
| `joy` | the virtual joystick (86Box local patch 0105): four axes reach both ends, buttons go down and up |

# Silicon experiments (G4EXP)

A few things DOS-GL does on the G400/G450, and one on the G200, rest on
readings of the specification that no public driver confirms. The open
questions are listed in MGA-Glide's `docs/g400-dual-texture.md` §7. 86Box
models the readings DOS-GL chose (local patches 0009 and 0010), so in the
emulator every experiment passes by construction. Only the silicon can say
whether they hold.

`examples/g4exp` (`build/exe/G4EXP.EXE`) draws one small scene per
experiment and reads it back. It prints what it found (`HX-STAT g4exp ...`)
and a verdict (`HX-TEST`), and saves the picture as `E<n><tag>.PPM`. A PASS
means the card does what DOS-GL assumes. The pictures are synthetic, with no
game data in them.

## Running

```
make examples dostools
tools/silicon/g4exp.py loopa [--card g450,g400,g200]   # emulator: out/g4exp-CARD/
tools/silicon/g4exp.py bench --pc bench-g400           # a bench PC (run on the host)
tools/silicon/g4exp.py report DIR...                   # the table again, from DIR/serial.log
```

`bench` publishes the job through MGA-Glide's `tools/bench/run.py` (the checkout
in `MGA_GLIDE`, default `~/MGA-Glide`). The results land in that checkout's
`out/bench/<pc>/g4exp-*/`. On silicon a FAIL job is an answer, not an error.

To dry-run without hardware, use the virtual bench PC:
`MGA_DOCKER_NETWORK=host tools/dev python3 tools/bench/vpc.py start g450` in
MGA-Glide, then `--pc vbench-g450`.

One job runs G4EXP six times, each under one DOS-GL switch. The tag names
the run's pictures (`E2C.PPM` is E2 under `DGL_COMBINER=1`):

| Tag | Switch | Runs | For |
|---|---|---|---|
| D | none (the defaults) | E1 E2 E4 E5 E6 E7 | everything |
| C | `DGL_COMBINER=1` | E1 E2 E4 E6 | E2 |
| T | `DGL_TC2_EXTRA=8000` | E1 E2 E4 E5 E6 | E3 |
| L | `DGL_TLUT=1` | E5 | E5 on the G400/G450 |
| S | `DGL_TLUT=0` | E5 | E5 reference |
| P | `DGL_ILOAD=0` | E6 | E6 with CPU writes |

`DGL_COMBINER` and `DGL_TC2_EXTRA` act only on cards with two texture units.
E1 and E4 need two units; on the G200 they are skipped.

## Emulator baseline (2026-09-29)

Every experiment passes under every switch on the emulated G450, G400 and
G200. The job also passed on the virtual bench PC (`vbench-g450`: picked
up, 22 files uploaded; that was before run P existed), so the bench path is
ready.

In E3, every T picture is identical to its D picture. E5 uses the LUT where
expected: in D on the G200, and in L everywhere. E6 writes through the
engine in D and by the CPU in P. The PCI values 86Box
reports, for comparison with the cards:

| Card | Device, revision | OPTION (40h) | OPTION2 (50h), OPTION3 (54h) |
|---|---|---|---|
| G450 | 0525, 82 | 40091120 | 0, 0 (not modelled) |
| G400 | 0525, 03 | 50040120 | 0, 0 (not modelled) |
| G200 | 0521, 00 | 4007dd21 | 0, 0 |

## The experiments

### E1: map-1 routing and coordinates (G400/G450)

**DOS-GL assumes** the reading of `TEXCTL2.map1` in which the map-1 bit takes
effect on the *next* register write. For each trapezoid, the HAL
(`hal/src/setup/trap.c`) writes:

1. `TEXCTL2|MAP1`;
2. map 1's TEXCTL, sizes and TMR0-5 (first trapezoid only);
3. map 1's TMR6-8;
4. `TEXCTL2` again, map1 clear;
5. then draws.

X's EXA code relies on the same reading.

**E1a:** unit 0 is REPLACE with a red|green texture. Unit 1 is DECAL with a
blue|clear texture on swapped coordinates.

| Quadrants (bottom, top) | Meaning |
|---|---|
| blue blue, red green | as assumed |
| red green, red green | unit 1 is not seen: map 1 was never programmed, or stage 1 does not run |
| blue green, blue green | map 1 was sampled with map 0's coordinates: TMR writes meant for map 1 reached map 0 (or were shared) |
| anything else | look at `E1A?.PPM` |

If E1a fails, the map-1 write order in `trap.c` is wrong for this silicon. Try
the other reading: map1 effective on the write that sets it. Then update
86Box patch 0009 to match.

**E1b, E1c:** a perspective floor. Both units use the same coordinates on
checkerboards: unit 0 red/green, unit 1 half-transparent blue/clear. Where
the maps agree, the floor is purple/green. Red or teal pixels are where map
1's coordinates drifted from map 0's. In E1c, map 1 is 32x32, which gives it
a different texture-coordinate prescale.

The line gives the counts (`agree=`, `red=`, `teal=`), and the test passes
below 0.2%. Where the pixels are in `E1B?.PPM` tells you which part of the
map-1 setup is wrong:

- near the horizon: q precision, the map-1 prescale `k1`;
- along one side: the left-edge correction for map 1's TMRs.

### E2: modulate with one texture (all cards; the question is the G400/G450)

**DOS-GL assumes** that on the G400 the legacy lighting module still works
beside the stage combiner. DOS-GL draws MODULATE with `TEXCTL.tmodulate`, and
both TDUALSTAGE words 0 (texel pass-through), which is the specification's
rule for the "Rev A" chips. Mesa instead uses the combiner word
(`0x40600000`, colour and alpha) with tmodulate off. `DGL_COMBINER=1` does
what Mesa does.

The scene is a white texture, with alpha 1 on the left and 1/2 on the right,
under the vertex colour (1, 1/2, 0) with alpha 1/2, blended over black. The
verdict names the outcome:

| Verdict | Left, right | Meaning |
|---|---|---|
| modulated | (128,64,0), (64,32,0) | colour and alpha modulated |
| colour-not-modulated | (128,128,128), (64,64,64) | the texel colour passed through |
| alpha-not-modulated | (255,128,0), (128,64,0) | the texel alpha passed through |
| neither-modulated | (255,255,255), (128,128,128) | the legacy module is off |

What each combination means:

- **D modulated:** keep the legacy path.
- **D not modulated, C modulated:** make the combiner the default on that
  chip (`force_combiner` in `src/gl/emit.c`). Then update 86Box patch 0009,
  which applies tmodulate before the stages.
- **C wrong, D modulated:** this is Rev A behaviour; the combiner words must
  stay 0.
- **Both wrong:** look at the pictures.

On the G200 only the legacy path exists, so a FAIL there is a DOS-GL bug.

### E3: TEXCTL2 bit 15 (G400/G450)

Every Linux and X G400 path ORs 0x8000 into TEXCTL2 (`MGA_G400_TC2_MAGIC`),
but the specification marks the bit reserved. Run T sets the bit on every
TEXCTL2 write. The report compares each T picture's CRC with D's.

- **Identical:** the bit makes no visible difference in these scenes. DOS-GL
  leaves it off. The switch stays available for the games (Quake 2 under
  `DGL_TC2_EXTRA=8000` against its timedemo pictures).
- **Different:** find which pictures differ and whether T or D matches the
  expected colours. If only T is right, set the bit always.

### E4: leaving dual texturing (G400/G450)

When the Linux kernel leaves the dual-texture pipe (`mga_g400_emit_pipe`), it
runs a sequence of its own: a dummy draw, DWGSYNC, then TEXCTL2 toggling
dualtex. MGA-Glide's `docs/g400-dual-texture.md` §4 lists the steps. DOS-GL
just rewrites TEXCTL2.

The scene is a grid of 32 cells, dual and single interleaved with no sync
between them. Dual cells are (192,64,64) and single cells are cyan. Any
wrong cell means DOS-GL needs the kernel's sequence at the dual-to-single
switch (in the HAL's `tex_emit`). The verdict gives the first wrong cell.
The pattern is in `g4exp.c`.

### E5: paletted textures through the LUT (G200, and the G400/G450 under L)

**DOS-GL assumes:**

- the LUT holds RGB565 entries;
- the BITBLT RSTR recipe loads it (`engine_tlut_load`, 86Box patch 0010);
- `atype` does not matter during the load.

The scene is four stripes of indices 0-3 through the shared palette, then the
palette is reversed and a second quad is drawn: two LUT loads within one
frame. Entry 0 is (0,132,0). As RGB565 that is 0x0420; if the LUT read it as
ARGB1555, the stripe would come out near (8,8,0). The `e5 entry=` lines give
every stripe's colour, and the verdict says which path ran (`tlut` or
`expanded`).

- **S (expanded)** must pass on every card. A FAIL there is a DOS-GL bug.
- **G200 D fails, S passes:** the load recipe or entry format is wrong on
  silicon. Make `DGL_TLUT=0` the default and look at the entry colours.
- **G400 L passes:** the LUT works there too, despite the specification's
  advice to expand 8-bit textures. Consider making it the default: it halves
  the texture VRAM of GLQuake's paletted textures, as it does on the G200.

### E6: texture cache after in-place texel writes (all cards)

DOS-GL writes a `glTexSubImage2D` rectangle straight into the texture's VRAM
when it can (`sub_in_place` in `src/gl/texture.c`). There are two paths:

- **D:** through the drawing engine (ILOAD), queued in order with the draws.
  This is the default where the level is wide enough.
- **P:** `DGL_ILOAD=0`, written by the CPU through the framebuffer. If the
  texture was drawn since the last sync, DOS-GL waits for the engine first.

86Box has no texture cache. On silicon, stale texels could survive either
kind of write.

The texture is first drawn red. Then:

| Step | Write | Then | Expect |
|---|---|---|---|
| a | `glFinish`; whole texture green | drawn with no register change | green |
| b | at once, an 8x8 corner blue (P: after a sync) | drawn with no register change | blue corner |
| c | another texture drawn; corner yellow | drawn (TEXORG rewritten) | yellow corner |

The verdict counts stale samples per step (1024 samples per quad; 16 in the
corner). It also reports how the writes went:

- D: "engine 3, cpu 0 (after sync 0), re-uploaded 0";
- P: "engine 0, cpu 3 (after sync 2), re-uploaded 0".

Any other split means the path was not exercised, so the result is
inconclusive.

| Outcome | Change |
|---|---|
| all clean | nothing |
| D clean, P stale | keep ILOAD; send the CPU path's rectangles through a re-upload instead |
| a or b stale, c clean | a TEXORG write refreshes the cache: after an in-place write, mark the texture state dirty so validate re-emits it |
| c stale too | the cache survives register writes: drop that path's in-place writes (always re-upload), or find a flush |

### E7: identity (all cards)

E7 prints the chip, revision, VRAM, the DOS-GL capabilities, and PCI config
space 00h-5Ch. That covers OPTION (40h), and on the G400 OPTION2 (50h) and
OPTION3 (54h). The specification's "Rev A" notes (the E2 question) name no
PCI revision, so E7 records every card's values next to its E2 outcome.

## Recording results

For each card, add a row with the date, card, revision and the report's table
to this document. Turn each FAIL into three things:

- a DOS-GL or HAL change;
- an 86Box patch update, so the emulator models the silicon;
- a new `g4exp.py loopa` baseline.

Record each answer in MGA-Glide's `docs/g400-dual-texture.md` §7 as well.

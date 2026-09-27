# Testing DOS-GL

| Loop | Command | What it checks |
|---|---|---|
| Host unit tests | `make tests-host` | `gl.h` against ClassiCube's declarations, matrices, clipping, primitive assembly, texel conversion, VRAM heap, display lists |
| Loop A (86Box) | `make loopa TEST=probe CARD=g450` | a program on an emulated Matrox card (`g450`, `g400`, `g200`, `g100`) booting Matrox's own BIOS; results in `out/<test>/` |
| Conformance | `make conform CARD=g450 [TESTS="t01 t04"]` | tests/conform: each test's DOS build in 86Box against its host build on Mesa OSMesa (the reference), after RGB565 quantisation with edge masks and the tolerances in `manifest.json` |
| ClassiCube | `make loopa-classicube CARD=g450` | ClassiCube's GL 1.1 backend on DOS-GL, singleplayer for `CC_FRAMES` frames with the procedural test pack; screenshots in `out/classicube-<card>/` |
| Loop B (bench) | `python3 third_party/mgahal/tools/bench/run.py --pc bench-g450 --exe build/exe/PROBE.EXE` | the same programs on a real PC (see `third_party/mgahal/docs/bench.md`); the 86Box virtual bench PC (`vpc.py`) dry-runs it |

The harness, 86Box patches and bench tooling are MGA-Glide's, vendored in
`third_party/mgahal` (refresh with `make sync-hal`; `make check-hal`
verifies it). Emulated cards are models: green in 86Box means plausible,
and only Loop B declares a milestone done (PRD §11).

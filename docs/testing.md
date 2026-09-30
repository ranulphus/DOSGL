# Testing DOS-GL

| Loop | Command | What it checks |
|---|---|---|
| Host unit tests | `make tests-host` | `gl.h` against ClassiCube's declarations, matrices, clipping, primitive assembly, texel conversion, VRAM heap, display lists |
| Loop A (86Box) | `make loopa TEST=probe CARD=g450` | a program on an emulated Matrox card (`g450`, `g400`, `g200`, `g100`) booting Matrox's own BIOS; results in `out/<test>/` |
| Conformance | `make conform CARD=g450 [TESTS="t01 t04"]` | tests/conform: each test's DOS build in 86Box against its host build on Mesa OSMesa (the reference), after RGB565 quantisation with edge masks and the tolerances in `manifest.json` |
| ClassiCube | `make loopa-classicube CARD=g450` | ClassiCube's GL 1.1 backend on DOS-GL, singleplayer for `CC_FRAMES` frames with the procedural test pack; screenshots in `out/classicube-<card>/` |
| Half-Life | `make loopa-halflife MODE=boot\|server\|maps\|map\|keys\|play\|record\|timedemo CARD=g450` | Xash3D FWGS and hlsdk-portable (the pinned forks in `tools/halflife/deps.mk`, one static executable, `tools/halflife/build.sh`) on the owner's WON data (`tools/halflife/fixtures.py`); `boot` prints `-version` and starts the client, `server` loads a map in `-dedicated` mode, `maps` several; `map` plays one in the client at a fixed step (`tools/halflife/cards.sh` compares its frames across cards); `keys` types `KEYS` into the client; `play` is the H3 exit (`tools/halflife/keys/h3play.keys`: new game from the menu, quick save and load, the c0a0 tram's landmark, quit; checked from the log with the SB16's DMA, the heap and `config.cfg`). Key scripts wait for the engine's own serial lines (Loop A `@TEXT` anchors; the engine prints `HL-ACTIVE <map>` when the client is in a level and takes keys), since 86Box runs about three times slower than the wall clock here. `record` (`DEMO`, `MAP`) plays `tools/halflife/demos/DEMO.cfg` at a fixed 20 ms step with `td_nochangelevel` and keeps DEMO.dem as a local fixture (`$MGA_CACHE/fixtures/games/hldemos`, never committed): `hlbench1` is the c0a0 tram ride, `hlbench2` c1a2 with MP5 fire (scripted: a demo a person records on the bench would be more representative). `timedemo` plays one at the same step with `td_seed 1` and vsync off; `TEST=ID` writes DOSBench's H and T lines and frame 200 (DOSBench `HLD1`, `HLD2`). `tools/halflife/winvm.sh [CARD]` makes `dist/halflife-CARD-vm.zip`, a ready-to-boot 86Box machine (128 MB, SB16, PS/2 mouse with CuteMouse) with the game and the owner's data on D: and `HL.BAT`: retail data, for the owner's machine only. The PC has 128 MB and a Sound Blaster 16 (`MEM`, `SOUND`); the console is in `out/<name>/files/HL.TXT`. `tools/halflife/datacheck.py` lists what the game code names that the data lacks |
| Loop B (bench) | `python3 third_party/mgahal/tools/bench/run.py --pc bench-g450 --exe build/exe/PROBE.EXE` | the same programs on a real PC (see `third_party/mgahal/docs/bench.md`); the 86Box virtual bench PC (`vpc.py`) dry-runs it |

The harness, 86Box patches and bench tooling are MGA-Glide's, vendored in
`third_party/mgahal` (refresh with `make sync-hal`; `make check-hal`
verifies it). Emulated cards are models: green in 86Box means plausible,
and only Loop B declares a milestone done (PRD §11).

The emulator needs every patch in the series, including CPU patch 0103.
Without it, a DJGPP program dies with a #GP inside `__dpmi_int` whenever an
interrupt frame straddles into a stack page the program has not yet
touched. Whether that happens depends only on where the stack sits:
conformance t13 and t15 hit it after the R2 changes, with no fault of
their own. `STACKPG.EXE` (`make -C third_party/mgahal
build/djgpp/STACKPG.EXE`) must PASS in Loop A; see
`third_party/mgahal/docs/loops.md`.

# Matrox HAL and test harness

Exported from MGA-Glide 81c0686 by `tools/hal-export.sh`. Shared by
MGA-Glide (Open Watcom, DOS/4GW) and DOS-GL (DJGPP):

| Path | Contents |
|---|---|
| `hal/` | PCI, capabilities, VBE, FIFO pacing, engine, DAC, trapezoid setup; ports for DOS/4GW, DJGPP and the host |
| `tests/unit/` | host reference rasteriser, the setup test and the setup goldens (register state at every draw) |
| `tests/hal/` | `smoke.c` (any toolchain), `probe.c`, `romdump.c` |
| `tools/loopa/`, `tools/86box/` | Loop A harness and the pinned 86Box with the local patches (never upstreamed): emulated G200, G400 and G450 on Matrox's own BIOSes; `tools/games/mkimage.sh` builds the game disk for `run.py --game` (the caller supplies the games list with `--games-file`) |
| `tools/bench/` | Loop B: bench job runner, upload sink, capture helper, the 86Box virtual bench PC |
| `tests/rig/`, `tools/rig/` | Loop C rig for a Matrox card under Linux (`hal/port/linux.c`) |

```
make setup-djgpp && make hal-djgpp smoke-djgpp
make hal-host tests-host
```

See `docs/loops.md`, `docs/bench.md`, `docs/emulated-g200.md` and `docs/emulated-g400.md`.
`MANIFEST` lists every file's sha256 so a vendored copy can be checked for local edits.

# DOS-GL has moved into RetroGPU

On 2026-11-08 DOS-GL became `gl/` in **RetroGPU**:
https://github.com/ranulphus/RetroGPU

RetroGPU holds the hardware layer (the HAL), the test platform and both APIs
on it: DOS-GL in `gl/`, with its whole tree as it was here, and the Glide
runtime in `glide/`. DOS-GL builds the HAL and harness from its parent
directory, so a HAL change and its effect on GL are one commit.

This repository's history was merged into RetroGPU (merge commit `91846e1`),
so every DOS-GL commit ID resolves there. The last commit here before the
move is `7a277d1`. Programs that built from `~/DOSGL` now use `RETROGPU`
(default `~/RetroGPU`), with DOS-GL at `$(RETROGPU)/gl`.

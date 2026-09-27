# Third-party material

| Path | What | Licence |
|---|---|---|
| `third_party/mgahal/` | The shared Matrox HAL, test harness, bench tooling and local 86Box patches, vendored from MGA-Glide (`tools/sync-hal.sh`; version in `VERSION`) | MIT (same author) |
| `third_party/classicube/` | ClassiCube, pinned submodule (the reference consumer, PRD §11.6) | BSD-3-Clause (ClassiCube's own licence) |

Derived register definitions inside the HAL carry their upstream notices
(X.org `mga` and Mesa/DRI `mga` drivers, MIT), as recorded in MGA-Glide.
No Matrox BIOS images, retail software or game data are committed; the
86Box ROM set and Matrox's BIOS package are fetched into the local cache.

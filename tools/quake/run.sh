#!/usr/bin/env bash
# Run a Quake port on DOS-GL in Loop A (86Box, the vendored MGA-Glide runner).
#
#   run.sh GAME [CARD] [-- GAME ARGS...]
#
#   GAME   quake | lq    Quake on the retail or the LibreQuake data: timedemo demo1
#          quake2        Quake 2: map base1
#   CARD   g450 (default), g400 or g200
# Environment: FRAMES (DGL_EXIT_AFTER, default 1500 / 600), STATS (DGL_STATS,
# default 1), SNAP (DGL_SNAP: frames to capture as out/NAME/files/F*.PPM, the
# same pictures on every run), SHOTS (screenshot seconds after boot), NAME (result directory
# out/NAME, default GAME-gl-CARD). Needs build/quake (tools/quake/build.sh) and
# the fixtures (tools/quake/fixtures.py). Screenshots of retail data stay local.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
game=${1:?usage: run.sh GAME [CARD] [-- ARGS...]}; shift
card=g450
if [ $# -gt 0 ] && [ "$1" != "--" ]; then card=$1; shift; fi
[ "${1:-}" = "--" ] && shift
hal=${MGAHAL_DIR:-$root/third_party/mgahal}     # MGAHAL_DIR: another copy of the harness
q=$root/build/quake
name=${NAME:-$game-gl-$card}
common=(--games-file "$here/games.json" --game "$game" --card "$card" --out "$root/out/$name"
        --pre "SET DGL_STATS=${STATS:-1}" --timeout 1800 --idle 300)
[ -n "${SNAP:-}" ] && common+=(--pre "SET DGL_SNAP=$SNAP")
case $game in
  quake|lq)
    args=${*:-"-nosound -nocdaudio -width 640 -height 480 +timedemo demo1"}
    set -- "${common[@]}" --file "$q/QDOSDGL.EXE=D:/QUAKE/QDOSDGL.EXE" \
      --pre "SET DGL_EXIT_AFTER=${FRAMES:-1500}" --shots "${SHOTS:-40,70,100,140}" \
      --cmd "D:" --cmd "CD \\QUAKE" --cmd "QDOSDGL.EXE $args" ;;
  quake2)
    args=${*:-"+set gl_mode 3 +set s_initsound 0 +map base1"}
    set -- "${common[@]}" --file "$q/Q2DGL.EXE=D:/QUAKE2/Q2DGL.EXE" \
      --file "$q/GAMEX86.DXE=D:/QUAKE2/BASEQ2/GAMEX86.DXE" --file "$q/DOSLFN.COM=D:/QUAKE2/DOSLFN.COM" \
      --pre "SET DGL_EXIT_AFTER=${FRAMES:-600}" --shots "${SHOTS:-35,50,65}" \
      --cmd "D:" --cmd "CD \\QUAKE2" --cmd "DOSLFN" --cmd "Q2DGL.EXE $args" ;;
  *) echo "run.sh: unknown game $game" >&2; exit 2 ;;
esac
cd "$root"
exec "$hal/tools/dev" python3 "$hal/tools/loopa/run.py" --name "$name" "$@"

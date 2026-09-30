#!/usr/bin/env bash
# Run Half-Life on DOS-GL in Loop A (86Box, the vendored MGA-Glide runner).
#
#   run.sh MODE [CARD] [-- HLDGL ARGS...]
#
#   MODE   boot     HLDGL -version, then a client start with the game
#          server   HLDGL -dedicated: skill.cfg, then MAP (default c1a0) for a
#                   few frames, and quit
#          maps     HLDGL -dedicated, the console on COM1 (HL_SERIAL): MAPS
#                   (default t0a0 c0a0 c1a0 c1a1) one after another, EVERY
#                   (default 250) frames each, then quit (HL_AT)
#   CARD   g450 (default), g400 or g200
# The engine's console output is in out/NAME/files/HL.TXT (and VER.TXT).
# Environment: MAP, SHOTS (screenshot seconds after boot), PRE (one more
# RUN.BAT line), NAME (result directory out/NAME, default hl-MODE-CARD),
# MGAHAL_DIR (another copy of the harness).
# Needs build/halflife (tools/halflife/build.sh) and the fixture
# (tools/halflife/fixtures.py). Screenshots of retail data stay local.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
mode=${1:?usage: run.sh MODE [CARD] [-- ARGS...]}; shift
card=g450
if [ $# -gt 0 ] && [ "$1" != "--" ]; then card=$1; shift; fi
[ "${1:-}" = "--" ] && shift
hal=${MGAHAL_DIR:-$root/third_party/mgahal}
# The DOS helpers Loop A puts in C:\HX (make sync-hal starts the tree afresh).
[ -f "$hal/build/ow/dos/UTEXIT.COM" ] || make -C "$root" -s dostools
b=$root/build/halflife
name=${NAME:-hl-$mode-$card}
common=(--games-file "$here/games.json" --game halflife --card "$card" --out "$root/out/$name"
        --pre "SET HL_SERIAL=1" --pre "SET HL_CRASHLOG=C:\\OUT\\CRASH.TXT"
        --file "$b/HLDGL.EXE=D:/HL/HLDGL.EXE" --file "$b/EXTRAS.PK3=D:/HL/VALVE/EXTRAS.PK3"
        --file "$b/DOSLFN.COM=D:/HL/DOSLFN.COM" --timeout 1800 --idle 300)
[ -n "${PRE:-}" ] && common+=(--pre "$PRE")
[ -n "${SHOTS:-}" ] && common+=(--shots "$SHOTS")
start=(--cmd "D:" --cmd "CD \\HL" --cmd "DOSLFN")
case $mode in
  boot)
    args=${*:-"-dev 2 -game valve"}
    set -- "${common[@]}" "${start[@]}" --cmd "HLDGL.EXE -version > C:\\OUT\\VER.TXT" \
      --cmd "HLDGL.EXE $args > C:\\OUT\\HL.TXT" ;;
  server)
    # skill.cfg first: a map given on the command line spawns before the
    # game's own exec of it runs
    args=${*:-"-dedicated -dev 2 -game valve +maxplayers 1 +exec skill.cfg +map ${MAP:-c1a0} +wait +wait +wait +wait +quit"}
    set -- "${common[@]}" "${start[@]}" --cmd "HLDGL.EXE $args > C:\\OUT\\HL.TXT" ;;
  maps)
    set -- ${MAPS:-t0a0 c0a0 c1a0 c1a1}
    first=$1; shift
    at="" f=0
    for m in "$@"; do f=$((f + ${EVERY:-250})); at="$at${at:+,}$f map $m"; done
    at="$at${at:+,}$((f + ${EVERY:-250})) quit"
    args="-dedicated -dev 1 -game valve +maxplayers 1 +exec skill.cfg +map $first"
    set -- "${common[@]}" --pre "SET HL_AT=$at" "${start[@]}" --cmd "HLDGL.EXE $args > C:\\OUT\\HL.TXT" ;;
  *) echo "run.sh: unknown mode $mode" >&2; exit 2 ;;
esac
cd "$root"
rc=0
"$hal/tools/dev" python3 "$hal/tools/loopa/run.py" --name "$name" "$@" || rc=$?
for f in VER.TXT HL.TXT; do
  p=$(find "$root/out/$name/files" -iname "$f" 2>/dev/null | head -1)
  [ -n "$p" ] && { echo "== $f"; sed 's/\x1b\[[0-9;]*m//g' "$p" | grep -v '^\s*$' | tail -${TAIL:-12}; }
done
exit $rc

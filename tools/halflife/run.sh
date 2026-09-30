#!/usr/bin/env bash
# Run Half-Life on DOS-GL in Loop A (86Box, the vendored MGA-Glide runner).
#
#   run.sh MODE [CARD] [-- HLDGL ARGS...]
#
#   MODE   boot     HLDGL -version, then a client start with the game
#          server   HLDGL -dedicated: skill.cfg, then MAP (default c1a0) for a
#                   few frames, and quit
#          map      the client, a local game on MAP (default c1a0) for FRAMES
#                   (default 600) frames, then quit; WIDTH x HEIGHT (default
#                   640x480), a fixed 1/50 s game step (host_framerate) so
#                   the frames SNAP names (DGL_SNAP) are the same every run;
#                   AT adds console commands at frame counts before the quit
#                   (HL_AT: "300 save h3,400 changelevel2 c1a0d c1a0toc1a0d")
#          play     keys mode with tools/halflife/keys/PLAY.keys (default h3play:
#                   the H3 exit, checked from the log afterwards)
#          keys     the client with KEYS typed into it (Loop A --keys:
#                   SECONDS:SCANCODE[:down|up],...), quitting after FRAMES
#                   (default 100000; menu frames are cheap) frames if nothing ends it sooner
#          maps     HLDGL -dedicated, the console on COM1 (HL_SERIAL): MAPS
#                   (default t0a0 c0a0 c1a0 c1a1) one after another, EVERY
#                   (default 250) frames each, then quit (HL_AT)
#   CARD   g450 (default), g400 or g200
# The engine's console output is in out/NAME/files/HL.TXT (and VER.TXT).
# Environment: MAP, MEM (the PC's RAM in MB, default 128), SOUND (86Box sound
# card, default sb16; none for no card), MOUSE (ps2 with CuteMouse, default;
# none), SHOTS (screenshot
# seconds after boot), PRE (one more
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
        --file "$b/DOSLFN.COM=D:/HL/DOSLFN.COM" --mem "${MEM:-128}" --timeout 1800 --idle 300)
# SOUND=none: no sound card (the PC otherwise has a Sound Blaster 16 at its defaults)
[ "${SOUND:-sb16}" != none ] && common+=(--sound "${SOUND:-sb16}" --pre "SET BLASTER=A220 I5 D1 H5 T6")
# MOUSE=none: no mouse (otherwise PS/2 with CuteMouse; KEYS may hold SECONDS:mouse:DX:DY[:BUTTONS])
common+=(--mouse "${MOUSE:-ps2}")
[ -n "${SNAP:-}" ] && common+=(--pre "SET DGL_SNAP=$SNAP")
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
  map)
    args=${*:-"-game valve -width ${WIDTH:-640} -height ${HEIGHT:-480} +host_framerate 0.02 +exec skill.cfg +map ${MAP:-c1a0}"}
    set -- "${common[@]}" --pre "SET HL_AT=${AT:+$AT,}${FRAMES:-600} quit" "${start[@]}" --cmd "HLDGL.EXE $args > C:\\OUT\\HL.TXT" ;;
  keys|play)
    if [ "$mode" = play ]; then
      # one item per line (@TEXT anchors keep their spaces), # comments
      KEYS=${KEYS:-$(sed 's/#.*//; s/^[[:space:]]*//; s/[[:space:]]*$//' "$here/keys/${PLAY:-h3play}.keys" | grep -v '^$' | paste -sd, -)}
    fi
    args=${*:-"-game valve +exec skill.cfg"}
    set -- "${common[@]}" --pre "SET HL_AT=${FRAMES:-100000} quit" --keys "${KEYS:?KEYS=SECONDS:SCANCODE,...}" \
      "${start[@]}" --cmd "HLDGL.EXE $args > C:\\OUT\\HL.TXT" --cmd "COPY VALVE\\CONFIG.CFG C:\\OUT > NUL" ;;
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
if [ "$mode" = play ] && [ "${PLAY:-h3play}" = h3play ]; then
  # the H3 exit: new game, quick save and load, the tram's landmark, quit;
  # the SB16's DMA advancing; the heap under the PC's RAM; config written
  log=$(sed 's/\x1b\[[0-9;]*m//g' "$root/out/$name/serial.log" 2>/dev/null)
  check() { if grep -qE "$2" <<<"$log"; then echo "play: ok   $1"; else echo "play: FAIL $1"; rc=1; fi; }
  check "new game (c0a0)" "HL-LOG execing maps/c0a0_load.cfg"
  check "quick save" "HL-LOG Saving game to save/quick.sav"
  check "quick load" "HL-LOG Loading game from save/quick.sav"
  check "landmark c0a0 -> c0a0a" "HL-LOG Spawn Server: c0a0a \[c0a0toa\]"
  check "quit" "HL-LOG >quit"
  check "SB16 DMA advancing" "HL-LOG Audio: Sound Blaster played [1-9][0-9]* samples"
  check "clean exit" "^HX-DONE 0"
  peak=$(grep -oE "heap_peak_kb=[0-9]+" <<<"$log" | tail -1 | cut -d= -f2 || true)
  if [ -n "$peak" ] && [ "$peak" -lt $(( ${MEM:-128} * 1024 )) ]; then echo "play: ok   heap peak ${peak} KB"; else echo "play: FAIL heap peak ${peak:-?} KB"; rc=1; fi
  if find "$root/out/$name/files" -iname CONFIG.CFG | grep -q .; then echo "play: ok   config.cfg written"; else echo "play: FAIL config.cfg"; rc=1; fi
fi
for f in VER.TXT HL.TXT; do
  p=$(find "$root/out/$name/files" -iname "$f" 2>/dev/null | head -1)
  if [ -n "$p" ] && [ "${TAIL:-12}" -gt 0 ]; then
    echo "== $f"; sed 's/\x1b\[[0-9;]*m//g' "$p" | grep -v '^\s*$' | tail -"${TAIL:-12}" || true
  fi
done
exit $rc

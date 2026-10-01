#!/usr/bin/env bash
# Run PrBoom-plus on DOS-GL in Loop A (86Box, the vendored MGA-Glide runner).
#
#   run.sh MODE [CARD] [-- PRBOOMP ARGS...]
#
#   MODE   nodraw   every built-in demo of the IWADs (DEMOS, default
#                   "DOOM:1 DOOM:2 DOOM:3 DOOM:4 DOOM2:1 DOOM2:2 DOOM2:3") as
#                   -timedemo -nodraw -nosound -checksum, one program run
#                   each; afterwards each one's gametics and final checksum
#                   are compared with tools/doom/reference.json, and the
#                   config written at exit and the absence of any video
#                   set-up are checked
#          timedemo the same demos shown and heard (SOUND defaults to sb16),
#                   each also written as DOSBench test <D1|D2>DEMO<N>
#                   (C:\OUT\RESULTS.TXT; frame DBSHOT, default 200, as
#                   L<ID>.PPM); GEOM=WxH picks the resolution; checked as
#                   nodraw, plus each test's DOSBench line
#          cmd      PRBOOMP.EXE with ARGS (default -iwad DOOM.WAD)
#   CARD   g450 (default), g400 or g200
# The program's output is in out/NAME/files/*.LOG and on the serial log
# (DOOM_SERIAL: DOOM-LOG lines between HX-START and HX-DONE).
# Environment: DEMOS, MEM (the PC's RAM in MB, default 64), SOUND (86Box
# sound card, default none; sb16 also sets BLASTER), MOUSE (none by default;
# ps2 with CuteMouse), WAV=1 (record the sound card to out/NAME/audio.wav),
# SHOTS (screenshot seconds after boot), PRE (one more
# RUN.BAT line), NAME (result directory out/NAME, default doom-MODE-CARD),
# NORUN=1 (check out/NAME from an earlier run without running again),
# MGAHAL_DIR (another copy of the harness), DOOM_BUILD (another build
# directory holding PRBOOMP.EXE and PRBOOM.WAD). DJGPP's long file names are off
# (LFN=n): the program must live with 8.3 names.
# Needs build/doom (tools/doom/build.sh) and the fixture (tools/doom/fixtures.py).
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
b=${DOOM_BUILD:-$root/build/doom}
[ -f "$b/PRBOOMP.EXE" ] || { echo "run.sh: no $b/PRBOOMP.EXE (tools/doom/build.sh)" >&2; exit 2; }
name=${NAME:-doom-$mode-$card}
common=(--games-file "$here/games.json" --game doom --card "$card" --out "$root/out/$name"
        --pre "SET DOOM_SERIAL=1" --pre "SET LFN=n"
        --file "$b/PRBOOMP.EXE=D:/DOOM/PRBOOMP.EXE" --file "$b/PRBOOM.WAD=D:/DOOM/PRBOOM.WAD"
        --mem "${MEM:-64}" --timeout 2400 --idle 300)
[ "$mode" = timedemo ] && SOUND=${SOUND:-sb16}
if [ "${SOUND:-none}" != none ]; then
  common+=(--sound "$SOUND" --pre "SET BLASTER=A220 I5 D1 H5 T6")
else
  common+=(--sound "")
fi
common+=(--mouse "${MOUSE:-none}")
[ -n "${WAV:-}" ] && common+=(--wav)
[ -n "${PRE:-}" ] && common+=(--pre "$PRE")
[ -n "${SHOTS:-}" ] && common+=(--shots "$SHOTS")
start=(--cmd "D:" --cmd "CD \\DOOM")
case $mode in
  nodraw)
    demos=${DEMOS:-DOOM:1 DOOM:2 DOOM:3 DOOM:4 DOOM2:1 DOOM2:2 DOOM2:3}
    cmds=()
    for d in $demos; do
      w=${d%:*} n=${d#*:}
      id=$( [ "$w" = DOOM2 ] && echo D2 || echo D1 )DEMO$n
      cmds+=(--cmd "PRBOOMP.EXE -iwad $w.WAD -timedemo demo$n -nodraw -nosound -checksum C:\\OUT\\$id.SUM $* > C:\\OUT\\$id.LOG")
    done
    set -- "${common[@]}" "${start[@]}" "${cmds[@]}" --cmd "COPY PRBOOM.CFG C:\\OUT > NUL" ;;
  timedemo)
    # every demo in DEMOS shown and heard (SOUND defaults to sb16 here) as
    # a timedemo, written as DOSBench test <IWAD><N> (C:\OUT\RESULTS.TXT,
    # frame DBSHOT (default 200) as L<ID>.PPM), with the game-state checksum
    demos=${DEMOS:-DOOM:1 DOOM:2 DOOM:3 DOOM:4 DOOM2:1 DOOM2:2 DOOM2:3}
    cmds=()
    for d in $demos; do
      w=${d%:*} n=${d#*:}
      id=$( [ "$w" = DOOM2 ] && echo D2 || echo D1 )DEMO$n
      cmds+=(--cmd "PRBOOMP.EXE -iwad $w.WAD ${GEOM:+-geom $GEOM }-timedemo demo$n -dosbench C:\\OUT $id -dbshot ${DBSHOT:-200} -checksum C:\\OUT\\$id.SUM $* > C:\\OUT\\$id.LOG")
    done
    set -- "${common[@]}" "${start[@]}" "${cmds[@]}" ;;
  cmd)
    args=${*:-"-iwad DOOM.WAD"}
    set -- "${common[@]}" "${start[@]}" --cmd "PRBOOMP.EXE $args > C:\\OUT\\DOOM.LOG" \
      --cmd "COPY PRBOOM.CFG C:\\OUT > NUL" ;;
  *) echo "run.sh: unknown mode $mode" >&2; exit 2 ;;
esac
cd "$root"
rc=0
# NORUN=1: only check an earlier run's results in out/NAME
[ -n "${NORUN:-}" ] || "$hal/tools/dev" python3 "$hal/tools/loopa/run.py" --name "$name" "$@" || rc=$?
out=$root/out/$name
if [ "$mode" = nodraw ] || [ "$mode" = timedemo ]; then
  MODE=$mode python3 - "$here/reference.json" "$out" $demos <<'EOF' || rc=1
import json, os, re, sys
ref = json.load(open(sys.argv[1]))["demos"]
out, demos = sys.argv[2], sys.argv[3:]
files = {f.upper(): os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(out, "files")) for f in fs}
serial = open(os.path.join(out, "serial.log"), "rb").read().decode("latin-1").replace("\r", "")
ok = True
def check(what, good, detail=""):
    global ok
    ok &= good
    print("%s: %-4s %s%s" % (os.environ["MODE"], "ok" if good else "FAIL", what, "  " + detail if detail else ""))
# one HX-START .. HX-DONE block per program run, in order
blocks = re.split(r"^(?=HX-START PRBOOMP)", serial, flags=re.M)[1:]
for i, d in enumerate(demos):
    w, n = d.split(":")
    key = "%s demo%s" % (w, n)
    sid = ("D2" if w == "DOOM2" else "D1") + "DEMO" + n
    blk = blocks[i] if i < len(blocks) else ""
    m = re.search(r"^DOOM-TIMED gametics=(\d+)", blk, re.M)
    tics = int(m.group(1)) if m else None
    s = files.get(sid + ".SUM")
    final = None
    if s:
        for line in open(s, errors="replace"):
            if line.startswith("final: "):
                final = line.split()[1]
    m = re.search(r"^HX-DONE (-?\d+)", blk, re.M)
    done = int(m.group(1)) if m else None
    want = ref.get(key, {})
    check("%-11s gametics %s, final %s, exit %s" % (key, tics, final, done),
          tics == want.get("gametics") and final == want.get("final") and done == 0,
          "" if tics == want.get("gametics") and final == want.get("final")
          else "want %s %s" % (want.get("gametics"), want.get("final")))
if os.environ["MODE"] == "nodraw":
    check("config written (PRBOOM.CFG)", "PRBOOM.CFG" in files)
    check("no video set-up", not re.search(r"I_UpdateVideoMode|I_InitGraphics", serial))
else:
    # the DOSBench lines: frames, fps and the captured frame's CRC per test
    res = files.get("RESULTS.TXT")
    tl = [l.split() for l in open(res) if l.startswith("T ")] if res else []
    for d in demos:
        w, n = d.split(":")
        sid = ("D2" if w == "DOOM2" else "D1") + "DEMO" + n
        t = [dict(x.split("=", 1) for x in l[1:] if "=" in x) for l in tl if "test=" + sid in l]
        if t:
            t = t[-1]
            print("timedemo: %-8s frames=%s fps=%s p99_ms=%s crc=%s" % (sid, t.get("frames"), t.get("fps"), t.get("p99_ms"), t.get("crc")))
        check("%s DOSBench line" % sid, bool(t))
sys.exit(0 if ok else 1)
EOF
fi
for f in DOOM.LOG; do
  p=$(find "$out/files" -iname "$f" 2>/dev/null | head -1)
  if [ -n "$p" ] && [ "${TAIL:-12}" -gt 0 ]; then
    echo "== $f"; grep -v '^\s*$' "$p" | tail -"${TAIL:-12}" || true
  fi
done
exit $rc

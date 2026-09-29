#!/usr/bin/env python3
"""Conformance suite (PRD §11.5, D16): each test's DOS build runs in 86Box
(Loop A) on the chosen card, its host build renders the reference with Mesa
OSMesa, and the frames are compared (RGB565 quantisation, edge masks,
per-test tolerances from tests/conform/manifest.json).

Runs inside the dev container (make conform CARD=g450 [TESTS="t01 t02"]
[PRE="SET DGL_GUARD_PX=800"], the last a RUN.BAT line before every test).
Results: out/conform/<card>/<test>/ (serial.log, frames, diffs, result.json)
and out/conform/summary-<card>.json. Exit status 0 when every test passes.
"""
import argparse
import concurrent.futures as cf
import glob
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MGAHAL = os.path.join(ROOT, "third_party", "mgahal")
sys.path.insert(0, os.path.join(MGAHAL, "tools"))
sys.path.insert(0, os.path.join(MGAHAL, "tools", "loopa"))
import imgcmp  # noqa: E402
import png  # noqa: E402

MANIFEST = json.load(open(os.path.join(ROOT, "tests", "conform", "manifest.json")))


def tests_available():
    return sorted(os.path.basename(p).split("_")[0] for p in glob.glob(os.path.join(ROOT, "tests/conform/t[0-9]*_*.c")))


PRE = []                                            # --pre: RUN.BAT lines for every test


def run_dos(test, card, out):
    exe = os.path.join(ROOT, "build", "exe", "conform", test.upper() + ".EXE")
    cmd = [sys.executable, os.path.join(MGAHAL, "tools", "loopa", "run.py"), "--name", "%s-%s" % (card, test),
           "--exe", exe, "--card", card, "--out", out, "--timeout", "240", "--idle", "90"]
    for line in PRE + MANIFEST.get(test, {}).get("pre", []):   # RUN.BAT lines (e.g. SET DGL_...)
        cmd += ["--pre", line]
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.load(open(os.path.join(out, "result.json")))


def run_ref(test, out):
    os.makedirs(out, exist_ok=True)
    exe = os.path.join(ROOT, "build", "host", "conform", test)
    subprocess.run([exe], env=dict(os.environ, CT_OUT=out), check=True, stdout=subprocess.DEVNULL)
    for p in glob.glob(os.path.join(out, "*.ppm")):
        w, h, rgb = png.read_ppm(p)
        png.write_png(p[:-4] + ".png", w, h, rgb)


def check(test, card):
    out = os.path.join(ROOT, "out", "conform", card, test)
    ref = os.path.join(ROOT, "out", "conform", "ref", test)
    run_ref(test, ref)
    res = run_dos(test, card, out)
    report = {"test": test, "card": card, "run": res["status"], "frames": {}}
    ok = res["status"] == "PASS"
    for rp in sorted(glob.glob(os.path.join(ref, "*.png"))):
        name = os.path.basename(rp)[:-4]
        got = os.path.join(out, name + ".png")
        m = dict(MANIFEST.get("_default", {}), **MANIFEST.get(name, {}))
        if not os.path.exists(got):
            report["frames"][name] = {"ok": False, "reason": "missing"}
            ok = False
            continue
        c = imgcmp.compare(rp, got, m.get("tol", 24), m.get("frac", 0.005), m.get("edge", True),
                           False, os.path.join(out, name + ".diff.png"), m.get("ignore", ()))
        report["frames"][name] = c
        ok = ok and c["ok"]
    report["ok"] = ok
    json.dump(report, open(os.path.join(out, "check.json"), "w"), indent=1)
    return report


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("tests", nargs="*")
    ap.add_argument("--card", default=os.environ.get("CARD", "g450"))
    ap.add_argument("--jobs", type=int, default=int(os.environ.get("LOOPA_JOBS", "4")))
    ap.add_argument("--pre", action="append", default=[], help="RUN.BAT line before every test (SET DGL_...)")
    a = ap.parse_args()
    PRE.extend(a.pre)
    tests = a.tests or tests_available()
    with cf.ThreadPoolExecutor(a.jobs) as ex:
        reports = list(ex.map(lambda t: check(t, a.card), tests))
    for r in reports:
        worst = max([f.get("frac", 1.0) or 0 for f in r["frames"].values()] or [1.0])
        print("conform %-4s %-5s %s run=%-9s frames=%d worst-frac=%.4f" % (
            r["test"], a.card, "PASS" if r["ok"] else "FAIL", r["run"], len(r["frames"]), worst))
    json.dump(reports, open(os.path.join(ROOT, "out", "conform", "summary-%s.json" % a.card), "w"), indent=1)
    return 0 if all(r["ok"] for r in reports) else 1


if __name__ == "__main__":
    sys.exit(main())

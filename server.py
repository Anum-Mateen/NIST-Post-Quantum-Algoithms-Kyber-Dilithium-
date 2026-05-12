"""
server.py  —  PQC Benchmark Live Backend
==========================================
Place this file in the SAME folder as:
  main.cpp, kyber.h, dilithium.h

Install dependencies (run once):
  pip install flask flask-cors

Run:
  python server.py

Then open dashboard.html in your browser.
"""

import os
import csv
import shutil
import subprocess
import threading
import time
import platform
import random
from pathlib import Path
from flask import Flask, jsonify, request
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

# ── Paths ───────────────────────────────────────────────────────────────────
BASE_DIR = Path(__file__).parent
CSV_FILE = BASE_DIR / "results.csv"
IS_WIN   = platform.system() == "Windows"

# All the names your exe might have been compiled as (CMake or manual g++)
EXE_CANDIDATES = [
    BASE_DIR / "build" / "Debug"   / "qs_mini_project_cpp.exe",
    BASE_DIR / "build" / "Release" / "qs_mini_project_cpp.exe",
    BASE_DIR / "build" / "qs_mini_project_cpp.exe",
    BASE_DIR / "build" / "Debug"   / "qs_mini_project_cpp",
    BASE_DIR / "build" / "Release" / "qs_mini_project_cpp",
    BASE_DIR / "build" / "qs_mini_project_cpp",
    BASE_DIR / "build" / "Debug"   / "pqc_bench.exe",
    BASE_DIR / "build" / "Release" / "pqc_bench.exe",
    BASE_DIR / "build" / "pqc_bench.exe",
    BASE_DIR / "pqc_bench.exe",
    BASE_DIR / "pqc_bench",
]

# Where WE put the exe when compiling with g++ ourselves
G_EXE = BASE_DIR / ("pqc_bench.exe" if IS_WIN else "pqc_bench")
SRC   = BASE_DIR / "main.cpp"


def find_exe():
    """Return path to any existing exe, or None."""
    if G_EXE.exists():
        return G_EXE
    for p in EXE_CANDIDATES:
        if p.exists():
            return p
    return None


def find_compiler():
    for c in ["g++", "clang++"]:
        if shutil.which(c):
            return c
    return None


COMPILER = find_compiler()

# ── State ────────────────────────────────────────────────────────────────────
state = {
    "running":    False,
    "progress":   0,
    "phase":      "idle",
    "log":        [],
    "last_run":   None,
    "build_ok":   None,
    "error":      None,
}
lock = threading.Lock()


def log(msg):
    with lock:
        state["log"].append(str(msg))
    print(msg)


# ── Build with g++ ────────────────────────────────────────────────────────────
def build_with_gpp():
    if not COMPILER:
        log("ERROR: No C++ compiler found on PATH.")
        log("       Windows: install MinGW-w64 and add bin/ to PATH.")
        log("       Linux/Mac: sudo apt install g++  or  brew install gcc")
        with lock:
            state["error"]    = "No g++ compiler found. See log."
            state["build_ok"] = False
        return False

    if not SRC.exists():
        log(f"ERROR: {SRC} not found.")
        with lock:
            state["error"]    = f"main.cpp not found at {SRC}"
            state["build_ok"] = False
        return False

    cmd = [COMPILER, "-O2", "-std=c++17", str(SRC), "-o", str(G_EXE)]
    if IS_WIN:
        cmd += ["-static-libgcc", "-static-libstdc++"]

    log(f"Compiling: {' '.join(cmd)}")
    with lock:
        state["phase"]    = "building"
        state["progress"] = 10

    try:
        r = subprocess.run(cmd, cwd=str(BASE_DIR),
                           capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired:
        log("ERROR: Compilation timed out.")
        with lock:
            state["error"] = "Compile timeout."
            state["build_ok"] = False
        return False
    except Exception as e:
        log(f"ERROR: {e}")
        with lock:
            state["error"] = str(e)
            state["build_ok"] = False
        return False

    if r.returncode != 0:
        log("Compilation FAILED:")
        for line in r.stderr.splitlines():
            log("  " + line)
        with lock:
            state["error"]    = "Compilation failed — see log."
            state["build_ok"] = False
        return False

    log(f"Build succeeded -> {G_EXE}")
    with lock:
        state["build_ok"] = True
        state["error"]    = None
        state["progress"] = 25
    return True


# ── Run exe ───────────────────────────────────────────────────────────────────
def run_exe(exe_path, runs, seed):
    env = os.environ.copy()
    env["PQC_SEED"] = str(seed)

    log(f"Running: {exe_path}  runs={runs}  seed={seed}")
    with lock:
        state["phase"]    = "running"
        state["progress"] = 30

    try:
        r = subprocess.run(
            [str(exe_path), str(runs)],
            cwd=str(BASE_DIR),
            capture_output=True, text=True,
            timeout=600, env=env,
        )
    except subprocess.TimeoutExpired:
        log("ERROR: Benchmark timed out (>10 min).")
        with lock:
            state["error"] = "Benchmark timeout."
        return False
    except Exception as e:
        log(f"ERROR: {e}")
        with lock:
            state["error"] = str(e)
        return False

    for line in r.stdout.splitlines():
        log(line)

    if r.returncode != 0:
        for line in r.stderr.splitlines():
            log("STDERR: " + line)
        with lock:
            state["error"] = f"Exe exited with code {r.returncode}."
        return False

    with lock:
        state["last_run"] = time.strftime("%Y-%m-%dT%H:%M:%S")
        state["error"]    = None
        state["progress"] = 100
    log("Done — results.csv updated.")
    return True


# ── Full pipeline ─────────────────────────────────────────────────────────────
def pipeline(runs):
    seed = random.randint(1, 2**31 - 1)

    with lock:
        state.update(running=True, progress=0, phase="idle", error=None,
                     log=[f"=== Benchmark request: {runs} runs, seed={seed} ==="])

    try:
        exe = find_exe()

        if exe is None:
            log("No compiled exe found anywhere. Attempting auto-compile with g++...")
            ok = build_with_gpp()
            if not ok:
                return
            exe = G_EXE
        else:
            log(f"Found existing exe: {exe}")
            with lock:
                state["progress"] = 25

        run_exe(exe, runs, seed)

    finally:
        with lock:
            state["running"] = False
            state["phase"]   = "idle"


# ── CSV reader ─────────────────────────────────────────────────────────────────
def read_csv():
    if not CSV_FILE.exists():
        return []
    rows = []
    with open(CSV_FILE, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            rows.append({
                "algorithm":     row["Algorithm"],
                "operation":     row["Operation"],
                "mean_us":       float(row["Mean_us"]),
                "stddev_us":     float(row["StdDev_us"]),
                "min_us":        float(row["Min_us"]),
                "max_us":        float(row["Max_us"]),
                "keysize_bytes": int(row["KeySize_bytes"]),
            })
    return rows


# ── Routes ────────────────────────────────────────────────────────────────────

@app.route("/api/status")
def api_status():
    with lock:
        s = dict(state)
    exe = find_exe()
    s["compiler"]   = COMPILER
    s["exe_found"]  = exe is not None
    s["exe_path"]   = str(exe) if exe else None
    s["src_exists"] = SRC.exists()
    s["csv_exists"] = CSV_FILE.exists()
    return jsonify(s)


@app.route("/api/data")
def api_data():
    rows = read_csv()
    if not rows:
        return jsonify({"error": "results.csv not found or empty. Run the benchmark first."}), 404
    kyber = [r for r in rows if r["algorithm"] == "Kyber"]
    dil   = [r for r in rows if r["algorithm"] == "Dilithium"]
    mtime = time.strftime("%Y-%m-%dT%H:%M:%S",
                          time.localtime(CSV_FILE.stat().st_mtime)) if CSV_FILE.exists() else None
    return jsonify({"rows": rows, "kyber": kyber, "dilithium": dil, "last_modified": mtime})


@app.route("/api/run", methods=["POST"])
def api_run():
    with lock:
        if state["running"]:
            return jsonify({"error": "Already running. Please wait."}), 409
    body = request.get_json(silent=True) or {}
    runs = int(body.get("runs", 50))
    if not (1 <= runs <= 500):
        return jsonify({"error": "runs must be 1-500."}), 400
    threading.Thread(target=pipeline, args=(runs,), daemon=True).start()
    return jsonify({"message": f"Started ({runs} runs).", "runs": runs})


@app.route("/api/log")
def api_log():
    with lock:
        return jsonify({"log": list(state["log"])})


# ── Startup ───────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    exe = find_exe()
    print("=" * 60)
    print("  PQC Benchmark Server")
    print("=" * 60)
    print(f"  Folder   : {BASE_DIR}")
    print(f"  main.cpp : {'found' if SRC.exists() else 'NOT FOUND'}")
    print(f"  Compiler : {COMPILER or 'NOT FOUND (g++ not on PATH)'}")
    if exe:
        print(f"  Exe      : found at {exe}")
        print(f"             Ready to run — just click Run in the dashboard.")
    else:
        print(f"  Exe      : not found yet")
        if COMPILER:
            print(f"             Will auto-compile on first Run click.")
        else:
            print(f"             Build in VSCode (F7) OR install g++.")
    print()
    print("  API:  GET /api/status  GET /api/data")
    print("        POST /api/run {runs:50}  GET /api/log")
    print()
    print("  Open dashboard.html in your browser.")
    print("=" * 60)
    app.run(host="0.0.0.0", port=5000, debug=False)
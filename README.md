# PQC PROJECT — SETUP & EXECUTION GUIDE
## Kyber KEM vs Dilithium Digital Signature Benchmark
### For: Students, Researchers, Future Batches

---

## WHAT THIS PROJECT DOES

A C++ benchmark that:
- Implements **Kyber-512** (Key Encapsulation Mechanism)
- Implements **Dilithium2** (Digital Signature Scheme)
- Measures execution time, key sizes, and correctness
- Outputs `results.csv` for analysis

A Python script that generates **4 research-quality graphs** from the CSV.

---

## PREREQUISITES

### 1. Visual Studio Build Tools 2022 (C++ compiler)
Download (free, ~6 GB):
```
https://aka.ms/vs/17/release/vs_BuildTools.exe
```
During install, check only:
- ✅ **Desktop development with C++**

(You do NOT need the full Visual Studio IDE)

### 2. CMake
Download the `.msi` installer:
```
https://cmake.org/download/
```
During install:
- ✅ **Add CMake to the system PATH for all users**

Verify after install — open any terminal and run:
```
cmake --version
```
Should print `cmake version 3.x.x`

### 3. VSCode Extensions (install from Extensions tab `Ctrl+Shift+X`)
- `C/C++` — by Microsoft
- `CMake Tools` — by Microsoft
- `CMake` — by twxs (syntax highlighting)

### 4. Python 3
Download: https://www.python.org/downloads/
- ✅ Check **"Add Python to PATH"** during install

Then install libraries:
```
pip install matplotlib pandas numpy
```

---

## FOLDER STRUCTURE

```
qs_mini_project_ccp/
├── kyber.h          ← Kyber KEM implementation
├── dilithium.h      ← Dilithium Signature implementation
├── main.cpp         ← Benchmark + timing + CSV output
├── graphs.py        ← Python graphing script
├── CMakeLists.txt   ← Build config
├── README.md        ← This file
└── build/           ← Auto-created by CMake
    └── Debug/
        └── qs_mini_project_cpp.exe
```

---

## STEP-BY-STEP: BUILD & RUN

### Step 1 — Open project in VSCode
```
File → Open Folder → select your project folder
```

### Step 2 — Select Kit
Press `Ctrl+Shift+P` → type:
```
CMake: Select Kit
```
Choose: **Visual Studio Build Tools 2022 Release - amd64**

### Step 3 — Configure
Press `Ctrl+Shift+P` → type:
```
CMake: Configure
```
Wait for it to finish (bottom bar will stop spinning).

### Step 4 — Build
Press **F7**

You should see:
```
[100%] Built target qs_mini_project_cpp
Build finished with exit code 0
```

### Step 5 — Run the benchmark
Open terminal in VSCode `` Ctrl+` `` and run:
```
.\build\Debug\qs_mini_project_cpp.exe
```

Expected output:
```
**********************************************************************
  PQC BENCHMARK: Kyber KEM vs Dilithium Digital Signature
**********************************************************************

======================================================================
  KYBER KEM BENCHMARK  (runs=2)
======================================================================
  Correctness (enc==dec): PASS ✓
  ...

======================================================================
  DILITHIUM SIGNATURE BENCHMARK  (runs=2)
======================================================================
  Correctness (valid msg)   : PASS ✓
  Correctness (tampered msg): PASS ✓
  ...

[CSV] Results saved to: results.csv
```

### Step 6 — Generate graphs
In the same terminal:
```
python graphs.py
```

This creates a `graphs/` folder with 4 PNG files ready for your report.

---

---

## OPTIONAL: LIVE DASHBOARD (WEB INTERFACE)

A live interactive dashboard is included for viewing benchmark results in real time.

### How to use it — step by step

### Step 1 — Install Python dependencies (one time only)
Open a terminal in your project folder and run:

```bash
pip install flask flask-cors
```

### Step 2 — Build your C++ project
Press **F7** in VSCode like normal.

The server will automatically find the `.exe` inside your `build/` folder.

### Step 3 — Start the server
Run:

```bash
python server.py
```

You will see the server address and whether the benchmark `.exe` was detected.

**Leave this terminal open while using the dashboard.**

### Step 4 — Open the dashboard
Open:

```text
dashboard.html
```

in your browser (double-click the file).

That's it.

---

## WHAT THE LIVE DASHBOARD DOES

### Auto-refresh every 5 seconds
A small ring animation in the top-right counts down.

Every 5 seconds the dashboard checks whether `results.csv` changed.

If the CSV updates:
- ✅ All charts refresh automatically
- ✅ Tables update instantly
- ✅ No page reload needed

### Run button
Choose the number of benchmark runs:
- `2`
- `10`
- `50`
- `100`

Or enter any custom number.

Click:

```text
▶ Run Benchmark
```

A progress panel appears showing live log output from the C++ executable.

When execution finishes:
- Results are written to `results.csv`
- All charts automatically refresh

### Status pill
The dashboard shows current system status:

| Status | Meaning |
|--------|----------|
| 🟢 **Server ready** | System is ready |
| 🟠 **Running…** | Benchmark currently executing |
| 🔴 **Error** | Something failed |

### Live computed values
All dashboard values are calculated directly from the current `results.csv`.

This includes:
- Comparison ratios (e.g., `Kyber 34× faster`)
- Metric cards
- Size comparison bars
- Benchmark statistics

Everything recalculates dynamically whenever the CSV changes.

---

## CMakeLists.txt (verify yours matches this)

```cmake
cmake_minimum_required(VERSION 3.15)
project(qs_mini_project_cpp)
set(CMAKE_CXX_STANDARD 17)
add_executable(qs_mini_project_cpp main.cpp)
```

---

## CHANGING NUMBER OF RUNS

For report-quality data, use 50 runs. Edit `main.cpp`:
```cpp
int runs = 50;   // change this number
```
Then F7 to rebuild, then run again.

Or pass as argument:
```
.\build\Debug\qs_mini_project_cpp.exe 50
```

---

## OUTPUT FILES FOR REPORT

| File | Use for |
|------|---------|
| `results.csv` | Table I and Table II in IEEE report |
| `graphs/01_time_comparison.png` | Figure 1 — Execution time comparison |
| `graphs/02_size_comparison.png` | Figure 2 — Key & artifact sizes |
| `graphs/03_per_operation.png` | Figure 3 — Per-operation breakdown |
| `graphs/04_variability.png` | Figure 4 — Performance consistency |

---

## TROUBLESHOOTING

**`cmake --version` not found after install**
→ Restart VSCode completely (close all windows, reopen)

**"No Kit Selected" or kit not found**
→ Make sure Visual Studio Build Tools 2022 is installed
→ `Ctrl+Shift+P` → `CMake: Scan for Kits` → try Select Kit again

**Build fails with C2338 or type errors**
→ Make sure you are using the latest `dilithium.h` from this project
→ Do NOT use the original version — it has been rewritten for MSVC

**`.exe` runs but shows wrong path for results.csv**
→ The CSV is saved relative to where you run the `.exe` from
→ Always run from the terminal inside VSCode (not by double-clicking)

**`python graphs.py` says `results.csv` not found**
→ Make sure you ran the `.exe` first and it printed `[CSV] Results saved`
→ Run `python graphs.py` from the same folder as `results.csv`

**ModuleNotFoundError: matplotlib**
→ Run: `pip install matplotlib pandas numpy`

**Need to stop the Flask server?**

If the server is running and you want to stop it:

### Method 1 — Terminal (Recommended)
In the terminal where the server is running:

```text
Ctrl + C
```

This cleanly shuts down the Flask server.

### Method 2 — Task Manager
Open:

```text
Ctrl + Shift + Esc
```

Then:
- Find `python.exe`
- Right-click
- Select **End Task**

### Method 3 — Command Line
Open a new terminal and run:

```bash
taskkill /f /im python.exe
```

---

### Example successful shutdown

```text
✅ Server Stopped Successfully!
The Flask server has been stopped.

Found the running server:
- Running on port 5000
- Process ID detected

Terminated the process:
- Used taskkill to force stop

Verified shutdown:
- Port 5000 no longer listening
```

---

### What `server.py` does

The `server.py` file creates a local web server that:

- Runs on `http://localhost:5000`
- Provides a live dashboard for PQC benchmarks
- Auto-refreshes every 5 seconds
- Allows benchmarks to run from a web interface

If you only want to run the **C++ benchmark + Python graphs**, you can safely ignore this file.

The main benchmark system works perfectly without the dashboard server.

---

## ALGORITHM REFERENCE (Viva Prep)

### KYBER — Key Encapsulation Mechanism (KEM)
| Step | What happens |
|------|-------------|
| KeyGen | Generate public/private key pair from random secret + LWE noise |
| Encapsulate | Use public key → produce (ciphertext, shared secret) |
| Decapsulate | Use private key + ciphertext → recover shared secret |

- Based on: **Module Learning With Errors (MLWE)**
- Security: **IND-CCA2** (safe against chosen-ciphertext attacks)
- Use case: TLS key exchange, secure channels, VPNs

### DILITHIUM — Digital Signature Scheme
| Step | What happens |
|------|-------------|
| KeyGen | Generate keys from secret vectors s1, s2 |
| Sign | sk + message → signature (Fiat-Shamir with clamped randomness) |
| Verify | pk + message + signature → true / false |

- Based on: **Module-LWE + Module-SIS**
- Security: **EUF-CMA** (existentially unforgeable under chosen message attack)
- Use case: Document signing, firmware signing, certificate authorities

### SHARED FOUNDATION
- Both use polynomial rings: **Zq[X]/(X^N + 1)**
- Both standardized by **NIST in 2024**: FIPS 203 (Kyber), FIPS 204 (Dilithium)
- Both **resist quantum computers** (Shor's algorithm, Grover's algorithm)
- Key sizes larger than RSA/ECC but remain practical for real-world use

### WHY KYBER IS FASTER THAN DILITHIUM (for your analysis section)
- Kyber KEM is a simpler structure — one matrix multiply per operation
- Dilithium sign builds a large challenge string from w1 (256×4 coefficients)
- Dilithium verify recomputes A×z which is K×L polynomial multiplications
- Both use O(N²) naive poly-mul in this implementation (NTT = 10-100× faster)

---

*Project by: Anum*
*For future batches: read `kyber.h` and `dilithium.h` top-to-bottom — every step is commented.*

#!/usr/bin/env python3
"""
graphs.py — Generate research graphs from pqc_bench results.csv
Run: python3 graphs.py
Requires: pip install matplotlib pandas numpy
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
import os

CSV_FILE = "results.csv"
OUT_DIR  = "graphs"
os.makedirs(OUT_DIR, exist_ok=True)

# ── Style ──────────────────────────────────────────────────────────────────
plt.rcParams.update({
    "font.family":  "DejaVu Sans",
    "figure.dpi":   150,
    "axes.spines.top":   False,
    "axes.spines.right": False,
    "axes.grid": True,
    "grid.alpha": 0.3,
})

COLORS = {"Kyber": "#2563EB", "Dilithium": "#DC2626"}

def load_data():
    df = pd.read_csv(CSV_FILE)
    print(f"[INFO] Loaded {len(df)} rows from {CSV_FILE}")
    print(df.to_string(index=False))
    return df

# ── Graph 1: Mean execution time comparison (grouped bar) ──────────────────
def graph_time_comparison(df):
    ops_map = {
        "Kyber":     ["KeyGen", "Encapsulate", "Decapsulate"],
        "Dilithium": ["KeyGen", "Sign",        "Verify"],
    }
    labels = ["KeyGen", "Core Op\n(Enc/Sign)", "Secondary Op\n(Dec/Verify)"]

    fig, ax = plt.subplots(figsize=(10, 6))
    x = np.arange(len(labels))
    width = 0.35

    for i, (algo, ops) in enumerate(ops_map.items()):
        means  = []
        errors = []
        for op in ops:
            row = df[(df["Algorithm"] == algo) & (df["Operation"] == op)]
            means.append(row["Mean_us"].values[0] if len(row) else 0)
            errors.append(row["StdDev_us"].values[0] if len(row) else 0)

        offset = (i - 0.5) * width
        bars = ax.bar(x + offset, means, width, label=algo,
                      color=COLORS[algo], alpha=0.85,
                      yerr=errors, capsize=5, error_kw={"elinewidth":1.5})

        for bar, m in zip(bars, means):
            ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + max(errors)*0.05,
                    f"{m:.0f}", ha="center", va="bottom", fontsize=9, fontweight="bold")

    ax.set_xlabel("Operation", fontsize=12)
    ax.set_ylabel("Time (µs)", fontsize=12)
    ax.set_title("Kyber vs Dilithium — Execution Time Comparison\n(mean ± std dev, 50 runs)",
                 fontsize=14, fontweight="bold")
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=11)
    ax.legend(fontsize=11)
    plt.tight_layout()
    path = f"{OUT_DIR}/01_time_comparison.png"
    plt.savefig(path)
    plt.close()
    print(f"[SAVED] {path}")

# ── Graph 2: Key & artifact sizes ──────────────────────────────────────────
def graph_size_comparison(df):
    metrics = {
        "Kyber":     {
            "KeyGen":      ("Public Key",    df),
            "Encapsulate": ("Ciphertext",    df),
            "Decapsulate": ("Private Key",   df),
        },
        "Dilithium": {
            "KeyGen":      ("Public Key",    df),
            "Sign":        ("Signature",     df),
            "Verify":      ("Public Key",    df),
        }
    }

    size_labels  = ["Public Key", "Ciphertext /\nSignature", "Private Key"]
    kyber_sizes  = []
    dil_sizes    = []

    ops_k = ["KeyGen", "Encapsulate", "Decapsulate"]
    ops_d = ["KeyGen", "Sign",        "Verify"]

    for op in ops_k:
        row = df[(df["Algorithm"] == "Kyber") & (df["Operation"] == op)]
        kyber_sizes.append(row["KeySize_bytes"].values[0] if len(row) else 0)
    for op in ops_d:
        row = df[(df["Algorithm"] == "Dilithium") & (df["Operation"] == op)]
        dil_sizes.append(row["KeySize_bytes"].values[0] if len(row) else 0)

    x = np.arange(len(size_labels))
    width = 0.35
    fig, ax = plt.subplots(figsize=(10, 6))

    b1 = ax.bar(x - width/2, kyber_sizes,  width, label="Kyber",     color=COLORS["Kyber"],     alpha=0.85)
    b2 = ax.bar(x + width/2, dil_sizes,    width, label="Dilithium", color=COLORS["Dilithium"], alpha=0.85)

    for bars in [b1, b2]:
        for bar in bars:
            h = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2, h + 10,
                    f"{int(h)}B", ha="center", va="bottom", fontsize=9, fontweight="bold")

    ax.set_xlabel("Artifact", fontsize=12)
    ax.set_ylabel("Size (bytes)", fontsize=12)
    ax.set_title("Kyber vs Dilithium — Key & Artifact Sizes", fontsize=14, fontweight="bold")
    ax.set_xticks(x)
    ax.set_xticklabels(size_labels, fontsize=11)
    ax.legend(fontsize=11)
    plt.tight_layout()
    path = f"{OUT_DIR}/02_size_comparison.png"
    plt.savefig(path)
    plt.close()
    print(f"[SAVED] {path}")

# ── Graph 3: Stacked total overhead per scheme ─────────────────────────────
def graph_total_overhead(df):
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    for ax, algo, ops in zip(axes,
                              ["Kyber",     "Dilithium"],
                              [["KeyGen","Encapsulate","Decapsulate"],
                               ["KeyGen","Sign","Verify"]]):
        means = []
        for op in ops:
            row = df[(df["Algorithm"] == algo) & (df["Operation"] == op)]
            means.append(row["Mean_us"].values[0] if len(row) else 0)

        # Create bars with gradient effect by varying alpha individually
        bars = []
        alphas = [0.6, 0.85, 1.0]
        for i, (op, mean, alpha_val) in enumerate(zip(ops, means, alphas)):
            bar = ax.bar(i, mean, color=COLORS[algo], alpha=alpha_val, 
                        edgecolor="white", linewidth=1.5)
            bars.append(bar)
            ax.text(i, mean * 0.5, f"{mean:.0f}µs", ha="center", va="center",
                   color="white", fontsize=11, fontweight="bold")

        ax.set_title(f"{algo} — Per-Operation Time", fontsize=13, fontweight="bold")
        ax.set_ylabel("Mean Time (µs)")
        ax.set_ylim(0, max(means) * 1.25)
        ax.set_xticks(range(len(ops)))
        ax.set_xticklabels(ops)

    plt.suptitle("Per-Operation Timing Breakdown", fontsize=15, fontweight="bold", y=1.02)
    plt.tight_layout()
    path = f"{OUT_DIR}/03_per_operation.png"
    plt.savefig(path)
    plt.close()
    print(f"[SAVED] {path}")

# ── Graph 4: Error bar chart (variability analysis) ────────────────────────
def graph_variability(df):
    fig, ax = plt.subplots(figsize=(12, 6))

    all_labels = []
    all_means  = []
    all_stds   = []
    all_colors = []

    for algo, ops in [("Kyber",["KeyGen","Encapsulate","Decapsulate"]),
                      ("Dilithium",["KeyGen","Sign","Verify"])]:
        for op in ops:
            row = df[(df["Algorithm"] == algo) & (df["Operation"] == op)]
            if len(row):
                all_labels.append(f"{algo}\n{op}")
                all_means.append(row["Mean_us"].values[0])
                all_stds.append(row["StdDev_us"].values[0])
                all_colors.append(COLORS[algo])

    x = np.arange(len(all_labels))
    ax.bar(x, all_means, color=all_colors, alpha=0.75,
           yerr=all_stds, capsize=7, error_kw={"elinewidth":2, "ecolor":"#374151"})

    ax.set_xticks(x)
    ax.set_xticklabels(all_labels, fontsize=10)
    ax.set_ylabel("Time (µs)", fontsize=12)
    ax.set_title("Performance Variability: Mean ± Std Dev\n(lower std = more consistent)",
                 fontsize=13, fontweight="bold")

    kyber_patch = mpatches.Patch(color=COLORS["Kyber"],     label="Kyber")
    dil_patch   = mpatches.Patch(color=COLORS["Dilithium"], label="Dilithium")
    ax.legend(handles=[kyber_patch, dil_patch], fontsize=11)

    plt.tight_layout()
    path = f"{OUT_DIR}/04_variability.png"
    plt.savefig(path)
    plt.close()
    print(f"[SAVED] {path}")

# ── Main ───────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    if not os.path.exists(CSV_FILE):
        print(f"[ERROR] {CSV_FILE} not found. Run pqc_bench first.")
        exit(1)

    df = load_data()
    graph_time_comparison(df)
    graph_size_comparison(df)
    graph_total_overhead(df)
    graph_variability(df)

    print(f"\n[DONE] All graphs saved in ./{OUT_DIR}/")
    print("       Use these images in your IEEE report figures.")
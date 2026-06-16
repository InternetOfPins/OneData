#!/usr/bin/env python3
"""Plot OneData benchmark results from benchmark/results/*.log
Usage: python3 benchmark/plot_bench.py [--all]
  --all  include runs where flat_chain test was broken (pre-fix)
"""

import re, sys
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

RESULTS_DIR = Path(__file__).parent / "results"
PROBES = ["baseline", "flat_chain", "watch_stack", "find_first", "find_last", "foreach"]
RT_METRICS = ["get+set", "watch", "up+down", "forEach ns/comp", "find+get"]
MARKERS = ["s", "o", "^", "D", "v", "P"]

# ── parser ────────────────────────────────────────────────────────────────────

def parse_log(path):
    text = path.read_text()
    result = {"file": path.name}

    m = re.search(r"Date:\s+(.+)", text)
    result["date"] = m.group(1).strip() if m else path.stem

    m = re.search(r"(N=\d+(?:\s+N=\d+)+)", text)
    result["sizes"] = [int(x) for x in re.findall(r"N=(\d+)", m.group(1))] if m else []

    compile_data = {}
    for probe in PROBES:
        m = re.search(rf"^{probe}\s+([\d ]+)$", text, re.MULTILINE)
        if m:
            compile_data[probe] = [int(x) for x in m.group(1).split()]
    result["compile"] = compile_data

    runtime = {}
    for block in re.finditer(r"N=(\d+):\n(.*?)(?=\n  N=|\Z)", text, re.DOTALL):
        n, btext = int(block.group(1)), block.group(2)
        rt = {}
        for pat, key in [
            (r"get\+set\s+([\d.]+)\s+ns/op",                          "get+set"),
            (r"set\+changed\+sync\s+([\d.]+)\s+ns/op",                "watch"),
            (r"up\+down\s+([\d.]+)\s+ns/op",                          "up+down"),
            (r"forEach\s+[\d.]+\s+ns/call\s+\(([\d.]+)\s+ns/comp\)",  "forEach ns/comp"),
            (r"find\+get\s+([\d.]+)\s+ns/op",                         "find+get"),
        ]:
            mm = re.search(pat, btext)
            if mm:
                rt[key] = float(mm.group(1))
        if rt:
            runtime[n] = rt
    result["runtime"] = runtime
    return result

def is_valid(log):
    """Reject runs where flat_chain didn't force chain instantiation (pre-fix logs)."""
    sizes, ct = log["sizes"], log["compile"]
    if 100 not in sizes or "flat_chain" not in ct or "baseline" not in ct:
        return True
    idx = sizes.index(100)
    return ct["flat_chain"][idx] > ct["baseline"][idx] * 10

# ── plotting ──────────────────────────────────────────────────────────────────

def short_label(log):
    return log["date"][11:]  # HH:MM:SS

def plot(logs):
    colors = plt.cm.tab10.colors
    fig, axes = plt.subplots(1, 3, figsize=(17, 5))
    fig.suptitle("OneData Benchmark", fontsize=13, fontweight="bold")

    # ── 1. flat_chain vs baseline across runs ─────────────────────────────────
    ax = axes[0]
    ax.set_title("Compile time: flat_chain vs baseline")
    ax.set_xlabel("Chain size N")
    ax.set_ylabel("ms  (log scale)")
    ax.set_yscale("log")
    for i, log in enumerate(logs):
        sizes, ct, lbl = log["sizes"], log["compile"], short_label(log)
        c = colors[i % 10]
        if "flat_chain" in ct:
            ax.plot(sizes, ct["flat_chain"], "o-",  color=c, label=f"flat_chain {lbl}")
        if "baseline"   in ct:
            ax.plot(sizes, ct["baseline"],   "s--", color=c, alpha=0.45,
                    label=f"baseline {lbl}")
    ax.legend(fontsize=7)
    ax.grid(True, alpha=0.3)

    # ── 2. all probes — latest run ────────────────────────────────────────────
    ax = axes[1]
    ax.set_title(f"Compile time: all probes  ({short_label(logs[-1])})")
    ax.set_xlabel("Chain size N")
    ax.set_ylabel("ms  (log scale)")
    ax.set_yscale("log")
    log = logs[-1]
    for j, probe in enumerate(PROBES):
        if probe in log["compile"]:
            ax.plot(log["sizes"], log["compile"][probe],
                    MARKERS[j] + "-", label=probe, color=colors[j])
    ax.legend(fontsize=7)
    ax.grid(True, alpha=0.3)

    # ── 3. runtime ns/op — N=20, all runs ────────────────────────────────────
    ax = axes[2]
    ax.set_title("Runtime  N=20  (ns/op or ns/comp)")
    ax.set_ylabel("ns")
    x     = np.arange(len(RT_METRICS))
    width = 0.8 / len(logs)
    for i, log in enumerate(logs):
        rt   = log["runtime"].get(20) or next(iter(log["runtime"].values()), {})
        vals = [rt.get(m, 0) for m in RT_METRICS]
        off  = (i - len(logs) / 2 + 0.5) * width
        ax.bar(x + off, vals, width, label=short_label(log),
               color=colors[i % 10], alpha=0.82)
    ax.set_xticks(x)
    ax.set_xticklabels(RT_METRICS, rotation=18, ha="right", fontsize=8)
    ax.legend(fontsize=7)
    ax.grid(True, alpha=0.3, axis="y")

    plt.tight_layout()
    out = RESULTS_DIR / "bench_plot.png"
    plt.savefig(str(out), dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.show()

# ── main ──────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    include_all = "--all" in sys.argv
    logs = sorted(RESULTS_DIR.glob("*.log"))
    if not logs:
        sys.exit(f"No log files found in {RESULTS_DIR}")
    parsed = [parse_log(p) for p in logs]
    if not include_all:
        parsed = [l for l in parsed if is_valid(l)]
        if not parsed:
            parsed = [parse_log(p) for p in logs]  # fallback
    print(f"Plotting {len(parsed)} run(s): {[l['date'] for l in parsed]}")
    plot(parsed)

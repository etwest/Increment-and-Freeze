#!/usr/bin/env python3
"""Parse IAF miss-ratio-curve dumps out of a WiredTiger log and plot them.

Usage:
    plot_mrc.py <wiredtiger.log | mongod.log> [output.png] [--all]

The log may be a plain WiredTiger log or a mongod JSON log; the format is
detected automatically. In a mongod log each dump is a single JSON record
whose attr.message.msg holds the verbose line and the CSV, joined by newlines.

Each dump in the log looks like this -- one verbose line, then a bare CSV:

    [ts][pid:tid], ..., [WT_VERB_EVICTION][INFO]: IAF-SUMMARY cache_bytes=...,hit_rate_pct=...
    1626176,1005668,940742          <- total_requests, curve_blocks, raw_accesses
    Cache Size,Hits
    1,1226176
    2,1226176
    ...

The miss-ratio curve is the miss curve, total_requests - Hits, divided by
raw_accesses. See tools/MRC-GUIDE.md, "Sampling".

Dumps are cumulative within a connection, so the last one covers the last
connection's whole run. By default only the last is plotted; --all overlays
every dump so you can see the curve converge.

Only stdlib + matplotlib. No pandas.
"""
import json
import re
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# IAF quantizes the cache-size axis to 256-byte blocks. A "Cache Size" of N in
# the CSV means N * 256 bytes. Sampling is already corrected for in the dump --
# the emitted sizes are real blocks, not sampled ones.
BLOCK = 256

SUMMARY = re.compile(r"IAF-SUMMARY (.*)")
HEADER = re.compile(r"^\d+,\d+,\d+$")
ROW = re.compile(r"^(\d+),(\d+)$")


def is_mongod_log(path):
    """A mongod log is one JSON object per line; a WiredTiger log is not."""
    with open(path, errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if not line.startswith("{"):
                return False
            try:
                return isinstance(json.loads(line), dict)
            except ValueError:
                return False
    return False


def mongod_lines(f):
    """Yield WiredTiger message text from a mongod JSON log, one line at a time.

    Records that are not WiredTiger messages, or that don't parse, are turned
    into an empty line so that they terminate any dump in progress.
    """
    for line in f:
        try:
            rec = json.loads(line)
            msg = rec["attr"]["message"]
            if isinstance(msg, dict):
                msg = msg["msg"]
        except (ValueError, KeyError, TypeError):
            yield ""
            continue
        if not isinstance(msg, str):
            yield ""
            continue
        for sub in msg.split("\n"):
            yield sub
        yield ""


def parse(path):
    """Return one dict per dump: summary, sz (blocks), hits, total, raw."""
    mongod = is_mongod_log(path)
    with open(path, errors="replace") as f:
        lines = mongod_lines(f) if mongod else (l.rstrip("\n") for l in f)
        return parse_lines(lines)


def parse_lines(lines):
    dumps = []
    pending = None
    for line in lines:

        m = SUMMARY.search(line)
        if m:
            fields = {}
            for kv in m.group(1).split(","):
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    fields[k] = v
            pending = {"summary": fields, "sz": [], "hits": [], "total": None,
                       "raw": None}
            continue

        if pending is None:
            continue

        if pending["total"] is None:
            if HEADER.match(line):
                header = line.split(",")
                pending["total"] = int(header[0])
                pending["raw"] = int(header[2])
            continue

        if line == "Cache Size,Hits":
            continue

        m = ROW.match(line)
        if m:
            pending["sz"].append(int(m.group(1)))
            pending["hits"].append(int(m.group(2)))
        else:
            # Anything else ends this dump.
            if pending["sz"]:
                dumps.append(pending)
            pending = None

    if pending is not None and pending["sz"]:
        dumps.append(pending)
    return dumps


def miss_ratio(hits, total, raw):
    # Never divide by total: it depends on which heavy hitters were sampled.
    return [(total - h) / raw for h in hits]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    plot_all = "--all" in sys.argv
    if not args:
        print(__doc__)
        return 1

    log = args[0]
    out = args[1] if len(args) > 1 else "mrc.png"

    dumps = parse(log)
    if not dumps:
        print("No IAF-SUMMARY dumps found in %s." % log)
        print("The connection needs verbose=[eviction:0] (or higher) to emit them.")
        return 1

    print("Found %d dump(s)." % len(dumps))
    chosen = dumps if plot_all else [dumps[-1]]

    fig, ax = plt.subplots(figsize=(9, 5.5))
    # Number dumps by their position in the log, not in the plotted subset.
    for i, d in enumerate(chosen, start=len(dumps) - len(chosen)):
        # The CSV is sparse: rows are only emitted where the curve moves, so it
        # is a step function. Draw it as one rather than interpolating.
        gb = [s * BLOCK / (1024 ** 3) for s in d["sz"]]
        ax.step(gb, miss_ratio(d["hits"], d["total"], d["raw"]), where="post",
                lw=2.0 if d is chosen[-1] else 1.0,
                alpha=1.0 if d is chosen[-1] else 0.35,
                label="dump %d (%d accesses)" % (i + 1, d["raw"]))

    last = chosen[-1]["summary"]

    # Mark the configured cache size and the hit rate WiredTiger actually saw,
    # so the prediction and the observation can be read off the same axes.
    if "cache_bytes" in last:
        cache_gb = int(last["cache_bytes"]) / (1024 ** 3)
        ax.axvline(cache_gb, color="#d62728", ls="--", lw=1.5)
        ax.annotate("configured cache\n%.2f GB" % cache_gb,
                    xy=(cache_gb, 0.5), xytext=(4, 0), textcoords="offset points",
                    color="#d62728", fontsize=9, va="center")
    if last.get("stats_enabled") == "true" and "hit_rate_pct" in last:
        observed_miss = 1.0 - float(last["hit_rate_pct"]) / 100.0
        ax.axhline(observed_miss, color="#2ca02c", ls=":", lw=1.5)
        ax.annotate("observed miss ratio %.4f" % observed_miss,
                    xy=(0.02, observed_miss), xycoords=("axes fraction", "data"),
                    xytext=(0, 4), textcoords="offset points",
                    color="#2ca02c", fontsize=9)

    if last.get("curve_covers_cache") == "false":
        ax.set_title("WARNING: curve does not reach the configured cache size",
                     color="#d62728")

    ax.set_xscale("log")
    ax.set_xlabel("cache size (GB, log scale)")
    ax.set_ylabel("miss ratio")
    ax.grid(alpha=0.3)
    ax.legend(fontsize=8)
    plt.tight_layout()
    plt.savefig(out, dpi=130)
    print("wrote %s" % out)

    for k in ("cache_bytes", "curve_covers_cache", "pages_requested",
              "pages_read", "hit_rate_pct", "stats_enabled"):
        if k in last:
            print("  %-20s %s" % (k, last[k]))
    return 0


if __name__ == "__main__":
    sys.exit(main())

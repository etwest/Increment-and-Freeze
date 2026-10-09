#!/usr/bin/env python3
"""Parse IAF miss-ratio-curve dumps out of a WiredTiger log and plot them.

Usage:
    plot_mrc.py <wiredtiger.log | mongod.log> [output.png]
                [--all | --windows | --since N | --list]
                [--curve all|internal] [--xmin-gb GB]
                [--log-x] [--window-bands]

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

WiredTiger logs two curves, told apart by the summary's curve= field: "all"
covers every page access, "internal" covers internal pages only. --curve picks
one; the default is "all".

Dumps are cumulative within a connection, so the last one covers the last
connection's whole run, load phase included. Dumps are numbered from 1 in log
order. By default only the last is plotted; --all overlays every dump so you
can see the curve converge. --windows instead plots each interval between
consecutive dumps on its own, so a change in the workload shows up as a change
in the curve. --since N plots one curve for every access after dump N, e.g.
the task without the load before it, with WiredTiger's miss ratio over the
same accesses. --list prints each dump's time, the accesses since the dump
before, and the observed and predicted miss ratios over them, to find where a
phase starts. A window's or --since curve still uses reuse distances from the
whole run, i.e. a cache that was warm when it began. Rows sit on a fixed grid
(see MRC-GUIDE), so these curves are exact, but one that holds few accesses is
noisy.

Cache size is on a linear axis from 0, so the plot shows what each added GB
buys. The main curve's smallest sizes rise steeply and run off the top; the
miss-ratio axis is fitted from 10 MB up. --log-x uses a log axis instead,
starting the main curve at 10 MB. --xmin-gb sets the left edge either way.
The internal curve omits the configured cache size, which is far beyond it.

The dotted line marks WiredTiger's eviction target (eviction_target from the
"Opening WiredTiger" config, else WiredTiger's default of 80%): eviction keeps
the cache at about that fill, so compare the observed miss ratio with the
curve there rather than at the configured size.

Only stdlib + matplotlib. No pandas.
"""
import argparse
import bisect
import json
import re
import sys
import time

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

# IAF quantizes the cache-size axis to blocks of block_bytes (from the summary
# line). A "Cache Size" of N in the CSV means N * block_bytes. Logs written
# before block_bytes was reported used 256-byte blocks. Sampling is already
# corrected for in the dump -- the emitted sizes are real blocks, not sampled
# ones.
OLD_BLOCK = 256


def block_bytes(d):
    return int(d["summary"].get("block_bytes", OLD_BLOCK))

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
    """Yield (time, text) for WiredTiger message text in a mongod JSON log, one
    line at a time. The time is the record's, as logged.

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
            yield None, ""
            continue
        if not isinstance(msg, str):
            yield None, ""
            continue
        t = rec.get("t")
        t = t.get("$date", "")[:19] if isinstance(t, dict) else None
        for sub in msg.split("\n"):
            yield t, sub
        yield None, ""


WT_TIME = re.compile(r"^\[(\d+):\d+\]")


def wt_lines(f):
    """Yield (time, text) for each line of a WiredTiger log. WiredTiger stamps
    messages with seconds since the epoch; show them as local time."""
    for line in f:
        line = line.rstrip("\n")
        m = WT_TIME.match(line)
        t = time.strftime("%Y-%m-%dT%H:%M:%S",
                          time.localtime(int(m.group(1)))) if m else None
        yield t, line


# WiredTiger's default eviction_target: eviction keeps the cache at this
# percentage of its configured size.
DEFAULT_EVICTION_TARGET = 80.0


def eviction_target(path):
    """The eviction_target (percent) the log's wiredtiger_open configured, as
    mongod logs it on "Opening WiredTiger", or WiredTiger's default."""
    pat = re.compile(r"(?<![a-z_])eviction_target=(\d+(?:\.\d+)?)")
    target = DEFAULT_EVICTION_TARGET
    with open(path, errors="replace") as f:
        for line in f:
            for m in pat.finditer(line):
                target = float(m.group(1))
    return target


def parse(path):
    """Return one dict per dump: summary, sz (blocks), hits, total, raw, time."""
    mongod = is_mongod_log(path)
    with open(path, errors="replace") as f:
        lines = mongod_lines(f) if mongod else wt_lines(f)
        return parse_lines(lines)


def parse_lines(lines):
    dumps = []
    pending = None
    for t, line in lines:

        m = SUMMARY.search(line)
        if m:
            fields = {}
            for kv in m.group(1).split(","):
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    fields[k] = v
            pending = {"summary": fields, "sz": [], "hits": [], "total": None,
                       "raw": None, "time": t}
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


# Windows with fewer accesses than this are too noisy to plot: about 9,000 is
# the least that keeps the sd near 0.05 at small cache sizes.
MIN_WINDOW_ACCESSES = 10000

# Default left edge of the x axis for the main curve. Smaller caches aren't
# configured in practice. The internal curve is far smaller, so it has none.
DEFAULT_XMIN_GB = 10 / 1024


def label_observed(fig, ax, miss):
    """Write the observed miss ratio on its line, at the first of: left end
    above, right end above, left below, right below, that stays clear of the
    legend; at the left end above if none is clear."""
    renderer = fig.canvas.get_renderer()
    legend = ax.get_legend()
    boxes = [legend.get_window_extent(renderer)] if legend else []
    frame = ax.get_window_extent(renderer)
    text = "observed miss ratio %.4f" % miss
    for x, ha, dy in ((0.02, "left", 4), (0.98, "right", 4),
                      (0.02, "left", -12), (0.98, "right", -12)):
        t = ax.annotate(text, xy=(x, miss), xycoords=("axes fraction", "data"),
                        xytext=(0, dy), textcoords="offset points", ha=ha,
                        color="#2ca02c", fontsize=9)
        bb = t.get_window_extent(renderer)
        inside = frame.y0 <= bb.y0 and bb.y1 <= frame.y1
        if inside and not any(bb.overlaps(b) for b in boxes):
            return
        t.remove()
    ax.annotate(text, xy=(0.02, miss), xycoords=("axes fraction", "data"),
                xytext=(0, 4), textcoords="offset points", color="#2ca02c", fontsize=9)


def hits_at(d, size):
    """Hits at a cache size: the last row at or below it. Exact on the grid,
    which is where differencing and averaging look it up."""
    i = bisect.bisect_right(d["sz"], size) - 1
    return d["hits"][i] if i >= 0 else 0


def difference(prev, cur):
    """The curve of the accesses between two cumulative dumps of one partition."""
    return {
        "summary": cur["summary"],
        "sz": cur["sz"],
        "hits": [h - hits_at(prev, s) for s, h in zip(cur["sz"], cur["hits"])],
        "total": cur["total"] - prev["total"],
        "raw": cur["raw"] - prev["raw"],
        "time": cur["time"],
    }


# Differencing the first dump of a connection against this leaves it as it is.
NOTHING = {"summary": {}, "sz": [], "hits": [], "total": 0, "raw": 0, "time": None}


def new_connection(evs, j):
    """Whether dump j (from 0) starts a connection: its access count went down."""
    return j == 0 or evs[j][0]["raw"] < evs[j - 1][0]["raw"]


def events(dumps):
    """Group dumps into dump events, one list of partitions each.

    WiredTiger writes one record per partition, partition 0 first, each time
    it dumps. Logs from before partitions existed have one curve per event.
    """
    out = []
    for d in dumps:
        if int(d["summary"].get("partition", 0)) == 0 or not out:
            out.append([])
        out[-1].append(d)
    return out


def window_events(evs):
    """Difference consecutive events partition by partition.

    Returns (j, event) for the interval that ends at dump j (from 0). A dump
    that starts a new connection isn't differenced against the one before it.
    """
    out = []
    for j in range(1, len(evs)):
        if new_connection(evs, j):
            continue
        ev = [difference(p, c) for p, c in zip(evs[j - 1], evs[j])]
        if ev[0]["raw"] >= MIN_WINDOW_ACCESSES:
            out.append((j, ev))
    return out


def since_event(evs, n):
    """The curve of every access after dump n (from 1) up to the last dump.

    Exact, like a window: the rows sit on the grid. Returns None if a new
    connection starts after dump n, since the dumps no longer accumulate.
    """
    if any(new_connection(evs, j) for j in range(n, len(evs))):
        return None
    return [difference(p, c) for p, c in zip(evs[n - 1], evs[-1])]


def observed_between(prev, cur):
    """WiredTiger's measured miss ratio between two dumps' summaries, or None
    without statistics or requests."""
    if cur.get("stats_enabled") != "true" or "pages_requested" not in cur:
        return None
    req = int(cur["pages_requested"]) - int(prev.get("pages_requested", 0))
    read = int(cur["pages_read"]) - int(prev.get("pages_read", 0))
    return read / req if req > 0 else None


def predicted_at(ev, blocks):
    """The mean of an event's partitions' miss ratios at one cache size."""
    return sum((d["total"] - hits_at(d, blocks)) / d["raw"] for d in ev) / len(ev)


def list_dumps(evs):
    """Print one line per dump: when, how many accesses since the dump before,
    and the measured and predicted miss ratios over those accesses."""
    print("%4s  %-19s  %12s  %8s  %9s" % ("dump", "time", "accesses", "observed",
                                          "predicted"))
    for j, ev in enumerate(evs):
        restart = new_connection(evs, j)
        prev = [NOTHING] * len(ev) if restart else evs[j - 1]
        win = [difference(p, c) for p, c in zip(prev, ev)]
        summ = ev[0]["summary"]
        obs = observed_between(prev[0]["summary"], summ)
        pred = None
        if win[0]["raw"] > 0 and summ.get("curve_covers_cache") == "true":
            pred = predicted_at(win, int(summ["cache_blocks"]))
        print("%4d  %-19s  %12d  %8s  %9s%s" % (
            j + 1, ev[0]["time"] or "", win[0]["raw"],
            "%.4f" % obs if obs is not None else "-",
            "%.4f" % pred if pred is not None else "-",
            "  new connection" if restart and j > 0 else ""))


# Two-sided 95% t quantiles by degrees of freedom. The band's spread is
# estimated from only k partitions, so it needs t with k - 1 df, not z = 1.96.
T95 = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 7: 2.365,
       8: 2.306, 9: 2.262, 10: 2.228, 15: 2.131, 20: 2.086, 30: 2.042}


def t95(df):
    if df > 30:
        return 1.96
    # Between tabulated values, the smaller df's quantile is the conservative one.
    return T95[max(k for k in T95 if k <= df)]


def combine(ev):
    """Average an event's partitions onto one curve, with a 95% interval.

    Returns (sizes in blocks, mean miss ratio, interval half-width or None).
    The partitions are disjoint samples of rate q, so the mean of k of them is
    a single sample of rate kq, and their spread overstates its error; the
    finite-population factor sqrt(1 - kq) corrects for that. The interval
    covers the variance of the mean, which dominates: above the smallest sizes,
    sampling bias is a few percent of it.
    """
    k = len(ev)
    sizes = sorted(set(s for d in ev for s in d["sz"]))
    mrs = [[(d["total"] - hits_at(d, s)) / d["raw"] for s in sizes] for d in ev]
    mean = [sum(col) / k for col in zip(*mrs)]
    if k < 2:
        return sizes, mean, None
    q = 2.0 ** -int(ev[0]["summary"].get("sampling_log2", 0))
    fpc = max(0.0, 1.0 - k * q)
    t = t95(k - 1)
    half = [t * (sum((x - m) ** 2 for x in col) / (k - 1) / k * fpc) ** 0.5
            for col, m in zip(zip(*mrs), mean)]
    return sizes, mean, half


def shared_limits(evs, yfit, extra=None):
    """(largest cache size in GB, largest miss ratio) over every view of a run.

    The whole-run curve (with its band), every cumulative dump and every
    window all count, so the default plot, --all and --windows of one log come
    out on identical axes and can be compared side by side. A --since curve
    (extra) also counts, with its band. Miss ratios count from yfit up.
    """
    curves = [(e, False) for e in evs[:-1]] + [(evs[-1], True)]
    curves += [(w, False) for _, w in window_events(evs)]
    if extra:
        curves.append((extra, True))
    xmax = ymax = 0.0
    for ev, with_band in curves:
        sizes, mr, half = combine(ev)
        gb = [s * block_bytes(ev[0]) / (1024 ** 3) for s in sizes]
        first = max(0, bisect.bisect_right(gb, yfit) - 1)
        top = [m + h for m, h in zip(mr, half)] if with_band and half else mr
        xmax = max(xmax, gb[-1])
        ymax = max([ymax] + top[first:])
    return xmax, ymax


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log")
    ap.add_argument("out", nargs="?", default="mrc.png")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--all", action="store_true",
                      help="overlay every cumulative dump")
    mode.add_argument("--windows", action="store_true",
                      help="plot each interval between consecutive dumps")
    mode.add_argument("--since", type=int, metavar="N",
                      help="plot only the accesses after dump N, e.g. to leave out a load "
                      "phase (--list numbers the dumps)")
    mode.add_argument("--list", action="store_true",
                      help="list the dumps, with each interval's accesses and miss ratios, "
                      "instead of plotting")
    ap.add_argument("--curve", choices=("all", "internal"), default="all")
    ap.add_argument("--window-bands", action="store_true",
                    help="with --windows, shade each window's 95%% interval")
    ap.add_argument("--log-x", action="store_true",
                    help="log cache-size axis instead of linear (main curve then starts at 10 MB)")
    ap.add_argument("--xmin-gb", type=float, default=None,
                    help="left edge of the x axis, in GB (default: 10 MB for the main "
                    "curve, none for the internal one)")
    args = ap.parse_args()

    # Logs from before the internal curve existed have no curve= field.
    dumps = [d for d in parse(args.log)
             if d["summary"].get("curve", "all") == args.curve]
    if not dumps:
        print("No IAF-SUMMARY curve=%s dumps found in %s." % (args.curve, args.log))
        print("The connection needs verbose=[eviction:0] (or higher) to emit them.")
        return 1

    evs = events(dumps)
    nparts = len(evs[-1])
    print("Found %d curve=%s dump(s), %d partition(s) each." % (len(evs), args.curve, nparts))
    if args.list:
        list_dumps(evs)
        return 0
    since = None
    # Dumps are numbered from 1 by their position in the log, as --list shows.
    if args.windows:
        wins = window_events(evs)
        if not wins:
            print("Need at least two dumps from one connection for --windows.")
            return 1
        chosen = [e for _, e in wins]
        # Each window is coloured, and the colour bar numbered, by the dump that ends it.
        ends = [j + 1 for j, _ in wins]
        labels = [None] * len(wins)
    elif args.since is not None:
        if not 1 <= args.since < len(evs):
            print("--since needs a dump from 1 to %d; the log has %d." % (len(evs) - 1, len(evs)))
            return 1
        since = since_event(evs, args.since)
        if since is None:
            print("A new connection starts after dump %d, so the dumps since don't add up."
                  % args.since)
            return 1
        chosen = [since]
        labels = ["after dump %d (%d accesses)" % (args.since, since[0]["raw"])]
    else:
        chosen = evs if args.all else [evs[-1]]
        labels = ["dump %d (%d accesses)" % (i, e[0]["raw"])
                  for i, e in enumerate(chosen, start=len(evs) - len(chosen) + 1)]

    last = chosen[-1][0]["summary"]
    xmin = args.xmin_gb
    if xmin is None and args.curve == "all" and args.log_x:
        xmin = DEFAULT_XMIN_GB
    # Fit the miss-ratio axis to the curve from here up. With a linear axis the
    # main curve's smallest sizes stay drawn but run off the top: they rise
    # steeply and depend mostly on which pages were sampled.
    yfit = xmin if xmin is not None else (DEFAULT_XMIN_GB if args.curve == "all" else 0)
    xmax, yhi = shared_limits(evs, yfit, since)

    fig, ax = plt.subplots(figsize=(9, 5.5))
    observed_miss = None
    for i, (ev, label) in enumerate(zip(chosen, labels)):
        # Rows sit on a grid of cache sizes. The true curve is monotone between
        # grid points, so join them with straight lines rather than steps.
        sizes, mr, se = combine(ev)
        gb = [s * block_bytes(ev[0]) / (1024 ** 3) for s in sizes]
        lo = [m - e for m, e in zip(mr, se)] if se else mr
        hi = [m + e for m, e in zip(mr, se)] if se else mr
        # A miss ratio above 1 is a sampling artifact (the sample drew more
        # than its share of accesses), so the curve and its band are drawn
        # clipped to 1.
        mr = [min(1.0, m) for m in mr]
        lo = [min(1.0, m) for m in lo]
        hi = [min(1.0, m) for m in hi]
        last_one = i == len(chosen) - 1
        if args.windows:
            # Windows are peers, so colour them along a sequence, not by recency.
            color = plt.cm.viridis((ends[i] - ends[0]) / max(1, ends[-1] - ends[0]))
            # The colour bar names windows, so the legend doesn't list them.
            ax.plot(gb, mr, lw=1.5, color=color)
        elif args.all and not last_one:
            # Earlier dumps, likewise named by the colour bar, in the background.
            color = plt.cm.viridis(i / max(1, len(chosen) - 2))
            ax.plot(gb, mr, lw=1.0, alpha=0.5, color=color)
        else:
            line, = ax.plot(gb, mr, lw=2.0, label=label,
                            color="k" if args.all and len(chosen) > 1 else None)
            color = line.get_color()
        # Band only the curve being read, so overlays stay legible. Windows
        # overlap too much for bands unless asked for.
        band = args.window_bands if args.windows else i == len(chosen) - 1
        if se and band:
            # Not below the size the miss-ratio axis is fitted from: the
            # partitions disagree wildly there and the band balloons.
            keep = [k for k, g in enumerate(gb) if g >= yfit]
            ax.fill_between([gb[k] for k in keep], [lo[k] for k in keep],
                            [hi[k] for k in keep], color=color, alpha=0.25, lw=0,
                            label="95%% interval (%d partitions)" % len(ev)
                            if i == len(chosen) - 1 else None)

    # Mark the configured cache size and the hit rate WiredTiger actually saw,
    # so the prediction and the observation can be read off the same axes.
    # The internal pages fill a sliver of the cache, so the configured size
    # would only stretch the internal curve's axis.
    if "cache_bytes" in last and args.curve == "all":
        cache_gb = int(last["cache_bytes"]) / (1024 ** 3)
        ax.axvline(cache_gb, color="#d62728", ls="--", lw=1.5)
        ax.annotate("configured cache\n%.2f GB" % cache_gb,
                    xy=(cache_gb, 0.5), xycoords=("data", "axes fraction"),
                    xytext=(4, 0), textcoords="offset points",
                    color="#d62728", fontsize=9, va="center")
        # Eviction holds the cache at its target, so that is about how much
        # the cache holds: read the curve there to compare with the observed.
        target = eviction_target(args.log)
        target_gb = cache_gb * target / 100.0
        ax.axvline(target_gb, color="#d62728", ls=":", lw=1.5)
        ax.annotate("eviction target\n%g%%, %.2f GB" % (target, target_gb),
                    xy=(target_gb, 0.7), xycoords=("data", "axes fraction"),
                    xytext=(-4, 0), textcoords="offset points", ha="right",
                    color="#d62728", fontsize=9, va="center")
    # The summary's hit rate covers the whole connection, so --since measures
    # its own from the page counts, and windows, being many, get none.
    if since is not None:
        observed_miss = observed_between(evs[args.since - 1][0]["summary"], last)
    elif not args.windows and last.get("stats_enabled") == "true" and "hit_rate_pct" in last:
        observed_miss = 1.0 - float(last["hit_rate_pct"]) / 100.0
    if observed_miss is not None:
        # Labelled once the layout is final, clear of the legend (below).
        ax.axhline(observed_miss, color="#2ca02c", ls=":", lw=1.5, zorder=1)

    title = "internal pages only" if args.curve == "internal" else ""
    if args.curve == "all" and last.get("curve_covers_cache") == "false":
        title = "WARNING: curve does not reach the configured cache size" + (
            " (%s)" % title if title else "")
        ax.set_title(title, color="#d62728")
    elif title:
        ax.set_title(title)

    # Fixed limits, the same in every view of this log (see shared_limits).
    if args.log_x:
        ax.set_xscale("log")
        ax.set_xlim(xmin or None, xmax * 1.1)
    else:
        ax.set_xlim(xmin or 0, xmax * 1.02)
    # The axis stays within [0, 1], even where a band or a noisy window dips
    # below 0 or a sampled curve's smallest sizes rise past 1.
    ax.set_ylim(0, min(1.0, yhi + 0.05 * (yhi or 1.0)))
    ax.set_xlabel("cache size (GB, log scale)" if args.log_x else "cache size (GB)")
    ax.set_ylabel("miss ratio")
    ax.grid(alpha=0.3)
    # A colour bar, numbered by dump, names the windows or the earlier dumps
    # however many there are; a legend of them would crowd the plot.
    scale = None
    if args.windows:
        scale = (ends[0], ends[-1], "window ending at dump", "window")
    elif args.all and len(chosen) > 1:
        scale = (1, len(chosen) - 1, "cumulative, ending at dump", "dump")
    if scale:
        lo, hi, what, noun = scale
        lo, hi = (lo, hi) if hi > lo else (lo - 0.5, lo + 0.5)
        sm = plt.cm.ScalarMappable(cmap=plt.cm.viridis, norm=plt.Normalize(lo, hi))
        bar = fig.colorbar(sm, ax=ax, label=what)
        bar.locator = MaxNLocator(integer=True)
        bar.update_ticks()
        # Say which way time runs along the bar.
        bar.ax.set_title("later\n" + noun, fontsize=8)
        bar.ax.set_xlabel("earlier\n" + noun, fontsize=8)
    if ax.get_legend_handles_labels()[0]:
        ax.legend(fontsize=8)
    plt.tight_layout()
    if observed_miss is not None:
        label_observed(fig, ax, observed_miss)
    plt.savefig(args.out, dpi=130)
    print("wrote %s" % args.out)

    for k in ("block_bytes", "cache_bytes", "curve_covers_cache",
              "pages_requested", "pages_read", "hit_rate_pct", "stats_enabled"):
        if k in last:
            print("  %-20s %s" % (k, last[k]))
    if since is not None and observed_miss is not None:
        print("  %-20s %.4f" % ("miss ratio after %d" % args.since, observed_miss))

    # How well the block size fits the page sizes: the share of accesses under
    # a quarter block, and how much rounding up inflated the bytes written.
    n = int(last.get("sampled_accesses", 0))
    if n:
        b = int(last["sampled_bytes"])
        print("  %-20s %.2f%% of sampled accesses" % (
            "small accesses", 100.0 * int(last["small_accesses"]) / n))
        if b:
            print("  %-20s %.2f%% over actual sizes" % (
                "rounding inflation", 100.0 * (int(last["rounded_bytes"]) - b) / b))
    return 0


if __name__ == "__main__":
    sys.exit(main())

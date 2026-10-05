#!/usr/bin/env python3
"""Parse IAF miss-ratio-curve dumps out of a WiredTiger log and plot them.

Usage:
    plot_mrc.py <wiredtiger.log | mongod.log> [output.png]
                [--all | --windows] [--curve all|internal] [--xmin-gb GB]
                [--bias-levels PCTS] [--log-x] [--window-bands]

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
connection's whole run. By default only the last is plotted; --all overlays
every dump so you can see the curve converge. --windows instead plots each
interval between consecutive dumps on its own, so a change in the workload
shows up as a change in the curve. A window's curve still uses reuse distances
from the whole run, i.e. a cache that was warm when the window began. Rows
sit on a fixed grid (see MRC-GUIDE), so a window's curve is exact, but one that
holds few accesses is noisy.

Horizontal red and grey bars on the whole-run curve show its provable bias at
the configured cache and at a few evenly spaced sizes: in expectation, the curve
at C lies between the true curve at the bar's two ends, give or take 0.01 in
miss ratio (see MRC-GUIDE, "Sampling"). IAF logs the bias at the configured
cache (provable_eps_at_cache) and the sizes at which it reaches 1, 5, 10, 25,
50, 100 and 200% (provable_floor_blocks_by_pct); the other bars interpolate
between those. --bias-levels instead draws a dotted vertical line at each of
the given levels. Logs without the fields, or unsampled ones, get neither.

Cache size is on a linear axis from 0, so the plot shows what each added GB
buys. The main curve's smallest sizes rise steeply and run off the top; the
miss-ratio axis is fitted from 10 MB up. --log-x uses a log axis instead,
starting the main curve at 10 MB. --xmin-gb sets the left edge either way.
The internal curve omits the configured cache size, which is far beyond it.

Only stdlib + matplotlib. No pandas.
"""
import argparse
import bisect
import json
import math
import re
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

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


# Cache-size error levels to mark. IAF logs the floor for each one; logs that
# only carry the 10% floor scale it as 1/eps^2, which is approximate.
FLOOR_EPS = 0.1
LOGGED_EPS = (0.01, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0)
MARK_EPS = (0.01, 0.05, 1.0, 2.0)
# Bias bars drawn besides the one at the configured cache, as fractions of the x range.
BAR_AT = (0.25, 0.5, 0.75, 0.97)
# Windows with fewer accesses than this are too noisy to plot: about 9,000 is
# the least that keeps the sd near 0.05 at the floor.
MIN_WINDOW_ACCESSES = 10000

# Default left edge of the x axis for the main curve. Smaller caches aren't
# configured in practice. The internal curve is far smaller, so it has none.
DEFAULT_XMIN_GB = 10 / 1024


def label_observed(fig, ax, miss, bar_spans, avoid):
    """Write the observed miss ratio on its line, at the first of: left end
    above, right end above, left below, right below, that covers no bias bar,
    bias label or legend; at the left end above if none is clear."""
    renderer = fig.canvas.get_renderer()
    pad = 3 * fig.dpi / 72
    boxes = [a.get_window_extent(renderer) for a in avoid]
    if ax.get_legend():
        boxes.append(ax.get_legend().get_window_extent(renderer))
    for left, right, y in bar_spans:
        (x0, y0), (x1, _) = ax.transData.transform([(left, y), (right, y)])
        boxes.append(matplotlib.transforms.Bbox([[x0 - pad, y0 - 2 * pad],
                                                 [x1 + pad, y0 + 2 * pad]]))
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


def interp(x, xs, ys):
    """Linear interpolation of ys over ascending xs, held flat past either end."""
    i = bisect.bisect_left(xs, x)
    if i == 0:
        return ys[0]
    if i == len(xs):
        return ys[-1]
    t = (x - xs[i - 1]) / (xs[i] - xs[i - 1])
    return ys[i - 1] + t * (ys[i] - ys[i - 1])


def bias_at_gb(d, gb):
    """Provable bias eps at a cache of gb GB, or None where it exceeds 200% or
    the log has no levels. Interpolates log eps against log size between the
    logged levels; past the 1% level eps falls as 1/sqrt(size), as the floor
    grows as 1/eps^2."""
    pts = sorted((g, e) for e, g in provable_floors_gb(d, LOGGED_EPS))
    if not pts or gb < pts[0][0]:
        return None
    lx, ly = [math.log(g) for g, _ in pts], [math.log(e) for _, e in pts]
    x = math.log(gb)
    if x >= lx[-1]:
        return math.exp(ly[-1] - 0.5 * (x - lx[-1]))
    return math.exp(interp(x, lx, ly))


def provable_floors_gb(d, levels=MARK_EPS):
    """[(eps, GB)] for each requested error level, or [] if the log has no floor."""
    f = d["summary"]
    gb = lambda blocks: int(blocks) * block_bytes(d) / (1024 ** 3)
    if "provable_floor_blocks_by_pct" in f:
        logged = {}
        for item in f["provable_floor_blocks_by_pct"].split("|"):
            pct, blocks = item.split(":")
            if int(blocks):
                logged[int(pct) / 100] = gb(blocks)
        return [(e, logged[e]) for e in levels if e in logged]
    floor = provable_floor_gb(d)
    return [(e, floor * (FLOOR_EPS / e) ** 2) for e in levels] if floor else []


def provable_floor_gb(d):
    """Smallest cache size, in GB, at which the sampled curve is provably accurate.

    IAF computes it (Iaf_provable_floor_blocks) and WiredTiger logs it as
    provable_floor_blocks. Returns None for logs that predate the field.
    """
    try:
        return int(d["summary"]["provable_floor_blocks"]) * block_bytes(d) / (1024 ** 3)
    except (KeyError, ValueError):
        return None


def hits_at(d, size):
    """Hits at a cache size: the last row at or below it. Exact on the grid,
    which is where differencing and averaging look it up."""
    i = bisect.bisect_right(d["sz"], size) - 1
    return d["hits"][i] if i >= 0 else 0


def windows(dumps):
    """Difference consecutive cumulative dumps into one curve per interval.

    A dump whose access count went down starts a new connection, so it is
    not differenced against the one before it.
    """
    out = []
    for prev, cur in zip(dumps, dumps[1:]):
        if cur["raw"] <= prev["raw"]:
            continue
        out.append({
            "summary": cur["summary"],
            "sz": cur["sz"],
            "hits": [h - hits_at(prev, s) for s, h in zip(cur["sz"], cur["hits"])],
            "total": cur["total"] - prev["total"],
            "raw": cur["raw"] - prev["raw"],
        })
    return out


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
    """Difference consecutive events partition by partition."""
    k = min(len(e) for e in evs)
    per_part = [windows([e[i] for e in evs]) for i in range(k)]
    return [list(ws) for ws in zip(*per_part) if ws[0]["raw"] >= MIN_WINDOW_ACCESSES]


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
    covers the variance of the mean, which dominates: bias above the floor is a
    few percent of it.
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


def shared_limits(evs, yfit):
    """(largest cache size in GB, largest miss ratio) over every view of a run.

    The whole-run curve (with its band), every cumulative dump and every
    window all count, so the default plot, --all and --windows of one log come
    out on identical axes and can be compared side by side. Miss ratios count
    from yfit up.
    """
    curves = [(e, False) for e in evs[:-1]] + [(evs[-1], True)]
    if len(evs) > 1:
        curves += [(w, False) for w in window_events(evs)]
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
    ap.add_argument("--curve", choices=("all", "internal"), default="all")
    ap.add_argument("--window-bands", action="store_true",
                    help="with --windows, shade each window's 95%% interval")
    ap.add_argument("--bias-levels", default="",
                    help="draw vertical provable-bias lines at these percent errors in "
                    "cache size instead of bias bars (logged: 1,5,10,25,50,100,200)")
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
    if args.windows:
        chosen = window_events(evs)
        if not chosen:
            print("Need at least two dumps from one connection for --windows.")
            return 1
        labels = ["window %d (%d accesses)" % (i + 1, e[0]["raw"])
                  for i, e in enumerate(chosen)]
    else:
        chosen = evs if args.all else [evs[-1]]
        # Number dumps by their position in the log, not in the plotted subset.
        labels = ["dump %d (%d accesses)" % (i + 1, e[0]["raw"])
                  for i, e in enumerate(chosen, start=len(evs) - len(chosen))]

    last = chosen[-1][0]["summary"]
    xmin = args.xmin_gb
    if xmin is None and args.curve == "all" and args.log_x:
        xmin = DEFAULT_XMIN_GB
    # Fit the miss-ratio axis to the curve from here up. With a linear axis the
    # main curve's smallest sizes stay drawn but run off the top: they rise
    # steeply and are below the provable floor anyway.
    yfit = xmin if xmin is not None else (DEFAULT_XMIN_GB if args.curve == "all" else 0)
    levels = tuple(int(x) / 100 for x in args.bias_levels.split(",") if x.strip())
    floors = provable_floors_gb(evs[-1][0], levels)
    logged = provable_floors_gb(evs[-1][0], LOGGED_EPS)
    if logged:
        print("Provable within: " + ", ".join(
            "%d%% above %.3g GB" % (round(100 * e), gb) for e, gb in logged))
    if args.curve == "all" and "provable_eps_at_cache" in evs[-1][0]["summary"]:
        print("Provable bias at the configured cache: ±%.3g%%"
              % (100 * float(evs[-1][0]["summary"]["provable_eps_at_cache"])))
    xmax, yhi = shared_limits(evs, yfit)

    fig, ax = plt.subplots(figsize=(9, 5.5))
    whole_gb = whole_mr = None
    observed_miss = None
    # What the observed miss ratio's label must not cover: bias bars, in data
    # coordinates as (left, right, y), and their labels.
    bar_spans, avoid = [], []
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
        whole_gb, whole_mr = gb, mr
        if args.windows:
            # Windows are peers, so colour them along a sequence, not by recency.
            color = plt.cm.viridis(i / max(1, len(chosen) - 1))
            ax.plot(gb, mr, lw=1.5, label=label, color=color)
        else:
            last_one = i == len(chosen) - 1
            line, = ax.plot(gb, mr, lw=2.0 if last_one else 1.0,
                            alpha=1.0 if last_one else 0.35, label=label)
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
    # The observed hit rate is cumulative, so it doesn't belong on a window.
    if (not args.windows and last.get("stats_enabled") == "true"
            and "hit_rate_pct" in last):
        observed_miss = 1.0 - float(last["hit_rate_pct"]) / 100.0
        # Labelled once the layout is final, clear of the bias bars (below).
        ax.axhline(observed_miss, color="#2ca02c", ls=":", lw=1.5, zorder=1)

    title = "internal pages only" if args.curve == "internal" else ""
    if args.curve == "all" and last.get("curve_covers_cache") == "false":
        title = "WARNING: curve does not reach the configured cache size" + (
            " (%s)" % title if title else "")
        ax.set_title(title, color="#d62728")
    elif title:
        ax.set_title(title)

    # Provable bias as horizontal bars on the whole-run curve: the expected
    # curve at C lies between the true curve at (1 - eps) C and (1 + eps) C,
    # give or take 0.01. Windows share the whole run's sample, so the bars go
    # on the whole-run curve only, and the vertical lines replace them.
    if not args.windows and not floors and whole_gb:
        bars = []
        if args.curve == "all" and "cache_bytes" in last:
            cache_gb = int(last["cache_bytes"]) / (1024 ** 3)
            eps = float(last.get("provable_eps_at_cache", "inf"))
            if eps < float("inf") and cache_gb <= xmax:
                bars.append((cache_gb, eps, "#d62728"))
        for f in BAR_AT:
            g = f * xmax
            if (xmin and g < xmin) or any(abs(g - b[0]) < 0.1 * xmax for b in bars):
                continue
            eps = bias_at_gb(evs[-1][0], g)
            if eps:
                bars.append((g, eps, "0.3"))
        # Miss ratio per point of height, to place labels clear of the line.
        ytop = min(1.0, yhi + 0.05 * (yhi or 1.0))
        per_pt = ytop / (ax.get_window_extent().height * 72 / fig.dpi)
        for g, eps, color in bars:
            y = interp(g, whole_gb, whole_mr)
            left = max(0.0, g * (1 - eps))
            ax.errorbar([g], [y], xerr=[[g - left], [g * eps]], fmt="o", color=color,
                        ms=4, capsize=4, lw=1.5, zorder=5)
            # Label above the bar, or below it where the observed miss ratio's
            # line would run through the label and there is room below.
            above = (observed_miss is None
                     or not 3 * per_pt <= observed_miss - y <= 20 * per_pt
                     or y - 16 * per_pt < 0
                     or -16 * per_pt <= observed_miss - y <= -3 * per_pt)
            avoid.append(ax.annotate("±%.2g%%" % (100 * eps), xy=(g, y),
                                     xytext=(0, 7 if above else -14), textcoords="offset points",
                                     ha="center", color=color, fontsize=9))
            bar_spans.append((left, g * (1 + eps), y))
    # Markers off either edge are left out; the printed summary still lists them.
    shown = [(e, gb) for e, gb in floors
             if not (xmin and gb < xmin) and gb <= xmax * (1.1 if args.log_x else 1.02)]
    for e, gb in shown:
        ax.axvline(gb, color="0.45", ls=":", lw=1.2)
    # Fixed limits, the same in every view of this log (see shared_limits).
    if args.log_x:
        ax.set_xscale("log")
        ax.set_xlim(xmin or None, xmax * 1.1)
    else:
        ax.set_xlim(xmin or 0, xmax * 1.02)
    # The axis stays within [0, 1], even where a band or a noisy window dips
    # below 0 or a sampled curve's smallest sizes rise past 1.
    ax.set_ylim(0, min(1.0, yhi + 0.05 * (yhi or 1.0)))
    # Label the markers, skipping any that would print on top of the last one.
    last_px = None
    for e, gb in sorted(shown, key=lambda m: m[1]):
        px = ax.transData.transform((gb, 0))[0]
        if last_px is not None and px - last_px < 40:
            continue
        ax.annotate("±%d%%" % round(100 * e), xy=(gb, 1.0),
                    xycoords=("data", "axes fraction"), xytext=(3, -12),
                    textcoords="offset points", color="0.35", fontsize=8)
        last_px = px
    ax.set_xlabel("cache size (GB, log scale)" if args.log_x else "cache size (GB)")
    ax.set_ylabel("miss ratio")
    ax.grid(alpha=0.3)
    if args.windows and len(chosen) > 8:
        # Too many windows to name; a colour bar keeps the plot readable.
        sm = plt.cm.ScalarMappable(cmap=plt.cm.viridis,
                                   norm=plt.Normalize(1, len(chosen)))
        fig.colorbar(sm, ax=ax, label="window")
    else:
        ax.legend(fontsize=8)
    plt.tight_layout()
    if observed_miss is not None:
        label_observed(fig, ax, observed_miss, bar_spans, avoid)
    plt.savefig(args.out, dpi=130)
    print("wrote %s" % args.out)

    for k in ("block_bytes", "cache_bytes", "curve_covers_cache",
              "pages_requested", "pages_read", "hit_rate_pct", "stats_enabled"):
        if k in last:
            print("  %-20s %s" % (k, last[k]))

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

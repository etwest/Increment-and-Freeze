# MongoDB integration

Build files for linking Increment-and-Freeze into `mongod` via WiredTiger's cache analysis
build (`HAVE_ANALYZE_CACHE`), a plotter for the curves it logs (`plot_mrc.py`), and notes on
reading that output from a mongod log.

| file | goes to (in the mongo tree) |
|---|---|
| `increment_and_freeze/BUILD.bazel` | `src/third_party/increment_and_freeze/BUILD.bazel` |
| `increment_and_freeze/README.md` | `src/third_party/increment_and_freeze/README.md` |
| `increment_and_freeze/scripts/import.sh` | `src/third_party/increment_and_freeze/scripts/import.sh` |
| `wiredtiger-BUILD.bazel.patch` | applied to `src/third_party/wiredtiger/BUILD.bazel` |

**GPL-2.0.** Increment-and-Freeze is GPL-2.0. The vendored copy is for local analysis builds
only and must not be shipped; see `increment_and_freeze/README.md`.

## Setup

From the root of a mongo checkout, with this repository at `$IAF`:

    cp -R $IAF/integrations/mongo/increment_and_freeze src/third_party/
    src/third_party/increment_and_freeze/scripts/import.sh
    git apply $IAF/integrations/mongo/wiredtiger-BUILD.bazel.patch

`import.sh` clones this repository at the pinned `REVISION` (branch `sampling`) and copies the
library sources, headers and license into `src/third_party/increment_and_freeze/dist/`. It
refuses to run if `dist/` already exists; to update, bump `REVISION`, delete `dist/`, and
re-run it. If a new revision adds a translation unit or a header the library includes, add it
to `SRCS`/`HDRS` in `import.sh` and to `srcs` in `BUILD.bazel`.

The patch adds `HAVE_ANALYZE_CACHE` to `WT_DEFINES` and makes the WiredTiger library depend on
`//src/third_party/increment_and_freeze:iaf_api`. It does not add the analysis code itself:
`src/third_party/wiredtiger` must already contain the WiredTiger side of the integration
(branch `iaf` of `github.com/DanielDeLayo/wiredtiger`).

## What WiredTiger emits

WiredTiger calls `Iaf_write` on every page access through `__wt_page_in`, whether or not the
page was already cached, but skips cache-only lookups (`WT_READ_CACHE`). The size is the
page's in-memory footprint. That footprint includes attached updates, so it is usually
larger than the on-disk page. Accesses to internal pages are also written to a second IAF
instance, which produces a curve for internal pages alone. The instances are configured as
follows (`src/include/analyze_cache_inline.h`):

- **Sampling:** the main curve is 4 disjoint samples of 1 in 16 (`WT_IAF_PARTITIONS = 4`,
  `WT_IAF_SAMPLING_LOG2 = 4`), so 1 in 4 pages in all; their spread gives the curve an error bar.
  The internal curve samples 1 in 4 (`WT_IAF_INTERNAL_SAMPLING_LOG2 = 2`). It misses the steep
  drop the root and upper levels cause at the smallest sizes, but matches the unsampled curve above
  them.
- **Curve bound: 4 × `cache_size`**, set when the cache is created and again on reconfigure.
- **Dumps** after each chunk the main curve's first partition processes, and at connection
  close. Each dump writes one record per partition of the main curve, then the internal curve, so
  they line up in time. Each dump covers the whole connection so far. IAF state doesn't survive a close, so a run that populates, restarts and
  runs again produces two independent sequences. Use the last dump before the final shutdown.

Each dump is a summary line followed by the CSV described in `tools/MRC-GUIDE.md`:

    IAF-SUMMARY curve=all,partition=0,partitions=4,block_bytes=1024,grid_ratio=1.04,
    sampling_log2=4,
    cache_bytes=1073741824,cache_blocks=1048576,curve_max_blocks=4194304,
    curve_covers_cache=true,bytes_inuse=745567396,
    pages_requested=79344883,pages_read=25065,hit_rate_pct=99.9684,stats_enabled=true,
    sampled_accesses=19293492,small_accesses=0,sampled_bytes=191006570832,
    rounded_bytes=191016228864
    76786752,676294,79344883
    Cache Size,Hits
    ...

| field | meaning |
|---|---|
| `curve` | `all` for every page access, `internal` for internal pages only |
| `partition`, `partitions` | which of the curve's disjoint samples this record holds, out of how many |
| `block_bytes` | bytes per unit of the curve's cache-size axis (`IAF_BLOCK_SIZE`) |
| `grid_ratio` | ratio between successive cache sizes in the rows (`IAF_GRID_RATIO`) |
| `sampling_log2` | each partition samples 1 in 2^`sampling_log2` pages |
| `cache_bytes`, `cache_blocks` | configured cache size, in bytes and in curve units |
| `curve_max_blocks` | largest cache size the curve can represent |
| `curve_covers_cache` | whether `cache_blocks <= curve_max_blocks` |
| `bytes_inuse` | bytes currently in the cache (internal pages only, for `curve=internal`) |
| `pages_requested`, `pages_read` | WiredTiger's page requests and cache misses (likewise) |
| `hit_rate_pct` | WiredTiger's measured hit rate |
| `stats_enabled` | whether the three fields above are meaningful |
| `sampled_accesses`, `small_accesses` | sampled accesses (all partitions), and those under a quarter block |
| `sampled_bytes`, `rounded_bytes` | their total size, before and after rounding up to blocks |

`cache_bytes` is WiredTiger's `cache_size`, i.e. `--wiredTigerCacheSizeGB` or its default, not
the machine's memory.

## Getting the curve out of mongod

No extra configuration is needed. WiredTiger logs each dump as a verbose message in the
eviction category at INFO level, and mongod enables every WiredTiger verbose category at INFO by
default. mongod also opens WiredTiger with `statistics=(fast)`, so the summary line carries the
measured hit rate.

Each dump is one JSON record in the mongod log: `id` 22430 ("WiredTiger message"), component
`WTEVICT`. The summary line and the CSV are joined by newlines in `attr.message.msg`.

mongod truncates any log attribute larger than `maxLogSizeKB` (10 KB by default) and marks the
record with a `truncated` field. A dump can approach that size, and truncation drops the
largest cache sizes -- the end of the curve nearest the configured cache. If a record is
truncated, raise the limit, e.g. `--setParameter maxLogSizeKB=64`.

## Plotting and checking the curve

    python3 $IAF/integrations/mongo/plot_mrc.py mongod.log mrc.png          # last dump
    python3 $IAF/integrations/mongo/plot_mrc.py mongod.log mrc.png --all    # every dump, overlaid
    python3 $IAF/integrations/mongo/plot_mrc.py mongod.log mrc.png --windows  # each interval
    python3 $IAF/integrations/mongo/plot_mrc.py mongod.log --list             # list the dumps
    python3 $IAF/integrations/mongo/plot_mrc.py mongod.log mrc.png --since 9  # after dump 9
    python3 $IAF/integrations/mongo/plot_mrc.py mongod.log intl.png --curve internal

`plot_mrc.py` needs only matplotlib (`pip install -r requirements.txt`). It also reads a plain
WiredTiger log. It computes the miss-ratio curve as `tools/MRC-GUIDE.md` describes and
plots cache size in GB. It marks the configured cache size (dashed vertical line), WiredTiger's
eviction target (dotted vertical line) and WiredTiger's observed miss ratio (horizontal line), and
titles the plot with a warning when `curve_covers_cache=false`.

Compare the observed miss ratio with the curve at the eviction target, not at the configured size.
Eviction keeps the cache at about its target (`eviction_target`, 80% by default), so that is
roughly how much the cache holds. The plotter takes the target from the `wiredtiger_open`
config that mongod logs on "Opening WiredTiger", or uses WiredTiger's default of 80%.

Cache size is on a linear axis from 0, so the plot shows what each added GB buys. The main
curve's smallest sizes rise steeply and run off the top: its miss-ratio axis is fitted from 10 MB
up. `--log-x` uses a log axis instead, starting the main curve at 10 MB, and `--xmin-gb` sets the
left edge either way. The internal curve omits the configured cache size, which is far beyond it.

Every view of one log (the default, `--all`, `--windows`, `--since`) uses the same axes, fitted to the
whole-run curve and every window together, so plots of one run can be laid side by side. `--all`
draws the earlier dumps in the background, coloured in order and numbered by a colour bar, with
the last dump on top; the legend names only the last dump and its band. The
95% band isn't drawn below 10 MB, where the partitions disagree too much for it to be legible. The
miss-ratio axis stays within 0 to 1: values past 1 at the smallest sizes, a sampling artifact
(see "Sampling" in `tools/MRC-GUIDE.md`), are drawn at 1. Curves
are drawn as straight lines between the grid's cache sizes. Windows are drawn without their intervals, which overlap into a blur; `--window-bands` shades
them. `--windows` drops intervals with fewer than 10,000 accesses, such as the short
one before shutdown: their curves are mostly noise.

The plotted curve is the mean of the partitions' curves. The shaded band is a 95% interval for
that mean: t with k − 1 degrees of freedom times the partitions' spread over √k, times
√(1 − k/2^`sampling_log2`) because disjoint samples of a fixed population vary less together than
independent ones would. It covers the curve's variance, which dominates: above the smallest
sizes, sampling bias is a few percent of it. Expect the band to be wide at the smallest sizes, where a few hot pages
decide the curve, and thin elsewhere.

It also prints the share of small accesses and how much rounding to blocks inflated the sizes. If
either is large, the block size is too coarse for the workload's pages.

### Load and task phases

Dumps are cumulative, so the default plot covers the whole connection: if the task runs in the
same `mongod` as the load before it, its curve includes the load. Read the task's curve alone, as
the cache was sized for the task:

1. `--list` prints one line per dump, numbered from 1: its time as logged, the page accesses since
   the dump before, and over those accesses WiredTiger's measured miss ratio and the curve's
   predicted one at the configured cache. A phase change shows up as a step in the miss ratios
   and usually in the access rate. A restarted `mongod` is marked `new connection`; its dumps
   start over, so the default plot of a log whose task followed a restart is the task alone.

       dump  time                     accesses  observed  predicted
          8  2026-10-06T10:00:34        884830    0.0000     0.0000
          9  2026-10-06T10:00:34        888819    0.0000     0.0000
         10  2026-10-06T10:00:40       1194893    0.3065     0.2977
         11  2026-10-06T10:00:50       1424834    0.4441     0.4318

   Here a hot phase ends and a cold one begins during dump 10's interval.

2. `--since N` plots one curve for every access after dump N up to the last dump, with its 95%
   band and WiredTiger's miss ratio over the same accesses. Pick the last dump before the task
   starts; above, `--since 9`. The interval in which the phase changed is mixed, so starting one
   dump later gives a cleaner but shorter curve.

`--windows` plots each interval between consecutive dumps as its own curve, to see whether and when
the workload changed. A colour bar numbers each window by the dump that ends it; the legend doesn't
list them.
It shows no observed miss ratio, as there is one per window; `--list` prints them.

A window's or `--since` curve is exact on the grid: it is the difference of two dumps. It still
uses reuse distances from the whole connection, as a cache that was already warm would see them,
so the first accesses after a phase change are scored against pages the earlier phase touched.
One that holds few accesses is noisy.

To check the prediction, read the curve at `cache_blocks` and compare it with
`1 - hit_rate_pct / 100`. If `curve_covers_cache=false`, the curve stops short of the
configured cache and the two can't be compared.

IAF models LRU over page accesses. WiredTiger's cache isn't pure LRU, and it also holds
per-page overhead and update structures, so the observed miss ratio can sit above the
prediction by a workload-dependent amount. An observed miss ratio well below the prediction is
unexpected and worth investigating.

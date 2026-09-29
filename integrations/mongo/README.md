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
larger than the on-disk page. It then configures IAF as follows:

- **Sampling 1 in 4** (`WT_IAF_SAMPLING_LOG2 = 2` in `src/include/analyze_cache_inline.h`).
- **Curve bound: 4 × `cache_size`**, set when the cache is created and again on reconfigure.
- **Dumps** after each processed chunk and at connection close. Each dump covers the whole
  connection so far. IAF state doesn't survive a close, so a run that populates, restarts and
  runs again produces two independent sequences. Use the last dump before the final shutdown.

Each dump is a summary line followed by the CSV described in `tools/MRC-GUIDE.md`:

    IAF-SUMMARY cache_bytes=1073741824,cache_blocks=4194304,curve_max_blocks=16777216,
    curve_covers_cache=true,bytes_inuse=745567396,pages_requested=79344883,pages_read=25065,
    hit_rate_pct=99.9684,stats_enabled=true
    77173968,2705176,79344883
    Cache Size,Hits
    ...

| field | meaning |
|---|---|
| `cache_bytes`, `cache_blocks` | configured cache size, in bytes and in curve units |
| `curve_max_blocks` | largest cache size the curve can represent |
| `curve_covers_cache` | whether `cache_blocks <= curve_max_blocks` |
| `bytes_inuse` | bytes currently in the cache |
| `pages_requested`, `pages_read` | WiredTiger's page requests and cache misses |
| `hit_rate_pct` | WiredTiger's measured hit rate |
| `stats_enabled` | whether the three fields above are meaningful |

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

`plot_mrc.py` needs only matplotlib (`pip install -r requirements.txt`). It also reads a plain
WiredTiger log. It computes the miss-ratio curve as `tools/MRC-GUIDE.md` describes and
plots cache size in GB. It marks the configured cache size (vertical line) and WiredTiger's
observed miss ratio (horizontal line), and titles the plot with a warning when
`curve_covers_cache=false`.

To check the prediction, read the curve at `cache_blocks` and compare it with
`1 - hit_rate_pct / 100`. If `curve_covers_cache=false`, the curve stops short of the
configured cache and the two can't be compared.

IAF models LRU over page accesses. WiredTiger's cache isn't pure LRU, and it also holds
per-page overhead and update structures, so the observed miss ratio can sit above the
prediction by a workload-dependent amount. An observed miss ratio well below the prediction is
unexpected and worth investigating.

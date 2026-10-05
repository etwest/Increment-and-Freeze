# Reading the IAF miss-ratio curve

How to interpret the curve that `Iaf_stringify` and `Iaf_dump_file` (`includes/iaf_api.h`)
produce. Integrations may wrap it in their own log format; see `integrations/` for those.

## Output format

    77173968,2705176,79344883
    Cache Size,Hits
    1,6371
    2,6371
    ...

The first line is metadata, not column headers:

| field | meaning |
|---|---|
| `total_requests` | sampled access count, scaled up by the sampling rate |
| `max_cache_size` | largest cache size in the curve, in blocks |
| `raw_accesses` | exact access count, including accesses that were not sampled |

`Cache Size,Hits` follows, then one row per reported cache size. `Hits` is the number of
accesses that would hit in an LRU cache of that size. To load it with pandas, use
`read_csv(path, skiprows=1)`.

## Units

`Cache Size` is in blocks of `IAF_BLOCK_SIZE` (1024) bytes, not bytes or objects.
`Iaf_write(h, addr, bytes)` counts each access as `ceil(bytes / 1024)` blocks, so a 4 KiB object
spans 4 units and a cache of N units holds N × 1024 bytes. The `max_cache_size` argument to
`Iaf_create` is in the same units.

- The axis cannot resolve cache sizes finer than 1 KiB.
- Rounding up overstates each distinct object by at most 1023 B.
- `Iaf_get_size_stats` reports how much that matters for the sizes actually written, over
  sampled accesses: how many were under a quarter block, and the total bytes before and after
  rounding.
- An object's size may change between accesses. Sizes behave like an LRU cache that holds each
  object at its current size. When an object is reused, its own size is the size on that reuse
  access. Every other object accessed since its previous access counts once, at the size of its
  latest access before the reuse.

## Computing the curve

    misses = total_requests - hits
    mrc    = misses / raw_accesses

`misses` is non-increasing in cache size. Do not use `1 - hits / total_requests`: it matches
`mrc` only when sampling is off, which is when `total_requests == raw_accesses`.

Rows are written on a fixed grid of cache sizes: 1, then each `IAF_GRID_RATIO` (1.04) times the
last, rounded to whole blocks, plus the curve's last size. The grid is the same for every dump,
so the difference of two dumps' rows is the exact curve for the accesses between them. An 18 GB
cache with a 4× bound is about 460 rows. Values are exact at the grid sizes. Between two rows
the curve is monotone, so it lies between their values; joining rows with straight lines is fine
for plotting, and a value read off the grid is uncertain by at most the difference of its two
neighbors.

The curve covers cache sizes up to the bound set by `Iaf_create` or `Iaf_set_max_cache_blocks`
(`Iaf_max_cache_blocks` reports it). It says nothing about larger caches.

## Sampling

`Iaf_create(sampling_log2, ...)` samples 1 in 2^`sampling_log2` addresses; 0 disables sampling.
The output is already scaled back up: `Cache Size` is in real blocks, and `total_requests` and
`Hits` are sampled counts multiplied by the rate. Do not scale them again.

Sampling selects whole addresses. An address that receives many accesses is either entirely in
the sample or entirely out, so `total_requests` can differ from `raw_accesses` by much more than
a few percent. Two properties follow:

- `misses` is unaffected once the cache is large enough to keep such an address resident,
  because its accesses then add equally to `total_requests` and `Hits`. That's why `mrc` divides
  by `raw_accesses`, which is exact, and not by `total_requests`, which carries this error.
- At the smallest cache sizes, almost every access misses, so `mrc` approaches
  `total_requests / raw_accesses`. That ratio exceeds 1 when the sample drew more than its
  share of accesses. This is expected; keep it in the data, though a plot may draw it at 1.

`Iaf_provable_floor_blocks_at(h, ε)` is the smallest cache size at which the sampled curve is
provably accurate to ε (`Iaf_provable_floor_blocks` is ε = 10%). A sampled access with true
stack depth D has sampled depth Bin(D, p), so the expected curve at C pages is the true curve
smoothed by Pr[Bin(D, p) > Cp], which rises with D. If depth (1−ε)C counts as a miss, and depth
(1+ε)C as a hit, each with probability at most η = 1%, the expected curve lies between the true
curve at (1 ± ε)C, give or take η. This holds per cache size, so it doesn't grow with the trace
length. IAF finds the smallest such C from exact binomial tails, about 550/p pages at ε = 10%
(Chernoff, 3 ln(1/η)/ε² ≈ 1,400/p, is 2.5× looser), times the mean sampled access size. It
bounds bias only; the curve also varies around its expectation, most where heavy hitters miss
and where the curve is steep. Errors below the floor don't spread to larger sizes. Without
sampling it is 0. `Iaf_provable_eps_at(h, blocks)` is the inverse: the smallest ε that holds at a
given cache size, such as the configured one.

With sampling, the curve is written with a continuity correction. A sampled access counts as a
hit when the sampled pages between its uses fit in p times the cache, and that sampled size moves
in steps of one page, so IAF compares against the middle of the step: it counts a miss at true
size C when the sampled size exceeds Cp − h, with h = E[w²]/(2E[w]) over sampled access sizes w
(the size-biased half step; w/2 when pages are equal). That shifts every cache size up by h/p,
roughly halves the curve's bias and centres it on zero. The shift is fixed at the first dump
with any accesses, so every later dump maps the grid the same way. The floor uses
the same threshold. Below the shift, about one sampled page, the curve says nothing.

`Iaf_create_partitions(sampling_log2, k, ...)` keeps k disjoint samples, partitions 0 .. k−1
of the hash, each at rate q = 2^−`sampling_log2`, with one curve each (`Iaf_stringify_partition`).
Their mean is exactly a single sample at rate kq, and its standard error is
sd/√k · √(1 − kq), where sd is the partitions' spread. With few partitions, use a t quantile
with k − 1 degrees of freedom for an interval (3.18 for 95% at k = 4). Each partition's floor is set by q, not
kq, so splitting raises the floor k-fold in exchange for the error bar.

Accuracy is lowest at both ends of the curve. At very small sizes, the curve depends mostly on
which addresses were sampled. Near the working-set size, the last few misses come from a few
sampled addresses, so the curve moves in steps: absolute error stays small, but relative error
can be large.

## Successive dumps

Each dump covers every access since `Iaf_create` or the last `Iaf_reset`, not just the accesses
since the previous dump. Comparing successive dumps shows whether the curve has converged; if the
last few differ materially, the trace was too short.

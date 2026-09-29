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

`Cache Size` is in blocks of `IAF_BLOCK_SIZE` (256) bytes, not bytes or objects.
`Iaf_write(h, addr, bytes)` counts each access as `ceil(bytes / 256)` blocks, so a 4 KiB object
spans 16 units and a cache of N units holds N × 256 bytes. The `max_cache_size` argument to
`Iaf_create` is in the same units.

- The axis cannot resolve cache sizes finer than 256 B.
- Rounding up overstates each distinct object by at most 255 B.
- An object's size may change between accesses. Sizes behave like an LRU cache that holds each
  object at its current size. When an object is reused, its own size is the size on that reuse
  access. Every other object accessed since its previous access counts once, at the size of its
  latest access before the reuse.

## Computing the curve

    misses = total_requests - hits
    mrc    = misses / raw_accesses

`misses` is non-increasing in cache size. Do not use `1 - hits / total_requests`: it matches
`mrc` only when sampling is off, which is when `total_requests == raw_accesses`.

Rows can be sparse. If the curve spans fewer than 1000 cache sizes, every size is written.
Otherwise a row is written only when the cache size has grown 5% or `Hits` has grown 1% (at
least one) since the previous row, so spacing is roughly geometric. Treat the curve as a step
function: the value at a size is the last row at or below it. Don't interpolate linearly across
gaps.

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
  share of accesses. This is expected; do not clamp it.

Accuracy is lowest at both ends of the curve. At very small sizes, the curve depends mostly on
which addresses were sampled. Near the working-set size, the last few misses come from a few
sampled addresses, so the curve moves in steps: absolute error stays small, but relative error
can be large.

## Successive dumps

Each dump covers every access since `Iaf_create` or the last `Iaf_reset`, not just the accesses
since the previous dump. Comparing successive dumps shows whether the curve has converged; if the
last few differ materially, the trace was too short.

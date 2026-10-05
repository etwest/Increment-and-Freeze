// C-compatible header

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdint.h>
#include <atomic>
#include <sstream>
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <memory>
#include <vector>

#include "iaf_api.h"
#include "bounded_iaf.h"
#include "cache_sim.h"

/* One sample of the address space: a partition of the sampling hash, with its own curve and lock,
 * so that samples, and separate curves fed from the same accesses, don't contend. */
struct IafPart
{
    BoundedIAF b;

    /* Guarded by lock, so only accesses sampled into this partition are counted. */
    Iaf_size_stats size_stats{};

    /* Sum of squared access sizes in blocks, for the continuity correction's step. */
    double blocks_sq = 0;

    /* The continuity correction's shift, in blocks, once fixed (see iaf_print); negative until. */
    double shift = -1;

    std::mutex lock;

    IafPart(int sampling_log2, size_t partition, size_t max_cache_size):
    b(sampling_log2, 101010101010, partition, 100000, max_cache_size) {};
};

struct Iaf_t
{
    /* Disjoint partitions 0 .. n-1 of the hash, each sampling 1 in 2^sampling_log2 addresses. */
    std::vector<std::unique_ptr<IafPart>> parts;

    /* Every access, sampled or not. Each partition's curve reports this as its access count, so
     * the accesses a partition didn't sample never need to take its lock. */
    std::atomic<uint64_t> accesses{0};

    Iaf_t(int sampling_log2, size_t partitions, size_t max_cache_size)
    {
        for (size_t i = 0; i < partitions; ++i)
            parts.push_back(std::make_unique<IafPart>(sampling_log2, i, max_cache_size));
    }
};

/* Bring a partition's access count up to date. Caller must hold part.lock. */
static void iaf_sync_accesses(Iaf h, IafPart& part)
{
    part.b.set_access_count(h->accesses.load(std::memory_order_relaxed));
}

Iaf Iaf_create(int sampling_log2, size_t max_cache_size)
{
    return Iaf_create_partitions(sampling_log2, 1, max_cache_size);
}

Iaf Iaf_create_partitions(int sampling_log2, size_t partitions, size_t max_cache_size)
{
    /* There are only 2^sampling_log2 partitions to have. */
    const size_t available = (size_t)1 << sampling_log2;
    if (partitions == 0)
        partitions = 1;
    if (partitions > available)
        partitions = available;
    return new Iaf_t(sampling_log2, partitions, max_cache_size);
}

size_t Iaf_partitions(Iaf h)
{
    return h == nullptr ? 0 : h->parts.size();
}

constexpr size_t kBlockSize = IAF_BLOCK_SIZE;
static_assert(BoundedIAF::kGridRatio == IAF_GRID_RATIO, "IAF_GRID_RATIO must match the writer");

void Iaf_reset(Iaf h)
{
    if (h == nullptr)
        return;
    for (auto& part : h->parts) {
        std::scoped_lock guard{part->lock};
        part->size_stats = Iaf_size_stats{};
        part->blocks_sq = 0;
        part->shift = -1;
        part->b.reset();
    }
    h->accesses.store(0, std::memory_order_relaxed);
}

bool Iaf_write(Iaf h, void* addr, size_t bytes)
{
    assert(addr != (void*)IAF_ID_UNINIT && "Uninitialized addr!");
    assert(addr != (void*)IAF_ID_NEED_REINIT && "Addr marked for reinit but never reinit!");
    if (addr == (void*)IAF_ID_IGNORE || addr == (void*)IAF_PAGE_OVERFLOW)
        return false;

    h->accesses.fetch_add(1, std::memory_order_relaxed);

    /* The partition is a pure function of the address and immutable state, so the accesses no
     * partition samples -- most of them, once sampling is on -- never need to touch a lock. */
    const size_t i = h->parts[0]->b.partition_of((req_count_t)addr);
    if (i >= h->parts.size())
        return false;
    IafPart& part = *h->parts[i];

    std::scoped_lock guard{part.lock};
    // A zero-byte access still occupies a slot; a zero-block request would freeze at distance 0.
    req_count_t nblocks = bytes == 0 ? 1 : (bytes + kBlockSize - 1) / kBlockSize;
    part.size_stats.accesses++;
    if (bytes < kBlockSize / 4)
        part.size_stats.small_accesses++;
    part.size_stats.bytes += bytes;
    part.size_stats.rounded_bytes += nblocks * kBlockSize;
    part.blocks_sq += (double)nblocks * (double)nblocks;
    const bool processed = part.b.memory_access((req_count_t)addr, nblocks);

    /* Report only partition 0's chunks: every partition sees about the same number of sampled
     * accesses, so this keeps dumps at one per chunk rather than one per chunk per partition. */
    return processed && i == 0;
}

static void iaf_print(Iaf h, size_t partition, std::ostream& os)
{
    IafPart& part = *h->parts[partition];
    std::scoped_lock guard{part.lock};
    iaf_sync_accesses(h, part);

    /* Continuity correction. A sampled access counts as a hit when the sampled size of the pages
     * between its uses fits in p times the cache. That size moves in steps of one sampled page, so
     * compare against the middle of the step: count a miss at true size C when it exceeds C p - h,
     * which shifts every cache size up by h / p. For pages of unequal size the step a threshold
     * lands in is size-biased, so h = E[w^2] / (2 E[w]) over sampled access sizes w, not E[w] / 2.
     * It roughly halves the curve's bias and centres it on zero. The shift is fixed at the first
     * dump with any accesses, so that every dump of a connection maps the grid the same way and
     * successive dumps difference exactly; an estimate from few accesses costs a little bias at
     * the smallest sizes, a shift that moved would corrupt every window across the move. */
    const Iaf_size_stats& sz = part.size_stats;
    double shift = part.shift;
    if (shift < 0) {
        const double blocks = (double)sz.rounded_bytes / (double)kBlockSize;
        const double half_step = sz.accesses == 0 ? 0.5 : part.blocks_sq / blocks / 2.0;
        shift = half_step * (double)part.b.get_samples_per_measure();
        if (sz.accesses > 0)
            part.shift = shift;
    }

    part.b.flush();
    part.b.print_small_csv_streaming(os, shift);
}

char* Iaf_stringify_partition(Iaf h, size_t partition)
{
    if (h == nullptr || partition >= h->parts.size())
        return nullptr;
    std::stringstream ss;
    iaf_print(h, partition, ss);

    // strdup so the result stays free()-able by Iaf_free_string. stringstream::view() would save a
    // copy of the string here, but it is C++20 and the bazel build does not pin a standard.
    const std::string s = ss.str();
    return strdup(s.c_str());
}

char* Iaf_stringify(Iaf h)
{
    return Iaf_stringify_partition(h, 0);
}

void Iaf_dump_file(Iaf h, const char* filepath)
{
    std::ofstream out(filepath);
    iaf_print(h, 0, out);
}

void Iaf_free_string(char* s)
{
    free(s);
}

void Iaf_destroy(Iaf* h)
{
    /* The caller guarantees nothing else uses *h by now; the locks die with it. */
    delete *h;
    *h = nullptr;
}

size_t Iaf_max_cache_blocks(Iaf h)
{
    if (h == nullptr)
        return 0;
    std::scoped_lock guard{h->parts[0]->lock};
    return h->parts[0]->b.get_max_cache_size();
}

void Iaf_set_max_cache_blocks(Iaf h, size_t max_cache_blocks)
{
    if (h == nullptr || max_cache_blocks == 0)
        return;
    for (auto& part : h->parts) {
        std::scoped_lock guard{part->lock};
        part->b.set_max_cache_size(max_cache_blocks);
    }
}

void Iaf_get_size_stats(Iaf h, Iaf_size_stats* out)
{
    *out = Iaf_size_stats{};
    if (h == nullptr)
        return;
    for (auto& part : h->parts) {
        std::scoped_lock guard{part->lock};
        out->accesses += part->size_stats.accesses;
        out->small_accesses += part->size_stats.small_accesses;
        out->bytes += part->size_stats.bytes;
        out->rounded_bytes += part->size_stats.rounded_bytes;
    }
}

/* Miss-ratio slack the floor allows on top of its cache-size error eps. */
constexpr double kFloorEta = 0.01;

/* Largest cache-size error Iaf_provable_eps_at reports; past it, it returns HUGE_VAL. */
constexpr double kMaxProvableEps = 16.0;

/* log Pr[Bin(n, p) = k]. */
static double binom_log_pmf(uint64_t n, uint64_t k, double p)
{
    return std::lgamma((double)n + 1) - std::lgamma((double)k + 1) -
      std::lgamma((double)(n - k) + 1) + (double)k * std::log(p) + (double)(n - k) * std::log1p(-p);
}

/* Sum of Pr[Bin(n, p) = k] from k = from outward, in steps of step (+1 or -1), until the terms
 * vanish. Only call it moving away from the mode, where the terms shrink geometrically, so the
 * cost is a few standard deviations of terms, not n p. */
static double binom_tail_from(uint64_t n, uint64_t from, int step, double p)
{
    const double odds = p / (1.0 - p);
    double term = std::exp(binom_log_pmf(n, from, p));
    double sum = 0.0;
    for (uint64_t k = from;;) {
        sum += term;
        if (term <= sum * 1e-17 || (step > 0 ? k == n : k == 0))
            break;
        term *= step > 0 ? (double)(n - k) / (double)(k + 1) * odds :
                           (double)k / (double)(n - k + 1) / odds;
        k += step;
    }
    return std::min(1.0, sum);
}

/* Pr[Bin(n, p) <= t]. The smaller tail is summed directly and the other is its complement. */
static double binom_cdf(uint64_t n, double t, double p)
{
    if (t < 0)
        return 0.0;
    const uint64_t top = (uint64_t)std::floor(t);
    if (top >= n)
        return 1.0;
    if ((double)top < (double)n * p)
        return binom_tail_from(n, top, -1, p);
    return 1.0 - binom_tail_from(n, top + 1, +1, p);
}

/* Whether a sample at rate p is within eps at a cache of c pages: an access of true depth
 * (1 - eps) c must miss, and one of (1 + eps) c hit, each with probability at most kFloorEta. */
static bool floor_holds(uint64_t c, double p, double eps)
{
    /* With the same continuity correction the curve is written with (see iaf_print). */
    const double threshold = (double)c * p - 0.5;
    /* Past eps = 1 the lower side is empty: no access is shallower than depth 0. */
    const uint64_t shallow = eps >= 1.0 ? 0 : (uint64_t)std::floor((1.0 - eps) * (double)c);
    const uint64_t deep = (uint64_t)std::ceil((1.0 + eps) * (double)c);
    const double false_miss = 1.0 - binom_cdf(shallow, threshold, p);
    const double false_hit = binom_cdf(deep, threshold, p);
    return false_miss <= kFloorEta && false_hit <= kFloorEta;
}

size_t Iaf_provable_floor_blocks_at(Iaf h, double eps)
{
    if (h == nullptr || eps <= 0)
        return 0;

    /* Each partition is its own sample at rate p, and averaging them does not reduce bias, so the
     * floor is set by the per-partition rate. The mean size pools every partition. */
    const double p = 1.0 / (double)h->parts[0]->b.get_samples_per_measure();
    Iaf_size_stats sz;
    Iaf_get_size_stats(h, &sz);
    if (p == 1.0 || sz.accesses == 0)
        return 0;

    /* A sampled access with true stack depth D has sampled depth Bin(D, p), so the expected sampled
     * curve at c pages is the true curve smoothed by Pr[Bin(D, p) > c p], which rises with D. If an
     * access at depth (1 - eps) c counts as a miss with probability at most eta, and one at
     * (1 + eps) c as a hit with probability at most eta, the expected curve lies between the true
     * curve at (1 + eps) c and (1 - eps) c, give or take eta. That holds at each cache size on its
     * own, so there is no union bound over accesses. The exact tails are about 2.5x tighter than
     * the Chernoff bound 3 ln(1/eta) / eps^2; the condition only gets easier as c grows, so find
     * the smallest c by bisection. */
    uint64_t lo = 1, hi = 1;
    while (!floor_holds(hi, p, eps))
        hi *= 2;
    while (lo < hi) {
        const uint64_t mid = lo + (hi - lo) / 2;
        if (floor_holds(mid, p, eps))
            hi = mid;
        else
            lo = mid + 1;
    }
    const double mean_bytes = (double)sz.bytes / (double)sz.accesses;
    return (size_t)std::ceil((double)lo * mean_bytes / (double)kBlockSize);
}

size_t Iaf_provable_floor_blocks(Iaf h)
{
    return Iaf_provable_floor_blocks_at(h, 0.1);
}

double Iaf_provable_eps_at(Iaf h, size_t blocks)
{
    if (h == nullptr)
        return 0.0;
    const double p = 1.0 / (double)h->parts[0]->b.get_samples_per_measure();
    Iaf_size_stats sz;
    Iaf_get_size_stats(h, &sz);
    if (p == 1.0 || sz.accesses == 0)
        return 0.0;

    /* The inverse of the floor: the same condition, with the cache fixed in pages and eps found by
     * bisection, since a larger eps only moves both depths further from the threshold. */
    const double mean_bytes = (double)sz.bytes / (double)sz.accesses;
    const uint64_t c = (uint64_t)((double)blocks * (double)kBlockSize / mean_bytes);
    double lo = 0.0, hi = kMaxProvableEps;
    if (c == 0 || !floor_holds(c, p, hi))
        return HUGE_VAL;
    while (hi - lo > 1e-4 * hi) {
        const double mid = (lo + hi) / 2;
        if (floor_holds(c, p, mid))
            hi = mid;
        else
            lo = mid;
    }
    return hi;
}

std::atomic<uint64_t> counter(IAF_ID_RESERVED_BOUNDARY); // Initialize an atomic counter to minimum value

uint64_t Iaf_grab_id(Iaf h)
{
    return ++counter;
}
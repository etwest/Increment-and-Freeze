// C-compatible header
#pragma once

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

struct Iaf_t;
typedef struct Iaf_t* Iaf;

#define IAF_ID_UNINIT 0
#define IAF_ID_NEED_REINIT 1
#define IAF_ID_IGNORE 2
#define IAF_PAGE_OVERFLOW 3
#define IAF_ID_RESERVED_BOUNDARY 3

/*
 * Granularity, in bytes, of a single unit on the cache-size axis of the curve produced by
 * Iaf_stringify / Iaf_dump_file. Iaf_write rounds each access up to a whole number of these, and
 * the max_cache_size argument to Iaf_create is counted in them. Callers need this to convert
 * between their own byte-denominated cache sizes and the curve.
 */
#define IAF_BLOCK_SIZE 1024

/*
 * The curve's rows are written at cache sizes 1, then each IAF_GRID_RATIO times the last (rounded
 * to whole blocks), the same for every dump, so successive dumps can be differenced exactly.
 */
#define IAF_GRID_RATIO 1.04

/*
 * How well IAF_BLOCK_SIZE fits the sizes actually written, over sampled accesses in all
 * partitions. An access is small when it is under a quarter of a block, so rounding it up more
 * than quadruples it. rounded_bytes / bytes is how much rounding inflates the sizes the curve is
 * built from.
 */
typedef struct {
    uint64_t accesses;
    uint64_t small_accesses;
    uint64_t bytes;
    uint64_t rounded_bytes;
} Iaf_size_stats;

Iaf Iaf_create(int sampling_log2, size_t max_cache_size);

/*
 * Several independent samples at once: partitions 0 .. partitions-1 of the sampling hash, each
 * sampling 1 in 2^sampling_log2 addresses into a curve of its own. They are disjoint, so together
 * they cost what one sample of partitions / 2^sampling_log2 would, and their spread gives an error
 * bar. At most 2^sampling_log2 partitions; Iaf_create is the same with one. Iaf_stringify and
 * Iaf_dump_file report partition 0; Iaf_write reports partition 0's chunks.
 */
Iaf Iaf_create_partitions(int sampling_log2, size_t partitions, size_t max_cache_size);
size_t Iaf_partitions(Iaf h);
char* Iaf_stringify_partition(Iaf h, size_t partition);
void Iaf_destroy(Iaf* h);
void Iaf_reset(Iaf h);
bool Iaf_write(Iaf h, void* addr, size_t bytes);
char* Iaf_stringify(Iaf h);
void Iaf_dump_file(Iaf h, const char* filepath);
void Iaf_flush(Iaf h);
uint64_t Iaf_grab_id(Iaf h);

/*
 * The next ID Iaf_grab_id hands out, and setting it. IDs are process-wide, not per handle. A caller
 * whose IDs outlive the process (WiredTiger stores them on disk) saves the next ID and sets it again
 * at the next start, so IDs aren't reused.
 */
uint64_t Iaf_check_next_id(Iaf h);
void Iaf_set_next_id(Iaf h, uint64_t id);
void Iaf_get_size_stats(Iaf h, Iaf_size_stats* out);

/*
 * Largest cache size, in IAF_BLOCK_SIZE units, that the curve can represent. Requests older than
 * this are dropped, so the curve says nothing about cache sizes beyond it.
 */
size_t Iaf_max_cache_blocks(Iaf h);

/*
 * Re-bound the curve, in IAF_BLOCK_SIZE units. Intended to be called once the caller knows the
 * cache size it wants the curve to cover, which it may not at Iaf_create time. Raising the bound
 * applies to subsequent requests only; history already dropped under a smaller bound is gone. A
 * bound of zero is ignored rather than dropping everything.
 */
void Iaf_set_max_cache_blocks(Iaf h, size_t max_cache_blocks);
void Iaf_free_string(char*);

#ifdef __cplusplus
}
#endif

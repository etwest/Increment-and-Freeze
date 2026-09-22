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
#define IAF_BLOCK_SIZE 256

Iaf Iaf_create(int sampling_log2, size_t max_cache_size);
void Iaf_destroy(Iaf* h);
void Iaf_reset(Iaf h);
bool Iaf_write(Iaf h, void* addr, size_t bytes);
char* Iaf_stringify(Iaf h);
void Iaf_dump_file(Iaf h, const char* filepath);
void Iaf_flush(Iaf h);
uint64_t Iaf_grab_id(Iaf h);

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

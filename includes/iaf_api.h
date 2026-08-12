// C-compatible header
#pragma once

#include <stddef.h>
#include <stdint.h>
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

Iaf Iaf_create(int sampling_log2, size_t max_cache_size);
bool Iaf_write(Iaf h, void* addr, size_t bytes);
char* Iaf_stringify(Iaf h);
void Iaf_destroy(Iaf* h);
void Iaf_flush(Iaf h);
uint64_t Iaf_grab_id(Iaf h);
void Iaf_free_string(char*);

#ifdef __cplusplus
}
#endif

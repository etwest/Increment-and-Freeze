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
#define IAF_ID_RESERVED_BOUNDARY 2 

Iaf Iaf_create(int sampling_log2, size_t max_cache_size);
void Iaf_write(Iaf h, void* addr);
void Iaf_print(Iaf h);
void Iaf_destroy(Iaf* h);
uint64_t Iaf_grab_id(Iaf h);

#ifdef __cplusplus
}
#endif

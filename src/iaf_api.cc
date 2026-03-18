// C-compatible header

#include "iaf_api.h"
#include "bounded_iaf.h"
#include "cache_sim.h"

struct Iaf_t
{
    BoundedIAF b;

    Iaf_t(int sampling_log2, size_t max_cache_size): 
    b(sampling_log2, 101010101010, 0, 65536, max_cache_size) {};
};

Iaf Iaf_create(int sampling_log2, size_t max_cache_size)
{
    return new Iaf_t(sampling_log2, max_cache_size);
}

void Iaf_write(Iaf h, void* addr)
{
    h->b.memory_access((req_count_t)addr);
}

void Iaf_print(Iaf h)
{
}

void Iaf_destroy(Iaf h)
{
    delete h;
}

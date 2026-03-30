// C-compatible header

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdint.h>

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

std::mutex iaf_lock;
void Iaf_write(Iaf h, void* addr)
{
    std::scoped_lock lock{iaf_lock};  //TODO: Remove this lock eventually?
    h->b.memory_access((req_count_t)addr);
}

void Iaf_print(Iaf h)
{
    h->b.csv_success_function(std::cout, h->b.get_success_function());
}

void Iaf_destroy(Iaf* h)
{
    delete *h;
    *h = nullptr;
}

std::atomic<uint64_t> counter(0); // Initialize an atomic counter to 0

uint64_t Iaf_grab_id(Iaf h)
{
    return ++counter;
}
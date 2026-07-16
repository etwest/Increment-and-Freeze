// C-compatible header

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdint.h>
#include <atomic>
#include <sstream>
#include <cstring>

#include "iaf_api.h"
#include "bounded_iaf.h"
#include "cache_sim.h"

struct Iaf_t
{
    BoundedIAF b;

    Iaf_t(int sampling_log2, size_t max_cache_size): 
    b(sampling_log2, 101010101010, 0, 100000, max_cache_size) {};
};

Iaf Iaf_create(int sampling_log2, size_t max_cache_size)
{
    return new Iaf_t(sampling_log2, max_cache_size);
}

std::mutex iaf_lock;
bool Iaf_write(Iaf h, void* addr)
{
    assert(addr != (void*)IAF_ID_UNINIT && "Uninitialized addr!");
    assert(addr != (void*)IAF_ID_NEED_REINIT && "Addr marked for reinit but never reinit!");
    if (addr == (void*)IAF_ID_IGNORE || addr == (void*)IAF_PAGE_OVERFLOW)
        return false;
    std::scoped_lock lock{iaf_lock};  //TODO: Remove this lock eventually?
    return h->b.memory_access((req_count_t)addr);
}

void Iaf_print(Iaf h)
{
    std::scoped_lock lock{iaf_lock};
    //h->b.csv_success_function(std::cerr, h->b.get_success_function());
   h->b.print_small_csv(std::cerr, h->b.get_success_function());

}

char* Iaf_stringify(Iaf h)
{
    std::scoped_lock lock{iaf_lock};
    std::stringstream ss;
    h->b.print_small_csv(ss, h->b.get_success_function());
    const std::string& s = ss.str();
    return strdup(s.c_str());
}

void Iaf_destroy(Iaf* h)
{
    std::scoped_lock lock{iaf_lock};
    delete *h;
    *h = nullptr;
}

std::atomic<uint64_t> counter(IAF_ID_RESERVED_BOUNDARY); // Initialize an atomic counter to minimum value

uint64_t Iaf_grab_id(Iaf h)
{
    return ++counter;
}
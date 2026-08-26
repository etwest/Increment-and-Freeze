// C-compatible header

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdint.h>
#include <atomic>
#include <sstream>
#include <cstring>
#include <fstream>

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
constexpr size_t kBlockSize = 256;

void Iaf_reset(Iaf h)
{
    if (h == nullptr)
        return;
    std::scoped_lock lock{iaf_lock};
    h->b.reset();
}

bool Iaf_write(Iaf h, void* addr, size_t bytes)
{
    assert(addr != (void*)IAF_ID_UNINIT && "Uninitialized addr!");
    assert(addr != (void*)IAF_ID_NEED_REINIT && "Addr marked for reinit but never reinit!");
    if (addr == (void*)IAF_ID_IGNORE || addr == (void*)IAF_PAGE_OVERFLOW)
        return false;
    
    std::scoped_lock lock{iaf_lock};  //TODO: Remove this lock eventually?
    req_count_t nblocks = (bytes + kBlockSize - 1) / kBlockSize;
    return h->b.memory_access((req_count_t)addr, nblocks);
}

char* Iaf_stringify(Iaf h)
{
    std::scoped_lock lock{iaf_lock};
    std::stringstream ss;
    h->b.flush();
    h->b.print_small_csv(ss, h->b.get_success_function());
    const std::string& s = ss.str();
    return strdup(s.c_str());
}

void Iaf_dump_file(Iaf h, const char* filepath)
{
    std::scoped_lock lock{iaf_lock};
    std::ofstream out(filepath);
    h->b.flush();
    h->b.print_small_csv(out, h->b.get_success_function());
}

void Iaf_free_string(char* s)
{
    free(s);
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
// C-compatible header

#include "iaf_api.h"
#include "bounded_iaf.h"
#include "cache_sim.h"

struct Iaf_t
{
    BoundedIAF b;

    Iaf_t(): b() {};
};

Iaf Iaf_create(void)
{
    return new Iaf_t();
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

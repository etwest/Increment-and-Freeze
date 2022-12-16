#ifndef ONLINE_CACHE_SIMULATOR_SIM_FACTORY_H_
#define ONLINE_CACHE_SIMULATOR_SIM_FACTORY_H_

#include "container_cache_sim.h"
#include "iak_wrapper.h"
#include "increment_and_freeze.h"
#include "ost_cache_sim.h"

// An enum describing the different CacheSims
enum CacheSimType {
  OS_TREE,
  OS_SET,
  IAF,
  CHUNK_IAF,
};

std::unique_ptr<CacheSim> new_simulator(CacheSimType sim_enum, size_t min_chunk = 65536,
                                        size_t mem_limit = 0) {
  switch (sim_enum) {
    case OS_TREE:
      return std::make_unique<OSTCacheSim>();
    case OS_SET:
      return std::make_unique<ContainerCacheSim>();
    case IAF:
      return std::make_unique<IncrementAndFreeze>();
    case CHUNK_IAF:
      if (mem_limit != 0)
        return std::make_unique<IAKWrapper>(min_chunk, mem_limit);
      else
        return std::make_unique<IAKWrapper>(min_chunk);
    default:
      std::cerr << "ERROR: Unrecognized sim_enum!" << std::endl;
      exit(EXIT_FAILURE);
  }
}

#endif  // ONLINE_CACHE_SIMULATOR_SIM_FACTORY_H_

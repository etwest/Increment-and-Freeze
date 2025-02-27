/*
 * Increment-and-Freeze is an efficient library for computing LRU hit-rate curves.
 * Copyright (C) 2023 Daniel DeLayo, Bradley Kuszmaul, Evan West
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef ONLINE_CACHE_SIMULATOR_SIM_FACTORY_H_
#define ONLINE_CACHE_SIMULATOR_SIM_FACTORY_H_

#include "bounded_iaf.h"
#include "container_cache_sim.h"
#include "increment_and_freeze.h"
#include "ost_cache_sim.h"

// An enum describing the different CacheSims
enum CacheSimType {
  OS_TREE,
  OS_SET,
  IAF,
  BOUND_IAF,
};

struct SimulatorArgs {
  size_t sampling_rate = 0;
  size_t min_chunk = 65536;
  size_t k_mem_limit = 0;
};

std::unique_ptr<CacheSim> new_simulator(CacheSimType sim_enum, SimulatorArgs args) {
  switch (sim_enum) {
    case OS_TREE:
      return std::make_unique<OSTCacheSim>();
    case OS_SET:
      return std::make_unique<ContainerCacheSim>();
    case IAF:
      return std::make_unique<IncrementAndFreeze>(args.sampling_rate);
    case BOUND_IAF:
      if (args.k_mem_limit != 0)
        return std::make_unique<BoundedIAF>(args.min_chunk, args.k_mem_limit);
      else
        return std::make_unique<BoundedIAF>(args.min_chunk);
    default:
      std::cerr << "ERROR: Unrecognized sim_enum!" << std::endl;
      exit(EXIT_FAILURE);
  }
}

#endif  // ONLINE_CACHE_SIMULATOR_SIM_FACTORY_H_

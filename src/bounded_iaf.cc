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

#include "bounded_iaf.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "increment_and_freeze.h"

bool BoundedIAF::memory_access(req_count_t addr, req_count_t nblocks) {
  auto &requests = chunk_input.requests;

  access_number++;
  if (!should_sample(addr)) return false;

  
  sample_access_number++;
  
  // small optimization, first check that the request is not a repeated request. Only valid for a
  // single block, before and after: a wider request is a hit only once the cache holds all of it,
  // and a request that changes size must go through IAF to update the size it is counted at.
  if (nblocks == 1 && requests.size() && addr == requests.back().addr &&
      requests.back().nblocks == 1) {
    ++num_duplicates;
  } else {
    requests.push_back({addr, (req_count_t) requests.size() + 1, nblocks});

    if (requests.size() >= get_u()) {
      // std::cout << "requests chunk array:" << std::endl;
      // for (auto req : requests) {
      //   std::cout << req.first << "," << req.second << std::endl;
      // }

      process_requests();
      return true;
    }
  }
  return false;
}

void print_result(IncrementAndFreeze::ChunkOutput& result) {
  std::cout << "living requests" << std::endl;
  for (auto living: result.living_requests)
    std::cout << living.addr << "," << living.access_number << " ";
  std::cout << std::endl;

  std::cout << "hits vector: " << result.hits_vector.size() << std::endl;
  for (size_t i = 0; i < result.hits_vector.size() - 1; i++)
    std::cout << result.hits_vector[i+1] << " ";
  std::cout << std::endl;
}

void BoundedIAF::process_requests() {
  STARTTIME(proc_req);
  // std::cout << std::endl;
  // std::cout << "Processing chunk" << std::endl;
  // std::cout << "Living requests " << chunk_input.output.living_requests.size() << std::endl;
  // std::cout << "New Requests " << chunk_input.chunk_requests.size() - chunk_input.output.living_requests.size() << std::endl;
  // std::cout << "CHUNK:" << std::endl;
  // for (auto item : chunk_input.chunk_requests)
  //   std::cout << item.second << ":" << item.first << " ";
  // std::cout << std::endl;

  // auto start = std::chrono::high_resolution_clock::now();

  iaf_alg.process_chunk(chunk_input, max_living_req);

  // update maximum memory usage
  if (iaf_alg.get_memory_usage() > memory_usage)
    memory_usage = iaf_alg.get_memory_usage();
  
  // auto depth_time =  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
  // std::cout << "GET DEPTH TIME: " << depth_time << std::endl;

  ChunkOutput& result = chunk_input.output;
  // print_result(result);

  size_t living_nblocks = 0;
  for (auto &req : result.living_requests) {
    living_nblocks += req.nblocks;
  }
  // Trim only at the bound. The living set can total less than hits already recorded -- pages
  // shrink -- and those hits are real, so do not size the vector down to the living set.
  if (result.hits_vector.size() < 1 + living_nblocks)
    result.hits_vector.resize(1 + living_nblocks);
  else if (result.hits_vector.size() > 1 + max_living_req)
    result.hits_vector.resize(1 + max_living_req);

  // Fix the index of the living requests so they count up from 1
  size_t num_living = 0;
  for (auto &living_req : result.living_requests)
    living_req.access_number = ++num_living;

  chunk_input.requests.clear();
  // std::cout << "Size of hits vector = " << result.hits_vector.size() << std::endl;
  // std::cout << "Number of living requests = " << living.size() << std::endl;
  // std::cout << "First index of distance histogram = " << chunk_input.output.hits_vector[1] << std::endl;

  // prepare for next iteration
  update_u(living_nblocks);
  chunk_input.requests.reserve(get_u());
  chunk_input.requests.insert(chunk_input.requests.end(), result.living_requests.begin(), result.living_requests.end());
  STOPTIME(proc_req);
}

void BoundedIAF::flush() {
  if (chunk_input.requests.size() - chunk_input.output.living_requests.size() > 0) {
    process_requests();
  }
}

void BoundedIAF::print_small_csv_streaming(std::ostream& os, double shift) {
  // Ensure all requests processed
  flush();

  const SuccessVector& hits = chunk_input.output.hits_vector;
  const size_t samples_per_measure = sample_mask + 1;
  if (!sample_mask)
    shift = 0;

  // Size the success function would have had, without building it. The shift moves every entry
  // up by that many blocks, so the curve runs that much further to keep its last entry.
  const size_t unshifted_size =
    hits.size() == 0 ? 1 : 1 + (hits.size() - 1) * samples_per_measure;
  if (unshifted_size <= 1)
    return;
  const size_t succ_size = unshifted_size + (size_t)std::ceil(shift);

  size_t total_requests = access_number - 1;
  if (sample_mask)
    total_requests = (sample_access_number - 1) * samples_per_measure;

  os << total_requests << "," << succ_size - 1 << "," << access_number - 1 << std::endl;
  os << "Cache Size,Hits" << std::endl;

  // succ[page] is the prefix sum of hits up to the entry covering that page. Without sampling that
  // is one hits entry per page; with sampling each entry covers samples_per_measure pages and the
  // count is scaled back up. Pages are visited in increasing order, so the prefix sum only ever
  // moves forward.
  size_t hits_idx = 0;
  size_t running_count = num_duplicates;
  auto succ_at = [&](size_t page) {
    size_t want = page;
    if (sample_mask) {
      // Entry b covers caches of (b - 1) * S + 1 blocks and up, moved up by shift.
      const double shifted = std::ceil((double)page - shift);
      want = shifted < 1 ? 0 : ((size_t)shifted - 1) / samples_per_measure + 1;
      // The last page rounds a fractional shift up, which can reach one entry past the end.
      want = std::min(want, hits.size() - 1);
    }
    for (; hits_idx < want; ++hits_idx)
      running_count += hits[hits_idx + 1];
    return sample_mask ? running_count * samples_per_measure : running_count;
  };

  // Rows sit on a fixed geometric grid of cache sizes, rounded to whole blocks: 1, then each
  // kGridRatio times the last, up to the end of the curve. Every dump of a connection uses the
  // same grid, so subtracting one dump's rows from the next gives an exact per-interval curve.
  // The last size is written too, if the grid skips it.
  size_t value = 0;
  size_t last_printed = 0;
  for (double g = 1;; g *= kGridRatio) {
    const size_t size = (size_t)std::llround(g);
    if (size <= last_printed)
      continue;
    if (size >= succ_size)
      break;
    value = succ_at(size);
    os << size << "," << value << std::endl;
    last_printed = size;
  }
  if (succ_size - 1 > last_printed)
    os << succ_size - 1 << "," << succ_at(succ_size - 1) << std::endl;
}

CacheSim::SuccessVector BoundedIAF::get_success_function() {
  // Ensure all requests processed
  flush();

  // TODO: parallel prefix sum for integrating

  //TODO: This will have to be ported over to IaF...

  CacheSim::SuccessVector success_func;
  if (sample_mask > 0) {
    // We operate on a downsampled vector. Renaming
    const SuccessVector &downsampled_success = chunk_input.output.hits_vector;

    // We start with 0 hits on a cache of size 0
    // And num_duplicates hits on a cache of size 1
    size_t running_count = num_duplicates;
    
    // rename with units. A power of 2
    size_t samples_per_measure = sample_mask + 1;
    
    // Allocate space for the extrapolation (0 cache, plus samples_per_measure spots for all others
    // But we still want a fixed-size output :)
    if (downsampled_success.size())
      success_func = SuccessVector(1+ ((downsampled_success.size()-1) * samples_per_measure));
    else
      success_func = SuccessVector(1);

    // Our downsampled cache of size 1 represents all caches size [1, samples_per_measure)

    // integrate to convert to success function
    for (req_count_t i = 1; i < downsampled_success.size(); i++) {
      // Pretend all samples_per_measure caches have the same value!
      running_count += downsampled_success[i];

      // The size 0 cache doesn't spread out to more slots, but we must account for it
      size_t start_pos = 1 + (i-1) * samples_per_measure;
      size_t end_pos = std::min(size_t(1 + i * samples_per_measure), success_func.size());
      
      // Spread the data
      for (size_t j = start_pos; j < end_pos; j++) {
        success_func[j] = running_count * samples_per_measure; 
      }
    }
  } else {
    // start with num_duplicates to count those
    size_t running_count = num_duplicates;
    // integrate to convert to success function
    success_func = SuccessVector(chunk_input.output.hits_vector.size());
    for (size_t i = 1; i < chunk_input.output.hits_vector.size(); i++) {
      running_count += chunk_input.output.hits_vector[i];
      success_func[i] = running_count;
    }
  }

  //for (auto& success : success_func)
  //  success /= running_count;
  // std::cout << max_recorded_chunk_size << std::endl;
  return success_func;
}


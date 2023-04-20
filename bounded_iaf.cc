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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "increment_and_freeze.h"

void BoundedIAF::memory_access(req_count_t addr) {
  ++access_number;
  chunk_input.requests.push_back({addr, (req_count_t) chunk_input.requests.size() + 1});

  if (chunk_input.requests.size() >= get_u()) {
    // std::cout << "requests chunk array:" << std::endl;
    // for (auto req : requests) {
    //   std::cout << req.first << "," << req.second << std::endl;
    // }

    process_requests();
  }
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

  iaf_alg.process_chunk(chunk_input);

  // update maximum memory usage
  if (iaf_alg.get_memory_usage() > memory_usage)
    memory_usage = iaf_alg.get_memory_usage();
  
  // auto depth_time =  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
  // std::cout << "GET DEPTH TIME: " << depth_time << std::endl;

  ChunkOutput& result = chunk_input.output;
  // print_result(result);

  result.hits_vector.resize(1 + std::min(result.living_requests.size(), max_living_req));

  // Resize the living requests if necessary to fit within max_living_req
  if (result.living_requests.size() > max_living_req) {
    auto it = result.living_requests.begin();
    size_t size = result.living_requests.size();
    result.living_requests.erase(it, it + (size - max_living_req));
  }

  // Fix the index of the living requests so they count up from 1
  size_t num_living = 0;
  for (auto &living_req : result.living_requests)
    living_req.access_number = ++num_living;

  chunk_input.requests.clear();
  // std::cout << "Size of hits vector = " << result.hits_vector.size() << std::endl;
  // std::cout << "Number of living requests = " << living.size() << std::endl;
  // std::cout << "First index of distance histogram = " << chunk_input.output.hits_vector[1] << std::endl;

  // prepare for next iteration
  update_u(chunk_input.output.living_requests.size());
  chunk_input.requests.reserve(get_u());
  chunk_input.requests.insert(chunk_input.requests.end(), result.living_requests.begin(), result.living_requests.end());
  STOPTIME(proc_req);
}

CacheSim::SuccessVector BoundedIAF::get_success_function() {
  // Ensure all requests processed
  if (chunk_input.requests.size() - chunk_input.output.living_requests.size() > 0) {
    // std::cout << "Processing chunk of size " << chunk_input.requests.size() << " before get_success_function()." << std::endl;
    process_requests();
  }

  // TODO: parallel prefix sum for integrating
  size_t running_count = 0;
  CacheSim::SuccessVector success_func(chunk_input.output.hits_vector.size());
  for (size_t i = 1; i < chunk_input.output.hits_vector.size(); i++) {
    running_count += chunk_input.output.hits_vector[i];
    success_func[i] = running_count;
  }

  //for (auto& success : success_func)
  //  success /= running_count;
  // std::cout << max_recorded_chunk_size << std::endl;
  return success_func;
}


#include "iak_wrapper.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "increment_and_freeze.h"
#include "params.h"

void IAKWrapper::memory_access(uint64_t addr) {
  chunk_input.chunk_requests.push_back({addr, chunk_input.chunk_requests.size() + 1});

  if (chunk_input.chunk_requests.size() >= get_u()) {
    // std::cout << "requests chunk array:" << std::endl;
    // for (auto req : requests) {
    //   std::cout << req.first << "," << req.second << std::endl;
    // }

    process_requests();
  }
}

void print_result(::IAKOutput result) {
  std::cout << "living requests" << std::endl;
  for (auto living: result.living_requests)
    std::cout << living.first << "," << living.second << " ";
  std::cout << std::endl;

  std::cout << "depth vector: " << result.depth_vector.size() << std::endl;
  for (size_t i = 0; i < result.depth_vector.size() - 1; i++)
    std::cout << result.depth_vector[i+1] << " ";
  std::cout << std::endl;
}

void IAKWrapper::process_requests() {
  // update recorded chunk size
  if (chunk_input.chunk_requests.size() > max_recorded_chunk_size)
    max_recorded_chunk_size = chunk_input.chunk_requests.size();

  // std::cout << std::endl;
  // std::cout << "Processing chunk" << std::endl;
  // std::cout << "Living requests " << chunk_input.output.living_requests.size() << std::endl;
  // std::cout << "New requests " << chunk_input.chunk_requests.size() << std::endl;
  // std::cout << "CHUNK:" << std::endl;
  // for (auto item : chunk_input.chunk_requests)
  //   std::cout << item.first << "," << item.second << std::endl;
  
  size_t prev_living_size = chunk_input.output.living_requests.size();

  // auto start = std::chrono::high_resolution_clock::now();
  iak_alg.get_depth_vector(chunk_input);
  // auto depth_time =  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();

  // std::cout << "GET DEPTH TIME: " << depth_time << std::endl;

  IAKOutput& result = chunk_input.output;
  // print_result(result);

  distance_histogram.resize(1 + std::min(result.living_requests.size(), max_living_req));

  // update depth_vector based upon living requests input to current chunk
  for (size_t i = 1; i < prev_living_size; i++) {
    result.depth_vector[i] += prev_living_size - i;
  }
  
  // Do not add living requests to distance histogram
  // 2-pointer walk through the depth vector and living requests
  // add to distance_histogram if access number not in living requests
  size_t living_idx = 0;
  for (size_t i = 1; i < result.depth_vector.size(); i++) {
    size_t depth = result.depth_vector[i]; // stack depth of request i
    assert(depth / max_u_mult < distance_histogram.size());

    // If we still have living requests to process and the current request is not a living request
    if (living_idx >= result.living_requests.size() || i != result.living_requests[living_idx].second) {
      if (depth < distance_histogram.size()) // don't update with depth out of bounds
        distance_histogram[depth]++;
    }
    // If current is a living request, increment living request pointer
    else if (living_idx < result.living_requests.size())
      ++living_idx;
  }

  // Resize the living requests if necessary to fit within max_living_req
  if (result.living_requests.size() > max_living_req) {
    auto it = result.living_requests.begin();
    size_t size = result.living_requests.size();
    result.living_requests.erase(it, it + (size - max_living_req));
  }

  // Fix the index of the living requests so they count up from 1
  size_t num_living = 0;
  for (auto &living_req : result.living_requests)
    living_req.second = ++num_living;

  chunk_input.chunk_requests.clear();
  // std::cout << "Size of depth vector = " << result.depth_vector.size() << std::endl;
  // std::cout << "Number of living requests = " << living.size() << std::endl;
  // std::cout << "First index of distance histogram = " << distance_histogram[1] << std::endl;

  // prepare for next iteration
  update_u(chunk_input.output.living_requests.size());
  chunk_input.chunk_requests.reserve(get_u());
  chunk_input.chunk_requests.insert(chunk_input.chunk_requests.end(), result.living_requests.begin(), result.living_requests.end());
}

std::vector<size_t> IAKWrapper::get_success_function() {
  // Ensure all requests processed
  if (chunk_input.chunk_requests.size() - chunk_input.output.living_requests.size() > 0) {
    // std::cout << "Processing chunk of size " << chunk_input.chunk_requests.size() << " before get_success_function()." << std::endl;
    process_requests();
    std::cout << "logu max memory usage: " << sizeof(::Op) * 2 * 2 * max_recorded_chunk_size * 1.0 / kGB << " GB" << std::endl;
  }

  // TODO: parallel prefix sum for integrating
  uint64_t running_count = 0;
  std::vector<size_t> success_func(distance_histogram.size());
  for (uint64_t i = 1; i < distance_histogram.size(); i++) {
    running_count += distance_histogram[i];
    success_func[i] = running_count;
  }

  //for (auto& success : success_func)
  //  success /= running_count;
  // std::cout << max_recorded_chunk_size << std::endl;
  return success_func;
}


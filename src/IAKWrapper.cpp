#include "IAKWrapper.h"

void IAKWrapper::memory_access(uint64_t addr) {
  requests.push_back({addr, requests.size() + living.size()});

  if (requests.size() >= get_u()) {
    
    process_requests();
  }
}

void IAKWrapper::process_requests() {
  MinInPlace::IAKOutput result = iak_alg.get_depth_vector(living, requests);

  distance_histogram.resize(result.living_requests.size()+result.depth_vector.size());
	for (size_t i = 0; i < result.living_requests.size(); i++)
	{
		result.depth_vector[i] += result.living_requests.size()-i -1;
	}
  for (auto depth : result.depth_vector)
  {
		if (depth >= distance_histogram.size())
			std::cout << "depth: " << depth << "/" << distance_histogram.size() << std::endl;
    assert(depth < distance_histogram.size());
    distance_histogram[depth]++;
  }

  living = result.living_requests;
  requests.clear();
}

std::vector<size_t> IAKWrapper::get_success_function() {
  // Ensure all requests processed
  if (requests.size() > 0) {
    process_requests();
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

  return success_func;
}


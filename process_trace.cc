#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <fstream>

#include "increment_and_freeze.h"

// For now this executable is just implemented using IAF
// As an example of processing a trace with one of the simulators
#ifdef ALL_METRICS
constexpr char ArgumentsString[] = "Arguments: succ_file, dist_file, trace, trace_format\n\
succ_file:    The file in which to place the success function and histogram.\n\
dist_file:    The file in which to place the distance vector.\n\
trace:        The file containing the request trace.\n\
trace_format: The format of the trace file. One of 'INT' (base 10 ints), 'HEX' (base 16 ints)";
int num_args = 4;
#else
constexpr char ArgumentsString[] = "Arguments: succ_file, trace, trace_format\n\
succ_file:    The file in which to place the success function.\n\
trace:        The file containing the request trace.\n\
trace_format: The format of the trace file. One of 'INT' (base 10 ints), 'HEX' (base 16 ints)";
int num_args = 3;
#endif  // ALL_METRICS

int main(int argc, char** argv) {
  if (argc != num_args + 1) {
    std::cerr << "ERROR: Incorrect number of arguments!" << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }

  // Parse output file(s)
  int arg = 1;
  std::ofstream succ_file(argv[arg++]);
  if (!succ_file) {
    std::cerr << "ERROR: Could not open output file: " << argv[1] << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }
#ifdef ALL_METRICS
  std::ofstream dist_file(argv[arg++]);
  if (!dist_file) {
    std::cerr << "ERROR: Could not open distance output file: " << argv[2] << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }
#endif  // ALL_METRICS

  // Parse trace file
  std::ifstream trace(argv[arg]);
  if (!trace) {
    std::cerr << "ERROR: Could not open input trace file: " << argv[arg] << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }
  arg++;

  // Parse trace format
  std::string format = argv[arg];
  int base = 10;
  if (format == "HEX") {
    base = 16;
  }
  else if (format != "INT") {
    std::cerr << "ERROR: Did not recognize string format: " << argv[arg] << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }
  arg++;

  // Create IAF object
  IncrementAndFreeze iaf;

  // Now actually process the trace file
  std::cout << "Reading trace file . . ." << std::endl;
  std::string line;
  while (getline(trace, line)) {
    // parse the string to an integer
    req_count_t id = std::stoull(line, nullptr, base);
    iaf.memory_access(id);
  }

  std::cout << "Computing success function . . ." << std::endl;
  CacheSim::SuccessVector succ = iaf.get_success_function();

  std::cout << "Dumping metrics to output file . . ." << std::endl;
#ifdef ALL_METRICS
  iaf.dump_all_metrics(succ_file, dist_file, succ);
#else
  iaf.dump_success_function(succ_file, succ);
#endif  // ALL_METRICS
}

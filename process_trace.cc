#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "increment_and_freeze.h"

#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

/*
--------------------------------------------------------------------
 Hash a vector of words and return a 32 bit value
--------------------------------------------------------------------
*/
uint32_t hashword(const uint32_t* k, /* the key, an array of uint32_t values */
                  size_t length,     /* the length of the key, in uint32_ts */
                  uint32_t initval)  /* the previous hash, or an arbitrary value */
{
  uint32_t a, b, c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + (((uint32_t)length) << 2) + initval;

  /*------------------------------------------------- handle most of the key */
  while (length > 3) {
    a += k[0];
    b += k[1];
    c += k[2];
    mix(a, b, c);
    length -= 3;
    k += 3;
  }

  /*------------------------------------------- handle the last 3 uint32_t's */
  switch (length) /* all the case statements fall through */
  {
    case 3:
      c += k[2];
    case 2:
      b += k[1];
    case 1:
      a += k[0];
      final(a, b, c);
    case 0: /* case 0: nothing left to add */
      break;
  }
  /*------------------------------------------------------ report the result */
  return c;
}

// For now this executable is just implemented using IAF
// As an example of processing a trace with one of the simulators
#ifdef ALL_METRICS
constexpr char ArgumentsString[] =
    "Arguments: succ_file, dist_file, trace, trace_format\n\
succ_file:    The file in which to place the success function and histogram.\n\
dist_file:    The file in which to place the distance vector.\n\
trace:        The file containing the request trace.\n\
trace_format: The format of the trace file. One of 'INT' (base 10 ints), 'HEX' (base 16 ints)\n\
sample_rate:  Sample 1 in sample_rate request ids.";
int num_args = 5;
#else
constexpr char ArgumentsString[] =
    "Arguments: succ_file, trace, trace_format\n\
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
  } else if (format != "INT") {
    std::cerr << "ERROR: Did not recognize string format: " << argv[arg] << std::endl;
    std::cerr << ArgumentsString << std::endl;
    exit(EXIT_FAILURE);
  }
  arg++;

#ifdef ALL_METRICS
  size_t sample_rate = std::stoull(argv[arg]);
#endif  // ALL_METRICS

  // Create IAF object
  IncrementAndFreeze iaf;

  // Now actually process the trace file
  std::cout << "Reading trace file . . ." << std::endl;
  std::string line;
  size_t request_index = 1;
  size_t sample_cutoff = (uint32_t)-1 / sample_rate;
  std::vector<size_t> sampled_requests;
  while (getline(trace, line)) {
    // parse the string to an integer
    req_count_t id = std::stoull(line, nullptr, base);

    // 1. Hash the id to hash value to determine if it should be sampled
    uint64_t hash_input = id;
    size_t hash = hashword((uint32_t*)(&hash_input), 2, 0xBEEFFEED);
    if (hash <= sample_cutoff) {
      sampled_requests.push_back(request_index);
      iaf.memory_access(id);
    }
    ++request_index;
  }

  std::cout << "Computing success function . . ." << std::endl;
  CacheSim::SuccessVector succ = iaf.get_success_function();

  std::cout << "Dumping metrics to output files . . ." << std::endl;
#ifdef ALL_METRICS
  iaf.dump_all_metrics(succ_file, dist_file, succ, sampled_requests, sample_rate);
#else
  iaf.dump_success_function(succ_file, succ);
#endif  // ALL_METRICS

  std::cout << "Memory usage = " << iaf.get_memory_usage() << "MiB" << std::endl;
}

#pragma once

#include "CacheSim.h"

#include <cassert>

enum ipOpType {Null, Subrange, Prefix, Postfix, Kill};

// A single operation, such as increment or kill
class ipOp {
 private:
	//TODO: Compress this down
  ipOpType type = Null;     // Do we increment or kill?
  uint64_t start;  // subrange start index 
  uint64_t end;    // subrange end index
	uint64_t target; // kill target
		
  uint64_t inc_amnt;      // subrange Increment amount
  uint64_t full_amnt = 0;      // fullrange Increment amount

 public:
  // create an increment
  ipOp(uint64_t start, uint64_t end)
      : type(Subrange), start(start), end(end), inc_amnt(1){};

  // create a kill
  ipOp(uint64_t kill) : type(Kill), target(kill){};
  
  // Uninitialized. Used to parallelize making a vector of this without push_back
  ipOp() : type(Null){};

  // create a new Op by projecting another one
  ipOp(const ipOp& oth_op, uint64_t proj_start, uint64_t proj_end);

  // This enforces correct merging of operators
	// Can only be done on equal subrange
  ipOp& operator+=(ipOp& oth) {
    assert(oth.type == Subrange);
    assert(type == Subrange);
    assert(start == oth.start);
    assert(end == oth.end);

    inc_amnt += oth.inc_amnt;
    full_amnt += oth.full_amnt;
    return *this;
  }
  
  void add_full(ipOp& oth) {
    full_amnt += oth.full_amnt;
  }
  
  bool isNull()
  {
    return (get_type() == Null && get_full_amnt() == 0);
  }

	// Used to determine how many spots in a projection this needs
	size_t score(size_t pstart, size_t pend)
	{
		switch(type)
		{
			case Null:
				return 0;
			case Subrange:
				// Our score is the number of endpoints within range
				// 2 endpoints within range keeps it a subrange, which can split
				// into two-- pre and post
				// If the subrange exactly ends on one or more endpoints, it becomes
				// a prefix, postfix, or passive increment and uses less space.	
				return (pstart < start && start < pstart ? 1 : 0) + 
				(pstart < end && end < pstart ? 1 : 0);
			case Prefix:
				return end >= pstart ? 1 : 0;
			case Postfix:
				return start <= pend ? 1 : 0;
			case Kill:
				return (target >= pstart && target <= pend) ? 1 : 0;
			default: assert(false); return 0;
		}
	}

  // is this operation passive in the projection defined by
  // proj_start and proj_end
  bool is_passive(uint64_t proj_start, uint64_t proj_end) {
    return type == Subrange && start <= proj_start && end >= proj_end;
  }
  
  
  ipOpType get_type() {return type;}
  uint64_t get_inc_amnt()  {return inc_amnt;}
  uint64_t get_full_amnt()  {return full_amnt;}
};

// A sequence of operators defined by a projection
class ProjSequence {
public:
  std::vector<ipOp>::iterator op_seq;
  std::vector<ipOp>::iterator scratch;
  // Pointers to valid end position in the vector
	size_t len;
  // The numerical range this sequence projects
  uint64_t start;
  uint64_t end;

  // Initialize an empty projection with bounds
  ProjSequence(uint64_t start, uint64_t end) : start(start), end(end) {};

  void partition(ProjSequence& left, ProjSequence& right)
	{
    assert(left.start <= left.end);
    assert(left.end+1 == right.start);
    assert(right.start <= right.end);

    size_t pos = 0;
    size_t left_bound = 0;
    size_t right_bound = 0;
    size_t ladd;
    size_t radd;

    // We have to do first left than right. Otherwise we can't know the splitting point in memory
    // (Without 3+ passes)
		for (size_t i = 0; i < len; i++)
		{
      ipOp& op = op_seq[i];
			ladd = op.score(left.start, left.end);
			if (ladd >= 1)
			{
				pos = project_op(op, left.start, left.end, pos);
			}
      left_bound += ladd;
    }

    // Now we 'clean up' any additional waste in scratch
    for (size_t i = pos; i < left_bound; i++)
    {
      scratch[i] = ipOp();
    }

    pos = left_bound;
		for (size_t i = 0; i < len; i++)
		{
      ipOp& op = op_seq[i];
			radd = op.score(right.start, right.end);
			if (radd >= 1)
			{	
				pos = project_op(op, right.start, right.end, pos);
			}
      right_bound += radd;
		}	

    // Now we 'clean up' any additional waste in scratch
    for (size_t i = pos; i < right_bound; i++)
    {
      scratch[i] = ipOp();
    }

		// This fails if there isn't enough memory allocated
		// It either means we did something wrong, or our memory
		// bound is incorrect.
		assert(left_bound + right_bound <= len);

    // At this point, scratch contains the end results + no garbage.
    // op_seq contains old work/ garbage
    std::swap(op_seq, scratch);

    //Now op_seq and scratch are properly named. We assign them to left and right.
    left.op_seq = op_seq;
    left.scratch = scratch;
    left.len = left_bound;
    
    right.op_seq = op_seq + left_bound;
    left.scratch = scratch + left_bound;
    left.len = right_bound;
	}


  // We project and merge here. 
  size_t project_op(ipOp& new_op, size_t start, size_t end, size_t pos) {
    ipOp proj_op = ipOp(new_op, start, end);

    if (proj_op.isNull()) return pos;

    // We *may* have a full_increment, but are still Null
    // We should replace this anyway
    if (scratch[0].get_type() == Null)
    {
      proj_op.add_full(scratch[0]);
      scratch[pos] = std::move(proj_op);
      return pos+1;
    }


    ipOp& last_op = scratch[pos - 1];
    // If either is not passive, then we cannot merge. We must add it.
    if (!proj_op.is_passive(start, end) || !last_op.is_passive(start, end)) {
      scratch[pos] = std::move(proj_op);
      return pos+1;
    }
    // merge with the last op in sequence
    last_op += proj_op;
    return pos;
  }
};

// Implements the IncrementAndKillInPlace algorithm
class IncrementAndKillInPlace: public CacheSim {
  using tuple = std::pair<uint64_t, uint64_t>;
  private:
    // A vector of all requests
    std::vector<tuple> requests;

    /* A vector of tuples for previous and next
     * Previous defines the last instance of a page
     * next defines the next instance of a page
     */
    std::vector<tuple> prevnext;

    /* This converts the requests into the previous and next vectors
     * Requests is copied, not modified.
     * Precondition: requests must be properly populated.
     */
    void calculate_prevnext();

    /* Returns the distance vector calculated from prevnext.
     * Precondition: prevnext must be properly populated.
     */
    std::vector<uint64_t> get_distance_vector();
    // Shortcut to access prev in prevnext.
    uint64_t& prev(uint64_t i) {return prevnext[i].first;}
    // Shortcut to access next in prevnext.
    uint64_t& next(uint64_t i) {return prevnext[i].second;}
    /* Helper dunction to get_distance_vector.
     * Recursively (and in parallel) populates the distance vector if the 
     * projection is small enough, or calls itself with smaller projections otherwise.
     */
    void do_projections(std::vector<uint64_t>& distance_vector, ProjSequence seq);
  public:
    // Logs a memory access to simulate. The order this function is called in matters.
    void memory_access(uint64_t addr);
    /* Returns the success function.
     * Does *a lot* of work.
     * When calling print_success_function, the answer is re-computed.
     */
    std::vector<uint64_t> get_success_function();
    IncrementAndKillInPlace() = default;
    ~IncrementAndKillInPlace() = default;
};

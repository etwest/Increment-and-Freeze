#pragma once

#include "CacheSim.h"

#include <cassert>

enum OpType {Null, Subrange, Prefix, Postfix, Kill};

// A single operation, such as increment or kill
class Op {
 private:
	//TODO: Compress this down
  OpType type;     // Do we increment or kill?
  uint64_t start;  // subrange start index 
  uint64_t end;    // subrange end index
	uint64_t target; // kill target
		
  uint64_t inc_amnt;      // subrange Increment amount
  uint64_t full_amnt = 0;      // fullrange Increment amount

 public:
  // create an increment
  Op(uint64_t start, uint64_t end)
      : type(Subrange), start(start), end(end), inc_amnt(1){};

  // create a kill
  Op(uint64_t kill) : type(Kill), target(kill){};
  
  // Uninitialized. Used to parallelize making a vector of this without push_back
  Op() : type(Null){};

  // create a new Op by projecting another one
  Op(const Op& oth_op, uint64_t proj_start, uint64_t proj_end);

  // This enforces correct merging of operators
	// Can only be done on equal subrange
  Op& operator+=(Op& oth) {
    assert(oth.type == Subrange);
    assert(type == Subrange);
    assert(start == oth.start);
    assert(end == oth.end);

    r += oth.r;
    return *this;
  }

	// Used to determine how many spots in a projection this needs
	size_t score(size_t pstart, size_t pend)
	{
		switch(type):
		{
			case Null:
				return 0;
			case Subrange:
				// Our score is the number of endpoints within range
				// 2 endpoints within range keeps it a subrange, which can split
				// into two-- pre and post
				// If the subrange exactly ends on one or more endpoints, it becomes
				// a prefix, postfix, or passive increment and uses less space.	
				return (pstart < start && start < pstart ? 1 : 0) + ;
				(pstart < end && end < pstart ? 1 : 0);
			case Prefix:
				return end >= pstart ? 1 : 0;
			case Postfix:
				return start <= pend ? 1 : 0;
			case Kill:
				return target >= pstart && target <= pend ? 1 : 0;
			default: assert(false);
		}
	}

  // is this operation passive in the projection defined by
  // proj_start and proj_end
  bool is_passive(uint64_t proj_start, uint64_t proj_end) {
    return type == Subrange && start <= proj_start && end >= proj_end;
  }
  
  
  OpType get_type() {return type;}
  uint64_t get_inc_amnt()  {return inc_amnt;}
  uint64_t get_full_amnt()  {return full_amnt;}
};

// A sequence of operators defined by a projection
class ProjSequence {
public:
  std::vector<Op>::iterator op_seq;
	size_t len;
  uint64_t start;
  uint64_t end;

  // Initialize an empty projection with bounds
  ProjSequence(uint64_t start, uint64_t end) : start(start), end(end) {};

	partition(ProjSequence& left, ProjSequence& right)
	{
		// We need to calculate the max space usage of a block as well as 
		// whether or not we're going to overwrite something we need.
		
		size_t lscore,rscore = 0;

		size_t rightmost_left = 0;
		size_t leftmost_right = len-1;
	
		size_t radd, ladd;
		size_t pos = 0;
		for (Op& op : op_seq)
		{
			ladd = op.score(left.start, left.end);
			radd = op.score(right.start, right.end);
			if (radd >= 1 && pos < leftmost_right)
			{	
				leftmost_right = pos;
			}
			if (ladd >= 1)
			{
				rightmost_left = pos; 
			}
			lscore += ladd;
			rscore += radd;
			pos++;
		}	
		// This fails if there isn't enough memory allocated
		// It either means we did something wrong, or our memory
		// bound is incorrect.
		assert(len >= lscore + rscore);

		// At this point, we know how much space the left and right needs
		// As well as where the instructions for the left and right start and end.
		// Now, we have to copy over any instructions that split into both
		// without violating the order of instructions.

		// Check if we have a trivial case. Nothing goes to both, move only 
		if (leftmost_right > rightmost_left)
		{
			// Try splitting tight to the lscore
			if (rightmost_left <= lscore)
			{
				left.project(op_seq, lscore);
				right.project(op_seq+lscore, len-lscore);
			} //try splitting tight to the rightmost_left
			else if (len - rightmost_left >= rscore)
			{
				left.project(op_seq, rightmost_left);
				right.project(op_seq+rightmost_left, len-rightmost_left);
			} 
			else
			{
				//TODO: move things around
			}
		}
		else
		{

		}

		
			 
		
	}


  // We project and merge here. 
  void add_op(Op& new_op) {
    Op proj_op = Op(new_op, start, end);
    if (proj_op.get_type() == Null) return;
    if (op_seq.size() == 0) {
      op_seq.push_back(proj_op);
      return;
    }

    Op& last_op = op_seq[op_seq.size() - 1];
    // If either is not passive, then we cannot merge. We must add it.
    if (!proj_op.is_passive(start, end) || !last_op.is_passive(start, end)) {
      op_seq.push_back(proj_op);
      return;
    }
    // merge with the last op in sequence
    last_op += proj_op;
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

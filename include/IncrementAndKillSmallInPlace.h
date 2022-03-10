#pragma once

#include <iostream>
#include <cassert>

#include "CacheSim.h"

namespace SmallInPlace {

  enum OpType {Null, Prefix, Postfix};

  // A single operation, such as increment or kill
  class Op {
    private:
      //TODO: Compress this down
      OpType type = Null;     // Do we increment or kill?
      uint32_t target; // kill target
      static constexpr uint32_t inc_amnt = 1;      // subrange Increment amount 
      int32_t full_amnt = 0;      // fullrange Increment amount

    public:
      // create an Prefix
      Op(uint64_t target, int64_t full_amnt)
        : type(Prefix), target(target), full_amnt(full_amnt){};

      // create a Postfix
      Op(uint64_t target) : type(Postfix), target(target){};

      // Uninitialized. Used to parallelize making a vector of this without push_back
      Op() : type(Null){};

      // create a new Op by projecting another one
      Op(const Op& oth_op, uint64_t proj_start, uint64_t proj_end);

      // This enforces correct merging of operators
      // Can only be done on equal subrange
      /*Op& operator+=(Op& oth) {
        assert(oth.type == Subrange);
        assert(type == Subrange);
        assert(start == oth.start);
        assert(end == oth.end);

        inc_amnt += oth.inc_amnt;
        full_amnt += oth.full_amnt;
        return *this;
        }*/
      bool affects(size_t victum)
      {
        return (type == Prefix && victum <= target) || (type == Postfix && victum >= target);
      }

      friend std::ostream& operator<<(std::ostream& os, const Op& op)
      {
        os << op.type << " @ " << op.target << ". + " << op.full_amnt;
        return os;
      }

      void add_full(Op& oth) {
        full_amnt += oth.full_amnt;
      }

      bool isNull()
      {
        return (get_type() == Null && get_full_amnt() == 0);
      }
      /*
      // Used to determine how many spots in a projection this needs
      size_t score(size_t proj_start, size_t proj_end)
      {
      // projected start and end
      size_t pstart = 0;
      size_t pend = 0;
      switch(type)
      {
      case Subrange:
      case Prefix:
      case Postfix:
      pstart = (start < proj_start || type == Prefix) ? proj_start : start;
      pend = (end > proj_end || type == Postfix) ? proj_end : end;
      if (pend < pstart)
      {
      return 0;
      }
      else if (pstart == proj_start && pend == proj_end) //full inc case
      {
      return 0;
      }
      else if (pstart == proj_start) // prefix case
      {
      return 0;
      }
      else if (pend == proj_end) //postfix case
      {
      return 0;
      }
      else //subrange case
      {
      assert(type == Subrange);
      return 1;
      }
      case Null:
      return 0;
      case Kill:
      if (proj_start <= target && target <= proj_end)
      return 0;
      else
      return 0;
      default: assert(false); return 0;
      }
      }
      */
      // is this operation passive in the projection defined by
      // proj_start and proj_end
      bool is_passive(uint64_t proj_start, uint64_t proj_end) {
        return !affects(proj_start) && !affects(proj_end);
      }


      OpType get_type() {return type;}
      uint64_t get_inc_amnt()  {return inc_amnt;}
      uint64_t get_full_amnt()  {return full_amnt;}
  };

  // A sequence of operators defined by a projection
  class ProjSequence {
    public:
      std::vector<Op>::iterator op_seq;
      std::vector<Op>::iterator scratch;
      // Pointers to valid end position in the vector
      size_t len;
      // The numerical range this sequence projects
      uint64_t start;
      uint64_t end;

      // Initialize an empty projection with bounds
      ProjSequence(uint64_t start, uint64_t end) : start(start), end(end) {};

      void partition(ProjSequence& left, ProjSequence& right)
      {
        /*size_t total = 0;
        size_t target = 9;
				std::cout << start << ", " << end << std::endl;
        for (size_t i = 0; i < len; i++)
        {
          total += op_seq[i].score(start, end);
          if (op_seq[i].affects(target))
					{
            std::cout << op_seq[i] << std::endl;
					}
        }
        std::cout << total << std::endl;
        total += end-start+1;
        assert(total <= len);
				*/
        assert(left.start <= left.end);
        assert(left.end+1 == right.start);
        assert(right.start <= right.end);
        assert(start == left.start);
        assert(end == right.end);

        size_t pos = 0;
        size_t left_bound = 0;
        size_t right_bound = 0;
        size_t ladd;
        size_t radd;

				//There's two things to keep track of here:
				// The upper bound on needed memory (left_bound, right_bound)
				// and how much memory we *actually* need right now.
				// 


        // We have to do first left than right. Otherwise we can't know the splitting point in memory
        // (Without 3+ passes)
        for (size_t i = 0; i < len; i++)
        {
          Op& op = op_seq[i];
          pos = project_op(op, left.start, left.end, pos);
        }

				size_t minleft = left.end-left.start+1;
        left_bound = 2*minleft;
        assert(left_bound <= len);
        assert(pos <= left_bound);


        // Now we 'clean up' any additional waste in scratch
        for (size_t i = pos; i < left_bound; i++)
        {
          scratch[i] = Op();
        }

        // for the sake of simplicity
        scratch += left_bound;
        pos = 0;
        for (size_t i = 0; i < len; i++)
        {
          Op& op = op_seq[i];
          pos = project_op(op, right.start, right.end, pos);
        }	

				size_t minright = right.end-right.start+1;
        right_bound = 2*minright;
        assert(left_bound + right_bound <= len);
        assert(pos <= left_bound + right_bound);

        // Now we 'clean up' any additional waste in scratch
        for (size_t i = pos; i < right_bound; i++)
        {
          scratch[i] = Op();
        }
        scratch -=left_bound;

        // This fails if there isn't enough memory allocated
        // It either means we did something wrong, or our memory
        // bound is incorrect.

        // At this point, scratch contains the end results + no garbage.
        // op_seq contains old work/ garbage
        std::swap(op_seq, scratch);

        //Now op_seq and scratch are properly named. We assign them to left and right.
        left.op_seq = op_seq;
        left.scratch = scratch;
        left.len = left_bound;

        right.op_seq = op_seq + left_bound;
        right.scratch = scratch + left_bound;
        right.len = right_bound;
        std::cout /*<< total*/ << "(" << len << ") " << " -> " << left.len << ", " << right.len << std::endl;
      }


      // We project and merge here. 
      size_t project_op(Op& new_op, size_t start, size_t end, size_t pos) {
        Op proj_op = Op(new_op, start, end);

        if (proj_op.isNull()) return pos;
        assert(proj_op.get_type() == Null || new_op.get_type() == Null || new_op.get_full_amnt() + new_op.get_inc_amnt() == proj_op.get_full_amnt() + proj_op.get_inc_amnt());

        // The first element is always replaced
        if (pos == 0)
        {
          scratch[0] = std::move(proj_op);
          return 1;
        }

        // The previous element may be replacable if it is Null
        // Unless we are a kill/Suffix, then we have an explicit barrier here.
        if (pos > 0 && scratch[pos-1].get_type() == Null && proj_op.get_type() != Postfix)
        {
          proj_op.add_full(scratch[pos-1]);
          scratch[pos-1] = std::move(proj_op);
          return pos;
        }

        // If we are null (a full increment), we do not need to take up space
        if (pos > 0 && proj_op.get_type() == Null)
        {
          scratch[pos-1].add_full(proj_op);
          return pos;
        }

        /*Op& last_op = scratch[pos - 1];
        // If either is not passive, then we cannot merge. We must add it.
        if (!proj_op.is_passive(start, end) || !last_op.is_passive(start, end)) {
        scratch[pos] = std::move(proj_op);
        return pos+1;
        }*/

        // merge with the last op in sequence
        //scratch[pos-1] += proj_op;

        //Neither is mergable.
        scratch[pos] = std::move(proj_op);
        return pos+1;
      }
  };

  // Implements the IncrementAndKillInPlace algorithm
  class IncrementAndKill: public CacheSim {
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
    //std::vector<uint64_t> get_distance_vector();
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
    IncrementAndKill() = default;
    ~IncrementAndKill() = default;
    std::vector<uint64_t> get_distance_vector();
  };
}

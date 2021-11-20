#include <cassert>
#include <cstdint>
#include <cstddef>

class OSTree
{
	private:
		size_t weight;
		OSTree* left = nullptr;
		OSTree* right = nullptr;
		
		uint64_t ts;
		uint64_t value;
		
		void rebalance();
		void check_balance();
	public:
		void insert(uint64_t ts, uint64_t val);
		void remove(size_t rank);
		//uint64_t rank(size_t rank); //what element is rank X
		size_t find(uint64_t ts); //what rank is X at

		OSTree(uint64_t, uint64_t);
		size_t getWeight() { return weight; };
};

class OSTreeHead
{
	public:
		OSTree* head;
		void insert(uint64_t ts, uint64_t val)
		{
			if (head == nullptr)
				head = new OSTree(ts, val);
			else
				head->insert(ts, val);
		};
		void remove(size_t rank)
		{
			assert(head != nullptr);
			assert(rank < head->getWeight());
			if (head->getWeight()== 1)
				delete head;
			else
				head->remove(rank);
		};
		size_t find(uint64_t ts) //what rank is X at
		{
			assert(head != nullptr);
			head->find(ts);
		};
};

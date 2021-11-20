#include <cassert>
#include <cstdint>
#include <cstddef>
#include <utility>

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
		std::pair<size_t, uint64_t> find(uint64_t ts); //what rank is X at

		OSTree(uint64_t, uint64_t);
		size_t getWeight() { return weight; };
		OSTree* getRight() { return right; };
		uint64_t getVal() { return value; };
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
		std::pair<size_t, uint64_t> find(uint64_t ts) //what rank is X at
		{
			assert(head != nullptr);
			return head->find(ts);
		};

		//TODO: This can be done during remove instead
		uint64_t getLast()
		{
			assert(head != nullptr);
			OSTree* ptr = head;
			while(ptr->getRight() != nullptr)
				ptr = ptr->getRight();
			return ptr->getVal();
		}
		size_t getWeight()
		{
			return head == nullptr? 0 : head->getWeight();
		}
};

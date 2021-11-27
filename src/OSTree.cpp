#include <OSTree.h>

OSTree::OSTree(uint64_t ts, uint64_t value)
: ts(ts), value(value)
{
	weight = 1;
};

OSTree::~OSTree()
{
	if (left != nullptr)
		delete left;
	if (right != nullptr)
		delete right;
}

void OSTree::insert(uint64_t newts, uint64_t newval)
{
	assert(newts != ts);

	if (bad_balance())
		rebalance();

	if (newts > ts)
	{
		if (left == nullptr)
		{
			left = new OSTree(newts, newval);
		}
		else
		{
			left->insert(newts, newval);
		}
	}
	else
	{
		if (right == nullptr)
		{
			right = new OSTree(newts, newval);
		}
		else
		{
			right->insert(newts, newval);
		}

	}
	weight++;

	if (left == nullptr)
		assert(weight == 1 + right->weight);
	else if (right == nullptr)
		assert(weight == 1 + left->weight);
	else
		assert(weight == 1 + left->weight + right->weight);
}

void OSTree::remove(size_t rank)
{
	assert(rank < weight);	
	assert(weight > 1);
	if (left == nullptr)
		assert(weight == 1 + right->weight);
	else if (right == nullptr)
		assert(weight == 1 + left->weight);
	else
		assert(weight == 1 + left->weight + right->weight);

	if (bad_balance())
		rebalance();

	weight--;
	
	size_t lweight = left == nullptr? 0 : left->weight;
	size_t rweight = right == nullptr? 0 : right->weight;

	if (lweight == rank)
	{
		OSTree* deleteMe = lweight > rweight ? left->getRightmost(): right->getLeftmost();	
		// We can't delete outselves, but we can pretend the be the next element!
		ts = deleteMe->ts;
		value = deleteMe->value;
		if (lweight > rweight)
		{
			if (lweight == 1)
			{
				delete left;
				left = nullptr;
			}
			else
				left->remove(lweight-1);	
		}
		else
		{
			if (rweight == 1)
			{
				delete right;
				right = nullptr;
			}
			else
				right->remove(0);
		}
	}
	else if (rank < lweight)
	{
		if (lweight == 1)
		{
			delete left;
			left = nullptr;
		}
		else if (lweight != 0)
		{
			left->remove(rank);
		}
		else	
		{
			assert(false);
			weight++;
		}
	}	
	else
	{
		if (rweight == 1)
		{
			delete right;
			right = nullptr;
		}
		else if (rweight != 0)
		{
			right->remove(rank - left->weight - 1);
		}
		else	
		{
			assert(false);
			weight++;
		}
	}
}

//Assume: The timestamp we're getting the rank of is in the tree
std::pair<size_t, uint64_t> OSTree::find(uint64_t searchts)
{
	size_t lweight = left == nullptr? 0 : left->weight;

	if (searchts == ts)
	{
		return {lweight, value};
	}
	else if (searchts > ts)
	{
		assert(left != nullptr);
		return left->find(searchts);
	}	
	else
	{
		assert(right != nullptr);
		std::pair<size_t, uint64_t> answer = right->find(searchts);
		answer.first += 1 +lweight;
		return answer;
	}
}

std::vector<std::pair<uint64_t, uint64_t>> OSTree::to_array()
{
	std::vector<std::pair<uint64_t, uint64_t>> larr;
	std::vector<std::pair<uint64_t, uint64_t>> rarr;
	if (left != nullptr) {
		larr = left->to_array();
	}

	if (right != nullptr) {
		rarr = right->to_array();
	}
	larr.push_back({ts, value});
	// TODO: what happens if rarr is empty
	return larr.insert(larr.end(), rarr.begin(), rarr.end());
}
		
void OSTree::rebalance()
{
	std::vector<std::pair<uint64_t, uint64_t>> arr_rep = to_array();
	size_t mid = arr_rep.size() / 2;
	ts    = arr_rep[mid].first;
	value = arr_rep[mid].second;

	left  = rebalance_helper(left,  arr_rep, 0, mid - 1);
	right = rebalance_helper(right, mid + 1, arr_rep.size() - 1);

}

OSTree *OSTree::rebalance_helper(OSTree *child, std::vector<std::pair<uint64_t, uint64_t>> array, 
  size_t first, size_t last)
{
	size_t mid   = ((last - first) / 2) + first;
	child->ts    = arr_rep[mid].first;
	child->value = arr_rep[mid].second;

	if (first == last) // basecase
	{
		if (child->left != nullptr) 
		{
			delete child->left;
			child->left = nullptr;
		}
		if (child->right != nullptr)
		{
			delete child->right;
			child->right = nullptr;
		}
		return child;
	}

	if (child->left == nullptr)
		child->left = new OSTree();
	if (child->right == nullptr)
		child->right = new OSTree();
	child->left  = child->rebalance_helper(child->left, arr_rep, first, mid - 1);
	child->right = child->rebalance_helper(child->right, mid + 1, last);
	return child;
}

bool OSTree::bad_balance()
{
	size_t lweight = left == nullptr? 1 : left->weight + 1;
	size_t rweight = right == nullptr? 1 : right->weight + 1;

	return lweight > 2 * rweight || rweight > 2 * lweight;
}
#include <OSTree.h>

OSTree::OSTree(uint64_t ts, uint64_t value)
: ts(ts), value(value)
{
	weight = 1;
};

void OSTree::insert(uint64_t newts, uint64_t newval)
{
	assert(newts != ts);
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
	weight--;
	
	size_t lweight = left == nullptr? 0 : left->weight;
	size_t rweight = right == nullptr? 0 : right->weight;

	if (lweight == rank-1)
	{
		OSTree* heavy = lweight > rweight ? left: right;	
		//we are heavy now!
		ts = heavy->ts;
		value = heavy->value;

		if (heavy->left == nullptr)
		{
			right = heavy->right;
			delete heavy;
		}
		else
		{
			heavy->remove(heavy->left->weight+1);
		}
	}
	else if (left->weight < rank-1)
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

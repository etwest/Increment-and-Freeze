#include "OSTree.h"

// All new trees have weight 1, as there are no children
OSTree::OSTree(uint64_t ts, uint64_t value)
    : ts(ts), value(value) {
    weight = 1;
};

OSTree::~OSTree() = default;

void OSTree::insert(uint64_t newts, uint64_t newval) {
    assert(newts != ts);

    // Ensure the tree is balanced on the way down
    // The tree may get out of balance after inserting,
    // but only by 1 element
    if (bad_balance())
        rebalance();

    // larger keys are inserted to the left
    // the element with oldest/smallest timestamp is the rightmost
    if (newts > ts) {
        if (left == nullptr)
            left = std::make_unique<OSTree>(newts, newval);
        else
            left->insert(newts, newval);
    }
    else {
        if (right == nullptr)
            right = std::make_unique<OSTree>(newts, newval);
        else
            right->insert(newts, newval);
    }
    weight++;

    // Ensure that the weights are still maintained properly
    if (left == nullptr)
        assert(weight == 1 + right->weight);
    else if (right == nullptr)
        assert(weight == 1 + left->weight);
    else
        assert(weight == 1 + left->weight + right->weight);
}

// Removes a child with rank predecessors
void OSTree::remove(size_t rank) {
    //Ensure the element exists and there is a child to remove
    assert(rank < weight);
    assert(weight > 1);

    // Ensure that the weights are still maintained properly
    if (left == nullptr)
        assert(weight == 1 + right->weight);
    else if (right == nullptr)
        assert(weight == 1 + left->weight);
    else
        assert(weight == 1 + left->weight + right->weight);

    // Rebalance the tree on the way down.
    // This may cause us to rebalance more than checking after,
    // But does not violate the O(1) amortized cost
    if (bad_balance())
        rebalance();

    weight--;

    size_t lweight = left == nullptr? 0 : left->weight;
    size_t rweight = right == nullptr? 0 : right->weight;

    // Delete ourself
    // Instead of invoking the memory manager on ourselves, we impersonate
    // the next or previous element and delete that instead
    // Such element must exist, as weight > 1
    if (lweight == rank) {
        //For the sake of balance, delete from the heaviest side.
        OSTree* deleteMe = lweight > rweight ? left->get_rightmost(): right->get_leftmost();
        // We can't delete outselves, but we can pretend the be the next element!
        ts = deleteMe->ts;
        value = deleteMe->value;
        if (lweight > rweight) {
            // Recursive base case
            if (lweight == 1) {
                left = nullptr;
            }
            else
                left->remove(lweight-1);
        }
        else {
            //Recursive base case
            if (rweight == 1) {
                right = nullptr;
            }
            else
                right->remove(0);
        }
    }
    else if (rank < lweight) {
        //delete from the left subtree

        if (lweight == 1) {
            left = nullptr;
        }
        else if (lweight != 0)
            // We skip over 0 elements, so rank remains the same
            left->remove(rank);
        else
            assert(false);
    }
    else {
        // delete from the right subtree

        if (rweight == 1) {
            right = nullptr;
        }
        else if (rweight != 0)
            // We skip over leftweight elements and ourselves
            right->remove(rank - left->weight - 1);
        else
            assert(false);
    }
}

//Assume: The timestamp we're getting the rank of is in the tree
std::pair<size_t, uint64_t> OSTree::find(uint64_t searchts) {
    size_t lweight = left == nullptr? 0 : left->weight;

    if (searchts == ts) //return ourselves
        return {lweight, value};
    else if (searchts > ts) { //check left
        assert(left != nullptr);
        return left->find(searchts);
    }
    else { //check right
        assert(right != nullptr);
        std::pair<size_t, uint64_t> answer = right->find(searchts);
        // We skip over leftweight elements plus ourselves
        answer.first += 1 +lweight;
        return answer;
    }
}

std::vector<std::pair<uint64_t, uint64_t>> OSTree::to_array() {
    std::vector<std::pair<uint64_t, uint64_t>> larr;
    std::vector<std::pair<uint64_t, uint64_t>> rarr;
    //build the left sorted array
    if (left != nullptr) {
        larr = left->to_array();
    }

    //build the right sorted array
    if (right != nullptr) {
        rarr = right->to_array();
    }
    // add ourselves after left
    larr.push_back({ts, value});
    // add the right sorted array after ourselves
    larr.insert(larr.end(), rarr.begin(), rarr.end());
    return larr;
}

void OSTree::rebalance() {
    // Turn our subtree into a sorted array
    std::vector<std::pair<uint64_t, uint64_t>> arr_rep = to_array();
    // Impersonate the median element
    size_t mid = arr_rep.size() / 2;
    ts    = arr_rep[mid].first;
    value = arr_rep[mid].second;

    // rebuild the left and right subtrees
    left  = rebalance_helper(std::move(left),
                             arr_rep, 0, mid - 1);
    right = rebalance_helper(std::move(right),
                             arr_rep, mid + 1, arr_rep.size() - 1);
    // total weight shouldn't change
    // and the weight should still be consistent
    assert(weight == 1 + left->weight + right->weight);
}

std::unique_ptr<OSTree> OSTree::rebalance_helper(
    std::unique_ptr<OSTree> child,
    std::vector<std::pair<uint64_t, uint64_t>>& arr_rep,
    size_t first, size_t last) {
    // We should definitely at least exist
    if (child == nullptr)
        child = std::make_unique<OSTree>(0,0);

    //Find the middle element without integer overflow
    size_t mid   = ((last - first) / 2) + first;

    // impersonate the middle element
    child->ts    = arr_rep[mid].first;
    child->value = arr_rep[mid].second;
    child->weight = 1 + last-first;

    if (first == last) { // basecase
        // Just us-- delete the children
        child->left = nullptr;
        child->right = nullptr;
    }
    else if (first == last-1) { // second base case: two elements
        if (first == mid) { // Child to the right
            if (child->left != nullptr) {
                child->left = nullptr;
            }
            child->right = child->rebalance_helper(std::move(child->right), arr_rep, mid + 1, last);
        }
        else { // Child to the left
            if (child->right != nullptr) {
                child->right = nullptr;
            }
            child->left = child->rebalance_helper(std::move(child->left), arr_rep, first, mid - 1);
        }

    }
    else { //recursively rebuild both left and right
        child->left  = child->rebalance_helper(std::move(child->left), arr_rep, first, mid - 1);
        child->right = child->rebalance_helper(std::move(child->right), arr_rep, mid + 1, last);
    }
    //ensure weights were properly set during rebulding
    size_t lweight = child->left == nullptr? 0 : child->left->weight;
    size_t rweight = child->right == nullptr? 0 : child->right->weight;
    assert(child->weight == lweight + rweight + 1);
    return child;
}

// Returns true if either subtree + the parent has twice the weight of the other.
// hint: The parent must be included to prevent infinite rebalance in the 2 node case
bool OSTree::bad_balance() {
    size_t lweight = left == nullptr? 1 : left->weight + 1;
    size_t rweight = right == nullptr? 1 : right->weight + 1;

    return lweight > 2 * rweight || rweight > 2 * lweight;
}

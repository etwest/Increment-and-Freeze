#include "OSTree.h"

// All new trees have weight 1, as there are no children
OSTree::OSTree(uint64_t ts, uint64_t value)
    : ts(ts), value(value) {
    weight = 1;
};

OSTree::~OSTree() = default;

std::unique_ptr<OSTree> OSTree::insert(std::unique_ptr<OSTree> ost,
                                       uint64_t newts, uint64_t newval) {
    if (!ost) {
        return std::make_unique<OSTree>(newts, newval);
    }
    assert(newts != ost->ts);

    // Ensure the tree is balanced on the way down
    // The tree may get out of balance after inserting,
    // but only by 1 element
    if (ost->bad_balance())
        ost = rebalance(std::move(ost));

    // larger keys are inserted to the left
    // the element with oldest/smallest timestamp is the rightmost
    if (newts > ost->ts) {
        ost->left = insert(std::move(ost->left), newts, newval);
    } else {
        ost->right = insert(std::move(ost->right), newts, newval);
    }
    ost->weight++;

    ost->validate();

    return ost;
}

void OSTree::validate() const {
  size_t left_weight = left ? left->weight : 1;
  size_t right_weight = right ? right->weight : 1;
  assert(weight == 1 + left_weight + right_weight);
  if (left) {
    assert(left->ts < ts);
  }
  if (right) {
    assert(ts < right->ts);
  }
}

// Removes a child with rank predecessors
// TODO: Try passing the first arg as a reference and returning void.
std::unique_ptr<OSTree> OSTree::remove(std::unique_ptr<OSTree> ost, size_t rank,
                                       std::unique_ptr<OSTree> &removed_node) {
    assert(ost);
    //Ensure the element exists.
    assert(rank < ost->weight);

    ost->validate();

    // No need to rebalance the tree on remove, since remove never increases the
    // depth of the tree.

    size_t lweight = ost->left ? ost->left->weight : 0;
    //size_t rweight = ost->right ? ost->right->weight : 0;

    if (lweight == rank) {
      // Delete ourself.
      if (!ost->left) {
        // No left child, so the right child becomes the new root.
        removed_node = std::move(ost);
        return std::move(removed_node->right);
      } else if (!ost->right) {
        removed_node = std::move(ost);
        return std::move(removed_node->left);
      } else {
        // Remove the leftmost child of the right subtree and make it be the root.
        std::unique_ptr<OSTree> leftmost_of_rightmost;
        ost->right = remove(std::move(ost->right), 0, leftmost_of_rightmost);
        leftmost_of_rightmost->left = std::move(ost->left);
        leftmost_of_rightmost->right = std::move(ost->right);
        leftmost_of_rightmost->weight = ost->weight - 1;
        removed_node = std::move(ost);
        return leftmost_of_rightmost;
      }
    } else if (rank < lweight) {
      // Delete from the left subtree.
      ost->left = remove(std::move(ost->left), rank, removed_node);
      --ost->weight;
      return ost;
    } else {
      // Delete from the right subtree.
      ost->right = remove(std::move(ost->right), rank - lweight - 1, removed_node);
      --ost->weight;
      return ost;
    }
}

//Assume: The timestamp we're getting the rank of is in the tree
std::pair<size_t, uint64_t> OSTree::find(uint64_t searchts) const {
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

void OSTree::to_array(std::unique_ptr<OSTree> ost,
                      std::vector<std::unique_ptr<OSTree>> &array,
                      size_t &index) {
  if (ost == nullptr) {
    return;
  } else {
    //build the left sorted array
    to_array(std::move(ost->left), array, index);
    size_t here = index++;
    to_array(std::move(ost->right), array, index);
    array[here] = std::move(ost);
  }
}

std::unique_ptr<OSTree> OSTree::from_array(std::vector<std::unique_ptr<OSTree>> &array,
                                           size_t lo, size_t hi) {
  if (lo >= hi) return nullptr;
  size_t mid = lo + (hi - lo) / 2; // carefully calculated to avoid overflow.
  std::unique_ptr<OSTree> result = std::move(array[mid]);
  result->left = from_array(array, lo, mid);
  result->right = from_array(array, mid + 1, hi);
  result->weight = 1 + (result->left ? result->left->weight : 0) + (result->right ? result->right->weight : 0);
  return result;
}

std::unique_ptr<OSTree> OSTree::rebalance(std::unique_ptr<OSTree> ost) {
    assert(ost);
    std::vector<std::unique_ptr<OSTree>> array(ost->weight);
    // Turn our subtree into a sorted array
    size_t index = 0;
    to_array(std::move(ost), array, index);
    assert(index == array.size());
    return from_array(array, 0, array.size());
}

// Returns true if either subtree + the parent has twice the weight of the other.
// hint: The parent must be included to prevent infinite rebalance in the 2 node case
bool OSTree::bad_balance() const {
    size_t lweight = left == nullptr? 1 : left->weight + 1;
    size_t rweight = right == nullptr? 1 : right->weight + 1;

    return lweight > 2 * rweight || rweight > 2 * lweight;
}

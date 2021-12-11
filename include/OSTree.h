#include <cassert>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

// Order Statistic Tree
// Must be managed by OSTreeHead
class OSTree {
    private:
        // The count of nodes in the tree, including yourself
        // The weight of a nullptr is 0.
        size_t weight;
        std::unique_ptr<OSTree> left;
        std::unique_ptr<OSTree> right;

        // Timestamp
        uint64_t ts;
        // Value-- such as address
        uint64_t value;

        /* Returns a sorted array of <timestamp, value> pairs
         * Used to rebalance the tree
        */
        std::vector<std::pair<uint64_t, uint64_t>> to_array();

        /* Rebalances the tree, including rerooting
        * Hint: Should only be called when bad_balance() returns true
        *       To retain an amortized rebalance cost of O(1)
        */
        void rebalance();

        /* Returns the left (or right) child pointer passed in, or a new one if null.
         * child:   (possibly null) child pointer
         * array:   sorted array
         * first:   first element we must place in our subtree
         * last:    last element we must place in our subtree, inclusive
         * Recursively builds a balanced tree.
         * Hint:    If a node exists, we adjust its values to desired.
         *          If it does not exist, we create it.
         *          If it exists and should not, we delete its subtree
        */
        std::unique_ptr<OSTree> rebalance_helper(
                std::unique_ptr<OSTree> child,
                std::vector<std::pair<uint64_t, uint64_t>>& array,
                size_t first, size_t last);
        //Returns whether or not the subtree needs to be rebalanced
        //true ->   must be rebalanced
        //false -> should not be rebalanced
        bool bad_balance();
    public:
        /* Inserts a new entry into the tree with key/timestamp ts and value val
         * ts:  The key to sort on.
         * val: The value to store
         * May rebalance the tree
        */
        void insert(uint64_t ts, uint64_t val);
        /* Removes the element with rank predecessors
         * May rebalance the tree
        */
        void remove(size_t rank);

        //uint64_t rank(size_t rank); //what element is rank X

        /* Returns the rank and value of element with key ts
         * ts:      The key to search for
         * Note:    ts must exist in the tree
        */
        std::pair<size_t, uint64_t> find(uint64_t ts); //what rank is X at

        /* Constructs a new Order Statistic tree node
         * ts:  key
         * value: value
        */
        OSTree(uint64_t ts, uint64_t value);
        // Destructs a node and its subtree recursively
        ~OSTree();

        size_t get_weight() { return weight; };
        //OSTree* get_right() { return right; };
        //OSTree* get_left()  { return left; };
        uint64_t get_val()  { return value; };


        // Returns the element of rank 0
        OSTree* get_leftmost() {
            if (left == nullptr)
                return this;
            return left->get_leftmost();
        };
        // Returns the element of max rank
        OSTree* get_rightmost() {
            if (right == nullptr)
                return this;
            return right->get_rightmost();
        };
};

/* Manages an OSTree
 * Handles edge cases
*/
class OSTreeHead {
    public:
        //Pointer to the root of the managed OSTree
        std::unique_ptr<OSTree> head = nullptr;

        //Destructs the head and entire managed tree, recursively
        ~OSTreeHead() = default;


        /* Inserts a new node into the OSTree
         * ts: key to be inserted
         * val: value to be inserted
        */
        void insert(uint64_t ts, uint64_t val) {
            if (head == nullptr)
                head = std::make_unique<OSTree>(ts, val);
            else
                head->insert(ts, val);
        };

        /* removes a node from the OSTree
         * rank: rank of node to be removed
        */
        void remove(size_t rank) {
            assert(head != nullptr);
            assert(rank < head->get_weight());
            if (head->get_weight()== 1)
            {
                head = nullptr;
            }
            else
                head->remove(rank);
        };

        /* returns a <rank, value> pair given a key
         * ts:   key to search on
         * Note: ts must exist in the tree
        */
        std::pair<size_t, uint64_t> find(uint64_t ts) {
            assert(head != nullptr);
            return head->find(ts);
        };

        /* TODO: This can be done during remove instead
         * returns the value of the element of highest rank
         * Note: Tree must be contain at least one element
        */
        uint64_t get_last() {
            assert(head != nullptr);
            return head->get_rightmost()->get_val();
        }
        /* Returns the number of elements in the tree
         * The weight of a nullptr is 0
        */
        size_t get_weight() {
            return head == nullptr? 0 : head->get_weight();
        }
};

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
        using Uptr = std::unique_ptr<OSTree>;

        // The count of nodes in the tree, including yourself
        // The weight of a nullptr is 0.
        size_t weight;
        Uptr left;
        Uptr right;

        // Timestamp
        uint64_t ts;
        // Value-- such as address
        uint64_t value;

        /* Puts the nodes of the tree in, in order, into array starting at
         * index.  Updates index to point at the first empty slot.
         */
        static void to_array(Uptr ost, std::vector<Uptr> &array, size_t &index);
        // Convert the nodes in array, which are sorted, into balanced tree.
        // lo and hi are a half-open range. All ranges should be half open.
        static Uptr from_array(std::vector<Uptr> &array, size_t lo, size_t hi);

        /* Rebalances the tree, including rerooting
        * Hint: Should only be called when bad_balance() returns true
        *       To retain an amortized rebalance cost of O(1)
        */
        static void rebalance(Uptr &ost);

        //Returns whether or not the subtree needs to be rebalanced
        //true ->   must be rebalanced
        //false -> should not be rebalanced
        bool bad_balance() const;

        // Check the representation invariants: The weights are correct and it's sorted right.
        void validate() const;
    public:
        /* Inserts a new entry into the tree with key/timestamp ts and value val
         * ts:  The key to sort on.
         * val: The value to store
         * May rebalance the tree
        */
        static void insert(Uptr &ost, uint64_t ts,
                           uint64_t val);
        /* Removes the element with rank predecessors
         * May rebalance the tree
         * Returns the new root.  Stores the deleted node in removed_node.
        */
        static void remove(
            Uptr &ost, size_t rank,
            Uptr &removed_node);

        //uint64_t rank(size_t rank); //what element is rank X

        /* Returns the rank and value of element with key ts
         * ts:      The key to search for
         * Note:    ts must exist in the tree
        */
        std::pair<size_t, uint64_t> find(uint64_t ts) const; //what rank is X at

        /* Constructs a new Order Statistic tree node
         * ts:  key
         * value: value
        */
        OSTree(uint64_t ts, uint64_t value);
        // Destructs a node and its subtree recursively
        ~OSTree();

        size_t get_weight() { return weight; };
        static size_t get_weight(const Uptr &ost) { return ost ? ost->weight : 0; }

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
          OSTree::insert(head, ts, val);
        };

        /* removes a node from the OSTree
         * rank: rank of node to be removed
        */
        void remove(size_t rank) {
            std::unique_ptr<OSTree> deleted;
            OSTree::remove(head, rank, deleted);
            assert(deleted);
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

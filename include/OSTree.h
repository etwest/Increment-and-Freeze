#include <cassert>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

class OSTree {
    private:
        size_t weight;
        OSTree* left = nullptr;
        OSTree* right = nullptr;

        uint64_t ts;
        uint64_t value;

        std::vector<std::pair<uint64_t, uint64_t>> to_array();

        void rebalance();
        OSTree *rebalance_helper(OSTree *child, std::vector<std::pair<uint64_t, uint64_t>>& array, 
                size_t first, size_t last);
        bool bad_balance();
    public:
        void insert(uint64_t ts, uint64_t val);
        void remove(size_t rank);
        //uint64_t rank(size_t rank); //what element is rank X
        std::pair<size_t, uint64_t> find(uint64_t ts); //what rank is X at

        OSTree(uint64_t, uint64_t);
        ~OSTree();

        size_t get_weight() { return weight; };
        OSTree* get_right() { return right; };
        OSTree* get_left()  { return left; };
        uint64_t get_val()  { return value; };

        OSTree* get_leftmost() {
            if (left == nullptr)
                return this;
            return left->get_leftmost();
        };
        OSTree* get_rightmost() {
            if (right == nullptr)
                return this;
            return right->get_rightmost();
        };
};

class OSTreeHead {
    public:
        OSTree* head = nullptr;
        ~OSTreeHead() {if (head != nullptr) delete head;};
        void insert(uint64_t ts, uint64_t val) {
            if (head == nullptr)
                head = new OSTree(ts, val);
            else
                head->insert(ts, val);
        };
        void remove(size_t rank) {
            assert(head != nullptr);
            assert(rank < head->get_weight());
            if (head->get_weight()== 1)
                delete head;
            else
                head->remove(rank);
        };
        std::pair<size_t, uint64_t> find(uint64_t ts) {
            assert(head != nullptr);
            return head->find(ts);
        };

        //TODO: This can be done during remove instead
        uint64_t get_last() {
            assert(head != nullptr);
            return head->get_rightmost()->get_val();
        }
        size_t get_weight() {
            return head == nullptr? 0 : head->get_weight();
        }
};

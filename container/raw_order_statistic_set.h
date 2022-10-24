// Base class for the OrderStatisticSet-related classes.
//
// This implementation uses a weight-balanced tree: The left child's size and
// the right child's size are kept within a constant factor of each other.
//
// This implementation makes a tradeoff in favor of reducing memory size at the
// expense of making iterators more fragile.  Many modifications to the tree
// invalidate iterators.  On the other hand, unlike for std::set, it's a
// random-access iterator, so you can add a number to an iterator in log time.
//
// References remain stable unless you remove the referred element from the
// tree.

#ifndef NET_BANDAID_BDN_CACHELIB_RAW_ORDER_STATISTIC_SET_H_
#define NET_BANDAID_BDN_CACHELIB_RAW_ORDER_STATISTIC_SET_H_

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "glog/logging.h"
#include "absl/memory/memory.h"

namespace cachelib {

namespace cachelib_internal {

// Template magic to make heterogeneous lookup work.
template <class, class = void>
struct IsTransparent : std::false_type {};
template <class T>
struct IsTransparent<T, std::void_t<typename T::is_transparent>>
    : std::true_type {};

template <bool is_transparent>
struct KeyArg {
  // Transparent. Forward `K`.
  template <typename K, typename key_type>
  using type = K;
};

template <>
struct KeyArg<false> {
  // Not transparent. Always use `key_type`.
  template <typename K, typename key_type>
  using type = key_type;
};

// The base class itself.  The `Traits` parameter class with the following
// public member types:
//
//   `key_type`: essentially the same as std::set:key_type.
//
//   `value_type`: essentially the same as std::set::value_type.

//   `Compare`: essentially the same as std::set::key_compare.  We support
//   heterogeneous lookup if the `Compare` has a member type named
//   `is_transparent`.
//
//   `Node`: Much of the real work of the tree is encapsulated in `Node`.
//   See the documentation below for RawNode.  All the operations that RawNode
//   supports must be supported by NodeType.  RawNode is a CRTP-style base class
//   that gets extended for the publicly defined container (e.g., OstSetNode
//   extends RawNode and it needs almost no extensions, whereas PrefixSumNode
//   must define some additional operations to support the prefix summation).
//
// For the `map` types, the `value_type` is typically a pair, which is different
// from the `Key`.  Maps also include `mapped_type`.
//
// The RawOrderStatisticSet provides no `public` members.  Everything is
// `protected`.  In the final tree, we then might write something like
//
//   `public: using Base::insert;`
//
// which makes the `insert` method visible.  All the member types and methods
// are explicitly made public this way.  The end-user documentation for those
// methods appears, e.g., in the comments for `OrderStatisticSet`.
//
// The `Node` operations that modify the tree generally take a `Node` which is
// the root of the subtree, and return the new root as a `Node`.  Thus,
// `Node::Insert` takes the root of the tree (as a `unique_ptr`) and a new
// value; inserts the value into the tree; possibly creates new nodes or
// rebalances the tree; and returns the new root (as a `unique_ptr`).  At the
// top level the resulting code looks something like this:
//
//   `root_ = Node::Insert(std::move(root_), new_value);`
//
// The actual arguments and return values for `Node::Insert` are slightly more
// complicated (so that a single method can handle `insert` and
// `insert_or_assign`).
//
// In `RawOrderStatisticSet`, the method names follow the capitalization and
// style of `std::map`.  For example, there is member type `key_type` (not
// `KeyType`) and a member function `insert_or_assign` (not `InsertOrAssign`).
// The NodeType uses Google-style, on the other hand, so there is
// `RawNode::Insert` (not `RawNode::insert`).
//
// There's currently no allocator parameter for this container.

template <class Traits>
class RawOrderStatisticSet {
 protected:
  using Node = typename Traits::Node;
  using Compare = typename Traits::Compare;

 private:
  template <bool is_const_iterator>
  class Iterator;

  template <class K>
  using key_arg = typename KeyArg<IsTransparent<Compare>::value>::template type<
      K, typename Traits::key_type>;

 public:
  using key_type = typename Traits::key_type;
  using value_type = typename Node::value_type;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using key_compare = Compare;
  using value_compare = Compare;
  using reference = value_type &;
  using const_reference = const value_type &;
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using node_type = typename Node::node_type;

  //**************** Observers ****************

  // Returns the number of elements in the tree.
  size_t size() const { return Node::Size(root_); }

  // Returns true if the tree is empty.
  bool empty() const { return !root_; }

  // Returns an iterator that points at the pair for `k` (or else `end()`)
  //
  // Usage note: The iterator provides a rank method that tells you the index,
  // so you can do a `find(k).rank()` to find the rank of `k`.
  //
  // The hetergeneous lookup version finds the element with the key that
  // compares equivalent to `k`.  This overload participates in overload
  // resolution only if the qualified-id `Compare::is_transparent` is valid and
  // denotes a type.  It allows calling this function without constructing an
  // instance of `Key`.
  template <class K = typename Traits::key_type>
  iterator find(const key_arg<K> &key) {
    return FindInternal<false>(key);
  }
  template <class K = typename Traits::key_type>
  const_iterator find(const key_arg<K> &key) const {
    return FindInternal<true>(key);
  }

  // Returns true if the container contains an element equivalent to `k`.
  template <class K = typename Traits::key_type>
  bool contains(const key_arg<K> &k) const {
    return find(k) != end();
  }

 private:
  // Helper function for `find()`.
  //
  // These helper methods are templates parameterized by `is_const_iterator` to
  // indicate whether we need to return a `const_iterator` or an `iterator`.
  template <bool is_const_iterator, class K = typename Traits::key_type>
  Iterator<is_const_iterator> FindInternal(const key_arg<K> &k) const {
    auto [idx, n] = Node::Find(root_, k, lessthan_);
    if (n) {
      return Iterator<is_const_iterator>(this, idx, n);
    } else {
      // Cannot just call end() because it might be cend() depending on whether
      // ValueType is const.  There are two versions of the map's `find` method,
      // one that returns an `iterator` and one that returns a `const_iterator`,
      // and the `End` method here returns `iterator` or `const_iterator`
      // depending on that type.
      return End<is_const_iterator>();
    }
  }

  // Helper function for `lower_bound()`.
  template <bool is_const_iterator, class K = typename Traits::key_type>
  Iterator<is_const_iterator> LowerBoundInternal(const key_arg<K> &k) const {
    auto [idx, n] = Node::LowerBound(root_, k, lessthan_);
    if (n) {
      return Iterator<is_const_iterator>(this, idx, n);
    } else {
      // Cannot just call end() (see the discussion in `FindInternal()`).
      return End<is_const_iterator>();
    }
  }

 protected:
  // Returns an iterator pointing to the first element that is not less than
  // `k`.
  template <class K>
  const_iterator lower_bound(const K &k) const {
    return LowerBoundInternal<true>(k);
  }
  template <class K>
  iterator lower_bound(const K &k) {
    return LowerBoundInternal<false>(k);
  }

 private:
  // Helper function for `upper_bound()`.
  template <bool is_const_iterator, class K = typename Traits::key_type>
  Iterator<is_const_iterator> UpperBoundInternal(const key_arg<K> &k) const {
    auto [idx, n] = Node::UpperBound(root_, k, lessthan_);
    if (n) {
      return Iterator<is_const_iterator>(this, idx, n);
    } else {
      // Cannot just call end() (see the discussion in `FindInternal()`).
      return End<is_const_iterator>();
    }
  }

 protected:
  // Returns an iterator pointing to the first element that is greater than `k`.
  template <class K>
  const_iterator upper_bound(const K &k) const {
    return UpperBoundInternal<true>(k);
  }
  template <class K>
  iterator upper_bound(const K &k) {
    return UpperBoundInternal<false>(k);
  }

 private:
  // Helper function for `Select()`.
  template <bool is_const_iterator>
  Iterator<is_const_iterator> SelectInternal(size_t idx) const {
    Node *n = Node::Select(root_, idx);
    if (n) {
      return Iterator<is_const_iterator>(this, idx, n);
    } else {
      // Cannot just call end() (see the discussion in `FindInternal()`).
      return End<is_const_iterator>();
    }
  }

 protected:
  // Returns an iterator that refers to the ith smallest element in the tree, or
  // end() if no such pair exists.
  const_iterator select(size_t idx) const { return SelectInternal<true>(idx); }
  iterator select(size_t idx) { return SelectInternal<false>(idx); }

  // These iterators are random-access
  // iterator, except that most operations take log time instead of constant
  // time.

 private:
  // Helper function for `begin()`.
  template <bool is_const_iterator>
  Iterator<is_const_iterator> Begin() const {
    return SelectInternal<is_const_iterator>(0);
  }

 protected:
  // Return an iterator to begin getting all the values of the tree.  The
  // iterator iterates from least key to greatest key.
  const_iterator cbegin() const { return Begin<true>(); }
  iterator begin() { return Begin<false>(); }
  const_iterator begin() const { return cbegin(); }

 private:
  // Helper for `end()`.  Returns an `iterator` or a `const_iterator` depending
  // on whether `ValueType` is `value_type` or `const value_type`.
  template <bool is_const_iterator>
  Iterator<is_const_iterator> End() const {
    return Iterator<is_const_iterator>(this, size(), nullptr);
  }

 protected:
  // Return the "end" iterator for the tree.
  const_iterator cend() const { return End<true>(); }
  iterator end() { return End<false>(); }
  const_iterator end() const { return cend(); }

  // Reverse iterators.
  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(End<true>());
  }
  reverse_iterator rbegin() { return reverse_iterator(End<false>()); }
  const_reverse_iterator rbegin() const { return crbegin(); }

  const_reverse_iterator crend() const {
    return const_reverse_iterator(cbegin());
  }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return crend(); }

  //**************** Mutators ****************

  // Erases all elements from tree.
  void clear() { root_.reset(); }

  // Insert value into the tree, return a pair. The first element of the pair is
  // iterator that points at the value in the tree, and the second element is
  // true if the value was not present before.
  //
  // Note: We haven't implemented (but could) the rich set of insert operations
  // or emplace that std::map provides.  For now we keep it simple and take
  // ownership of `value`.
  std::pair<iterator, bool> insert(value_type value) {
    auto [new_root, inserted_at_idx, inserted_at_node, did_insert] =
        Node::Insert(std::move(root_), std::move(value), 0, /*assign*/ false,
                     lessthan_);
    root_ = std::move(new_root);
    return std::pair(iterator(this, inserted_at_idx, inserted_at_node),
                     did_insert);
  }

  // Erases the value referenced by pos, returning an iterator that points at
  // the successor.
  iterator erase(iterator pos);

  // Erases k, if it exists, from the tree.
  template <class K = typename Traits::key_type>
  size_t erase(const key_arg<K> &k) {
    auto [new_root, rank_of_erased, new_successor, n_erased, extracted] =
        Node::Erase(std::move(root_), k, 0, nullptr, lessthan_);
    root_ = std::move(new_root);
    return n_erased;
  }

  node_type extract(const_iterator pos) {
    auto [new_root, new_idx, successor, n_erased, extracted] =
        Node::Erase(std::move(root_), Node::key(*pos), 0, nullptr, lessthan_);
    root_ = std::move(new_root);
    extracted.set_rank(new_idx);
    return std::move(extracted);
  }

  // Extract doesn't do heterogeneous lookup.
  node_type extract(typename Traits::key_type k) {
    auto [new_root, rank_of_erased, new_successor, n_erased, extracted] =
        Node::Erase(std::move(root_), k, 0, nullptr, lessthan_);
    root_ = std::move(new_root);
    extracted.set_rank(rank_of_erased);
    return std::move(extracted);
  }

  // Implements std::swap(*this, other).
  void swap(RawOrderStatisticSet &other) { std::swap(root_, other.root_); }

  key_compare key_comp() const { return key_compare(); }
  value_compare value_comp() const { return value_compare(); }

 public:
  //**************** Debugging and test support ****************

  // Checks the tree.  Runs in time O(Size()).  Useful for testing.
  void Check() const {
    if (root_) root_->Check(true, lessthan_);
  }

  // Print the internal structure of the tree.  Useful for debugging.  `kp`
  // stringifies a key.  The implementation is in the tests.
  std::ostream &PrintStructureForTest(
      std::ostream &out,
      std::function<std::string(const value_type &)> kp) const;

 protected:
  std::unique_ptr<Node> root_;
  key_compare lessthan_;
};

// The iterator is represented by an index and a pointer to the node.  The nodes
// have no parent or next or previous pointers to save space.  This makes the
// iterators fragile.  Any operation that adds or removes a node from the tree
// will invalidate iterators (specifically the index could end up being wrong).
// Changes to the tree shape (e.g., rotations) don't invalidate iterators,
// however.
//
// Since the node has no parent or next or prev pointers, incrementing the
// iterator requires performing a Select operation.
//
// `end()` is represented by an iterator with `index == Size()` and a nullptr
// node pointer.
//
// The iterator has an additional method, `rank(key)`, which returns the rank of
// the referenced value.
template <class Traits>
template <bool is_const_iterator>
class RawOrderStatisticSet<Traits>::Iterator {
  friend Iterator<true>;

 public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type =
      typename std::conditional_t<is_const_iterator,
                                  const typename Traits::stored_value_type,
                                  typename Traits::stored_value_type>;
  using difference_type = ptrdiff_t;
  using pointer = value_type *;
  using reference = value_type &;
  Iterator(const RawOrderStatisticSet *tree, size_t idx, Node *node)
      : tree_(const_cast<RawOrderStatisticSet *>(tree)),
        idx_(idx),
        node_(node) {
    DCHECK(idx <= tree->size());
  }

  // Define an implicit converting constructor to turn an iterator into a
  // const_iterator.
  //
  // Explicit conversion waiver granted in cl/404047257
  template <bool was_const,
            typename = std::enable_if_t<is_const_iterator || !was_const>>
  Iterator(Iterator<was_const> it)  // NOLINT(google-explicit-constructor)
      : Iterator(it.tree_, it.idx_, it.node_) {}

  // Prefix operators
  Iterator &operator++() { return *this += 1; }
  Iterator &operator--() { return *this -= 1; }
  // Postfix operators
  Iterator operator++(int) {
    Iterator tmp = *this;
    ++*this;
    return tmp;
  }
  Iterator operator--(int) {
    Iterator tmp = *this;
    --*this;
    return tmp;
  }
  // Pointer-like operators
  const value_type *operator->() const { return &node_->value_; }
  value_type *operator->() { return &node_->value_; }
  value_type &operator*() const { return node_->value_; }
  const value_type &operator[](std::ptrdiff_t idx) const {
    return *(*this + idx);
  }
  // Rank
  size_t rank() const { return idx_; }

  // The iterator is random access.
  Iterator &operator+=(ptrdiff_t n) {
    // It's undefined behavior to go off the end, so we might as well check
    // fail.
    if (n < 0) {
      CHECK_LE(-n, static_cast<ptrdiff_t>(idx_));  // Crash OK
      idx_ -= static_cast<size_t>(-n);
    } else {
      CHECK_LE(idx_ + static_cast<size_t>(n), tree_->size());  // Crash OK
      idx_ += static_cast<size_t>(n);
    }
    if (idx_ < tree_->size()) {
      node_ = Node::Select(tree_->root_, idx_);
    } else {
      node_ = nullptr;
    }
    return *this;
  }
  Iterator &operator-=(ptrdiff_t n) {
    *this += -n;
    return *this;
  }

  friend Iterator operator+(Iterator lhs, ptrdiff_t rhs) {
    lhs += rhs;
    return lhs;
  }

  friend Iterator operator+(ptrdiff_t n, Iterator v) { return v + n; }

  friend ptrdiff_t operator-(Iterator lhs, Iterator rhs) {
    // In C++17, this is conversion implementation defined when the difference
    // is negative.  In the implementations we useit does the right thing.  In
    // c++20 it's specified to do the right thing, specifically if lhs = rhs +
    // 1, then the conversion will yield -1.
    return static_cast<ptrdiff_t>(lhs.idx_ - rhs.idx_);
  }

  friend Iterator operator-(Iterator lhs, ptrdiff_t rhs) {
    lhs -= rhs;
    return lhs;
  }

 private:
  friend bool operator==(const Iterator &a, const Iterator &b) {
    // C++ standard library says that that operator== on iterators from
    // different containers is undefined behavior.
    DCHECK(a.tree_ == b.tree_);
    // end is represented by any idx that's too big.
    size_t size = a.tree_->size();
    if (a.idx_ >= size && b.idx_ >= size) return true;
    return a.idx_ == b.idx_;
  }
  friend bool operator!=(const Iterator &a, const Iterator &b) {
    return !(a == b);
  }
  friend bool operator<(const Iterator &a, const Iterator &b) {
    if (a == b) {
      return false;
    } else {
      return a.idx_ < b.idx_;
    }
  }
  friend bool operator<=(const Iterator &a, const Iterator &b) {
    return a < b || a == b;
  }
  friend bool operator>(const Iterator &a, const Iterator &b) { return b < a; }
  friend bool operator>=(const Iterator &a, const Iterator &b) {
    return a > b || a == b;
  }

  RawOrderStatisticSet *tree_;
  size_t idx_;
  Node *node_;
};

template <class Traits>
typename RawOrderStatisticSet<Traits>::iterator
RawOrderStatisticSet<Traits>::erase(iterator pos) {
  auto [new_root, new_idx, successor, n_erased, extracted] =
      Node::Erase(std::move(root_), Node::key(*pos), 0, nullptr, lessthan_);
  root_ = std::move(new_root);
  if (successor == nullptr) {
    return end();
  } else {
    return iterator(this, new_idx, successor);
  }
}

// When extending RawNode for the actual node type used in a tree, the extended
// type must define
//
//   1. a constructor;
//
//   2. a member type `value_type` (e.g., for sets, `value_type` is `Key`,
//      whereas for maps `value_type` is `std::pair<const Key, T>`); and
//
//   3. a static function `static const Key &key(const value_type &value)`
//      (e.g., for sets the `key` function should just return its argument and
//      for maps the `key` function should return `value.first`.
//
// StoredValue is the type, as stored.  For example `const Key` for sets, or
// std::pair<const Key, T>` for maps.  It must be the case that a Value can be
// converted to a StoredValue.
template <class Key, class Value, class StoredValue, class Compare,
          class NodeType, class ExtractNode>
class RawNode {
 private:
  using Node = NodeType;
  using value_type = Value;

 protected:
  using key_compare = Compare;

 public:
  explicit RawNode(Value v) : value_(std::move(v)) {}

  using node_type = ExtractNode;

  void SetMappedValue([[maybe_unused]] const Value &k) {}

  // Returns the number of nodes in the subtree.
  static size_t Size(const std::unique_ptr<Node> &n) {
    return n ? n->subtree_size_ : 0;
  }

  // Inserts k into the tree if there is no key equal to k.  If the value is
  // already present and assign is true, then replace the current "value" with
  // k's value.
  //
  // Returns a tuple which can be parsed as
  //
  //  auto [new_root, inserted_at_idx, inserted_at_node, did_insert] =
  //        Insert(...);
  //
  // where
  //
  //  `new_root` is the new root of the subtree that was previously rooted at
  //  `n`,
  //
  //  `inserted_at_idx` is the rank of `k` after the insertion,
  //
  //  `inserted_at_node` is the node containing `k` after the insertion, and
  //
  //  `did_insert` is true if and only if `k` wasn't previously in the tree
  //  (that is, if the insertion actually occurred, as compared to a no-op for a
  //  set or an assign for insert_or_assign in a map).
  static std::tuple<std::unique_ptr<Node>, size_t, Node *, bool> Insert(
      std::unique_ptr<Node> n, value_type k, size_t rank_so_far, bool assign,
      const key_compare &lessthan) {
    bool did_insert;
    size_t new_rank;
    Node *new_node;
    if (!n) {
      // Using new to access non-public constructor that make_unique cannot
      // access.
      n = absl::WrapUnique(new Node(std::move(k)));
      did_insert = true;
      new_node = n.get();
      new_rank = rank_so_far;
    } else {
      if (lessthan(Node::key(n->value_), Node::key(k))) {
        size_t left_size = Size(n->left_);
        auto [new_root, inserted_idx, inserted_node, sub_did_insert] =
            Insert(std::move(n->right_), std::move(k),
                   rank_so_far + left_size + 1, assign, lessthan);
        n->UpdateRight(std::move(new_root));
        new_rank = inserted_idx;
        did_insert = sub_did_insert;
        new_node = inserted_node;
      } else if (lessthan(Node::key(k), Node::key(n->value_))) {
        auto [new_root, inserted_idx, inserted_node, sub_did_insert] = Insert(
            std::move(n->left_), std::move(k), rank_so_far, assign, lessthan);
        n->UpdateLeft(std::move(new_root));
        new_rank = inserted_idx;
        did_insert = sub_did_insert;
        new_node = inserted_node;
      } else {
        // Equal
        new_rank = rank_so_far;
        new_node = n.get();
        if (assign) {
          n->SetMappedValue(k);
        }
        did_insert = false;
      }
    }
    n = MaybeRebalance(std::move(n));
    return std::tuple<std::unique_ptr<Node>, size_t, Node *, bool>(
        std::move(n), new_rank, new_node, did_insert);
  }

  //
  // Note: In the following functions, we don't bother to qualify the Karg
  // parameter with is_transparent.  The check is done in the tree.

  // Erases `k` from from the subtree rooted at node.  Returns the new subtree
  // root.  If no element is a key equivalent to `k` then the tree is not
  // modified.
  //
  // The arguments are
  //
  //   `n` the root of the subtree,
  //
  //   `k` the key of the element being erased,
  //
  //   `rank_so_far` is the rank of the smallest element in the subtree,
  //
  //   `successor_node` is the upper bound to the largest element in the subtree
  //   (or nullptr if there is no successor), and
  //
  //   `lessthan` is the comparison functor.
  //
  // Returns a tuple which can be parsed as
  //
  //  auto [new_root, rank_of_k, new_successor, n_erased, extracted] =
  //  Erase(...);
  //
  // where
  //
  //   `new_root` is the new root of the subtree that was previously rooted at
  //   `n`,
  //
  ///  `rank_of_k` is the rank of the `k` (which, incidentally, doesn't change
  ///  due to erasing `k`),
  //
  //  `new_successor` is the node containing the upper bound to `k` in the tree
  //  (or nullptr if all the elements in the tree are not greater than `k`), and
  //
  //  `n_erased` is the number of elements erased (zero or one).
  //
  //  `extracted` is a node_type containing the erased item if one was erased,
  //  otherwise an empty node_type.
  //
  template <class Karg>
  static std::tuple<std::unique_ptr<Node>, size_t /*rank*/,
                    Node * /*new_successor*/, size_t /*n_erased*/, node_type>
  Erase(std::unique_ptr<Node> n, const Karg &k, size_t rank_so_far,
        Node *successor_node, key_compare &lessthan) {
    // Compare k, not Node::key(k) since k is already a Karg (for a map, it's
    // just the key part of the map, and for a set it's the whole thing.)
    if (n == nullptr) {
      return {nullptr, rank_so_far, nullptr, 0, node_type()};
    } else if (lessthan(Node::key(n->value_), k)) {
      size_t leftsize = Size(n->left_);
      auto [newroot, rank, successor, n_erased, extracted] =
          Erase(std::move(n->right_), k, rank_so_far + 1 + leftsize,
                successor_node, lessthan);
      n->UpdateRight(std::move(newroot));
      return {MaybeRebalance(std::move(n)), rank, successor, n_erased,
              std::move(extracted)};
    } else if (lessthan(k, Node::key(n->value_))) {
      auto [newroot, rank, successor, n_erased, extracted] =
          Erase(std::move(n->left_), k, rank_so_far, n.get(), lessthan);
      n->UpdateLeft(std::move(newroot));
      return {MaybeRebalance(std::move(n)), rank, successor, n_erased,
              std::move(extracted)};
    } else {
      // Equal
      size_t leftsize = Size(n->left_);
      Node *successor = n->right_ ? LeftMost(n->right_) : successor_node;
      auto [newroot, extracted] = DeleteNode(std::move(n));
      return {std::move(newroot), rank_so_far + leftsize, successor, 1,
              std::move(extracted)};
    }
  }

  // Returns the rank of k in the subtree.  The rank is the number of keys less
  // than k.  Also returns a Node* if the key is actually present, else nullptr.
  template <class Karg>
  static std::pair<size_t, Node *> Find(const std::unique_ptr<Node> &n,
                                        const Karg &k,
                                        const key_compare &lessthan) {
    // Compare k, not Node::key(k) since k is already a Karg (for a map, it's
    // just the key part of the map, and for a set it's the whole thing.)
    if (!n) return {0, nullptr};
    if (lessthan(k, Node::key(n->value_))) {
      return Find(n->left_, k, lessthan);
    } else if (lessthan(Node::key(n->value_), k)) {
      auto [rank, resultnode] = Find(n->right_, k, lessthan);
      return {Size(n->left_) + 1 + rank, resultnode};
    } else {
      // Equal
      return {Size(n->left_), n.get()};
    }
  }

  template <class Karg>
  static std::pair<size_t, Node *> LowerBound(const std::unique_ptr<Node> &n,
                                              const Karg &k,
                                              const key_compare &lessthan) {
    // Compare k, not Node::key(k) since k is already a Karg (for a map, it's
    // just the key part of the map, and for a set it's the whole thing.)
    if (!n) return {0, nullptr};
    if (lessthan(k, Node::key(n->value_))) {
      auto [rank, resultnode] = LowerBound(n->left_, k, lessthan);
      if (resultnode == nullptr) {
        return {Size(n->left_), n.get()};
      } else {
        return {rank, resultnode};
      }
    } else if (lessthan(Node::key(n->value_), k)) {
      auto [rank, resultnode] = LowerBound(n->right_, k, lessthan);
      return {Size(n->left_) + 1 + rank, resultnode};
    } else {
      return {Size(n->left_), n.get()};
    }
  }

  template <class Karg>
  static std::pair<size_t, Node *> UpperBound(const std::unique_ptr<Node> &n,
                                              const Karg &k,
                                              const key_compare &lessthan) {
    // Compare k, not Node::key(k) since k is already a Karg (for a map, it's
    // just the key part of the map, and for a set it's the whole thing.)
    if (!n) return {0, nullptr};
    if (lessthan(k, Node::key(n->value_))) {
      auto [rank, resultnode] = UpperBound(n->left_, k, lessthan);
      if (resultnode == nullptr) {
        return {Size(n->left_), n.get()};
      } else {
        return {rank, resultnode};
      }
    } else {
      auto [rank, resultnode] = UpperBound(n->right_, k, lessthan);
      return {Size(n->left_) + 1 + rank, resultnode};
    }
  }

  // Returns a Node for the key ranked idx, if there is one, else returns
  // nullptr.
  static Node *Select(const std::unique_ptr<Node> &n, size_t idx) {
    if (n == nullptr) return nullptr;
    if (Size(n) <= idx) return nullptr;
    if (Size(n->left_) == idx) {
      return n.get();
    } else if (Size(n->left_) > idx) {
      return Select(n->left_, idx);
    } else {
      return Select(n->right_, idx - Size(n->left_) - 1ul);
    }
  }

  // Checks the tree invariants: The sizes of the subtree add up and the tree is
  // in search-tree order.  Doesn't check for balance, since we don't manage to
  // keep the tree always balanced.  This should probably be called only in test
  // code.
  void Check(bool check_recursive, const key_compare &lessthan) {
    CHECK_EQ(subtree_size_,  // Crash OK
             1 + Size(left_) + Size(right_));
    CHECK(IsInBalance());  // Crash OK
    if (left_) {
      CHECK(lessthan(Node::key(left_->value_),  // Crash OK
                     Node::key(value_)));
      if (check_recursive) left_->Check(check_recursive, lessthan);
    }
    if (right_) {
      CHECK(
          lessthan(Node::key(value_), Node::key(right_->value_)));  // Crash OK
      if (check_recursive) right_->Check(check_recursive, lessthan);
    }
  }

  // Prints a node using kp to stringify keys.  Useful for debugging.  The
  // implementation is in the tests.
  static std::ostream &Print(std::ostream &out, const std::unique_ptr<Node> &n,
                             std::function<std::string(const Value &key)> kp);

 protected:
  // Recomputes the subtree_size_ (and also the mixin).  This is needed whenever
  // one of the children changes size, or anything in the subtree rooted at n
  // chanes value.
  void RecomputeSummary() { subtree_size_ = 1ul + Size(left_) + Size(right_); }

 private:
  // Changes the left and right child of n to be left and right, respectively.
  void UpdateLeftAndRight(std::unique_ptr<Node> left,
                          std::unique_ptr<Node> right) {
    left_ = std::move(left);
    right_ = std::move(right);
    static_cast<Node *>(this)->RecomputeSummary();
  }
  // Changes the left_ to be left and updates the sums.
  void UpdateLeft(std::unique_ptr<Node> left) {
    left_ = std::move(left);
    static_cast<Node *>(this)->RecomputeSummary();
  }
  // Changes the right child of n to be left.
  void UpdateRight(std::unique_ptr<Node> right) {
    right_ = std::move(right);
    static_cast<Node *>(this)->RecomputeSummary();
  }

  static Node *LeftMost(const std::unique_ptr<Node> &n) {
    if (n->left_) {
      return LeftMost(n->left_);
    } else {
      return n.get();
    }
  }

  // Removes the rightmost node from the subtree, storing it in removed_node.
  // Returns the root of the revised subtree.
  static std::unique_ptr<Node> UnlinkRightMost(
      std::unique_ptr<Node> n, std::unique_ptr<Node> *removed_node) {
    DCHECK(n);
    if (n->right_) {
      n->UpdateRight(UnlinkRightMost(std::move(n->right_), removed_node));
      return MaybeRebalance(std::move(n));
    } else {
      std::unique_ptr<Node> left = std::move(n->left_);
      *removed_node = std::move(n);
      return left;
    }
  }

  // Removes this from the tree, returning the new root and an extracted_node
  // containing the removed value.
  static std::pair<std::unique_ptr<Node>, node_type> DeleteNode(
      std::unique_ptr<Node> n) {
    DCHECK(n);
    if (n->left_) {
      std::unique_ptr<Node> new_n;
      std::unique_ptr<Node> new_left =
          UnlinkRightMost(std::move(n->left_), &new_n);
      new_n->UpdateLeftAndRight(std::move(new_left), std::move(n->right_));
      new_n = MaybeRebalance(std::move(new_n));
      return {std::move(new_n),
              node_type(std::move(const_cast<value_type &>(n->value_)))};
    } else {
      return {std::move(n->right_),
              node_type(std::move(const_cast<value_type &>(n->value_)))};
    }
  }

  // Swaps a := b,  b := c, c := a (where c gets the original a).
  template <class T>
  static void swap3left(T &a, T &b, T &c) {
    std::swap(a, b);
    std::swap(b, c);
  }

  // Rotates the tree left, returning the new root.
  static std::unique_ptr<Node> RotateLeft(std::unique_ptr<Node> n) {
    swap3left(n->right_, n->right_->left_, n);
    if (n->left_) n->left_->RecomputeSummary();
    n->RecomputeSummary();
    return n;
  }

  // Rotates the tree right, returning the new root.
  static std::unique_ptr<Node> RotateRight(std::unique_ptr<Node> n) {
    swap3left(n->left_, n->left_->right_, n);
    if (n->right_) n->right_->RecomputeSummary();
    n->RecomputeSummary();
    return n;
  }

  // Double rotations
  static std::unique_ptr<Node> RotateRightLeft(std::unique_ptr<Node> n) {
    n->right_ = RotateRight(std::move(n->right_));
    return RotateLeft(std::move(n));
  }

  static std::unique_ptr<Node> RotateLeftRight(std::unique_ptr<Node> n) {
    n->left_ = RotateLeft(std::move(n->left_));
    return RotateRight(std::move(n));
  }

  // 4 is a convenient rebalancing factor.  Smaller factors lead to more work to
  // maintain balance, whereas larger factors lead to deeper trees (and hence
  // more work during search.).  Traditional weight-balanced trees require that
  // the rebalance factor be between 3.414 and 5.5 in order to make the
  // rotations work right.  See, for example,
  // https://en.wikipedia.org/wiki/Weight-balanced_tree.
  static constexpr size_t kRebalanceFactor = 4;

  // Returns true if the tree is balanced at this node.
  bool IsInBalance() const {
    size_t left_weight = 1 + Size(left_);
    size_t right_weight = 1 + Size(right_);
    size_t total_weight = left_weight + right_weight;
    // Return true if 1/4 <= left_weight/total_weight <= 3/4
    // where "4" is kRebalanceFactor and "3" is kRebalanceFactor-1.
    size_t four = kRebalanceFactor;
    size_t three = four - 1;
    bool r = (total_weight <= four * left_weight) &&
             (four * left_weight <= three * total_weight);
    return r;
  }

  // If needed, rebalances the tree at n, returning the new root.
  static std::unique_ptr<Node> MaybeRebalance(std::unique_ptr<Node> n) {
    DCHECK(n);
    if (n->IsInBalance()) {
      n->RecomputeSummary();  // even if it's balanced the summary could be
                              // wrong after a modification.
      return n;
    }
    const std::unique_ptr<Node> &left = n->left_;
    const std::unique_ptr<Node> &right = n->right_;
    if (Size(left) < Size(right)) {
      size_t rl_size = 1 + Size(right->left_);
      size_t rr_size = 1 + Size(right->right_);
      size_t sum_size = rl_size + rr_size;
      // Check to see if rl_size/sum_size < 2/3
      // where 2/3 is really (kRebalanceFactor-2)/(kRebalanceFactor-1).
      if (rl_size * (kRebalanceFactor - 1) <
          sum_size * (kRebalanceFactor - 2)) {
        return RotateLeft(std::move(n));
      } else {
        return RotateRightLeft(std::move(n));
      }
    } else {
      size_t ll_size = 1 + Size(left->left_);
      size_t lr_size = 1 + Size(left->right_);
      size_t sum_size = ll_size + lr_size;
      if (lr_size * (kRebalanceFactor - 1) <
          sum_size * (kRebalanceFactor - 2)) {
        return RotateRight(std::move(n));
      } else {
        return RotateLeftRight(std::move(n));
      }
    }
  }

 protected:
  std::unique_ptr<Node> right_;
  std::unique_ptr<Node> left_;
  size_t subtree_size_ = 1;

 public:  // Needs to be public so the iterator can get the value.
  StoredValue value_;
};

// SetExtractedNode is a move-only type that can own a value_type from a set.
// `extract()` returns a `node_type` which inherits from ExtractedNode.  In the
// standard library, extract operates without copying or moving the
// `value_type`.  In this library, may want to use a B-tree, and so the
// extraction may require a move.  The difference between SetExtractedNode
// (defined here) and MapExtractedNode is the accessors: MapExtractedNode
// provides `key()` and `mapped()` instead of `value()`.
template <class ValueType, class NodeType>
class SetExtractedNode {
 public:
  using value_type = ValueType;
  constexpr SetExtractedNode() = default;
  ~SetExtractedNode() = default;
  // moveable
  SetExtractedNode(SetExtractedNode &&nt)
      : value_(std::move(nt.value_)), rank_(nt.rank_) {}
  SetExtractedNode &operator=(SetExtractedNode &&nt) {
    value_ = std::move(nt.value_);
    rank_ = nt.rank_;
  }
  // not copyable
  SetExtractedNode(const SetExtractedNode &) = delete;
  SetExtractedNode &operator=(const SetExtractedNode &) = delete;
  [[nodiscard]] bool empty() const { return !value_.has_value(); }
  // Implicit conversion needed to comform to std::set::node_type.
  explicit operator bool() const { return value_.hash_value(); }
  value_type &value() { return *value_; }
  // This constructor is part of the standard library API, but it doesn't do
  // much harm to expose it.
  explicit SetExtractedNode(value_type &&value) : value_(std::move(value)) {}
  // Return the rank (at the time of the extraction) of the value.
  size_t rank() const { return rank_; }

  void set_rank(size_t rank) { rank_ = rank; }

 private:
  std::optional<value_type> value_ = std::nullopt;
  size_t rank_ = 0;
};

}  // namespace cachelib_internal
}  // namespace cachelib

#endif  // NET_BANDAID_BDN_CACHELIB_RAW_ORDER_STATISTIC_SET_H_

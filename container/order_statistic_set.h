// An OrderStatisticSet is like a std::set that allows you to determine the rank
// of a given element (i.e., how many elements less than the given elemtn exist
// in the tree) or to select an element by rank (e.g., to ask for the 143rd item
// in the tree).  It works by maintaining a count of how big each subtree is.
//
// The method names are lower-case, rather than using Google's CamelCase naming
// convention, in order to be more like std::map.
//
// There's currently no allocator parameter for this container.
//
// See the raw_order_statistics_set.h for a implementation information.

#ifndef NET_BANDAID_BDN_CACHELIB_ORDER_STATISTIC_SET_H_
#define NET_BANDAID_BDN_CACHELIB_ORDER_STATISTIC_SET_H_

#include <functional>
#include <utility>

#include "raw_order_statistic_set.h"  // IWYU pragma: export

namespace cachelib {
namespace cachelib_internal {

template <class Key, class Compare>
class OstSetNode
    : public RawNode<Key, Key, const Key, Compare, OstSetNode<Key, Compare>,
                     SetExtractedNode<Key, OstSetNode<Key, Compare>>> {
  using Base = typename OstSetNode::RawNode;

 public:
  explicit OstSetNode(Key k) : Base(std::move(k)) {}
  using value_type = Key;
  static const Key &key(const value_type &value) { return value; }
};

template <class KeyType, class CompareType, class NodeType>
struct OrderStatisticsSetTraits {
  using key_type = KeyType;
  using value_type = KeyType;
  using stored_value_type = const KeyType;
  using Compare = CompareType;
  using Node = NodeType;
};
}  // namespace cachelib_internal

template <class Key, class Compare = std::less<>>
class OrderStatisticSet
    : public cachelib_internal::RawOrderStatisticSet<
          cachelib_internal::OrderStatisticsSetTraits<
              Key, Compare, cachelib_internal::OstSetNode<Key, Compare>>> {
 private:
  using Base = typename OrderStatisticSet::RawOrderStatisticSet;

 public:
  using typename Base::const_pointer;
  using typename Base::const_reference;
  using typename Base::difference_type;
  using typename Base::key_compare;
  using typename Base::key_type;
  using typename Base::node_type;
  using typename Base::pointer;
  using typename Base::reference;
  using typename Base::size_type;
  using typename Base::value_compare;
  using typename Base::value_type;

  // Some notes about iterators:
  //
  // 1. Iterator and reference stability: The iterators are not stable in the
  //    face of any operation that adds or removes an element of the tree.
  //    References are stable as long as the element referenced remains in the
  //    tree.
  //
  // 2. The `iterator` classes provide an additional operation `rank()` method
  //    that tells you the number of elements before the iterator.  Thus you can
  //    do `find(k).rank()` to find the rank of the element with key `k`.

  using typename Base::iterator;
  using typename Base::const_iterator;
  using typename Base::reverse_iterator;
  using typename Base::const_reverse_iterator;

  // size()
  //
  // Returns the number of elements in the set.
  using Base::size;

  // bool empty() const;
  //
  // Returns true if there are no elements.
  using Base::empty;

  // iterator begin();
  // const_iterator begin() const;
  // const_iterator cbegin() const;
  //
  //   Returns an iterator to the first element of the container.  If the
  //   container is empty, returns `end()`.
  using Base::begin;

  using Base::cbegin;

  // iterator end();
  // const_iterator end() const;
  // const_iterator cend() const;
  //
  // Returns an iterator to a just-past-the-end of the container.  Attempting to
  // access the element referred to by this iterator results in undefined
  // behavior.
  using Base::end;

  using Base::cend;

  // reverse_iterator rbegin();
  // const_reverse_iterator rbegin() const;
  // const_reverse_iterator crbegin() const;
  //
  // Returns a reverse iterator to the first element of the reversed map.  This
  // corresponds to the last element of the non-reversed map.  If the map is
  // empty, the returned iterator is equal to rend().
  using Base::rbegin;

  using Base::crbegin;

  // reverse_iterator rend();
  // const_reverse_iterator rend() const;
  // const_reverse_iterator crend() const;
  //
  // Return a reverse iterator to just-past-the-end of the reversed map.  It
  // corresponds to just-in-front-of-the-beginning of the non-reversed map.
  // Attempting to access the element referred to by this iterator results in
  // undefined behavior
  using Base::rend;

  using Base::crend;

  // iterator find(const Key &key);
  // const_iterator find(const Key &key);
  //
  //   Finds an element with key equivalent to `key`.
  //
  //
  // template<class K> iterator find(const K &x);
  // template<class K> const_iterator find(const K &x) const;
  //
  //   Finds an element that compares equivalent to value `x`.  This overload
  //   particpates in overload resolution only if the qualified-id
  //   `Compare::is_transparent` is valid and denotes a type.  It allows calling
  //   the function without constructing an instance of `Key`.
  using Base::find;

  // bool contains(const Key &key) const;
  //
  //   Returns true if there is an element with key equivalent to `key`.
  //
  // template <class K> bool contains(const K &x) const;
  //
  //   Returns true if there is an element with key that compares equivalent to
  //   the value `x`.
  using Base::contains;

  // iterator lower_bound(const Key &key);
  // const_iterator lower_bound(const Key &key) const;
  //
  //   Returns an iterator pointing to the first element that is not less than
  //   `key`.
  //
  // template <class K> iterator lower_bound(const K &x);
  // template <class K> const_iterator lower_bound(const K& x) const;
  //
  //   Returns an iterator pointing to the first element that compares not less
  //   to the value of `x`.  This overload participates in overload resolution
  //   only if the qualified-id `Compare::is_transparent` is valid and denotes a
  //   type.  It allows calling this function without constructing an instance
  //   of `Key`.
  using Base::lower_bound;

  // iterator upper_bound(const Key &key);
  // const_iterator upper_bound(const Key &key) const;
  //
  //   Returns an iterator pointing to the first element that is greater than
  //   `key`.
  //
  // template <class K> iterator upper_bound(const K &x);
  // template <class K> const_iterator upper_bound(const K& x) const;
  //
  //   Returns an iterator pointing to the first element that compares greater
  //   to the value of `x`.  This overload participates in overload resolution
  //   only if the qualified-id `Compare::is_transparent` is valid and denotes a
  //   type.  It allows calling this functin without constructing an instance of
  //   `Key`.
  //
  // If there is no such element, then returns `end()`.
  using Base::upper_bound;

  // iterator Select(size_t idx);
  // const_iterator Select(size_t idx) const;
  //
  //   If `idx` is less than the number of elements in the container, returns an
  //   iterator to the element with rank `idx`, otherwise returns `end()`.  The
  //   rank of an element is the number of elements in the container that
  //   compare less than the element.
  using Base::select;

  // void clear() const;
  //
  // Erases all the elements from the container.  All iterators and references
  // become invalid.
  using Base::clear;

  // std::pair<iterator, bool> insert(const value_type &value);
  //
  // Inserts `value` into the container if the container doesn't already contain
  // an element with an equivalent key.  Returns a pair consisting of an
  // iterator to the inserted element (or the element that prevented the
  // insertin) and a bool indicating whether the insertion occurred.
  //
  // Note that the set of insert methods is not as rich as for std::set::insert.
  // The versions that are equivalent to emplace and with && aren't implemented.
  // The versions that take iterators and initializer lists aren't implemented.
  // Nodes aren't implemented.
  //
  // Iterators are invalidated.  References remain valid.
  using Base::insert;

  // iterator erase(iterator pos);
  //
  //   Removes the element at `pos`, returning an iterator following the removed
  //   element.
  //
  // size_type erase(const key_type &k);
  //
  //   Removes the element (if one exists) with key equivalent to `key`.
  //   Returns the number of elements removed (0 or 1).
  //
  // template <class K> size_t erase(const K &x);
  //
  //   Removes the element (if one exists) with key that compares equivalent to
  //   `x`.  This overload particates in overload resolution only if the
  //   qualified-id `Compare::is_transparent` is valid and dnotes a type, and
  //   neither `iterator` nor `const_iterator` is implicitly convertible from
  //   `K`.  It allows calling this function without constructing an instance of
  //   `Key`.  Returns the number of elements removed (0 or 1).
  //
  // References to unerased items remain valid.
  //
  // Iterators are invalidated (except for the iterator returned by `erase`).
  using Base::erase;

  // node_type extract(const key_type &k);
  //
  //   Removes the element (if one exists) with key equivalent to `key`.
  //   Returns a `node_type`.  In the standard library, `extract()` operates
  //   without copying or moving the `value_type`.  In this library, however,
  //   extraction can require a move.
  //
  //   node_type is a move-only type that can own a value_type from a set, and
  //   has the following operations.
  //
  //     bool node_type::empty() const;
  //     // Returns true if the extraction didn't return anything.
  //
  //     value_type &node_type::value();
  //     // Returns a reference to the extracted value.  Undefined if empty().
  //
  //     size_t node_type::rank() const;
  //     // Returns the rank of the value as it was before extracting it.
  using Base::extract;

  // void swap(OrderStatisticSet& other);
  //
  //   Exchanges the contents of the container with those of other.  Does not
  //   invoke move, copy, or swap operations on individual elements.
  //
  //   All references remain valid.
  //
  //   Unlike for std::set, all iterators become invalid.
  //
  //   The Compare objects must be swapable and they are exchanged using an
  //   unqualified call to non-member swap.
  using Base::swap;

  // key_compare key_comp() const;
  //
  //   Returns the key comparison function object.
  using Base::key_comp;

  // value_compare value_comp() const;
  //
  //   Returns the value comparison function object.
  using Base::value_comp;
};

}  // namespace cachelib
#endif  // NET_BANDAID_BDN_CACHELIB_ORDER_STATISTIC_SET_H_

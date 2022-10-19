#ifndef NET_BANDAID_BDN_CACHELIB_RAW_ORDER_STATISTIC_MAP_H_
#define NET_BANDAID_BDN_CACHELIB_RAW_ORDER_STATISTIC_MAP_H_

#include <optional>
#include <utility>

#include "net/bandaid/bdn/cachelib/raw_order_statistic_set.h"  // IWYU pragma: export

namespace cachelib {
namespace cachelib_internal {

template <class Key, class T, class Compare, class Derived, class ExtractType>
class RawMapNode
    : public cachelib_internal::RawNode<Key, std::pair<const Key, T>,
                                        std::pair<const Key, T>, Compare,
                                        Derived, ExtractType> {
  using Base = typename RawMapNode::RawNode;

 public:
  using value_type = std::pair<const Key, T>;
  explicit RawMapNode(value_type v) : Base(std::move(v)) {}
  static const Key &key(const value_type &value) { return value.first; }
  void SetMappedValue(const value_type &k) { this->value_.second = k.second; }
};

// There's currently no allocator parameter for this container.

template <class Traits>
struct RawOrderStatisticMapTraits : Traits {
  using value_type =
      std::pair<const typename Traits::key_type, typename Traits::mapped_type>;
};

template <class Traits>
class RawOrderStatisticMap : public cachelib_internal::RawOrderStatisticSet<
                                 RawOrderStatisticMapTraits<Traits>> {
  using Base = typename RawOrderStatisticMap::RawOrderStatisticSet;

 protected:
  using typename Base::Node;

  using key_type = typename Traits::key_type;
  using mapped_type = typename RawOrderStatisticMapTraits<Traits>::mapped_type;
  using typename Base::difference_type;
  using typename Base::size_type;
  using typename Base::value_type;
  using key_compare = typename Traits::Compare;
  struct value_compare {
    bool operator()(const value_type &lhs, const value_type &rhs) const {
      return key_compare()(lhs.first, rhs.first);
    }
  };
  value_compare value_comp() const { return value_compare(); }
  using reference = value_type &;
  using const_reference = const value_type &;
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using iterator = typename Base::iterator;
  using const_iterator = typename Base::const_iterator;
  using const_reverse_iterator = typename Base::const_reverse_iterator;

  // Inserts or assigns as does map::insert_or_assign().  If k isn't in the map,
  // the inserts it and return an iterator pointing at the newly inserted value,
  // and true.  If k is in the map, then assigns it, and returns the iterator
  // and false.
  //
  // Rationale: For std::map, the insert_or_assign() method takes templated
  // rvalue references.  For now, we'll just keep it simple here and take
  // ownership of the `k` and `v`.
  std::pair<iterator, bool> insert_or_assign(key_type k, mapped_type v) {
    auto [new_root, inserted_at_idx, inserted_at_node, did_insert] =
        Node::Insert(std::move(this->root_), {std::move(k), std::move(v)}, 0,
                     true, this->lessthan_);
    this->root_ = std::move(new_root);
    return {iterator(this, inserted_at_idx, inserted_at_node), did_insert};
  }
};

// Similar to SetExtractedNode, but for maps.
template <class KeyType, class MappedType>
class MapExtractedNode {
 public:
  using key_type = KeyType;
  using mapped_type = MappedType;
  constexpr MapExtractedNode() = default;
  ~MapExtractedNode() = default;
  // moveable
  MapExtractedNode(MapExtractedNode &&nt)
      : value_(std::move(nt.value_)), rank_(nt.rank_) {}
  MapExtractedNode &operator=(MapExtractedNode &&nt) {
    value_ = std::move(nt.value_);
    rank_ = nt.rank_;
  }
  // not copyable
  MapExtractedNode(const MapExtractedNode &) = delete;
  MapExtractedNode &operator=(const MapExtractedNode &) = delete;
  [[nodiscard]] bool empty() const { return !value_.has_value(); }
  // Implicit conversion needed to comform to std::set::node_type.
  explicit operator bool() const { return value_.hash_value(); }
  // These methods are not const, although the std API calls for them to be
  // const.
  key_type &key() { return value_->first; }
  mapped_type &mapped() { return value_->second; }

  // This constructor isn't part of the standard library API, but it seems like
  // a hassle to get it to be private, and it doesn't do much harm to expose it.
  explicit MapExtractedNode(std::pair<key_type, mapped_type> &&value)
      : value_(std::move(value)) {}

  size_t rank() const { return rank_; }
  void set_rank(size_t rank) { rank_ = rank; }

 private:
  std::optional<std::pair<key_type, mapped_type>> value_ = std::nullopt;
  size_t rank_;
};

}  // namespace cachelib_internal
}  // namespace cachelib

#endif  // NET_BANDAID_BDN_CACHELIB_RAW_ORDER_STATISTIC_MAP_H_

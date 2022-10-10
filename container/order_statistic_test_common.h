#ifndef NET_BANDAID_BDN_CACHELIB_ORDER_STATISTIC_TEST_COMMON_H_
#define NET_BANDAID_BDN_CACHELIB_ORDER_STATISTIC_TEST_COMMON_H_

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>

#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"
#include "raw_order_statistic_set.h"

namespace cachelib {

// A matcher for the order statistic tree iterator's `rank()` method.  The
// matcher takes an integer, which is the expected rank.  E.g.,
//   EXPECT_THAT(ost.begin(), Rank(0));
MATCHER_P(Rank, rank,
          absl::StrFormat("%s an iterator of rank which %s",
                          negation ? "isn't" : "is",
                          ::testing::DescribeMatcher<size_t>(rank))) {
  return ExplainMatchResult(rank, arg.rank(), result_listener);
}

// A matcher that an iterator references something that `matcher` matches.  This
// matcher checks for past-the-end (which doesn't match).  E.g.,
//  EXPECT_THAT(ost.insert(42).first, IteratorReferences(&ost, 42));
MATCHER_P2(IteratorReferences, container, matcher,
           absl::StrFormat(
               "%s an iterator referencing value which %s",
               negation ? "isn't" : "is",
               ::testing::DescribeMatcher<
                   typename std::remove_reference<arg_type>::type::value_type>(
                   matcher))) {
  if (arg == container->end()) {
    *result_listener << "and is past-the-end.";
    return false;
  }
  return ExplainMatchResult(matcher, *arg, result_listener);
}

// A matcher that an iterator is just-past-the end.  E.g.,
//   EXPECT_THAT(ost.end(), IteratAtEnd(&ost));
MATCHER_P(IteratorAtEnd, container,
          absl::StrFormat("%s reference to end", negation ? "isn't" : "is")) {
  return arg == container->end();
}
MATCHER_P(IteratorAtReverseEnd, container,
          absl::StrFormat("%s reference to (reverse) end",
                          negation ? "isn't" : "is")) {
  return arg == container->rend();
}

// Prints a container with an iterator in the same format that gmock prints
// std::map objects.  For example if we have a map with two pairs (1, 2) and (3,
// 4) we print
//      { (1, 2), (3, 4) }
template <class OST>
std::ostream& PrintOst(std::ostream& out, const OST& ost) {
  out << "{";
  bool first = true;
  for (const auto& value : ost) {
    if (!first) out << ",";
    first = false;
    out << " " << ::testing::PrintToString(value);
  }
  return out << " }";
}

// Compare equality of a set with a tree.
template <class K, class Tree>
bool operator==(const std::set<K>& map, const Tree& ost) {
  if (map.size() != ost.size()) return false;
  // Make sure everything in map is in ost.
  for (const auto& map_val : map) {
    auto it = ost.find(map_val);
    if (it == ost.end()) return false;
    if (map_val != *it) return false;
  }
  // Make sure everything in ost is in map.
  for (const auto& ost_val : ost) {
    const auto& it = map.find(ost_val);
    if (it == map.end()) return false;  // it's not there
    if (ost_val != *it) return false;
  }
  return true;
}

template <class K, class V, class Map>
bool operator==(const std::map<K, V>& map, const Map& ost) {
  if (map.size() != ost.size()) return false;
  // Make sure everything in map is in ost.
  for (const auto& mpair : map) {
    auto it = ost.find(mpair.first);
    if (it == ost.end()) return false;
    if (mpair.first != it->first) return false;
    if (mpair.second != it->second) return false;
  }
  // Make sure everything in ost is in map.
  for (const auto ost_pair : ost) {
    const auto& it = map.find(ost_pair.first);
    if (it == map.end()) return false;  // it's not there
    if (it->first != ost_pair.first) return false;
    if (it->second != ost_pair.second) return false;
  }
  return true;
}

// Given a map, finds some random number < `domain_limit` that isn't in the map
// and construct the pair comprising that number and another random number.  The
// random numbers need not be high quality.
static inline std::pair<size_t, size_t> find_pair_not_in_map(
    const std::map<size_t, size_t>& map, size_t domain_limit,
    absl::BitGen& bitgen) {
  while (true) {
    size_t domain_val = absl::Uniform<size_t>(bitgen, 0, domain_limit);
    if (map.find(domain_val) == map.end()) {
      return {domain_val, absl::Uniform<size_t>(bitgen)};
    }
  }
}

// Given a Order-Statistic map (i.e., one that has rank(), returns a a randomly
// chosen element.
template <class Tree>
std::pair<std::pair<size_t, size_t>, std::pair<size_t, size_t>>
find_pair_in_map(const Tree& tree, absl::BitGen& bitgen) {
  size_t rank = absl::Uniform<size_t>(bitgen, 0, tree.size());
  auto rr = tree.select(rank);
  EXPECT_NE(rr, tree.end());
  return {*rr, {rr->first, absl::Uniform<size_t>(bitgen)}};
}

// Run random inserts and deletes on an object of type Tree, and also on a
// std::map.  Make sure that the Tree and the map do the same thing.
template <class Tree, class ExtraChecks>
void RunRandomized(ExtraChecks extrachecks) {
  absl::BitGen bitgen;
  const size_t n_runs = 10;
  const size_t ops_per_run = 200;
  const size_t domain_max = 100000;
  std::map<size_t, size_t> map;
  Tree ost;
  for (size_t run = 0; run < n_runs; ++run) {
    map.clear();
    ost.clear();
    for (size_t opnum = 0; opnum < ops_per_run; ++opnum) {
      const size_t start_inserts = 10;
      switch (size_t randop = (opnum < start_inserts
                                   ? 0
                                   : absl::Uniform<size_t>(bitgen, 0, 5))) {
        case 0: {  // insert a random value that's not there.
          const auto pair = find_pair_not_in_map(map, domain_max, bitgen);
          // Check the lower bound
          if (auto mlb = map.lower_bound(pair.first); mlb == map.end()) {
            EXPECT_EQ(ost.lower_bound(pair.first), ost.end());
          } else {
            EXPECT_THAT(ost.lower_bound(pair.first),
                        IteratorReferences(
                            &ost, ::testing::Pair(mlb->first, mlb->second)));
          }
          // Check the upper bound
          EXPECT_EQ(map.lower_bound(pair.first), map.upper_bound(pair.first));
          EXPECT_EQ(ost.lower_bound(pair.first), ost.upper_bound(pair.first));

          auto mr = map.insert(pair);
          auto r = ost.insert(pair);
          EXPECT_THAT(r, Pair(IteratorReferences(&ost, pair), mr.second));
          break;
        }
        case 1: {  // insert_or_assign a random value that's not there.
          auto pair = find_pair_not_in_map(map, domain_max, bitgen);
          auto mr = map.insert_or_assign(pair.first, pair.second);
          auto r = ost.insert_or_assign(pair.first, pair.second);
          EXPECT_THAT(r, Pair(IteratorReferences(&ost, pair), mr.second));
          break;
        }
        case 2: {  // insert there is something that is there (if map not
                   // empty).
          if (!map.empty()) {
            auto [oldpair, newpair] = find_pair_in_map(ost, bitgen);
            // Check the lower bund
            EXPECT_EQ(map.lower_bound(oldpair.first), map.find(oldpair.first));
            EXPECT_EQ(ost.lower_bound(oldpair.first), ost.find(oldpair.first));
            // Check the upper bound
            EXPECT_EQ(map.upper_bound(oldpair.first),
                      ++map.find(oldpair.first));
            EXPECT_EQ(ost.upper_bound(oldpair.first),
                      ++ost.find(oldpair.first));
            EXPECT_THAT(map.insert(newpair),
                        Pair(IteratorReferences(&map, oldpair), false));
            EXPECT_THAT(ost.insert(newpair),
                        Pair(IteratorReferences(&ost, oldpair), false));
          }
          break;
        }
        case 3: {  // insert_or_assign something that is there.
          if (!map.empty()) {
            auto [oldpair, newpair] = find_pair_in_map(ost, bitgen);
            EXPECT_THAT(map.insert_or_assign(newpair.first, newpair.second),
                        Pair(IteratorReferences(&map, newpair), false));
            auto [ost_it, did_insert] =
                ost.insert_or_assign(newpair.first, newpair.second);
            EXPECT_THAT(did_insert, false);
            EXPECT_THAT(ost_it, IteratorReferences(&ost, newpair))
                << " found " << ost_it->first << ", " << ost_it->second;
          }
          break;
        }
        case 4: {  // Delete a random thing that's there.
          if (!map.empty()) {
            size_t rank = absl::Uniform<size_t>(bitgen, 0, map.size());
            auto rr = ost.select(rank);
            EXPECT_NE(rr, ost.end());
            size_t val = rr->first;
            map.erase(map.find(val));
            ost.erase(rr);
          }
          break;
        }
        default:
          DCHECK(0);
      }
      ost.Check();
      EXPECT_EQ(map.empty(), ost.empty());
      EXPECT_EQ(map, ost);
      extrachecks(map, ost);
      size_t rank = 0;
      for (auto it = ost.begin(); it != ost.end(); ++it) {
        EXPECT_EQ(it.rank(), rank);
        ++rank;
      }
    }
  }
}

namespace cachelib_internal {
template <class Traits>
std::ostream& RawOrderStatisticSet<Traits>::PrintStructureForTest(
    std::ostream& out, std::function<std::string(const value_type&)> kp) const {
  return Node::Print(out, root_, kp);
}

template <class Key, class Value, class StoredValue, class Compare,
          class NodeType, class ExtractedNodeType>
std::ostream&
RawNode<Key, Value, StoredValue, Compare, NodeType, ExtractedNodeType>::Print(
    std::ostream& out, const std::unique_ptr<Node>& n,
    std::function<std::string(const Value& key)> kp) {
  if (!n) return out << "()";
  out << "(" << kp(n->value_) << ":" << n->subtree_size_ << " ";
  Print(out, n->left_, kp) << " ";
  return Print(out, n->right_, kp) << ")";
}

}  // namespace cachelib_internal
}  // namespace cachelib

#endif  // NET_BANDAID_BDN_CACHELIB_ORDER_STATISTIC_TEST_COMMON_H_

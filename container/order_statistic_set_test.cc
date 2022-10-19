#include "order_statistic_set.h"

#include <cstddef>
#include <functional>
#include <iterator>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "order_statistic_test_common.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"

namespace cachelib {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::PrintToString;

// Print a map in the same format as gmock prints std::map.
template <class Key, class Compare>
std::ostream& operator<<(std::ostream& out,
                         const OrderStatisticSet<Key, Compare>& set) {
  return PrintOst(out, set);
}

// Make sure it all compiles.
template class OrderStatisticSet<size_t>;

// Checks the `public` types for OrderStatisticSet
TEST(OrderStatisticSetTest, SetPublicTypes) {
  using Set = OrderStatisticSet<size_t>;
  Set s;
  static_assert(std::is_same_v<Set::key_type, size_t>);
  static_assert(!std::is_same_v<Set::key_type, std::string>);
  static_assert(std::is_same_v<Set::value_type, size_t>);
  static_assert(std::is_same_v<Set::size_type, size_t>);
  static_assert(std::is_same_v<Set::reference, size_t&>);
  static_assert(std::is_same_v<Set::const_reference, const size_t&>);
  static_assert(std::is_same_v<Set::pointer, size_t*>);
  static_assert(std::is_same_v<Set::const_pointer, const size_t*>);
  static_assert(!cachelib_internal::IsTransparent<std::less<int>>::value);
  static_assert(cachelib_internal::IsTransparent<std::less<>>::value);
  static_assert(cachelib_internal::IsTransparent<Set::key_compare>::value);
  static_assert(!cachelib_internal::IsTransparent<
                OrderStatisticSet<size_t, std::less<int>>::key_compare>::value);
  static_assert(cachelib_internal::IsTransparent<Set::value_compare>::value);
}

// Checks the `public` types for OrderStatisticSet::iterator
TEST(OrderStatisticSetTest, IteratorPublicTypes) {
  using Set = OrderStatisticSet<size_t>;
  Set s;
  using It = Set::iterator;
  It it = s.begin();
  EXPECT_THAT(it, IteratorAtEnd(&s));
  static_assert(
      std::is_same_v<It::iterator_category, std::random_access_iterator_tag>);
  // The set iterator has a const value_type.
  static_assert(std::is_same_v<const size_t, It::value_type>);
  static_assert(std::is_same_v<ptrdiff_t, It::difference_type>);
  static_assert(std::is_same_v<const size_t*, It::pointer>);
  static_assert(std::is_same_v<const size_t&, It::reference>);
}

// Checks the `public` types for OrderStatisticSet::reverse_iterator
TEST(OrderStatisticSetTest, ReverseIteratorPublicTypes) {
  using Set = OrderStatisticSet<size_t>;
  Set s;
  using It = Set::reverse_iterator;
  It it = s.rbegin();
  EXPECT_THAT(it, IteratorAtReverseEnd(&s));
  static_assert(
      std::is_same_v<It::iterator_category, std::random_access_iterator_tag>);
  static_assert(std::is_same_v<const size_t, It::value_type>);
  static_assert(std::is_same_v<ptrdiff_t, It::difference_type>);
  static_assert(std::is_same_v<const size_t*, It::pointer>);
  static_assert(std::is_same_v<const size_t&, It::reference>);
}

// Checks the `public` types for OrderStatisticSet::const_iterator
TEST(OrderStatisticSetTest, ConstIteratorPublicTypes) {
  using Set = OrderStatisticSet<size_t>;
  Set s;
  using It = Set::const_iterator;
  It it = s.begin();
  EXPECT_THAT(it, IteratorAtEnd(&s));
  static_assert(
      std::is_same_v<It::iterator_category, std::random_access_iterator_tag>);
  static_assert(std::is_same_v<const size_t, It::value_type>);
  static_assert(std::is_same_v<ptrdiff_t, It::difference_type>);
  static_assert(std::is_same_v<const size_t*, It::pointer>);
  static_assert(std::is_same_v<const size_t&, It::reference>);
}

TEST(OrderStatisticSetTest, ConstReverseIteratorPublicTypes) {
  using Set = OrderStatisticSet<size_t>;
  Set s;
  using It = Set::const_reverse_iterator;
  It it = s.rbegin();
  EXPECT_THAT(it, IteratorAtReverseEnd(&s));
  static_assert(
      std::is_same_v<It::iterator_category, std::random_access_iterator_tag>);
  static_assert(std::is_same_v<const size_t, It::value_type>);
  static_assert(std::is_same_v<ptrdiff_t, It::difference_type>);
  static_assert(std::is_same_v<const size_t*, It::pointer>);
  static_assert(std::is_same_v<const size_t&, It::reference>);
}

// Check copy-constructibilibity and trivial-copy-constructibility of iterators.
TEST(OrderStatisticSetTest, IteratorCopyConstructible) {
  using Set = OrderStatisticSet<size_t>;

  static_assert(std::is_copy_constructible_v<Set::iterator>);
  static_assert(std::is_copy_constructible_v<Set::const_iterator>);
  static_assert(std::is_copy_constructible_v<Set::reverse_iterator>);
  static_assert(std::is_copy_constructible_v<Set::const_reverse_iterator>);

#if 0
  static_assert(std::is_trivially_copy_constructible_v<Set::iterator>);
  static_assert(std::is_trivially_copy_constructible_v<Set::const_iterator>);
  static_assert(std::is_trivially_copy_constructible_v<Set::reverse_iterator>);
  static_assert(
      std::is_trivially_copy_constructible_v<Set::const_reverse_iterator>);
#endif
}

// Make sure we can convert from iterator to const_iterator with imlicit
// constructors.  (And we would check that we cannot convert from const_iterator
// to iterator, but the google test infrastructure doesn't seem to have a way to
// test that code doesn't compile.)
TEST(OrderStatisticSetTest, IteratorImplicitConversionConstructor) {
  using Set = OrderStatisticSet<size_t>;

  Set s;
  Set::iterator it = s.begin();

  // Construct non-const from non-const
  Set::iterator it2(it);
  EXPECT_THAT(it2, IteratorAtEnd(&s));

  // Construct const from non-const
  Set::const_iterator cit(it);
  EXPECT_THAT(cit, IteratorAtEnd(&s));

  // This shouldn't  compile:
  // Construct const from non-const
  // Set::iterator foo2 = s.cbegin();

  // Construct const from const
  Set::const_iterator cit2(cit);
  EXPECT_THAT(cit2, IteratorAtEnd(&s));
}

// Make sure can copy from iterator to const_iterator.  Similarly to the
// IteratorImplicitConversionConstructor test, we'd like to be able to check
// that const_iterator cannot be copied to iterator.
TEST(OrderStatisticSetTest, IteratorImplicitConversionForCopy) {
  using Set = OrderStatisticSet<size_t>;
  Set s;
  Set::iterator it = s.begin();

  // Copy non-const from non-const
  Set::iterator it3 = it;
  EXPECT_THAT(it3, IteratorAtEnd(&s));

  // Copy const from non-const
  Set::const_iterator cit = it;
  EXPECT_THAT(cit, IteratorAtEnd(&s));

  // Copy non-const from const shouldn't compile
  // Set::iterator it4 = cit;

  // copy const from const
  Set::const_iterator cit2 = cit;
  EXPECT_THAT(cit2, IteratorAtEnd(&s));
}

TEST(OrderStatisticSetTest, IteratorConvertibility) {
  using Set = OrderStatisticSet<size_t>;
  static_assert(std::is_convertible_v<Set::iterator, Set::iterator>);
  static_assert(std::is_convertible_v<Set::iterator, Set::const_iterator>);
  static_assert(!std::is_convertible_v<Set::const_iterator, Set::iterator>);
  static_assert(
      std::is_convertible_v<Set::const_iterator, Set::const_iterator>);
}

TEST(OrderStatisticSetTest, Basic) {
  std::set<size_t> set;
  OrderStatisticSet<size_t> ost;
  EXPECT_TRUE(ost.empty());
  EXPECT_EQ(set, ost);
  EXPECT_EQ(ost.crbegin(), ost.crend());
  EXPECT_EQ(ost.rbegin(), ost.rend());
  set.insert(1);
  EXPECT_THAT(ost.insert(1),
              Pair(AllOf(IteratorReferences(&ost, 1u), Rank(0u)), true));
  EXPECT_EQ(set, ost);
  EXPECT_THAT(ost.lower_bound(0), IteratorReferences(&ost, 1u));
  EXPECT_THAT(ost.lower_bound(1), IteratorReferences(&ost, 1u));
  EXPECT_EQ(ost.lower_bound(2), ost.end());
  EXPECT_EQ(ost.lower_bound(2), ost.cend());
  set.insert(3);
  EXPECT_THAT(ost.insert(3), Pair(IteratorReferences(&ost, 3u), true));
  // Tree is now {1, 3}
  EXPECT_EQ(ost.size(), 2u);
  EXPECT_THAT(ost.find(0), IteratorAtEnd(&ost));
  EXPECT_THAT(ost.find(1), IteratorReferences(&ost, 1u));
  EXPECT_THAT(ost.find(2), IteratorAtEnd(&ost));
  EXPECT_THAT(ost.find(3), IteratorReferences(&ost, 3u));
  EXPECT_THAT(ost.find(4), IteratorAtEnd(&ost));
  EXPECT_NE(ost.select(0), ost.end());
  EXPECT_THAT(ost.select(0), IteratorReferences(&ost, 1u));
  EXPECT_THAT(ost.select(1), IteratorReferences(&ost, 3u));
  EXPECT_THAT(ost.select(2), IteratorAtEnd(&ost));
  EXPECT_THAT(ost.lower_bound(0), IteratorReferences(&ost, 1u));
  EXPECT_THAT(ost.lower_bound(1), IteratorReferences(&ost, 1u));
  EXPECT_THAT(ost.lower_bound(2), IteratorReferences(&ost, 3u));
  EXPECT_THAT(ost.lower_bound(3), IteratorReferences(&ost, 3u));
  EXPECT_THAT(ost.lower_bound(4), IteratorAtEnd(&ost));
  EXPECT_EQ(set, ost);
  {
    std::ostringstream out;
    out << ost;
    EXPECT_EQ(out.str(), "{ 1, 3 }");
  }
  {
    std::ostringstream out;
    ost.PrintStructureForTest(out, PrintToString<size_t>);
    EXPECT_EQ(out.str(), "(1:2 () (3:1 () ()))");
  }

  {
    OrderStatisticSet<size_t> s2;
    s2.insert(5);
    EXPECT_THAT(s2, ElementsAre(5u));
    EXPECT_THAT(ost, ElementsAre(1u, 3u));
    std::swap(s2, ost);
    EXPECT_THAT(ost, ElementsAre(5u));
    EXPECT_THAT(s2, ElementsAre(1u, 3u));
    ost.swap(s2);
    EXPECT_THAT(s2, ElementsAre(5u));
    EXPECT_THAT(ost, ElementsAre(1u, 3u));
  }

  // erase something not present
  EXPECT_EQ(ost.erase(99), 0u);
  EXPECT_EQ(set, ost);

  EXPECT_EQ(ost.erase(0), set.erase(0));
  EXPECT_EQ(set, ost);

  EXPECT_EQ(ost.erase(1), set.erase(1));
  EXPECT_EQ(set, ost);

  EXPECT_EQ(ost.erase(1), set.erase(1));  // erase 1 again (not present)
  EXPECT_EQ(set, ost);

  EXPECT_TRUE(ost.key_comp()(1, 2));
  EXPECT_TRUE(ost.value_comp()(1, 2));
}

TEST(OrderStatisticSetTest, Erase) {
  // TODO: Test with const key, sinc we'll need keys with const component for the map.
  std::set<size_t> set;
  OrderStatisticSet<size_t> ost;
  set.insert(1);
  ost.insert(1);
  EXPECT_EQ(set.erase(set.find(1)), set.end());
  EXPECT_EQ(ost.erase(ost.find(1)), ost.end());
  EXPECT_EQ(set, ost);
  set.insert(1);
  ost.insert(1);
  set.insert(2);
  ost.insert(2);
  EXPECT_THAT(set.erase(set.find(1)), IteratorReferences(&set, 2u));
  EXPECT_THAT(ost.erase(ost.find(1)), IteratorReferences(&ost, 2u));
  EXPECT_EQ(set, ost);
  EXPECT_THAT(set.erase(set.find(2)), IteratorAtEnd(&set));
  EXPECT_THAT(ost.erase(ost.find(2)), IteratorAtEnd(&ost));
  EXPECT_EQ(set, ost);
  {
    std::ostringstream out;
    ost.PrintStructureForTest(out, PrintToString<size_t>) << std::endl;
    EXPECT_EQ(out.str(), "()\n");
  }

  set.insert(1);
  ost.insert(1);
  EXPECT_EQ(set, ost);
  set.insert(2);
  ost.insert(2);
  set.insert(3);
  ost.insert(3);
  set.insert(4);
  ost.insert(4);
  EXPECT_EQ(set, ost);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(set.erase(--set.end()), set.end());
    EXPECT_EQ(ost.erase(--ost.end()), ost.end());
    EXPECT_EQ(set, ost);
    EXPECT_EQ(ost.size(), 3 - i);
  }
}

// Make sure that the iterator returned by erase has the right rank.
TEST(OrderStatisticSetTest, EraseIteratorRank) {
  OrderStatisticSet<size_t> set;
  set.insert(1);
  {
    auto it = set.erase(set.find(1));
    EXPECT_THAT(it, IteratorAtEnd(&set));
    EXPECT_EQ(it.rank(), 0);
  }
  set.insert(0);
  set.insert(1);
  set.insert(2);
  set.insert(3);
  set.insert(4);
  set.insert(5);
  {
    auto it = set.erase(set.find(3));
    EXPECT_THAT(it, IteratorReferences(&set, 4));
    EXPECT_EQ(it.rank(), 3);
  }
  {
    auto it = set.erase(set.find(5));
    EXPECT_THAT(it, IteratorAtEnd(&set));
    EXPECT_EQ(it.rank(), 4);
  }
  {
    auto it = set.erase(set.find(1));
    EXPECT_THAT(it, IteratorReferences(&set, 2));
    EXPECT_EQ(it.rank(), 1);
  }
}

// Check that we can pass in strings as strings, string_views, or string
// literals.
TEST(OrderStatisticSetTest, String) {
  // TODO: Test with const string.  THis works in google3, but not here.
  std::set<std::string> set;
  OrderStatisticSet<std::string> ost;
  EXPECT_EQ(set, ost);
  set.insert("a");
  EXPECT_THAT(ost.insert(std::string("a")),
              Pair(IteratorReferences(&ost, "a"), true));
  EXPECT_EQ(set, ost);
  set.insert("b");
  EXPECT_THAT(ost.insert("b"), Pair(IteratorReferences(&ost, "b"), true));
  set.insert("c");
  EXPECT_THAT(ost.insert("c"), Pair(IteratorReferences(&ost, "c"), true));
  EXPECT_EQ(ost.size(), 3u);
  EXPECT_THAT(ost.find(""), IteratorAtEnd(&ost));
  EXPECT_THAT(ost.find("a"), IteratorReferences(&ost, "a"));
  EXPECT_THAT(ost.find(std::string("a")), IteratorReferences(&ost, "a"));
  EXPECT_THAT(ost.find(absl::string_view("a")), IteratorReferences(&ost, "a"));
  EXPECT_THAT(ost.find("aa"), IteratorAtEnd(&ost));
  EXPECT_THAT(ost.find("b"), IteratorReferences(&ost, "b"));
  EXPECT_THAT(ost.find("bb"), IteratorAtEnd(&ost));
  EXPECT_THAT(ost.find("c"), IteratorReferences(&ost, "c"));
  EXPECT_THAT(ost.find("cc"), IteratorAtEnd(&ost));
  EXPECT_THAT(ost.select(0), IteratorReferences(&ost, "a"));
  EXPECT_THAT(ost.select(1), IteratorReferences(&ost, "b"));
  EXPECT_THAT(ost.select(2), IteratorReferences(&ost, "c"));
  EXPECT_THAT(ost.select(3), IteratorAtEnd(&ost));
  EXPECT_EQ(set, ost);
  EXPECT_EQ(set.erase(std::string("a")), ost.erase("a"));
  EXPECT_EQ(set, ost);
  EXPECT_EQ(ost.erase(absl::string_view("b")), set.erase("b"));
  EXPECT_EQ(set, ost);
  EXPECT_EQ(ost.erase("c"), set.erase("c"));
  EXPECT_EQ(set, ost);
}

TEST(OrderStatisticSetTest, Iterator) {
  OrderStatisticSet<std::string> ost;
  ost.insert("b");
  ost.insert("c");
  {
    auto it = ost.begin();
    EXPECT_THAT(it, IteratorReferences(&ost, "b"));
    EXPECT_EQ(it.rank(), 0u);
    OrderStatisticSet<std::string>::const_iterator it2 = it;
    EXPECT_THAT(it2, IteratorReferences(&ost, "b"));
    EXPECT_EQ(it2.rank(), 0u);
  }
  {
    auto it = ost.cbegin();
    EXPECT_THAT(it, IteratorReferences(&ost, "b"));
    ++it;
    EXPECT_THAT(it, IteratorReferences(&ost, "c"));
    ++it;
    EXPECT_THAT(it, IteratorAtEnd(&ost));
  }
}

// TODO: The set parameter used to work with const size_t.
size_t find_value_not_in_set(const std::set<size_t>& set,
                             size_t domain_max, absl::BitGen& bitgen) {
  while (true) {
    size_t domain_val = absl::Uniform<size_t>(bitgen, 0, domain_max);
    if (set.find(domain_val) == set.end()) {
      return domain_val;
    }
  }
}

template <class Tree, class ExtraChecks>
void RunRandomizedSet(ExtraChecks extrachecks) {
  absl::BitGen bitgen;
  const size_t n_runs = 10;
  const size_t ops_per_run = 200;
  const size_t domain_max = 100000;
  // TODO: This used to work for const size_t.
  std::set<size_t> set;
  Tree ost;
  for (size_t run = 0; run < n_runs; ++run) {
    set.clear();
    ost.clear();
    for (size_t opnum = 0; opnum < ops_per_run; ++opnum) {
      const size_t start_inserts = 10;
      switch (size_t randop = (opnum < start_inserts
                                   ? 0
                                   : absl::Uniform<size_t>(bitgen, 0, 3))) {
        case 0: {  // insert a random value that's not there.
          size_t value = find_value_not_in_set(set, domain_max, bitgen);
          // Check the lower bound
          if (auto mlb = set.lower_bound(value); mlb == set.end()) {
            EXPECT_EQ(ost.lower_bound(value), ost.end());
          } else {
            EXPECT_THAT(ost.lower_bound(value), IteratorReferences(&ost, *mlb));
          }
          // Check the upper bound
          EXPECT_EQ(set.lower_bound(value), set.upper_bound(value));
          EXPECT_EQ(ost.lower_bound(value), ost.upper_bound(value));

          auto mr = set.insert(value);
          auto r = ost.insert(value);
          EXPECT_THAT(r, Pair(IteratorReferences(&ost, value), mr.second));
          break;
        }
        case 1: {  // insert something that is there (if set not
                   // empty), which is a no-op.
          if (!set.empty()) {
            size_t rank = absl::Uniform<size_t>(bitgen, 0, set.size());
            auto rr = ost.select(rank);
            EXPECT_NE(rr, ost.end());
            size_t val = *rr;
            // Check the lower bund
            EXPECT_EQ(set.lower_bound(val), set.find(val));
            EXPECT_EQ(ost.lower_bound(val), ost.find(val));
            // Check the upper bound
            EXPECT_EQ(set.upper_bound(val), ++set.find(val));
            EXPECT_EQ(ost.upper_bound(val), ++ost.find(val));
            // Check that the insert apparently does nothing.
            EXPECT_THAT(set.insert(val),
                        Pair(IteratorReferences(&set, val), false));
            EXPECT_THAT(ost.insert(val),
                        Pair(IteratorReferences(&ost, val), false));
          }
          break;
        }
        case 2: {  // Delete a random thing that's there.
          if (!set.empty()) {
            size_t rank = absl::Uniform<size_t>(bitgen, 0, set.size());
            auto rr = ost.select(rank);
            EXPECT_NE(rr, ost.end());
            size_t val = *rr;
            set.erase(val);
            ost.erase(rr);
          }
          break;
        }
        default:
          DCHECK(0);
      }
      ost.Check();
      EXPECT_EQ(set.empty(), ost.empty());
      EXPECT_EQ(set, ost);
      extrachecks(set, ost);
      size_t rank = 0;
      for (auto it = ost.begin(); it != ost.end(); ++it) {
        EXPECT_EQ(it.rank(), rank);
        ++rank;
      }
    }
  }
}

TEST(OrderStatisticSetTest, Randomized) {
  RunRandomizedSet<OrderStatisticSet<const size_t>>(
      []([[maybe_unused]] auto& a, [[maybe_unused]] auto& b) {});
}

TEST(OrderStatisticSetTest, Extract) {
  using Set = OrderStatisticSet<std::string>;
  Set set;
  set.insert("a");
  {
    Set::node_type e = set.extract("b");
    EXPECT_TRUE(e.empty());
  }
  {
    Set::node_type e = set.extract("a");
    EXPECT_FALSE(e.empty());
    EXPECT_EQ(e.value(), "a");
    EXPECT_EQ(e.rank(), 0);
    EXPECT_TRUE(set.empty());
  }
  set.insert("c");
  set.insert("d");
  set.insert("e");
  {
    auto it = set.erase(set.find("d"));
    EXPECT_EQ(it.rank(), 1);
  }
  set.insert("d");
  {
    Set::node_type e = set.extract("d");
    EXPECT_FALSE(e.empty());
    EXPECT_EQ(e.value(), "d");
    EXPECT_EQ(e.rank(), 1);
  }
  set.insert("d");
  auto it = set.find("d");
  EXPECT_EQ(it.rank(), 1);
  Set::node_type e = set.extract(it);
  EXPECT_FALSE(e.empty());
  EXPECT_EQ(e.value(), "d");
  EXPECT_EQ(e.rank(), 1);
  EXPECT_EQ(set.size(), 2);
}

}  // namespace cachelib

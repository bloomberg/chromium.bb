// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/hash/hash.h"

#include <array>
#include <bitset>
#include <cstring>
#include <deque>
#include <forward_list>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash_testing.h"
#include "absl/hash/internal/spy_hash_state.h"
#include "absl/meta/type_traits.h"
#include "absl/numeric/int128.h"

namespace {

using absl::Hash;
using absl::hash_internal::SpyHashState;

template <typename T>
class HashValueIntTest : public testing::Test {
};
TYPED_TEST_SUITE_P(HashValueIntTest);

template <typename T>
SpyHashState SpyHash(const T& value) {
  return SpyHashState::combine(SpyHashState(), value);
}

// Helper trait to verify if T is hashable. We use absl::Hash's poison status to
// detect it.
template <typename T>
using is_hashable = std::is_default_constructible<absl::Hash<T>>;

TYPED_TEST_P(HashValueIntTest, BasicUsage) {
  EXPECT_TRUE((is_hashable<TypeParam>::value));

  TypeParam n = 42;
  EXPECT_EQ(SpyHash(n), SpyHash(TypeParam{42}));
  EXPECT_NE(SpyHash(n), SpyHash(TypeParam{0}));
  EXPECT_NE(SpyHash(std::numeric_limits<TypeParam>::max()),
            SpyHash(std::numeric_limits<TypeParam>::min()));
}

TYPED_TEST_P(HashValueIntTest, FastPath) {
  // Test the fast-path to make sure the values are the same.
  TypeParam n = 42;
  EXPECT_EQ(absl::Hash<TypeParam>{}(n),
            absl::Hash<std::tuple<TypeParam>>{}(std::tuple<TypeParam>(n)));
}

REGISTER_TYPED_TEST_CASE_P(HashValueIntTest, BasicUsage, FastPath);
using IntTypes = testing::Types<unsigned char, char, int, int32_t, int64_t, uint32_t,
                                uint64_t, size_t>;
INSTANTIATE_TYPED_TEST_CASE_P(My, HashValueIntTest, IntTypes);

enum LegacyEnum { kValue1, kValue2, kValue3 };

enum class EnumClass { kValue4, kValue5, kValue6 };

TEST(HashValueTest, EnumAndBool) {
  EXPECT_TRUE((is_hashable<LegacyEnum>::value));
  EXPECT_TRUE((is_hashable<EnumClass>::value));
  EXPECT_TRUE((is_hashable<bool>::value));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      LegacyEnum::kValue1, LegacyEnum::kValue2, LegacyEnum::kValue3)));
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      EnumClass::kValue4, EnumClass::kValue5, EnumClass::kValue6)));
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      std::make_tuple(true, false)));
}

TEST(HashValueTest, FloatingPoint) {
  EXPECT_TRUE((is_hashable<float>::value));
  EXPECT_TRUE((is_hashable<double>::value));
  EXPECT_TRUE((is_hashable<long double>::value));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      std::make_tuple(42.f, 0.f, -0.f, std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity())));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      std::make_tuple(42., 0., -0., std::numeric_limits<double>::infinity(),
                      -std::numeric_limits<double>::infinity())));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      // Add some values with small exponent to test that NORMAL values also
      // append their category.
      .5L, 1.L, 2.L, 4.L, 42.L, 0.L, -0.L,
      17 * static_cast<long double>(std::numeric_limits<double>::max()),
      std::numeric_limits<long double>::infinity(),
      -std::numeric_limits<long double>::infinity())));
}

TEST(HashValueTest, Pointer) {
  EXPECT_TRUE((is_hashable<int*>::value));

  int i;
  int* ptr = &i;
  int* n = nullptr;

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      std::make_tuple(&i, ptr, nullptr, ptr + 1, n)));
}

// TODO(EricWF): MSVC 15 has a bug that causes it to incorrectly evaluate the
// SFINAE in internal/hash.h, causing this test to fail.
#if !defined(_MSC_VER)
TEST(HashValueTest, PairAndTuple) {
  EXPECT_TRUE((is_hashable<std::pair<int, int>>::value));
  EXPECT_TRUE((is_hashable<std::pair<const int&, const int&>>::value));
  EXPECT_TRUE((is_hashable<std::tuple<int&, int&>>::value));
  EXPECT_TRUE((is_hashable<std::tuple<int&&, int&&>>::value));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      std::make_pair(0, 42), std::make_pair(0, 42), std::make_pair(42, 0),
      std::make_pair(0, 0), std::make_pair(42, 42), std::make_pair(1, 42))));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      std::make_tuple(std::make_tuple(0, 0, 0), std::make_tuple(0, 0, 42),
                      std::make_tuple(0, 23, 0), std::make_tuple(17, 0, 0),
                      std::make_tuple(42, 0, 0), std::make_tuple(3, 9, 9),
                      std::make_tuple(0, 0, -42))));

  // Test that tuples of lvalue references work (so we need a few lvalues):
  int a = 0, b = 1, c = 17, d = 23;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      std::tie(a, a), std::tie(a, b), std::tie(b, c), std::tie(c, d))));

  // Test that tuples of rvalue references work:
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      std::forward_as_tuple(0, 0, 0), std::forward_as_tuple(0, 0, 42),
      std::forward_as_tuple(0, 23, 0), std::forward_as_tuple(17, 0, 0),
      std::forward_as_tuple(42, 0, 0), std::forward_as_tuple(3, 9, 9),
      std::forward_as_tuple(0, 0, -42))));
}
#endif  // !defined(_MSC_VER)

TEST(HashValueTest, CombineContiguousWorks) {
  std::vector<std::tuple<int>> v1 = {std::make_tuple(1), std::make_tuple(3)};
  std::vector<std::tuple<int>> v2 = {std::make_tuple(1), std::make_tuple(2)};

  auto vh1 = SpyHash(v1);
  auto vh2 = SpyHash(v2);
  EXPECT_NE(vh1, vh2);
}

struct DummyDeleter {
  template <typename T>
  void operator() (T* ptr) {}
};

struct SmartPointerEq {
  template <typename T, typename U>
  bool operator()(const T& t, const U& u) const {
    return GetPtr(t) == GetPtr(u);
  }

  template <typename T>
  static auto GetPtr(const T& t) -> decltype(&*t) {
    return t ? &*t : nullptr;
  }

  static std::nullptr_t GetPtr(std::nullptr_t) { return nullptr; }
};

TEST(HashValueTest, SmartPointers) {
  EXPECT_TRUE((is_hashable<std::unique_ptr<int>>::value));
  EXPECT_TRUE((is_hashable<std::unique_ptr<int, DummyDeleter>>::value));
  EXPECT_TRUE((is_hashable<std::shared_ptr<int>>::value));

  int i, j;
  std::unique_ptr<int, DummyDeleter> unique1(&i);
  std::unique_ptr<int, DummyDeleter> unique2(&i);
  std::unique_ptr<int, DummyDeleter> unique_other(&j);
  std::unique_ptr<int, DummyDeleter> unique_null;

  std::shared_ptr<int> shared1(&i, DummyDeleter());
  std::shared_ptr<int> shared2(&i, DummyDeleter());
  std::shared_ptr<int> shared_other(&j, DummyDeleter());
  std::shared_ptr<int> shared_null;

  // Sanity check of the Eq function.
  ASSERT_TRUE(SmartPointerEq{}(unique1, shared1));
  ASSERT_FALSE(SmartPointerEq{}(unique1, shared_other));
  ASSERT_TRUE(SmartPointerEq{}(unique_null, nullptr));
  ASSERT_FALSE(SmartPointerEq{}(shared2, nullptr));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      std::forward_as_tuple(&i, nullptr,                    //
                            unique1, unique2, unique_null,  //
                            absl::make_unique<int>(),       //
                            shared1, shared2, shared_null,  //
                            std::make_shared<int>()),
      SmartPointerEq{}));
}

TEST(HashValueTest, FunctionPointer) {
  using Func = int (*)();
  EXPECT_TRUE(is_hashable<Func>::value);

  Func p1 = [] { return 2; }, p2 = [] { return 1; };
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      std::make_tuple(p1, p2, nullptr)));
}

struct WrapInTuple {
  template <typename T>
  std::tuple<int, T, size_t> operator()(const T& t) const {
    return std::make_tuple(7, t, 0xdeadbeef);
  }
};

TEST(HashValueTest, Strings) {
  EXPECT_TRUE((is_hashable<std::string>::value));
  EXPECT_TRUE((is_hashable<std::string>::value));

  const std::string small = "foo";
  const std::string dup = "foofoo";
  const std::string large = "large";
  const std::string huge = std::string(5000, 'a');
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      std::string(), absl::string_view(),
      std::string(""), absl::string_view(""),
      std::string(small), absl::string_view(small),
      std::string(dup), absl::string_view(dup),
      std::string(large), absl::string_view(large),
      std::string(huge), absl::string_view(huge))));

  // Also check that nested types maintain the same hash.
  const WrapInTuple t{};
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      //
      t(std::string()), t(absl::string_view()),
      t(std::string("")), t(absl::string_view("")),
      t(std::string(small)), t(absl::string_view(small)),
      t(std::string(dup)), t(absl::string_view(dup)),
      t(std::string(large)), t(absl::string_view(large)),
      t(std::string(huge)), t(absl::string_view(huge)))));

  // Make sure that hashing a `const char*` does not use its std::string-value.
  EXPECT_NE(SpyHash(static_cast<const char*>("ABC")),
            SpyHash(absl::string_view("ABC")));
}

// TODO(EricWF): MSVC 15 has a bug that causes it to incorrectly evaluate the
// SFINAE in internal/hash.h, causing this test to fail.
#if !defined(_MSC_VER)
TEST(HashValueTest, StdArray) {
  EXPECT_TRUE((is_hashable<std::array<int, 3>>::value));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      std::make_tuple(std::array<int, 3>{}, std::array<int, 3>{{0, 23, 42}})));
}
#endif  // !defined(_MSC_VER)

TEST(HashValueTest, StdBitset) {
  EXPECT_TRUE((is_hashable<std::bitset<257>>::value));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {std::bitset<2>("00"), std::bitset<2>("01"), std::bitset<2>("10"),
       std::bitset<2>("11")}));
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {std::bitset<5>("10101"), std::bitset<5>("10001"), std::bitset<5>()}));

  constexpr int kNumBits = 256;
  std::array<std::string, 6> bit_strings;
  bit_strings.fill(std::string(kNumBits, '1'));
  bit_strings[1][0] = '0';
  bit_strings[2][1] = '0';
  bit_strings[3][kNumBits / 3] = '0';
  bit_strings[4][kNumBits - 2] = '0';
  bit_strings[5][kNumBits - 1] = '0';
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {std::bitset<kNumBits>(bit_strings[0].c_str()),
       std::bitset<kNumBits>(bit_strings[1].c_str()),
       std::bitset<kNumBits>(bit_strings[2].c_str()),
       std::bitset<kNumBits>(bit_strings[3].c_str()),
       std::bitset<kNumBits>(bit_strings[4].c_str()),
       std::bitset<kNumBits>(bit_strings[5].c_str())}));
}  // namespace

template <typename T>
class HashValueSequenceTest : public testing::Test {
};
TYPED_TEST_SUITE_P(HashValueSequenceTest);

TYPED_TEST_P(HashValueSequenceTest, BasicUsage) {
  EXPECT_TRUE((is_hashable<TypeParam>::value));

  using ValueType = typename TypeParam::value_type;
  auto a = static_cast<ValueType>(0);
  auto b = static_cast<ValueType>(23);
  auto c = static_cast<ValueType>(42);

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      std::make_tuple(TypeParam(), TypeParam{}, TypeParam{a, b, c},
                      TypeParam{a, b}, TypeParam{b, c})));
}

REGISTER_TYPED_TEST_CASE_P(HashValueSequenceTest, BasicUsage);
using IntSequenceTypes =
    testing::Types<std::deque<int>, std::forward_list<int>, std::list<int>,
                   std::vector<int>, std::vector<bool>, std::set<int>,
                   std::multiset<int>>;
INSTANTIATE_TYPED_TEST_CASE_P(My, HashValueSequenceTest, IntSequenceTypes);

// Private type that only supports AbslHashValue to make sure our chosen hash
// implentation is recursive within absl::Hash.
// It uses std::abs() on the value to provide different bitwise representations
// of the same logical value.
struct Private {
  int i;
  template <typename H>
  friend H AbslHashValue(H h, Private p) {
    return H::combine(std::move(h), std::abs(p.i));
  }

  friend bool operator==(Private a, Private b) {
    return std::abs(a.i) == std::abs(b.i);
  }

  friend std::ostream& operator<<(std::ostream& o, Private p) {
    return o << p.i;
  }
};

TEST(HashValueTest, PrivateSanity) {
  // Sanity check that Private is working as the tests below expect it to work.
  EXPECT_TRUE(is_hashable<Private>::value);
  EXPECT_NE(SpyHash(Private{0}), SpyHash(Private{1}));
  EXPECT_EQ(SpyHash(Private{1}), SpyHash(Private{1}));
}

TEST(HashValueTest, Optional) {
  EXPECT_TRUE(is_hashable<absl::optional<Private>>::value);

  using O = absl::optional<Private>;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      std::make_tuple(O{}, O{{1}}, O{{-1}}, O{{10}})));
}

TEST(HashValueTest, Variant) {
  using V = absl::variant<Private, std::string>;
  EXPECT_TRUE(is_hashable<V>::value);

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      V(Private{1}), V(Private{-1}), V(Private{2}), V("ABC"), V("BCD"))));

#if ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_
  struct S {};
  EXPECT_FALSE(is_hashable<absl::variant<S>>::value);
#endif
}

// TODO(EricWF): MSVC 15 has a bug that causes it to incorrectly evaluate the
// SFINAE in internal/hash.h, causing this test to fail.
#if !defined(_MSC_VER)
TEST(HashValueTest, Maps) {
  EXPECT_TRUE((is_hashable<std::map<int, std::string>>::value));

  using M = std::map<int, std::string>;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      M{}, M{{0, "foo"}}, M{{1, "foo"}}, M{{0, "bar"}}, M{{1, "bar"}},
      M{{0, "foo"}, {42, "bar"}}, M{{1, "foo"}, {42, "bar"}},
      M{{1, "foo"}, {43, "bar"}}, M{{1, "foo"}, {43, "baz"}})));

  using MM = std::multimap<int, std::string>;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(std::make_tuple(
      MM{}, MM{{0, "foo"}}, MM{{1, "foo"}}, MM{{0, "bar"}}, MM{{1, "bar"}},
      MM{{0, "foo"}, {0, "bar"}}, MM{{0, "bar"}, {0, "foo"}},
      MM{{0, "foo"}, {42, "bar"}}, MM{{1, "foo"}, {42, "bar"}},
      MM{{1, "foo"}, {1, "foo"}, {43, "bar"}}, MM{{1, "foo"}, {43, "baz"}})));
}
#endif  // !defined(_MSC_VER)

template <typename T, typename = void>
struct IsHashCallble : std::false_type {};

template <typename T>
struct IsHashCallble<T, absl::void_t<decltype(std::declval<absl::Hash<T>>()(
                            std::declval<const T&>()))>> : std::true_type {};

template <typename T, typename = void>
struct IsAggregateInitializable : std::false_type {};

template <typename T>
struct IsAggregateInitializable<T, absl::void_t<decltype(T{})>>
    : std::true_type {};

TEST(IsHashableTest, ValidHash) {
  EXPECT_TRUE((is_hashable<int>::value));
  EXPECT_TRUE(std::is_default_constructible<absl::Hash<int>>::value);
  EXPECT_TRUE(std::is_copy_constructible<absl::Hash<int>>::value);
  EXPECT_TRUE(std::is_move_constructible<absl::Hash<int>>::value);
  EXPECT_TRUE(absl::is_copy_assignable<absl::Hash<int>>::value);
  EXPECT_TRUE(absl::is_move_assignable<absl::Hash<int>>::value);
  EXPECT_TRUE(IsHashCallble<int>::value);
  EXPECT_TRUE(IsAggregateInitializable<absl::Hash<int>>::value);
}

#if ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_
TEST(IsHashableTest, PoisonHash) {
  struct X {};
  EXPECT_FALSE((is_hashable<X>::value));
  EXPECT_FALSE(std::is_default_constructible<absl::Hash<X>>::value);
  EXPECT_FALSE(std::is_copy_constructible<absl::Hash<X>>::value);
  EXPECT_FALSE(std::is_move_constructible<absl::Hash<X>>::value);
  EXPECT_FALSE(absl::is_copy_assignable<absl::Hash<X>>::value);
  EXPECT_FALSE(absl::is_move_assignable<absl::Hash<X>>::value);
  EXPECT_FALSE(IsHashCallble<X>::value);
  EXPECT_FALSE(IsAggregateInitializable<absl::Hash<X>>::value);
}
#endif  // ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_

// Hashable types
//
// These types exist simply to exercise various AbslHashValue behaviors, so
// they are named by what their AbslHashValue overload does.
struct NoOp {
  template <typename HashCode>
  friend HashCode AbslHashValue(HashCode h, NoOp n) {
    return std::move(h);
  }
};

struct EmptyCombine {
  template <typename HashCode>
  friend HashCode AbslHashValue(HashCode h, EmptyCombine e) {
    return HashCode::combine(std::move(h));
  }
};

template <typename Int>
struct CombineIterative {
  template <typename HashCode>
  friend HashCode AbslHashValue(HashCode h, CombineIterative c) {
    for (int i = 0; i < 5; ++i) {
      h = HashCode::combine(std::move(h), Int(i));
    }
    return h;
  }
};

template <typename Int>
struct CombineVariadic {
  template <typename HashCode>
  friend HashCode AbslHashValue(HashCode h, CombineVariadic c) {
    return HashCode::combine(std::move(h), Int(0), Int(1), Int(2), Int(3),
                             Int(4));
  }
};

using InvokeTag = absl::hash_internal::InvokeHashTag;
template <InvokeTag T>
using InvokeTagConstant = std::integral_constant<InvokeTag, T>;

template <InvokeTag... Tags>
struct MinTag;

template <InvokeTag a, InvokeTag b, InvokeTag... Tags>
struct MinTag<a, b, Tags...> : MinTag<(a < b ? a : b), Tags...> {};

template <InvokeTag a>
struct MinTag<a> : InvokeTagConstant<a> {};

template <InvokeTag... Tags>
struct CustomHashType {
  size_t value;
};

template <InvokeTag allowed, InvokeTag... tags>
struct EnableIfContained
    : std::enable_if<absl::disjunction<
          std::integral_constant<bool, allowed == tags>...>::value> {};

template <
    typename H, InvokeTag... Tags,
    typename = typename EnableIfContained<InvokeTag::kHashValue, Tags...>::type>
H AbslHashValue(H state, CustomHashType<Tags...> t) {
  static_assert(MinTag<Tags...>::value == InvokeTag::kHashValue, "");
  return H::combine(std::move(state),
                    t.value + static_cast<int>(InvokeTag::kHashValue));
}

}  // namespace

namespace absl {
namespace hash_internal {
template <InvokeTag... Tags>
struct is_uniquely_represented<
    CustomHashType<Tags...>,
    typename EnableIfContained<InvokeTag::kUniquelyRepresented, Tags...>::type>
    : std::true_type {};
}  // namespace hash_internal
}  // namespace absl

#if ABSL_HASH_INTERNAL_SUPPORT_LEGACY_HASH_
namespace ABSL_INTERNAL_LEGACY_HASH_NAMESPACE {
template <InvokeTag... Tags>
struct hash<CustomHashType<Tags...>> {
  template <InvokeTag... TagsIn, typename = typename EnableIfContained<
                                     InvokeTag::kLegacyHash, TagsIn...>::type>
  size_t operator()(CustomHashType<TagsIn...> t) const {
    static_assert(MinTag<Tags...>::value == InvokeTag::kLegacyHash, "");
    return t.value + static_cast<int>(InvokeTag::kLegacyHash);
  }
};
}  // namespace ABSL_INTERNAL_LEGACY_HASH_NAMESPACE
#endif  // ABSL_HASH_INTERNAL_SUPPORT_LEGACY_HASH_

namespace std {
template <InvokeTag... Tags>  // NOLINT
struct hash<CustomHashType<Tags...>> {
  template <InvokeTag... TagsIn, typename = typename EnableIfContained<
                                     InvokeTag::kStdHash, TagsIn...>::type>
  size_t operator()(CustomHashType<TagsIn...> t) const {
    static_assert(MinTag<Tags...>::value == InvokeTag::kStdHash, "");
    return t.value + static_cast<int>(InvokeTag::kStdHash);
  }
};
}  // namespace std

namespace {

template <typename... T>
void TestCustomHashType(InvokeTagConstant<InvokeTag::kNone>, T...) {
  using type = CustomHashType<T::value...>;
  SCOPED_TRACE(testing::PrintToString(std::vector<InvokeTag>{T::value...}));
  EXPECT_TRUE(is_hashable<type>());
  EXPECT_TRUE(is_hashable<const type>());
  EXPECT_TRUE(is_hashable<const type&>());

  const size_t offset = static_cast<int>(std::min({T::value...}));
  EXPECT_EQ(SpyHash(type{7}), SpyHash(size_t{7 + offset}));
}

void TestCustomHashType(InvokeTagConstant<InvokeTag::kNone>) {
#if ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_
  // is_hashable is false if we don't support any of the hooks.
  using type = CustomHashType<>;
  EXPECT_FALSE(is_hashable<type>());
  EXPECT_FALSE(is_hashable<const type>());
  EXPECT_FALSE(is_hashable<const type&>());
#endif  // ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_
}

template <InvokeTag Tag, typename... T>
void TestCustomHashType(InvokeTagConstant<Tag> tag, T... t) {
  constexpr auto next = static_cast<InvokeTag>(static_cast<int>(Tag) + 1);
  TestCustomHashType(InvokeTagConstant<next>(), tag, t...);
  TestCustomHashType(InvokeTagConstant<next>(), t...);
}

TEST(HashTest, CustomHashType) {
  TestCustomHashType(InvokeTagConstant<InvokeTag{}>());
}

TEST(HashTest, NoOpsAreEquivalent) {
  EXPECT_EQ(Hash<NoOp>()({}), Hash<NoOp>()({}));
  EXPECT_EQ(Hash<NoOp>()({}), Hash<EmptyCombine>()({}));
}

template <typename T>
class HashIntTest : public testing::Test {
};
TYPED_TEST_SUITE_P(HashIntTest);

TYPED_TEST_P(HashIntTest, BasicUsage) {
  EXPECT_NE(Hash<NoOp>()({}), Hash<TypeParam>()(0));
  EXPECT_NE(Hash<NoOp>()({}),
            Hash<TypeParam>()(std::numeric_limits<TypeParam>::max()));
  if (std::numeric_limits<TypeParam>::min() != 0) {
    EXPECT_NE(Hash<NoOp>()({}),
              Hash<TypeParam>()(std::numeric_limits<TypeParam>::min()));
  }

  EXPECT_EQ(Hash<CombineIterative<TypeParam>>()({}),
            Hash<CombineVariadic<TypeParam>>()({}));
}

REGISTER_TYPED_TEST_CASE_P(HashIntTest, BasicUsage);
using IntTypes = testing::Types<unsigned char, char, int, int32_t, int64_t, uint32_t,
                                uint64_t, size_t>;
INSTANTIATE_TYPED_TEST_CASE_P(My, HashIntTest, IntTypes);

struct StructWithPadding {
  char c;
  int i;

  template <typename H>
  friend H AbslHashValue(H hash_state, const StructWithPadding& s) {
    return H::combine(std::move(hash_state), s.c, s.i);
  }
};

static_assert(sizeof(StructWithPadding) > sizeof(char) + sizeof(int),
              "StructWithPadding doesn't have padding");
static_assert(std::is_standard_layout<StructWithPadding>::value, "");

// This check has to be disabled because libstdc++ doesn't support it.
// static_assert(std::is_trivially_constructible<StructWithPadding>::value, "");

template <typename T>
struct ArraySlice {
  T* begin;
  T* end;

  template <typename H>
  friend H AbslHashValue(H hash_state, const ArraySlice& slice) {
    for (auto t = slice.begin; t != slice.end; ++t) {
      hash_state = H::combine(std::move(hash_state), *t);
    }
    return hash_state;
  }
};

TEST(HashTest, HashNonUniquelyRepresentedType) {
  // Create equal StructWithPadding objects that are known to have non-equal
  // padding bytes.
  static const size_t kNumStructs = 10;
  unsigned char buffer1[kNumStructs * sizeof(StructWithPadding)];
  std::memset(buffer1, 0, sizeof(buffer1));
  auto* s1 = reinterpret_cast<StructWithPadding*>(buffer1);

  unsigned char buffer2[kNumStructs * sizeof(StructWithPadding)];
  std::memset(buffer2, 255, sizeof(buffer2));
  auto* s2 = reinterpret_cast<StructWithPadding*>(buffer2);
  for (int i = 0; i < kNumStructs; ++i) {
    SCOPED_TRACE(i);
    s1[i].c = s2[i].c = '0' + i;
    s1[i].i = s2[i].i = i;
    ASSERT_FALSE(memcmp(buffer1 + i * sizeof(StructWithPadding),
                        buffer2 + i * sizeof(StructWithPadding),
                        sizeof(StructWithPadding)) == 0)
        << "Bug in test code: objects do not have unequal"
        << " object representations";
  }

  EXPECT_EQ(Hash<StructWithPadding>()(s1[0]), Hash<StructWithPadding>()(s2[0]));
  EXPECT_EQ(Hash<ArraySlice<StructWithPadding>>()({s1, s1 + kNumStructs}),
            Hash<ArraySlice<StructWithPadding>>()({s2, s2 + kNumStructs}));
}

TEST(HashTest, StandardHashContainerUsage) {
  std::unordered_map<int, std::string, Hash<int>> map = {{0, "foo"}, { 42, "bar" }};

  EXPECT_NE(map.find(0), map.end());
  EXPECT_EQ(map.find(1), map.end());
  EXPECT_NE(map.find(0u), map.end());
}

struct ConvertibleFromNoOp {
  ConvertibleFromNoOp(NoOp) {}  // NOLINT(runtime/explicit)

  template <typename H>
  friend H AbslHashValue(H hash_state, ConvertibleFromNoOp) {
    return H::combine(std::move(hash_state), 1);
  }
};

TEST(HashTest, HeterogeneousCall) {
  EXPECT_NE(Hash<ConvertibleFromNoOp>()(NoOp()),
            Hash<NoOp>()(NoOp()));
}

TEST(IsUniquelyRepresentedTest, SanityTest) {
  using absl::hash_internal::is_uniquely_represented;

  EXPECT_TRUE(is_uniquely_represented<unsigned char>::value);
  EXPECT_TRUE(is_uniquely_represented<int>::value);
  EXPECT_FALSE(is_uniquely_represented<bool>::value);
  EXPECT_FALSE(is_uniquely_represented<int*>::value);
}

struct IntAndString {
  int i;
  std::string s;

  template <typename H>
  friend H AbslHashValue(H hash_state, IntAndString int_and_string) {
    return H::combine(std::move(hash_state), int_and_string.s,
                      int_and_string.i);
  }
};

TEST(HashTest, SmallValueOn64ByteBoundary) {
  Hash<IntAndString>()(IntAndString{0, std::string(63, '0')});
}

struct TypeErased {
  size_t n;

  template <typename H>
  friend H AbslHashValue(H hash_state, const TypeErased& v) {
    v.HashValue(absl::HashState::Create(&hash_state));
    return hash_state;
  }

  void HashValue(absl::HashState state) const {
    absl::HashState::combine(std::move(state), n);
  }
};

TEST(HashTest, TypeErased) {
  EXPECT_TRUE((is_hashable<TypeErased>::value));
  EXPECT_TRUE((is_hashable<std::pair<TypeErased, int>>::value));

  EXPECT_EQ(SpyHash(TypeErased{7}), SpyHash(size_t{7}));
  EXPECT_NE(SpyHash(TypeErased{7}), SpyHash(size_t{13}));

  EXPECT_EQ(SpyHash(std::make_pair(TypeErased{7}, 17)),
            SpyHash(std::make_pair(size_t{7}, 17)));
}

}  // namespace

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/expected.h"

#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/types/strong_alias.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

template <typename T>
struct Strong {
  constexpr explicit Strong(T value) : value(std::move(value)) {}
  T value;
};

template <typename T>
struct Weak {
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Weak(T value) : value(std::move(value)) {}
  T value;
};

template <typename T>
struct StrongMoveOnly {
  constexpr explicit StrongMoveOnly(T&& value) : value(std::move(value)) {}
  constexpr StrongMoveOnly(StrongMoveOnly&& other)
      : value(std::exchange(other.value, {})) {}

  constexpr StrongMoveOnly& operator=(StrongMoveOnly&& other) {
    value = std::exchange(other.value, {});
    return *this;
  }

  T value;
};

template <typename T>
struct WeakMoveOnly {
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr WeakMoveOnly(T&& value) : value(std::move(value)) {}
  constexpr WeakMoveOnly(WeakMoveOnly&& other)
      : value(std::exchange(other.value, {})) {}
  T value;
};

TEST(Unexpected, ValueConstructor) {
  constexpr unexpected<int> unex(42);
  static_assert(unex.value() == 42);
}

TEST(Unexpected, DefaultConstructor) {
  constexpr unexpected<int> unex(absl::in_place);
  static_assert(unex.value() == 0);
}

TEST(Unexpected, InPlaceConstructor) {
  constexpr unexpected<std::pair<int, double>> unex(absl::in_place, 42, 3.14);
  static_assert(unex.value() == std::pair(42, 3.14));
}

TEST(Unexpected, InPlaceListConstructor) {
  unexpected<std::vector<int>> unex(absl::in_place, {1, 2, 3});
  EXPECT_EQ(unex.value(), std::vector({1, 2, 3}));
}

TEST(Unexpected, ValueIsQualified) {
  using Unex = unexpected<int>;
  static_assert(std::is_same_v<decltype(std::declval<Unex&>().value()), int&>);
  static_assert(std::is_same_v<decltype(std::declval<const Unex&>().value()),
                               const int&>);
  static_assert(std::is_same_v<decltype(std::declval<Unex>().value()), int&&>);
  static_assert(std::is_same_v<decltype(std::declval<const Unex>().value()),
                               const int&&>);
}

TEST(Unexpected, MemberSwap) {
  unexpected u1(42);
  unexpected u2(123);
  u1.swap(u2);

  EXPECT_EQ(u1.value(), 123);
  EXPECT_EQ(u2.value(), 42);
}

TEST(Unexpected, EqualityOperators) {
  static_assert(unexpected(42) == unexpected(42.0));
  static_assert(unexpected(42) != unexpected(43));
}

TEST(Unexpected, FreeSwap) {
  unexpected u1(42);
  unexpected u2(123);
  swap(u1, u2);

  EXPECT_EQ(u1.value(), 123);
  EXPECT_EQ(u2.value(), 42);
}

TEST(Expected, Triviality) {
  using TrivialExpected = expected<int, int>;
  static_assert(std::is_trivially_destructible_v<TrivialExpected>);

  using NonTrivialExpected = expected<int, std::string>;
  static_assert(!std::is_trivially_destructible_v<NonTrivialExpected>);
}

TEST(Expected, DefaultConstructor) {
  constexpr expected<int, int> ex;
  static_assert(ex.has_value());
  EXPECT_EQ(ex.value(), 0);

  static_assert(std::is_default_constructible_v<expected<int, int>>);
  static_assert(!std::is_default_constructible_v<expected<Strong<int>, int>>);
}

TEST(Expected, CopyConstructor) {
  {
    constexpr expected<int, int> ex1 = 42;
    constexpr expected<int, int> ex2 = ex1;
    static_assert(ex2.has_value());
    // Note: In theory this could be constexpr, but is currently not due to
    // implementation details of absl::get [1].
    // TODO: Make this a static_assert once this is fixed in Abseil, or we use
    // std::variant. Similarly in the tests below.
    // [1] https://github.com/abseil/abseil-cpp/blob/50739/absl/types/internal/variant.h#L548
    EXPECT_EQ(ex2.value(), 42);
  }

  {
    constexpr expected<int, int> ex1 = unexpected(42);
    constexpr expected<int, int> ex2 = ex1;
    static_assert(!ex2.has_value());
    EXPECT_EQ(ex2.error(), 42);
  }
}

TEST(Expected, MoveConstructor) {
  {
    expected<StrongMoveOnly<int>, int> ex1 = StrongMoveOnly(42);
    expected<StrongMoveOnly<int>, int> ex2 = std::move(ex1);
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(ex2.value().value, 42);
  }

  {
    expected<int, StrongMoveOnly<int>> ex1 = unexpected(StrongMoveOnly(42));
    expected<int, StrongMoveOnly<int>> ex2 = std::move(ex1);
    ASSERT_FALSE(ex2.has_value());
    EXPECT_EQ(ex2.error().value, 42);
  }
}

TEST(Expected, ExplicitConvertingCopyConstructor) {
  {
    expected<int, int> ex1 = 42;
    expected<Strong<int>, int> ex2(ex1);
    static_assert(!std::is_convertible_v<decltype(ex1), decltype(ex2)>);
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(ex2.value().value, 42);
  }

  {
    expected<int, int> ex1 = unexpected(42);
    expected<int, Strong<int>> ex2(ex1);
    static_assert(!std::is_convertible_v<decltype(ex1), decltype(ex2)>);
    ASSERT_FALSE(ex2.has_value());
    EXPECT_EQ(ex2.error().value, 42);
  }
}

TEST(Expected, ImplicitConvertingCopyConstructor) {
  {
    expected<int, int> ex1 = 42;
    expected<Weak<int>, Weak<int>> ex2 = ex1;
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(ex2.value().value, 42);
  }
  {
    expected<int, int> ex1 = unexpected(42);
    expected<Weak<int>, Weak<int>> ex2 = ex1;
    ASSERT_FALSE(ex2.has_value());
    EXPECT_EQ(ex2.error().value, 42);
  }
}

TEST(Expected, ExplicitConvertingMoveConstructor) {
  {
    expected<int, int> ex1 = 42;
    expected<StrongMoveOnly<int>, int> ex2(std::move(ex1));
    static_assert(
        !std::is_convertible_v<decltype(std::move(ex1)), decltype(ex2)>);
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(ex2.value().value, 42);
  }

  {
    expected<int, int> ex1 = unexpected(42);
    expected<int, StrongMoveOnly<int>> ex2(std::move(ex1));
    static_assert(
        !std::is_convertible_v<decltype(std::move(ex1)), decltype(ex2)>);
    ASSERT_FALSE(ex2.has_value());
    EXPECT_EQ(ex2.error().value, 42);
  }
}

TEST(Expected, ImplicitConvertingMoveConstructor) {
  {
    expected<int, int> ex1 = 42;
    expected<WeakMoveOnly<int>, int> ex2 = std::move(ex1);
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(ex2.value().value, 42);
  }

  {
    expected<int, int> ex1 = unexpected(42);
    expected<int, WeakMoveOnly<int>> ex2 = std::move(ex1);
    ASSERT_FALSE(ex2.has_value());
    EXPECT_EQ(ex2.error().value, 42);
  }
}

TEST(Expected, ExplicitValueConstructor) {
  {
    constexpr expected<Strong<int>, int> ex(42);
    static_assert(!std::is_convertible_v<int, decltype(ex)>);
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }

  {
    constexpr expected<StrongMoveOnly<int>, int> ex(42);
    static_assert(!std::is_constructible_v<decltype(ex), int&>);
    static_assert(!std::is_convertible_v<int, decltype(ex)>);
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }
}

TEST(Expected, ImplicitValueConstructor) {
  {
    constexpr expected<Weak<int>, int> ex = 42;
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }

  {
    constexpr expected<WeakMoveOnly<int>, int> ex = 42;
    static_assert(!std::is_convertible_v<int&, decltype(ex)>);
    static_assert(ex.has_value());
    EXPECT_EQ(ex.value().value, 42);
  }
}

TEST(Expected, ExplicitErrorConstructor) {
  {
    constexpr expected<int, Strong<int>> ex(unexpected(42));
    static_assert(!std::is_convertible_v<unexpected<int>, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }

  {
    constexpr expected<int, StrongMoveOnly<int>> ex(unexpected(42));
    static_assert(!std::is_constructible_v<decltype(ex), unexpected<int>&>);
    static_assert(!std::is_convertible_v<unexpected<int>, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }
}

TEST(Expected, ImplicitErrorConstructor) {
  {
    constexpr expected<int, Weak<int>> ex = unexpected(42);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }

  {
    constexpr expected<int, WeakMoveOnly<int>> ex = unexpected(42);
    static_assert(!std::is_convertible_v<unexpected<int>&, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }
}

TEST(Expected, InPlaceConstructor) {
  constexpr expected<Strong<int>, int> ex(absl::in_place, 42);
  static_assert(ex.has_value());
  EXPECT_EQ(ex.value().value, 42);
}

TEST(Expected, InPlaceListConstructor) {
  expected<std::vector<int>, int> ex(absl::in_place, {1, 2, 3});
  ASSERT_TRUE(ex.has_value());
  EXPECT_EQ(ex.value(), std::vector({1, 2, 3}));
}

TEST(Expected, UnexpectConstructor) {
  constexpr expected<int, Strong<int>> ex(unexpect, 42);
  static_assert(!ex.has_value());
  EXPECT_EQ(ex.error().value, 42);
}

TEST(Expected, UnexpectListConstructor) {
  expected<int, std::vector<int>> ex(unexpect, {1, 2, 3});
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error(), std::vector({1, 2, 3}));
}

TEST(Expected, AssignValue) {
  expected<int, int> ex = unexpected(0);
  EXPECT_FALSE(ex.has_value());

  ex = 42;
  ASSERT_TRUE(ex.has_value());
  EXPECT_EQ(ex.value(), 42);

  ex = 123;
  ASSERT_TRUE(ex.has_value());
  EXPECT_EQ(ex.value(), 123);
}

TEST(Expected, CopyAssignUnexpected) {
  expected<int, int> ex;
  EXPECT_TRUE(ex.has_value());

  ex = unexpected(42);
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error(), 42);

  ex = unexpected(123);
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error(), 123);
}

TEST(Expected, MoveAssignUnexpected) {
  expected<int, StrongMoveOnly<int>> ex;
  EXPECT_TRUE(ex.has_value());

  ex = unexpected(StrongMoveOnly(42));
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error().value, 42);

  ex = unexpected(StrongMoveOnly(123));
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error().value, 123);
}

TEST(Expected, Emplace) {
  expected<StrongMoveOnly<int>, int> ex = unexpected(0);
  EXPECT_FALSE(ex.has_value());

  ex.emplace(42);
  ASSERT_TRUE(ex.has_value());
  EXPECT_EQ(ex.value().value, 42);
}

TEST(Expected, EmplaceList) {
  expected<std::vector<int>, int> ex = unexpected(0);
  EXPECT_FALSE(ex.has_value());

  ex.emplace({1, 2, 3});
  ASSERT_TRUE(ex.has_value());
  EXPECT_EQ(ex.value(), std::vector({1, 2, 3}));
}

TEST(Expected, MemberSwap) {
  expected<int, int> ex1 = 42;
  expected<int, int> ex2 = unexpected(123);

  ex1.swap(ex2);
  ASSERT_FALSE(ex1.has_value());
  EXPECT_EQ(ex1.error(), 123);

  ASSERT_TRUE(ex2.has_value());
  EXPECT_EQ(ex2.value(), 42);
}

TEST(Expected, FreeSwap) {
  expected<int, int> ex1 = 42;
  expected<int, int> ex2 = unexpected(123);

  swap(ex1, ex2);
  ASSERT_FALSE(ex1.has_value());
  EXPECT_EQ(ex1.error(), 123);

  ASSERT_TRUE(ex2.has_value());
  EXPECT_EQ(ex2.value(), 42);
}

TEST(Expected, OperatorArrow) {
  expected<Strong<int>, int> ex(0);
  EXPECT_EQ(ex->value, 0);

  ex->value = 1;
  EXPECT_EQ(ex->value, 1);

  constexpr expected<Strong<int>, int> c_ex(0);
  EXPECT_EQ(c_ex->value, 0);
  static_assert(std::is_same_v<decltype((c_ex->value)), const int&>);
}

TEST(Expected, OperatorStar) {
  expected<int, int> ex;
  EXPECT_EQ(*ex, 0);

  *ex = 1;
  EXPECT_EQ(*ex, 1);

  using Ex = expected<int, int>;
  static_assert(std::is_same_v<decltype(*std::declval<Ex&>()), int&>);
  static_assert(
      std::is_same_v<decltype(*std::declval<const Ex&>()), const int&>);
  static_assert(std::is_same_v<decltype(*std::declval<Ex&&>()), int&&>);
  static_assert(
      std::is_same_v<decltype(*std::declval<const Ex&&>()), const int&&>);
}

TEST(Expected, HasValue) {
  constexpr expected<int, int> ex;
  static_assert(ex.has_value());

  constexpr expected<int, int> unex = unexpected(0);
  static_assert(!unex.has_value());
}

TEST(Expected, Value) {
  expected<int, int> ex;
  EXPECT_EQ(ex.value(), 0);

  ex.value() = 1;
  EXPECT_EQ(ex.value(), 1);

  using Ex = expected<int, int>;
  static_assert(std::is_same_v<decltype(std::declval<Ex&>().value()), int&>);
  static_assert(
      std::is_same_v<decltype(std::declval<const Ex&>().value()), const int&>);
  static_assert(std::is_same_v<decltype(std::declval<Ex&&>().value()), int&&>);
  static_assert(std::is_same_v<decltype(std::declval<const Ex&&>().value()),
                               const int&&>);
}

TEST(Expected, Error) {
  expected<int, int> ex = unexpected(0);
  EXPECT_EQ(ex.error(), 0);

  ex.error() = 1;
  EXPECT_EQ(ex.error(), 1);

  using Ex = expected<int, int>;
  static_assert(std::is_same_v<decltype(std::declval<Ex&>().error()), int&>);
  static_assert(
      std::is_same_v<decltype(std::declval<const Ex&>().error()), const int&>);
  static_assert(std::is_same_v<decltype(std::declval<Ex&&>().error()), int&&>);
  static_assert(std::is_same_v<decltype(std::declval<const Ex&&>().error()),
                               const int&&>);
}

TEST(Expected, ValueOr) {
  {
    expected<int, int> ex;
    EXPECT_EQ(ex.value_or(123), 0);

    expected<int, int> unex = unexpected(0);
    EXPECT_EQ(unex.value_or(123), 123);
  }

  {
    expected<WeakMoveOnly<int>, int> ex(0);
    EXPECT_EQ(std::move(ex).value_or(123).value, 0);

    expected<int, WeakMoveOnly<int>> unex = unexpected(WeakMoveOnly(0));
    EXPECT_EQ(std::move(unex).value_or(123), 123);
  }
}

TEST(Expected, EqualityOperators) {
  using ExInt = expected<int, int>;
  using ExLong = expected<long, long>;

  EXPECT_EQ(ExInt(42), ExLong(42));
  EXPECT_EQ(ExLong(42), ExInt(42));
  EXPECT_EQ(ExInt(42), 42);
  EXPECT_EQ(42, ExInt(42));
  EXPECT_EQ(ExInt(unexpect, 42), unexpected(42));
  EXPECT_EQ(unexpected(42), ExInt(unexpect, 42));

  EXPECT_NE(ExInt(42), ExLong(123));
  EXPECT_NE(ExLong(123), ExInt(42));
  EXPECT_NE(ExInt(42), 123);
  EXPECT_NE(123, ExInt(42));
  EXPECT_NE(ExInt(unexpect, 123), unexpected(42));
  EXPECT_NE(unexpected(42), ExInt(unexpect, 123));
  EXPECT_NE(ExInt(123), unexpected(123));
  EXPECT_NE(unexpected(123), ExInt(123));
}

TEST(ExpectedTest, DeathTests) {
  using ExpectedInt = expected<int, int>;
  using ExpectedDouble = expected<double, double>;

  ExpectedInt moved_from;
  ExpectedInt ex = std::move(moved_from);

  // Accessing moved from objects crashes.
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{moved_from}, "");
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{std::move(moved_from)}, "");
  EXPECT_DEATH_IF_SUPPORTED(ExpectedDouble{moved_from}, "");
  EXPECT_DEATH_IF_SUPPORTED(ExpectedDouble{std::move(moved_from)}, "");
  EXPECT_DEATH_IF_SUPPORTED(ex = moved_from, "");
  EXPECT_DEATH_IF_SUPPORTED(ex = std::move(moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(ex.swap(moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.swap(ex), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.operator->(), "");
  EXPECT_DEATH_IF_SUPPORTED(*moved_from, "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.has_value(), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.value(), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.error(), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.value_or(0), "");
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = (ex == moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = (moved_from == ex), "");
  // NOLINTEND(bugprone-use-after-move)

  // Accessing inactive union-members crashes.
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{}.error(), "");
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{unexpect}.value(), "");
}

TEST(ExpectedVoid, Triviality) {
  using TrivialExpected = expected<void, int>;
  static_assert(std::is_trivially_destructible_v<TrivialExpected>);

  using NonTrivialExpected = expected<void, std::string>;
  static_assert(!std::is_trivially_destructible_v<NonTrivialExpected>);
}

TEST(ExpectedVoid, DefaultConstructor) {
  constexpr expected<void, int> ex;
  static_assert(ex.has_value());
  static_assert(std::is_default_constructible_v<expected<void, int>>);
}

TEST(ExpectedVoid, InPlaceConstructor) {
  constexpr expected<void, int> ex(absl::in_place);
  static_assert(ex.has_value());
}

TEST(ExpectedVoid, CopyConstructor) {
  constexpr expected<void, int> ex1 = unexpected(42);
  constexpr expected<void, int> ex2 = ex1;
  static_assert(!ex2.has_value());
  EXPECT_EQ(ex2.error(), 42);
}

TEST(ExpectedVoid, MoveConstructor) {
  expected<void, StrongMoveOnly<int>> ex1 = unexpected(StrongMoveOnly(42));
  expected<void, StrongMoveOnly<int>> ex2 = std::move(ex1);
  ASSERT_FALSE(ex2.has_value());
  EXPECT_EQ(ex2.error().value, 42);
}

TEST(ExpectedVoid, ExplicitConvertingCopyConstructor) {
  constexpr expected<void, int> ex1 = unexpected(42);
  expected<const void, Strong<int>> ex2(ex1);
  static_assert(!std::is_convertible_v<decltype(ex1), decltype(ex2)>);
  ASSERT_FALSE(ex2.has_value());
  EXPECT_EQ(ex2.error().value, 42);
}

TEST(ExpectedVoid, ImplicitConvertingCopyConstructor) {
  constexpr expected<void, int> ex1 = unexpected(42);
  expected<const void, Weak<int>> ex2 = ex1;
  ASSERT_FALSE(ex2.has_value());
  EXPECT_EQ(ex2.error().value, 42);
}

TEST(ExpectedVoid, ExplicitConvertingMoveConstructor) {
  expected<void, int> ex1 = unexpected(42);
  expected<const void, StrongMoveOnly<int>> ex2(std::move(ex1));
  static_assert(
      !std::is_convertible_v<decltype(std::move(ex1)), decltype(ex2)>);
  ASSERT_FALSE(ex2.has_value());
  EXPECT_EQ(ex2.error().value, 42);
}

TEST(ExpectedVoid, ImplicitConvertingMoveConstructor) {
  expected<void, int> ex1 = unexpected(42);
  expected<const void, WeakMoveOnly<int>> ex2 = std::move(ex1);
  ASSERT_FALSE(ex2.has_value());
  EXPECT_EQ(ex2.error().value, 42);
}

TEST(ExpectedVoid, ExplicitErrorConstructor) {
  {
    constexpr expected<void, Strong<int>> ex(unexpected(42));
    static_assert(!std::is_convertible_v<unexpected<int>, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }

  {
    constexpr expected<void, StrongMoveOnly<int>> ex(unexpected(42));
    static_assert(!std::is_constructible_v<decltype(ex), unexpected<int>&>);
    static_assert(!std::is_convertible_v<unexpected<int>, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }
}

TEST(ExpectedVoid, ImplicitErrorConstructor) {
  {
    constexpr expected<void, Weak<int>> ex = unexpected(42);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }

  {
    constexpr expected<void, WeakMoveOnly<int>> ex = unexpected(42);
    static_assert(!std::is_convertible_v<unexpected<int>&, decltype(ex)>);
    static_assert(!ex.has_value());
    EXPECT_EQ(ex.error().value, 42);
  }
}

TEST(ExpectedVoid, UnexpectConstructor) {
  constexpr expected<void, Strong<int>> ex(unexpect, 42);
  static_assert(!ex.has_value());
  EXPECT_EQ(ex.error().value, 42);
}

TEST(ExpectedVoid, UnexpectListConstructor) {
  expected<void, std::vector<int>> ex(unexpect, {1, 2, 3});
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error(), std::vector({1, 2, 3}));
}

TEST(ExpectedVoid, CopyAssignUnexpected) {
  expected<void, int> ex;
  EXPECT_TRUE(ex.has_value());

  ex = unexpected(42);
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error(), 42);

  ex = unexpected(123);
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error(), 123);
}

TEST(ExpectedVoid, MoveAssignUnexpected) {
  expected<void, StrongMoveOnly<int>> ex;
  EXPECT_TRUE(ex.has_value());

  ex = unexpected(StrongMoveOnly(42));
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error().value, 42);

  ex = unexpected(StrongMoveOnly(123));
  ASSERT_FALSE(ex.has_value());
  EXPECT_EQ(ex.error().value, 123);
}

TEST(ExpectedVoid, Emplace) {
  expected<void, int> ex = unexpected(0);
  EXPECT_FALSE(ex.has_value());

  ex.emplace();
  ASSERT_TRUE(ex.has_value());
}

TEST(ExpectedVoid, MemberSwap) {
  expected<void, int> ex1;
  expected<void, int> ex2 = unexpected(123);

  ex1.swap(ex2);
  ASSERT_FALSE(ex1.has_value());
  EXPECT_EQ(ex1.error(), 123);

  ASSERT_TRUE(ex2.has_value());
}

TEST(ExpectedVoid, FreeSwap) {
  expected<void, int> ex1;
  expected<void, int> ex2 = unexpected(123);

  swap(ex1, ex2);
  ASSERT_FALSE(ex1.has_value());
  EXPECT_EQ(ex1.error(), 123);

  ASSERT_TRUE(ex2.has_value());
}

TEST(ExpectedVoid, OperatorStar) {
  expected<void, int> ex;
  *ex;
  static_assert(std::is_void_v<decltype(*ex)>);
}

TEST(ExpectedVoid, HasValue) {
  constexpr expected<void, int> ex;
  static_assert(ex.has_value());

  constexpr expected<void, int> unex = unexpected(0);
  static_assert(!unex.has_value());
}

TEST(ExpectedVoid, Value) {
  expected<void, int> ex;
  ex.value();
  static_assert(std::is_void_v<decltype(ex.value())>);
}

TEST(ExpectedVoid, Error) {
  expected<void, int> ex = unexpected(0);
  EXPECT_EQ(ex.error(), 0);

  ex.error() = 1;
  EXPECT_EQ(ex.error(), 1);

  using Ex = expected<void, int>;
  static_assert(std::is_same_v<decltype(std::declval<Ex&>().error()), int&>);
  static_assert(
      std::is_same_v<decltype(std::declval<const Ex&>().error()), const int&>);
  static_assert(std::is_same_v<decltype(std::declval<Ex&&>().error()), int&&>);
  static_assert(std::is_same_v<decltype(std::declval<const Ex&&>().error()),
                               const int&&>);
}

TEST(ExpectedVoid, EqualityOperators) {
  using Ex = expected<void, int>;
  using ConstEx = expected<const void, const int>;

  EXPECT_EQ(Ex(), ConstEx());
  EXPECT_EQ(ConstEx(), Ex());
  EXPECT_EQ(Ex(unexpect, 42), unexpected(42));
  EXPECT_EQ(unexpected(42), Ex(unexpect, 42));

  EXPECT_NE(Ex(unexpect, 123), unexpected(42));
  EXPECT_NE(unexpected(42), Ex(unexpect, 123));
  EXPECT_NE(Ex(), unexpected(0));
  EXPECT_NE(unexpected(0), Ex());
}

TEST(ExpectedVoidTest, DeathTests) {
  using ExpectedInt = expected<void, int>;
  using ExpectedDouble = expected<void, double>;

  ExpectedInt moved_from;
  ExpectedInt ex = std::move(moved_from);

  // Accessing moved from objects crashes.
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{moved_from}, "");
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{std::move(moved_from)}, "");
  EXPECT_DEATH_IF_SUPPORTED(ExpectedDouble{moved_from}, "");
  EXPECT_DEATH_IF_SUPPORTED(ExpectedDouble{std::move(moved_from)}, "");
  EXPECT_DEATH_IF_SUPPORTED(ex = moved_from, "");
  EXPECT_DEATH_IF_SUPPORTED(ex = std::move(moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(ex.swap(moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.swap(ex), "");
  EXPECT_DEATH_IF_SUPPORTED(*moved_from, "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.has_value(), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.value(), "");
  EXPECT_DEATH_IF_SUPPORTED(moved_from.error(), "");
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = (ex == moved_from), "");
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = (moved_from == ex), "");
  // NOLINTEND(bugprone-use-after-move)

  // Accessing inactive union-members crashes.
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{}.error(), "");
  EXPECT_DEATH_IF_SUPPORTED(ExpectedInt{unexpect}.value(), "");
}

}  // namespace

}  // namespace base

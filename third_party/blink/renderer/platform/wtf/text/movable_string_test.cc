// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/movable_string.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

class MovableStringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MovableStringTable::Instance().SetRendererBackgrounded(false);
  }
};

TEST_F(MovableStringTest, Simple) {
  MovableString movable_abc(String("abc").ReleaseImpl());

  EXPECT_TRUE(MovableString().IsNull());
  EXPECT_FALSE(movable_abc.IsNull());
  EXPECT_TRUE(movable_abc.Is8Bit());
  EXPECT_EQ(3u, movable_abc.length());
  EXPECT_EQ(3u, movable_abc.CharactersSizeInBytes());

  EXPECT_EQ(String("abc"), movable_abc.ToString());
  MovableString copy = movable_abc;
  EXPECT_EQ(copy.Impl(), movable_abc.Impl());
}

TEST_F(MovableStringTest, Park) {
  base::HistogramTester histogram_tester;

  MovableString movable(String("abc").Impl());
  EXPECT_FALSE(movable.Impl()->is_parked());
  EXPECT_TRUE(movable.Impl()->Park());
  EXPECT_TRUE(movable.Impl()->is_parked());

  // Not the only one to have a reference to the string.
  String abc("abc");
  MovableString movable2(abc.Impl());
  EXPECT_FALSE(movable2.Impl()->Park());
  abc = String();
  EXPECT_TRUE(movable2.Impl()->Park());

  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      MovableStringImpl::ParkingAction::kParkedInBackground, 2);
  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 2);
}

TEST_F(MovableStringTest, Unpark) {
  base::HistogramTester histogram_tester;

  MovableString movable(String("abc").Impl());
  auto before_address =
      reinterpret_cast<uintptr_t>(movable.ToString().Characters8());
  EXPECT_FALSE(movable.Impl()->is_parked());
  EXPECT_TRUE(movable.Impl()->Park());
  EXPECT_TRUE(movable.Impl()->is_parked());

  String unparked = movable.ToString();
  auto after_address = reinterpret_cast<uintptr_t>(unparked.Characters8());
#if DCHECK_IS_ON()
  // See comments in Park() and Unpark() for why the addresses are guaranteed to
  // be different.
  EXPECT_NE(before_address, after_address);
#else
  // Strings are not actually moved.
  EXPECT_EQ(before_address, after_address);
#endif
  EXPECT_EQ(String("abc"), unparked);
  EXPECT_FALSE(movable.Impl()->is_parked());

  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 2);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      MovableStringImpl::ParkingAction::kParkedInBackground, 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      MovableStringImpl::ParkingAction::kUnparkedInForeground, 1);
}

TEST_F(MovableStringTest, TableSimple) {
  base::HistogramTester histogram_tester;

  auto& table = MovableStringTable::Instance();
  EXPECT_EQ(0u, table.Size());

  std::vector<char> data(20000, 'a');
  MovableString movable(String(data.data(), data.size()).ReleaseImpl());
  ASSERT_FALSE(movable.Impl()->is_parked());

  EXPECT_EQ(1u, table.Size());

  // Small strings are not in the table.
  MovableString small(String("abc").ReleaseImpl());
  EXPECT_EQ(1u, table.Size());

  // No parking as the current state is not "backgrounded".
  table.SetRendererBackgrounded(false);
  ASSERT_FALSE(table.IsRendererBackgrounded());
  table.MaybeParkAll();
  EXPECT_FALSE(movable.Impl()->is_parked());
  histogram_tester.ExpectTotalCount("Memory.MovableStringsCount", 0);

  table.SetRendererBackgrounded(true);
  ASSERT_TRUE(table.IsRendererBackgrounded());
  table.MaybeParkAll();
  EXPECT_TRUE(movable.Impl()->is_parked());
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 1, 1);

  // Park and unpark.
  movable.ToString();
  EXPECT_FALSE(movable.Impl()->is_parked());
  table.MaybeParkAll();
  EXPECT_TRUE(movable.Impl()->is_parked());
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 1, 2);

  // More than one reference, no parking.
  table.SetRendererBackgrounded(false);
  String alive_unparked = movable.ToString();  // Unparked in foreground.
  table.SetRendererBackgrounded(true);
  table.MaybeParkAll();
  EXPECT_FALSE(movable.Impl()->is_parked());

  // Other reference is dropped, OK to park.
  alive_unparked = String();
  table.MaybeParkAll();
  EXPECT_TRUE(movable.Impl()->is_parked());

  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 5);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      MovableStringImpl::ParkingAction::kParkedInBackground, 3);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      MovableStringImpl::ParkingAction::kUnparkedInBackground, 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      MovableStringImpl::ParkingAction::kUnparkedInForeground, 1);
}

TEST_F(MovableStringTest, TableMultiple) {
  base::HistogramTester histogram_tester;

  size_t size_kb = 20;
  std::vector<char> data(size_kb * 1000, 'a');
  MovableString movable(String(data.data(), data.size()).ReleaseImpl());
  MovableString movable2(String(data.data(), data.size()).ReleaseImpl());

  auto& table = MovableStringTable::Instance();
  EXPECT_EQ(2u, table.Size());

  movable2 = MovableString();
  EXPECT_EQ(1u, table.Size());

  MovableString copy = movable;
  movable = MovableString();
  EXPECT_EQ(1u, table.Size());
  copy = MovableString();
  EXPECT_EQ(0u, table.Size());

  String str(data.data(), data.size());
  MovableString movable3(str.Impl());
  EXPECT_EQ(1u, table.Size());
  // De-duplicated.
  MovableString other_movable3(str.Impl());
  EXPECT_EQ(1u, table.Size());
  EXPECT_EQ(movable3.Impl(), other_movable3.Impl());

  // If all the references to a string are in the table, park it.
  str = String();
  table.SetRendererBackgrounded(true);
  ASSERT_TRUE(table.IsRendererBackgrounded());
  table.MaybeParkAll();
  EXPECT_TRUE(movable3.Impl()->is_parked());

  // Only drop it from the table when the last one is gone.
  movable3 = MovableString();
  EXPECT_EQ(1u, table.Size());
  other_movable3 = MovableString();
  EXPECT_EQ(0u, table.Size());

  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 1, 1);
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsTotalSizeKb",
                                      size_kb, 1);
  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      MovableStringImpl::ParkingAction::kParkedInBackground, 1);
}

TEST_F(MovableStringTest, TableDeduplication) {
  auto& table = MovableStringTable::Instance();
  size_t size_kb = 20;
  std::vector<char> data(size_kb * 1000, 'a');

  String a(data.data(), data.size());
  String b = a;
  {
    MovableString movable_a(a.Impl());
    EXPECT_EQ(1u, table.Size());
    {
      MovableString movable_b(b.Impl());
      // De-duplicated, as a and b share the same |Impl()|.
      EXPECT_EQ(1u, table.Size());
      EXPECT_EQ(movable_a.Impl(), movable_b.Impl());
    }
    // |movable_a| still in scope.
    EXPECT_EQ(1u, table.Size());
  }
  EXPECT_EQ(0u, table.Size());
}

}  // namespace WTF

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ParkableStringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ParkableStringTable::Instance().SetRendererBackgrounded(false);
  }
};

TEST_F(ParkableStringTest, Simple) {
  ParkableString parkable_abc(String("abc").ReleaseImpl());

  EXPECT_TRUE(ParkableString().IsNull());
  EXPECT_FALSE(parkable_abc.IsNull());
  EXPECT_TRUE(parkable_abc.Is8Bit());
  EXPECT_EQ(3u, parkable_abc.length());
  EXPECT_EQ(3u, parkable_abc.CharactersSizeInBytes());

  EXPECT_EQ(String("abc"), parkable_abc.ToString());
  ParkableString copy = parkable_abc;
  EXPECT_EQ(copy.Impl(), parkable_abc.Impl());
}

TEST_F(ParkableStringTest, Park) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(String("abc").Impl());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  EXPECT_TRUE(parkable.Impl()->Park());
  EXPECT_TRUE(parkable.Impl()->is_parked());

  // Not the only one to have a reference to the string.
  String abc("abc");
  ParkableString parkable2(abc.Impl());
  EXPECT_FALSE(parkable2.Impl()->Park());
  abc = String();
  EXPECT_TRUE(parkable2.Impl()->Park());

  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 2);
  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 2);
}

TEST_F(ParkableStringTest, Unpark) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(String("abc").Impl());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  EXPECT_TRUE(parkable.Impl()->Park());
  EXPECT_TRUE(parkable.Impl()->is_parked());

  String unparked = parkable.ToString();
  EXPECT_EQ(String("abc"), unparked);
  EXPECT_FALSE(parkable.Impl()->is_parked());

  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 2);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kUnparkedInForeground, 1);
}

TEST_F(ParkableStringTest, TableSimple) {
  base::HistogramTester histogram_tester;

  std::vector<char> data(20000, 'a');
  ParkableString parkable(String(data.data(), data.size()).ReleaseImpl());
  ASSERT_FALSE(parkable.Impl()->is_parked());

  auto& table = ParkableStringTable::Instance();
  EXPECT_EQ(1u, table.table_.size());

  // Small strings are not in the table.
  ParkableString small(String("abc").ReleaseImpl());
  EXPECT_EQ(1u, table.table_.size());

  // No parking as the current state is not "backgrounded".
  table.SetRendererBackgrounded(false);
  ASSERT_FALSE(table.IsRendererBackgrounded());
  table.MaybeParkAll();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  histogram_tester.ExpectTotalCount("Memory.MovableStringsCount", 0);

  table.SetRendererBackgrounded(true);
  ASSERT_TRUE(table.IsRendererBackgrounded());
  table.MaybeParkAll();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 1, 1);

  // Park and unpark.
  parkable.ToString();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  table.MaybeParkAll();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 1, 2);

  // More than one reference, no parking.
  table.SetRendererBackgrounded(false);
  String alive_unparked = parkable.ToString();  // Unparked in foreground.
  table.SetRendererBackgrounded(true);
  table.MaybeParkAll();
  EXPECT_FALSE(parkable.Impl()->is_parked());

  // Other reference is dropped, OK to park.
  alive_unparked = String();
  table.MaybeParkAll();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 5);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 3);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kUnparkedInBackground, 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kUnparkedInForeground, 1);
}

TEST_F(ParkableStringTest, TableMultiple) {
  base::HistogramTester histogram_tester;

  size_t size_kb = 20;
  std::vector<char> data(size_kb * 1000, 'a');
  ParkableString parkable(String(data.data(), data.size()).ReleaseImpl());
  ParkableString parkable2(String(data.data(), data.size()).ReleaseImpl());

  auto& table = ParkableStringTable::Instance();
  EXPECT_EQ(2u, table.table_.size());

  parkable2 = ParkableString();
  EXPECT_EQ(1u, table.table_.size());

  ParkableString copy = parkable;
  parkable = ParkableString();
  EXPECT_EQ(1u, table.table_.size());
  copy = ParkableString();
  EXPECT_EQ(0u, table.table_.size());

  String str(data.data(), data.size());
  ParkableString parkable3(str.Impl());
  EXPECT_EQ(1u, table.table_.size());
  // De-duplicated.
  ParkableString other_parkable3(str.Impl());
  EXPECT_EQ(1u, table.table_.size());
  EXPECT_EQ(parkable3.Impl(), other_parkable3.Impl());

  // If all the references to a string are in the table, park it.
  str = String();
  table.SetRendererBackgrounded(true);
  ASSERT_TRUE(table.IsRendererBackgrounded());
  table.MaybeParkAll();
  EXPECT_TRUE(parkable3.Impl()->is_parked());

  // Only drop it from the table when the last one is gone.
  parkable3 = ParkableString();
  EXPECT_EQ(1u, table.table_.size());
  other_parkable3 = ParkableString();
  EXPECT_EQ(0u, table.table_.size());

  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 1, 1);
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsTotalSizeKb",
                                      size_kb, 1);
  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 1);
}

}  // namespace blink

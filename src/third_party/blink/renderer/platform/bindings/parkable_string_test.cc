// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <thread>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/memory_coordinator.h"

namespace blink {

namespace {

constexpr size_t kSizeKb = 20;
// Compressed size of the string returned by |MakeLargeString()|.
// Update if the assertion in the |CheckCompressedSize()| test fails.
constexpr size_t kCompressedSize = 55;

String MakeLargeString() {
  std::vector<char> data(kSizeKb * 1000, 'a');
  return String(String(data.data(), data.size()).ReleaseImpl());
}

}  // namespace

class ParkableStringTest : public ::testing::Test {
 public:
  ParkableStringTest()
      : ::testing::Test(),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        scoped_feature_list_() {}

 protected:
  void RunPostedTasks() { scoped_task_environment_.RunUntilIdle(); }

  void WaitForDelayedParking() {
    scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(
        ParkableStringManager::kParkingDelayInSeconds));
    RunPostedTasks();
  }

  void WaitForStatisticsRecording() {
    scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(
        ParkableStringManager::kStatisticsRecordingDelayInSeconds));
    RunPostedTasks();
  }

  bool ParkAndWait(const ParkableString& string) {
    bool return_value =
        string.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways);
    RunPostedTasks();
    return return_value;
  }

  ParkableString CreateAndParkAll() {
    auto& manager = ParkableStringManager::Instance();
    // Checking that there are no other strings, to make sure this doesn't
    // cause side-effects.
    CHECK_EQ(0u, manager.Size());
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    manager.SetRendererBackgrounded(true);
    EXPECT_FALSE(parkable.Impl()->is_parked());
    WaitForDelayedParking();
    EXPECT_TRUE(parkable.Impl()->is_parked());
    return parkable;
  }

  void SetUp() override {
    ParkableStringManager::Instance().ResetForTesting();
    scoped_feature_list_.InitAndEnableFeature(
        kCompressParkableStringsInBackground);
  }

  void TearDown() override {
    // No leaks.
    CHECK_EQ(0u, ParkableStringManager::Instance().Size());
    // Delayed tasks may remain, clear the queues.
    scoped_task_environment_.FastForwardUntilNoTasksRemain();
    RunPostedTasks();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The main aim of this test is to check that the compressed size of a string
// doesn't change. If it does, |kCompressedsize| will need to be updated.
TEST_F(ParkableStringTest, CheckCompressedSize) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
  RunPostedTasks();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  EXPECT_EQ(kCompressedSize, parkable.Impl()->compressed_size());
}

TEST_F(ParkableStringTest, Simple) {
  ParkableString parkable_abc(String("abc").ReleaseImpl());

  EXPECT_TRUE(ParkableString().IsNull());
  EXPECT_FALSE(parkable_abc.IsNull());
  EXPECT_TRUE(parkable_abc.Is8Bit());
  EXPECT_EQ(3u, parkable_abc.length());
  EXPECT_EQ(3u, parkable_abc.CharactersSizeInBytes());
  EXPECT_FALSE(
      parkable_abc.may_be_parked());  // Small strings are not parkable.

  EXPECT_EQ(String("abc"), parkable_abc.ToString());
  ParkableString copy = parkable_abc;
  EXPECT_EQ(copy.Impl(), parkable_abc.Impl());
}

TEST_F(ParkableStringTest, Park) {
  base::HistogramTester histogram_tester;

  {
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    EXPECT_TRUE(parkable.may_be_parked());
    EXPECT_FALSE(parkable.Impl()->is_parked());
    EXPECT_TRUE(ParkAndWait(parkable));
    EXPECT_TRUE(parkable.Impl()->is_parked());
  }

  String large_string = MakeLargeString();
  ParkableString parkable(large_string.Impl());
  EXPECT_TRUE(parkable.may_be_parked());
  // Not the only one to have a reference to the string.
  EXPECT_FALSE(ParkAndWait(parkable));
  large_string = String();
  EXPECT_TRUE(ParkAndWait(parkable));

  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 2);
  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 2);

  {
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    EXPECT_TRUE(parkable.may_be_parked());
    EXPECT_FALSE(parkable.Impl()->is_parked());
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    parkable = ParkableString();  // release the reference.
    RunPostedTasks();             // Should not crash.
  }
}

TEST_F(ParkableStringTest, AbortParking) {
  {
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    EXPECT_TRUE(parkable.may_be_parked());
    EXPECT_FALSE(parkable.Impl()->is_parked());

    // The string is locked at the end of parking, should cancel it.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    parkable.Impl()->Lock();
    RunPostedTasks();
    EXPECT_FALSE(parkable.Impl()->is_parked());

    // Unlock, OK to park.
    parkable.Impl()->Unlock();
    EXPECT_TRUE(ParkAndWait(parkable));
  }

  {
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    // |ToString()| cancels parking as |content| is kept alive.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    {
      String content = parkable.Impl()->ToString();
      RunPostedTasks();
      EXPECT_FALSE(parkable.Impl()->is_parked());
    }
    EXPECT_TRUE(ParkAndWait(parkable));
  }

  {
    ParkableString parkable(MakeLargeString().ReleaseImpl());
    // Transient |Lock()| or |ToString()| doesn't cancel parking.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    parkable.Impl()->Lock();
    parkable.Impl()->ToString();
    parkable.Impl()->Unlock();
    RunPostedTasks();
    EXPECT_TRUE(parkable.Impl()->is_parked());

    // Synchronous parking respects locking and external references.
    parkable.ToString();
    EXPECT_TRUE(parkable.Impl()->has_compressed_data());
    parkable.Lock();
    EXPECT_FALSE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    parkable.Unlock();
    {
      String content = parkable.ToString();
      EXPECT_FALSE(
          parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    }
    // Parking is synchronous.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    EXPECT_TRUE(parkable.Impl()->is_parked());
  }
}

TEST_F(ParkableStringTest, Unpark) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(MakeLargeString().Impl());
  String unparked_copy = parkable.ToString().IsolatedCopy();
  EXPECT_TRUE(parkable.may_be_parked());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_TRUE(parkable.Impl()->is_parked());

  String unparked = parkable.ToString();
  EXPECT_EQ(unparked_copy, unparked);
  EXPECT_FALSE(parkable.Impl()->is_parked());

  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 2);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kUnparkedInForeground, 1);
}

TEST_F(ParkableStringTest, LockUnlock) {
  ParkableString parkable(MakeLargeString().Impl());
  ParkableStringImpl* impl = parkable.Impl();
  EXPECT_EQ(0, impl->lock_depth_);

  parkable.Lock();
  EXPECT_EQ(1, impl->lock_depth_);
  parkable.Lock();
  parkable.Unlock();
  EXPECT_EQ(1, impl->lock_depth_);
  parkable.Unlock();
  EXPECT_EQ(0, impl->lock_depth_);

  parkable.Lock();
  EXPECT_FALSE(ParkAndWait(parkable));
  parkable.Unlock();
  EXPECT_TRUE(ParkAndWait(parkable));

  parkable.ToString();
  std::thread t([&]() { parkable.Lock(); });
  t.join();
  EXPECT_FALSE(ParkAndWait(parkable));
  parkable.Unlock();
  EXPECT_TRUE(ParkAndWait(parkable));
}

TEST_F(ParkableStringTest, LockParkedString) {
  ParkableString parkable = CreateAndParkAll();
  ParkableStringImpl* impl = parkable.Impl();

  parkable.Lock();  // Locking doesn't unpark.
  EXPECT_TRUE(impl->is_parked());
  parkable.ToString();
  EXPECT_FALSE(impl->is_parked());
  EXPECT_EQ(1, impl->lock_depth_);

  EXPECT_FALSE(ParkAndWait(parkable));

  parkable.Unlock();
  EXPECT_EQ(0, impl->lock_depth_);
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_TRUE(impl->is_parked());
}

TEST_F(ParkableStringTest, ManagerSimple) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(MakeLargeString().Impl());
  ASSERT_FALSE(parkable.Impl()->is_parked());

  auto& manager = ParkableStringManager::Instance();
  EXPECT_EQ(1u, manager.Size());

  // Small strings are not tracked.
  ParkableString small(String("abc").ReleaseImpl());
  EXPECT_EQ(1u, manager.Size());

  // No parking as the current state is not "backgrounded".
  manager.SetRendererBackgrounded(true);
  manager.SetRendererBackgrounded(false);
  ASSERT_FALSE(manager.IsRendererBackgrounded());
  WaitForDelayedParking();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  histogram_tester.ExpectTotalCount("Memory.MovableStringsCount", 0);

  manager.SetRendererBackgrounded(true);
  ASSERT_TRUE(manager.IsRendererBackgrounded());
  WaitForDelayedParking();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 1, 1);

  // Park and unpark.
  parkable.ToString();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  manager.SetRendererBackgrounded(true);
  WaitForDelayedParking();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 1, 2);

  // More than one reference, no parking.
  manager.SetRendererBackgrounded(false);
  String alive_unparked = parkable.ToString();  // Unparked in foreground.
  manager.SetRendererBackgrounded(true);
  WaitForDelayedParking();
  EXPECT_FALSE(parkable.Impl()->is_parked());

  // Other reference is dropped, OK to park.
  alive_unparked = String();
  manager.SetRendererBackgrounded(true);
  WaitForDelayedParking();
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

TEST_F(ParkableStringTest, ManagerMultipleStrings) {
  base::HistogramTester histogram_tester;

  ParkableString parkable(MakeLargeString().Impl());
  ParkableString parkable2(MakeLargeString().Impl());

  auto& manager = ParkableStringManager::Instance();
  EXPECT_EQ(2u, manager.Size());

  parkable2 = ParkableString();
  EXPECT_EQ(1u, manager.Size());

  ParkableString copy = parkable;
  parkable = ParkableString();
  EXPECT_EQ(1u, manager.Size());
  copy = ParkableString();
  EXPECT_EQ(0u, manager.Size());

  String str = MakeLargeString();
  ParkableString parkable3(str.Impl());
  EXPECT_EQ(1u, manager.Size());
  // De-duplicated.
  ParkableString other_parkable3(str.Impl());
  EXPECT_EQ(1u, manager.Size());
  EXPECT_EQ(parkable3.Impl(), other_parkable3.Impl());

  // If all the references to a string are internal, park it.
  str = String();
  // This string is not parkable, bur should still be in size and count
  // histograms.
  ParkableString parkable4(MakeLargeString().Impl());
  String parkable4_content = parkable4.ToString();

  int parking_count = 0;
  manager.SetRendererBackgrounded(true);
  parking_count++;
  ASSERT_TRUE(manager.IsRendererBackgrounded());
  WaitForDelayedParking();
  EXPECT_TRUE(parkable3.Impl()->is_parked());
  manager.SetRendererBackgrounded(true);
  parking_count++;
  WaitForDelayedParking();
  WaitForStatisticsRecording();
  // Even though two parking tasks ran, only one metrics collection.
  histogram_tester.ExpectUniqueSample("Memory.ParkableString.TotalSizeKb",
                                      2 * kSizeKb, 1);
  // a 20kB string with only one character compresses down to <1kB, hence with
  // rounding:
  // - CompressedSizeKb == 0
  // - SavingsKb == 19
  // - CompressionRatio == 0 (<1%)
  size_t expected_savings = kSizeKb * 1000 - kCompressedSize;
  histogram_tester.ExpectUniqueSample("Memory.ParkableString.CompressedSizeKb",
                                      kCompressedSize / 1000, 1);
  histogram_tester.ExpectUniqueSample("Memory.ParkableString.SavingsKb",
                                      expected_savings / 1000, 1);
  histogram_tester.ExpectUniqueSample("Memory.ParkableString.CompressionRatio",
                                      100 * kCompressedSize / (kSizeKb * 1000),
                                      1);

  // Don't record statistics if the renderer moves to foreground before
  // recording statistics.
  manager.SetRendererBackgrounded(true);
  parking_count++;
  manager.SetRendererBackgrounded(false);
  manager.SetRendererBackgrounded(true);
  parking_count++;
  WaitForDelayedParking();
  WaitForStatisticsRecording();
  // Same count as above, no stats recording in the meantime.
  histogram_tester.ExpectUniqueSample("Memory.ParkableString.TotalSizeKb",
                                      2 * kSizeKb, 1);

  // Calling |ParkStringsWithCompressedDataAndRecordStatistics()| resets the
  // state, can now record stats next time.
  manager.SetRendererBackgrounded(true);
  parking_count++;
  WaitForDelayedParking();
  WaitForStatisticsRecording();
  histogram_tester.ExpectUniqueSample("Memory.ParkableString.TotalSizeKb",
                                      2 * kSizeKb, 2);

  // Only drop it from the managed strings when the last one is gone.
  parkable3 = ParkableString();
  EXPECT_EQ(2u, manager.Size());
  other_parkable3 = ParkableString();
  EXPECT_EQ(1u, manager.Size());
  parkable4 = ParkableString();
  EXPECT_EQ(0u, manager.Size());

  // 1 parked, 1 unparked. Bucket count is 2 as we collected stats twice.
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsCount", 2,
                                      parking_count);
  histogram_tester.ExpectUniqueSample("Memory.MovableStringsTotalSizeKb",
                                      2 * kSizeKb, parking_count);

  histogram_tester.ExpectTotalCount("Memory.MovableStringParkingAction", 1);
  histogram_tester.ExpectBucketCount(
      "Memory.MovableStringParkingAction",
      ParkableStringImpl::ParkingAction::kParkedInBackground, 1);
}

TEST_F(ParkableStringTest, ShouldPark) {
  String empty_string("");
  EXPECT_FALSE(ParkableStringManager::ShouldPark(*empty_string.Impl()));
  std::vector<char> data(20 * 1000, 'a');

  String parkable(String(data.data(), data.size()).ReleaseImpl());
  EXPECT_TRUE(ParkableStringManager::ShouldPark(*parkable.Impl()));

  std::thread t([]() {
    std::vector<char> data(20 * 1000, 'a');
    String parkable(String(data.data(), data.size()).ReleaseImpl());
    EXPECT_FALSE(ParkableStringManager::ShouldPark(*parkable.Impl()));
  });
  t.join();
}

#if defined(ADDRESS_SANITIZER)
TEST_F(ParkableStringTest, AsanPoisoningTest) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  const LChar* data = parkable.ToString().Characters8();
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_DEATH(EXPECT_NE(0, data[10]), "");
}

// Non-regression test for crbug.com/905137.
TEST_F(ParkableStringTest, CorrectAsanPoisoning) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
  // A main thread task is posted once compression is done.
  while (!scoped_task_environment_.MainThreadHasPendingTask()) {
    parkable.Lock();
    parkable.ToString();
    parkable.Unlock();
  }
  RunPostedTasks();
}
#endif  // defined(ADDRESS_SANITIZER)

TEST_F(ParkableStringTest, Compression) {
  base::HistogramTester histogram_tester;

  ParkableString parkable = CreateAndParkAll();
  ParkableStringImpl* impl = parkable.Impl();

  EXPECT_TRUE(impl->is_parked());
  EXPECT_TRUE(impl->has_compressed_data());
  EXPECT_EQ(kCompressedSize, impl->compressed_size());
  parkable.ToString();  // First decompression.
  EXPECT_FALSE(impl->is_parked());
  EXPECT_TRUE(impl->has_compressed_data());
  EXPECT_TRUE(
      impl->Park(ParkableStringImpl::ParkingMode::kIfCompressedDataExists));
  EXPECT_TRUE(impl->is_parked());
  parkable.ToString();  // Second decompression.

  histogram_tester.ExpectUniqueSample(
      "Memory.ParkableString.Compression.SizeKb", kSizeKb, 1);
  histogram_tester.ExpectTotalCount("Memory.ParkableString.Compression.Latency",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Memory.ParkableString.Compression.ThroughputMBps", 1);
  // |parkable| is decompressed twice.
  histogram_tester.ExpectUniqueSample(
      "Memory.ParkableString.Decompression.SizeKb", kSizeKb, 2);
  histogram_tester.ExpectTotalCount(
      "Memory.ParkableString.Decompression.Latency", 2);
  histogram_tester.ExpectTotalCount(
      "Memory.ParkableString.Decompression.ThroughputMBps", 2);
}

TEST_F(ParkableStringTest, DelayedTasks) {
  ParkableStringManager& manager = ParkableStringManager::Instance();
  base::HistogramTester histogram_tester;

  ParkableString parkable(MakeLargeString().Impl());
  manager.SetRendererBackgrounded(true);

  // Parking, and statistics.
  EXPECT_EQ(2u, scoped_task_environment_.GetPendingMainThreadTaskCount());
  WaitForDelayedParking();
  EXPECT_TRUE(parkable.Impl()->is_parked());
  histogram_tester.ExpectUniqueSample(
      "Memory.ParkableString.Compression.SizeKb", kSizeKb, 1);
  // Statistics haven't been recorded yet.
  histogram_tester.ExpectTotalCount("Memory.ParkableString.TotalSizeKb", 0);

  // Statistics task.
  EXPECT_EQ(1u, scoped_task_environment_.GetPendingMainThreadTaskCount());
  WaitForStatisticsRecording();
  EXPECT_EQ(0u, scoped_task_environment_.GetPendingMainThreadTaskCount());
  // Statistics should have been recorded.
  histogram_tester.ExpectTotalCount("Memory.ParkableString.TotalSizeKb", 1);
}

TEST_F(ParkableStringTest, SynchronousCompression) {
  ParkableStringManager& manager = ParkableStringManager::Instance();
  ParkableString parkable = CreateAndParkAll();

  parkable.ToString();
  EXPECT_TRUE(parkable.Impl()->has_compressed_data());
  // No waiting, synchronous compression.
  manager.ParkAllIfRendererBackgrounded(
      ParkableStringImpl::ParkingMode::kIfCompressedDataExists);
  EXPECT_TRUE(parkable.Impl()->is_parked());
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ParkableStringTest, OnPurgeMemory) {
  ParkableString parkable = CreateAndParkAll();

  parkable.ToString();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  EXPECT_TRUE(parkable.Impl()->has_compressed_data());

  MemoryCoordinator::Instance().OnPurgeMemory();
  EXPECT_TRUE(parkable.Impl()->is_parked());
}

}  // namespace blink

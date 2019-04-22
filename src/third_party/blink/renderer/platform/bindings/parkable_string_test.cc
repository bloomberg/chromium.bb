// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <random>
#include <thread>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"
#include "third_party/blink/renderer/platform/memory_pressure_listener.h"

namespace blink {

namespace {

constexpr size_t kSizeKb = 20;
// Compressed size of the string returned by |MakeLargeString()|.
// Update if the assertion in the |CheckCompressedSize()| test fails.
constexpr size_t kCompressedSize = 55;

String MakeLargeString(char c = 'a') {
  std::vector<char> data(kSizeKb * 1000, c);
  return String(data.data(), data.size()).ReleaseImpl();
}

}  // namespace

class ParkableStringTestBase : public ::testing::Test {
 public:
  ParkableStringTestBase()
      : ::testing::Test(),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        scoped_feature_list_() {}

 protected:
  void RunPostedTasks() { scoped_task_environment_.RunUntilIdle(); }

  bool ParkAndWait(const ParkableString& string) {
    bool success =
        string.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways);
    RunPostedTasks();
    return success;
  }

  void WaitForDelayedParking() {
    scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(
        ParkableStringManager::kParkingDelayInSeconds));
    RunPostedTasks();
  }

  void CheckOnlyCpuCostTaskRemains() {
    unsigned expected_count = 0;
    if (ParkableStringManager::Instance()
            .has_posted_unparking_time_accounting_task_) {
      expected_count = 1;
    }
    EXPECT_EQ(expected_count,
              scoped_task_environment_.GetPendingMainThreadTaskCount());
  }

  void SetUp() override { ParkableStringManager::Instance().ResetForTesting(); }

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

class ParkableStringTest : public ParkableStringTestBase {
 public:
  ParkableStringTest() : ParkableStringTestBase() {}

 protected:
  void SetUp() override {
    ParkableStringTestBase::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {kCompressParkableStringsInBackground},
        {kCompressParkableStringsInForeground});
  }

  void WaitForStatisticsRecording() {
    scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(
        ParkableStringManager::kStatisticsRecordingDelayInSeconds));
    RunPostedTasks();
  }

  ParkableString CreateAndParkAll() {
    auto& manager = ParkableStringManager::Instance();
    // Checking that there are no other strings, to make sure this doesn't
    // cause side-effects.
    CHECK_EQ(0u, manager.Size());
    ParkableString parkable(MakeLargeString('a').ReleaseImpl());
    manager.SetRendererBackgrounded(true);
    EXPECT_FALSE(parkable.Impl()->is_parked());
    WaitForDelayedParking();
    EXPECT_TRUE(parkable.Impl()->is_parked());
    return parkable;
  }
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

TEST_F(ParkableStringTest, DontCompressRandomString) {
  // Make a large random string. Large to make sure it's parkable, and random to
  // ensure its compressed size is larger than the initial size (at least from
  // gzip's header). Mersenne-Twister implementation is specified, making the
  // test deterministic.
  std::vector<unsigned char> data(kSizeKb * 1000);
  std::mt19937 engine(42);
  // uniform_int_distribution<T> is undefined behavior for T = unsigned char.
  std::uniform_int_distribution<int> dist(
      0, std::numeric_limits<unsigned char>::max());

  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<unsigned char>(dist(engine));
  }
  ParkableString parkable(String(data.data(), data.size()).ReleaseImpl());

  EXPECT_TRUE(parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
  RunPostedTasks();
  // Not parked because the temporary buffer wasn't large enough.
  EXPECT_FALSE(parkable.Impl()->is_parked());
}

TEST_F(ParkableStringTest, ParkUnparkIdenticalContent) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
  RunPostedTasks();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  EXPECT_EQ(MakeLargeString(), parkable.ToString());
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

TEST_F(ParkableStringTest, Equality) {
  ParkableString abc(String("abc").ReleaseImpl());
  ParkableString abc2(String("abc").ReleaseImpl());

  EXPECT_NE(abc.ToString().Impl(), abc2.ToString().Impl());
  EXPECT_TRUE(abc.Impl()->Equal(*abc2.Impl()));

  // Should not crash. Unlocking poisons the string with ASAN, checks that we
  // unpoison it correctly when calling Equal().
  ParkableString parkable(MakeLargeString('a').ReleaseImpl());
  parkable.Lock();
  parkable.Unlock();
  ParkableString parkable2(MakeLargeString('a').ReleaseImpl());
  EXPECT_EQ(parkable.Impl(), parkable2.Impl());
}

TEST_F(ParkableStringTest, Park) {
  base::HistogramTester histogram_tester;

  {
    ParkableString parkable(MakeLargeString('a').ReleaseImpl());
    EXPECT_TRUE(parkable.may_be_parked());
    EXPECT_FALSE(parkable.Impl()->is_parked());
    EXPECT_TRUE(ParkAndWait(parkable));
    EXPECT_TRUE(parkable.Impl()->is_parked());
  }

  String large_string = MakeLargeString('b');
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
    ParkableString parkable(MakeLargeString('c').ReleaseImpl());
    EXPECT_TRUE(parkable.may_be_parked());
    EXPECT_FALSE(parkable.Impl()->is_parked());
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    // Should not crash, it is allowed to call |Park()| twice in a row.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    parkable = ParkableString();  // release the reference.
    RunPostedTasks();             // Should not crash.
  }
}

TEST_F(ParkableStringTest, EqualityNoUnparking) {
  String large_string = MakeLargeString();
  String copy = large_string.IsolatedCopy();
  EXPECT_NE(large_string.Impl(), copy.Impl());

  ParkableString parkable(large_string.Impl());
  large_string = String();

  EXPECT_TRUE(parkable.may_be_parked());
  EXPECT_FALSE(parkable.Impl()->is_parked());
  EXPECT_TRUE(ParkAndWait(parkable));
  EXPECT_TRUE(parkable.Impl()->is_parked());

  ParkableString parkable_copy(copy.Impl());
  EXPECT_EQ(parkable_copy.Impl(), parkable.Impl());  // De-duplicated.
  EXPECT_TRUE(parkable_copy.Impl()->Equal(*parkable.Impl()));
  EXPECT_TRUE(parkable.Impl()->is_parked());
  EXPECT_TRUE(parkable_copy.Impl()->is_parked());

  EXPECT_EQ(1u, ParkableStringManager::Instance().Size());
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
    // Transient |Lock()| or |ToString()| cancel parking.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    parkable.Impl()->Lock();
    parkable.Impl()->ToString();
    parkable.Impl()->Unlock();
    RunPostedTasks();
    EXPECT_FALSE(parkable.Impl()->is_parked());

    // In order to test synchronous parking below, need to park the string
    // first.
    EXPECT_TRUE(
        parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
    RunPostedTasks();
    EXPECT_TRUE(parkable.Impl()->is_parked());
    parkable.ToString();

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

TEST_F(ParkableStringTest, AbortedParkingRetainsCompressedData) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(parkable.may_be_parked());
  EXPECT_FALSE(parkable.Impl()->is_parked());

  EXPECT_TRUE(parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
  parkable.ToString();  // Cancels parking.
  RunPostedTasks();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  // Compressed data is not discarded.
  EXPECT_TRUE(parkable.Impl()->has_compressed_data());

  // Synchronous parking.
  EXPECT_TRUE(parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways));
  EXPECT_TRUE(parkable.Impl()->is_parked());
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

  ParkableString parkable(MakeLargeString('a').Impl());
  ParkableString parkable2(MakeLargeString('b').Impl());

  auto& manager = ParkableStringManager::Instance();
  EXPECT_EQ(2u, manager.Size());

  parkable2 = ParkableString();
  EXPECT_EQ(1u, manager.Size());

  ParkableString copy = parkable;
  parkable = ParkableString();
  EXPECT_EQ(1u, manager.Size());
  copy = ParkableString();
  EXPECT_EQ(0u, manager.Size());

  String str = MakeLargeString('c');
  ParkableString parkable3(str.Impl());
  EXPECT_EQ(1u, manager.Size());
  // De-duplicated with the same underlying StringImpl.
  ParkableString other_parkable3(str.Impl());
  EXPECT_EQ(1u, manager.Size());
  EXPECT_EQ(parkable3.Impl(), other_parkable3.Impl());

  {
    // De-duplicated with a different StringImpl but the same content.
    ParkableString other_parkable3_different_string(
        MakeLargeString('c').ReleaseImpl());
    EXPECT_EQ(1u, manager.Size());
    EXPECT_EQ(parkable3.Impl(), other_parkable3_different_string.Impl());
  }

  // If all the references to a string are internal, park it.
  str = String();
  // This string is not parkable, bur should still be in size and count
  // histograms.
  ParkableString parkable4(MakeLargeString('d').Impl());
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
  while (scoped_task_environment_.GetPendingMainThreadTaskCount() == 0) {
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
  manager.ParkAll(ParkableStringImpl::ParkingMode::kIfCompressedDataExists);
  EXPECT_TRUE(parkable.Impl()->is_parked());
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ParkableStringTest, OnPurgeMemoryInBackground) {
  ParkableString parkable = CreateAndParkAll();
  EXPECT_TRUE(ParkableStringManager::Instance().IsRendererBackgrounded());

  parkable.ToString();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  EXPECT_TRUE(parkable.Impl()->has_compressed_data());

  MemoryPressureListenerRegistry::Instance().OnPurgeMemory();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  parkable.ToString();
  EXPECT_TRUE(parkable.Impl()->has_compressed_data());
}

TEST_F(ParkableStringTest, OnPurgeMemoryInForeground) {
  ParkableString parkable1 = CreateAndParkAll();
  ParkableString parkable2(MakeLargeString('b').ReleaseImpl());

  // Park everything.
  ParkableStringManager::Instance().SetRendererBackgrounded(true);
  WaitForDelayedParking();
  EXPECT_TRUE(parkable1.Impl()->is_parked());
  EXPECT_TRUE(parkable2.Impl()->is_parked());

  ParkableStringManager::Instance().SetRendererBackgrounded(false);

  // Different usage patterns:
  // 1. Parkable, will be parked synchronouly.
  // 2. Cannot be parked, compressed representation is purged.
  parkable1.ToString();
  String retained = parkable2.ToString();
  EXPECT_TRUE(parkable2.Impl()->has_compressed_data());

  MemoryPressureListenerRegistry::Instance().OnPurgeMemory();
  EXPECT_TRUE(parkable1.Impl()->is_parked());  // Parked synchronously.
  EXPECT_FALSE(parkable2.Impl()->is_parked());
  EXPECT_FALSE(parkable2.Impl()->has_compressed_data());  // Purged.

  parkable1.ToString();
  EXPECT_TRUE(parkable1.Impl()->has_compressed_data());
}

TEST_F(ParkableStringTest, ReportMemoryDump) {
  using base::trace_event::MemoryAllocatorDump;
  using testing::ByRef;
  using testing::Contains;
  using testing::Eq;

  auto& manager = ParkableStringManager::Instance();
  ParkableString parkable1(MakeLargeString('a').ReleaseImpl());
  ParkableString parkable2(MakeLargeString('b').ReleaseImpl());
  // Not reported in stats below.
  ParkableString parkable3(String("short string, not parkable").ReleaseImpl());

  manager.SetRendererBackgrounded(true);
  WaitForDelayedParking();
  parkable1.ToString();

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  base::trace_event::ProcessMemoryDump pmd(args);
  manager.OnMemoryDump(&pmd);
  base::trace_event::MemoryAllocatorDump* dump =
      pmd.GetAllocatorDump("parkable_strings");
  ASSERT_NE(nullptr, dump);

  constexpr size_t kStringSize = kSizeKb * 1000;
  MemoryAllocatorDump::Entry original("original_size", "bytes",
                                      2 * kStringSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(original))));

  // |parkable1| is unparked.
  MemoryAllocatorDump::Entry uncompressed("uncompressed_size", "bytes",
                                          kStringSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(uncompressed))));

  MemoryAllocatorDump::Entry compressed("compressed_size", "bytes",
                                        kCompressedSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(compressed))));

  // |parkable1| compressed data is overhead.
  MemoryAllocatorDump::Entry overhead("overhead_size", "bytes",
                                      kCompressedSize);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(overhead))));

  MemoryAllocatorDump::Entry metadata("metadata_size", "bytes",
                                      2 * sizeof(ParkableStringImpl));
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(metadata))));

  MemoryAllocatorDump::Entry savings(
      "savings_size", "bytes",
      2 * kStringSize -
          (kStringSize + 2 * kCompressedSize + 2 * sizeof(ParkableStringImpl)));
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(savings))));
}

TEST_F(ParkableStringTest, ForegroundParkingIsNotEnabled) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  WaitForDelayedParking();
  // No automatic parking.
  EXPECT_FALSE(parkable.Impl()->is_parked());
}

class ParkableStringForegroundParkingTest : public ParkableStringTestBase {
 public:
  ParkableStringForegroundParkingTest() : ParkableStringTestBase() {}

 protected:
  void WaitForAging() {
    EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
    scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(
        ParkableStringManager::kAgingIntervalInSeconds));
    RunPostedTasks();
  }

  void SetUp() override {
    ParkableStringTestBase::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        kCompressParkableStringsInForeground);
  }

  void TearDown() override {
    while (scoped_task_environment_.GetPendingMainThreadTaskCount() != 0) {
      WaitForAging();
    }

    ParkableStringTestBase::TearDown();
  }
};

TEST_F(ParkableStringForegroundParkingTest, Aging) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(parkable.Impl()->is_young());
  WaitForAging();
  EXPECT_FALSE(parkable.Impl()->is_young());

  parkable.Lock();
  EXPECT_TRUE(parkable.Impl()->is_young());
  // Locked strings don't age.
  WaitForAging();
  EXPECT_TRUE(parkable.Impl()->is_young());
  parkable.Unlock();
  WaitForAging();
  EXPECT_FALSE(parkable.Impl()->is_young());

  parkable.ToString();
  EXPECT_TRUE(parkable.Impl()->is_young());
  // No external reference, can age again.
  WaitForAging();
  EXPECT_FALSE(parkable.Impl()->is_young());

  // External references prevent a string from aging.
  String retained = parkable.ToString();
  EXPECT_TRUE(parkable.Impl()->is_young());
  WaitForAging();
  EXPECT_TRUE(parkable.Impl()->is_young());
}

TEST_F(ParkableStringForegroundParkingTest, OldStringsAreParked) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_TRUE(parkable.Impl()->is_young());
  WaitForAging();
  EXPECT_FALSE(parkable.Impl()->is_young());
  WaitForAging();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  // Unparked, two aging cycles before parking.
  parkable.ToString();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  WaitForAging();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  WaitForAging();
  EXPECT_TRUE(parkable.Impl()->is_parked());

  // Unparked, two consecutive no-access aging cycles before parking.
  parkable.ToString();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  WaitForAging();
  EXPECT_FALSE(parkable.Impl()->is_parked());
  parkable.ToString();
  WaitForAging();
  EXPECT_FALSE(parkable.Impl()->is_parked());
}

TEST_F(ParkableStringForegroundParkingTest, AgingTicksStopsAndRestarts) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  // Nothing more to do, the tick is not re-scheduled.
  WaitForAging();
  CheckOnlyCpuCostTaskRemains();

  // Unparking, the tick restarts.
  parkable.ToString();
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  WaitForAging();
  // And stops again. 2 ticks to park the string (age, then park), and one
  // checking that there is nothing left to do.
  CheckOnlyCpuCostTaskRemains();

  // New string, restarting the tick, temporarily.
  ParkableString parkable2(MakeLargeString().ReleaseImpl());
  WaitForAging();
  WaitForAging();
  WaitForAging();
  CheckOnlyCpuCostTaskRemains();
}

TEST_F(ParkableStringForegroundParkingTest, AgingTicksStopsWithNoProgress) {
  ParkableString parkable(MakeLargeString('a').ReleaseImpl());
  String retained = parkable.ToString();

  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  // The only string is referenced externally, nothing aging can change.
  CheckOnlyCpuCostTaskRemains();

  ParkableString parkable2(MakeLargeString('b').ReleaseImpl());
  WaitForAging();
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
  EXPECT_TRUE(parkable2.Impl()->is_parked());
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
  WaitForAging();
  // Once |parkable2| has been parked, back to the case where the only
  // remaining strings are referenced externally.
  CheckOnlyCpuCostTaskRemains();
}

TEST_F(ParkableStringForegroundParkingTest, OnlyOneAgingTask) {
  ParkableString parkable1(MakeLargeString('a').ReleaseImpl());
  ParkableString parkable2(MakeLargeString('b').ReleaseImpl());

  // Park both, and wait for the tick to stop.
  WaitForAging();
  WaitForAging();
  EXPECT_TRUE(parkable1.Impl()->is_parked());
  EXPECT_TRUE(parkable2.Impl()->is_parked());
  WaitForAging();
  CheckOnlyCpuCostTaskRemains();

  parkable1.ToString();
  parkable2.ToString();
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
  // Aging task + stats.
  EXPECT_EQ(2u, scoped_task_environment_.GetPendingMainThreadTaskCount());
}

TEST_F(ParkableStringForegroundParkingTest, AgingParkingInProgress) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());

  WaitForAging();
  parkable.Impl()->Park(ParkableStringImpl::ParkingMode::kAlways);
  // Aging should work if the string is being parked.
  WaitForAging();
  // The aging task is rescheduled.
  EXPECT_EQ(2u, scoped_task_environment_.GetPendingMainThreadTaskCount());

  EXPECT_TRUE(parkable.Impl()->is_parked());
}

TEST_F(ParkableStringForegroundParkingTest,
       NoBackgroundParkingWhenForegroundIsEnabled) {
  ParkableString parkable(MakeLargeString().ReleaseImpl());

  auto& manager = ParkableStringManager::Instance();
  CHECK_EQ(1u, manager.Size());

  {
    // Prevents foreground parking.
    String retained = parkable.ToString();
    WaitForAging();
    // As the reference is long-lived, the aging tick stops.
    CheckOnlyCpuCostTaskRemains();
  }

  manager.SetRendererBackgrounded(true);
  WaitForDelayedParking();
  // No foreground parking.
  EXPECT_FALSE(parkable.Impl()->is_parked());
}

TEST_F(ParkableStringForegroundParkingTest, ReportTotalUnparkingTime) {
  base::HistogramTester histogram_tester;

  // Need to make the string really large, otherwise unparking takes less than
  // 1ms, and the 0 bucket is populated.
  const size_t original_size = 5 * 1000 * 1000;
  std::vector<char> data(original_size, 'a');
  ParkableString parkable(String(data.data(), data.size()).ReleaseImpl());

  ParkAndWait(parkable);
  for (int i = 0; i < 10; ++i) {
    parkable.ToString();
    ASSERT_FALSE(parkable.Impl()->is_parked());
    WaitForAging();
    WaitForAging();
    ASSERT_TRUE(parkable.Impl()->is_parked());
    CheckOnlyCpuCostTaskRemains();
  }
  const size_t compressed_size = parkable.Impl()->compressed_size();

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  histogram_tester.ExpectTotalCount("Memory.ParkableString.MainThreadTime.5min",
                                    1);
  histogram_tester.ExpectBucketCount(
      "Memory.ParkableString.MainThreadTime.5min", 0, 0);

  if (base::ThreadTicks::IsSupported()) {
    histogram_tester.ExpectTotalCount(
        "Memory.ParkableString.ParkingThreadTime.5min", 1);
    histogram_tester.ExpectBucketCount(
        "Memory.ParkableString.ParkingThreadTime.5min", 0, 0);
  }

  histogram_tester.ExpectUniqueSample("Memory.ParkableString.TotalSizeKb.5min",
                                      original_size / 1000, 1);
  histogram_tester.ExpectUniqueSample(
      "Memory.ParkableString.CompressedSizeKb.5min", compressed_size / 1000, 1);

  size_t expected_savings = original_size - compressed_size;
  histogram_tester.ExpectUniqueSample("Memory.ParkableString.SavingsKb.5min",
                                      expected_savings / 1000, 1);
  histogram_tester.ExpectUniqueSample(
      "Memory.ParkableString.CompressionRatio.5min",
      100 * compressed_size / original_size, 1);
}

}  // namespace blink

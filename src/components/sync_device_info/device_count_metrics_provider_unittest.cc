// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_count_metrics_provider.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class DeviceCountMetricsProviderTest : public testing::Test {
 public:
  DeviceCountMetricsProviderTest()
      : metrics_provider_(
            base::BindRepeating(&DeviceCountMetricsProviderTest::GetTrackers,
                                base::Unretained(this))) {}

  void AddTracker(const std::map<sync_pb::SyncEnums_DeviceType, int>& count) {
    auto tracker = std::make_unique<FakeDeviceInfoTracker>();
    tracker->OverrideActiveDeviceCount(count);
    trackers_.emplace_back(std::move(tracker));
  }

  void GetTrackers(std::vector<const DeviceInfoTracker*>* trackers) {
    for (const auto& tracker : trackers_) {
      trackers->push_back(tracker.get());
    }
  }

  struct ExpectedCount {
    int total;
    int desktop_count;
    int phone_count;
    int tablet_count;
  };
  void TestProvider(const ExpectedCount& expected_count) {
    base::HistogramTester histogram_tester;
    metrics_provider_.ProvideCurrentSessionData(nullptr);
    histogram_tester.ExpectUniqueSample("Sync.DeviceCount2",
                                        expected_count.total, 1);
    histogram_tester.ExpectUniqueSample("Sync.DeviceCount2.Desktop",
                                        expected_count.desktop_count, 1);
    histogram_tester.ExpectUniqueSample("Sync.DeviceCount2.Phone",
                                        expected_count.phone_count, 1);
    histogram_tester.ExpectUniqueSample("Sync.DeviceCount2.Tablet",
                                        expected_count.tablet_count, 1);
  }

 private:
  DeviceCountMetricsProvider metrics_provider_;
  std::vector<std::unique_ptr<DeviceInfoTracker>> trackers_;
};

namespace {

TEST_F(DeviceCountMetricsProviderTest, NoTrackers) {
  TestProvider(ExpectedCount{});
}

TEST_F(DeviceCountMetricsProviderTest, SingleTracker) {
  AddTracker({{sync_pb::SyncEnums_DeviceType_TYPE_WIN, 1},
              {sync_pb::SyncEnums_DeviceType_TYPE_PHONE, 1}});
  TestProvider(ExpectedCount{
      .total = 2, .desktop_count = 1, .phone_count = 1, .tablet_count = 0});
}

TEST_F(DeviceCountMetricsProviderTest, MultipileTrackers) {
  AddTracker({{sync_pb::SyncEnums_DeviceType_TYPE_PHONE, 1}});
  AddTracker({{sync_pb::SyncEnums_DeviceType_TYPE_TABLET, 3},
              {sync_pb::SyncEnums_DeviceType_TYPE_MAC, 2}});
  AddTracker({{sync_pb::SyncEnums_DeviceType_TYPE_WIN, -120}});
  AddTracker({{sync_pb::SyncEnums_DeviceType_TYPE_WIN, 3}});
  TestProvider(ExpectedCount{
      .total = 5, .desktop_count = 3, .phone_count = 1, .tablet_count = 3});
}

TEST_F(DeviceCountMetricsProviderTest, OnlyNegative) {
  AddTracker({{sync_pb::SyncEnums_DeviceType_TYPE_PHONE, -121}});
  TestProvider(ExpectedCount{
      .total = 0, .desktop_count = 0, .phone_count = 0, .tablet_count = 0});
}

TEST_F(DeviceCountMetricsProviderTest, VeryLarge) {
  AddTracker({{sync_pb::SyncEnums_DeviceType_TYPE_LINUX, 123456789},
              {sync_pb::SyncEnums_DeviceType_TYPE_PHONE, 1}});
  TestProvider(ExpectedCount{
      .total = 100, .desktop_count = 100, .phone_count = 1, .tablet_count = 0});
}

}  // namespace

}  // namespace syncer

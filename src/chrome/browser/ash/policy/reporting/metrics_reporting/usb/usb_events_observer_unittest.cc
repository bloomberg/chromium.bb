// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/usb/usb_events_observer.h"
#include <sys/types.h>
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using testing::IsEmpty;
using testing::Not;
using ::testing::Pointwise;
using ::testing::StrEq;
using UsbEventInfoPtr = chromeos::cros_healthd::mojom::UsbEventInfoPtr;
using UsbEventInfo = chromeos::cros_healthd::mojom::UsbEventInfo;

namespace reporting {
namespace {

static constexpr int32_t kTestVid = 0xffee;
static constexpr int32_t kTestPid = 0x0;
static constexpr char kTestName[] = "TestName";
static constexpr char kTestVendor[] = "TestVendor";
static constexpr char kTestCategory1[] = "TestCategory1";
static constexpr char kTestCategory2[] = "TestCategory2";
const std::vector<std::string> kTestCategories = {kTestCategory1,
                                                  kTestCategory2};

class UsbEventsObserverTest : public ::testing::Test {
 public:
  UsbEventsObserverTest() = default;

  UsbEventsObserverTest(const UsbEventsObserverTest&) = delete;
  UsbEventsObserverTest& operator=(const UsbEventsObserverTest&) = delete;

  ~UsbEventsObserverTest() override = default;

  void SetUp() override { ::chromeos::CrosHealthdClient::InitializeFake(); }

  void TearDown() override {
    ::chromeos::CrosHealthdClient::Shutdown();

    // Wait for ServiceConnection to observe the destruction of the client.
    ::chromeos::cros_healthd::ServiceConnection::GetInstance()
        ->FlushForTesting();
  }

  UsbEventInfoPtr test_usb_event_info = UsbEventInfo::New(kTestVendor,
                                                          kTestName,
                                                          kTestVid,
                                                          kTestPid,
                                                          kTestCategories);

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(UsbEventsObserverTest, UsbOnAdd) {
  MetricData metric_data;
  UsbEventsObserver usb_observer;
  auto cb = base::BindLambdaForTesting(
      [&](MetricData md) { metric_data = std::move(md); });

  usb_observer.SetOnEventObservedCallback(std::move(cb));
  usb_observer.SetReportingEnabled(true);
  usb_observer.OnRemove(std::move(test_usb_event_info));

  ASSERT_TRUE(metric_data.has_event_data());
  ASSERT_TRUE(metric_data.telemetry_data().has_usb_telemetry());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_name());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_pid());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_vendor());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_vid());
  EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().name(),
              StrEq(kTestName));
  EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().pid(), Eq(kTestPid));
  EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().vendor(),
              StrEq(kTestVendor));
  EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().vid(), Eq(kTestVid));
  EXPECT_EQ(metric_data.event_data().type(), MetricEventType::USB_REMOVED);
  ASSERT_THAT(metric_data.telemetry_data().usb_telemetry().categories().size(),
              Eq(kTestCategories.size()));

  for (int i = 0; i < kTestCategories.size(); ++i) {
    EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().categories()[i],
                StrEq(kTestCategories[i]));
  }
}

TEST_F(UsbEventsObserverTest, UsbOnRemoved) {
  MetricData metric_data;
  UsbEventsObserver usb_observer;
  auto cb = base::BindLambdaForTesting(
      [&](MetricData md) { metric_data = std::move(md); });

  usb_observer.SetOnEventObservedCallback(std::move(cb));
  usb_observer.SetReportingEnabled(true);
  usb_observer.OnAdd(std::move(test_usb_event_info));

  ASSERT_TRUE(metric_data.has_event_data());
  ASSERT_TRUE(metric_data.telemetry_data().has_usb_telemetry());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_name());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_pid());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_vendor());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_vid());
  EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().name(),
              StrEq(kTestName));
  EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().pid(), Eq(kTestPid));
  EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().vendor(),
              StrEq(kTestVendor));
  EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().vid(), Eq(kTestVid));
  EXPECT_EQ(metric_data.event_data().type(), MetricEventType::USB_ADDED);
  ASSERT_THAT(metric_data.telemetry_data().usb_telemetry().categories().size(),
              Eq(kTestCategories.size()));

  for (int i = 0; i < kTestCategories.size(); ++i) {
    EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().categories()[i],
                StrEq(kTestCategories[i]));
  }
}

TEST_F(UsbEventsObserverTest, UsbOnAddUsingFakeCrosHealthdClient) {
  test::TestEvent<MetricData> result_metric_data;
  UsbEventsObserver usb_observer;

  usb_observer.SetOnEventObservedCallback(result_metric_data.cb());
  usb_observer.SetReportingEnabled(true);

  ::chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->EmitUsbAddEventForTesting();

  const auto metric_data = result_metric_data.result();
  ASSERT_TRUE(metric_data.has_event_data());
  ASSERT_TRUE(metric_data.telemetry_data().has_usb_telemetry());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_name());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_pid());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_vendor());
  EXPECT_TRUE(metric_data.telemetry_data().usb_telemetry().has_vid());
  EXPECT_THAT(metric_data.telemetry_data().usb_telemetry().categories(),
              IsEmpty());
  EXPECT_EQ(metric_data.event_data().type(), MetricEventType::USB_ADDED);
}
}  // namespace
}  // namespace reporting

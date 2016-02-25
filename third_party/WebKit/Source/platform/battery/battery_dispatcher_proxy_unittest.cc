// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/battery/battery_dispatcher_proxy.h"

#include "platform/battery/battery_status.h"
#include "platform/testing/MessageLoopForMojo.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class MockBatteryStatusListener : public BatteryDispatcherProxy::Listener {
  WTF_MAKE_NONCOPYABLE(MockBatteryStatusListener);
 public:
  MockBatteryStatusListener() : did_change_battery_status_(false) {}

  // BatteryDispatcherProxy::Listener method.
  void OnUpdateBatteryStatus(const BatteryStatus& status) override {
    status_ = status;
    did_change_battery_status_ = true;
  }

  const BatteryStatus& status() const { return status_; }
  bool did_change_battery_status() const { return did_change_battery_status_; }

 private:
  bool did_change_battery_status_;
  BatteryStatus status_;
};

class BatteryDispatcherProxyTest : public testing::Test {
 public:
  void UpdateBatteryStatus(const device::BatteryStatus& status) {
    device::BatteryStatusPtr status_ptr(device::BatteryStatus::New());
    *status_ptr = status;
    dispatcher_->OnDidChange(std::move(status_ptr));
  }

  const MockBatteryStatusListener& listener() const { return listener_; }

 protected:
  void SetUp() override {
    dispatcher_ = adoptPtr(new BatteryDispatcherProxy(&listener_));
  }

 private:
  base::MessageLoop message_loop_;
  MockBatteryStatusListener listener_;
  OwnPtr<BatteryDispatcherProxy> dispatcher_;
};

TEST_F(BatteryDispatcherProxyTest, UpdateListener) {
  // TODO(darin): This test isn't super interesting. It just exercises
  // conversion b/w device::BatteryStatus and blink::BatteryStatus.

  device::BatteryStatus status;
  status.charging = true;
  status.charging_time = 100;
  status.discharging_time = 200;
  status.level = 0.5;

  UpdateBatteryStatus(status);

  const BatteryStatus& received_status = listener().status();
  EXPECT_TRUE(listener().did_change_battery_status());
  EXPECT_EQ(status.charging, received_status.charging());
  EXPECT_EQ(status.charging_time, received_status.charging_time());
  EXPECT_EQ(status.discharging_time, received_status.discharging_time());
  EXPECT_EQ(status.level, received_status.level());
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider.h"

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "services/device/device_service_test_base.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class PlatformSensorProviderTest : public DeviceServiceTestBase {
 public:
  PlatformSensorProviderTest() = default;

  void SetUp() override { provider_.reset(new FakePlatformSensorProvider); }

  void TearDown() override { provider_.reset(); }

 protected:
  std::unique_ptr<FakePlatformSensorProvider> provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderTest);
};

TEST_F(PlatformSensorProviderTest, ResourcesAreFreed) {
  EXPECT_CALL(*provider_, FreeResources()).Times(2);

  provider_->set_next_request_result(FakePlatformSensorProvider::kFailure);
  provider_->CreateSensor(
      device::mojom::SensorType::AMBIENT_LIGHT,
      base::Bind([](scoped_refptr<PlatformSensor> s) { EXPECT_FALSE(s); }));

  provider_->CreateSensor(
      device::mojom::SensorType::AMBIENT_LIGHT,
      base::Bind([](scoped_refptr<PlatformSensor> s) { EXPECT_TRUE(s); }));
}

TEST_F(PlatformSensorProviderTest, ResourcesAreNotFreedOnPendingRequest) {
  EXPECT_CALL(*provider_, FreeResources()).Times(0);

  provider_->set_next_request_result(FakePlatformSensorProvider::kPending);
  provider_->CreateSensor(
      device::mojom::SensorType::AMBIENT_LIGHT,
      base::Bind([](scoped_refptr<PlatformSensor> s) { NOTREACHED(); }));

  provider_->CreateSensor(
      device::mojom::SensorType::AMBIENT_LIGHT,
      base::Bind([](scoped_refptr<PlatformSensor> s) { NOTREACHED(); }));
}

}  // namespace device

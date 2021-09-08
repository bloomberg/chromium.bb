// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/core_location_provider.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/cpp/test/fake_geolocation_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class CoreLocationProviderTest : public testing::Test {
 public:
  std::unique_ptr<CoreLocationProvider> provider_;

 protected:
  CoreLocationProviderTest() {}

  void InitializeProvider() {
    fake_geolocation_manager_ = std::make_unique<FakeGeolocationManager>();
    provider_ = std::make_unique<CoreLocationProvider>(
        base::ThreadTaskRunnerHandle::Get(), fake_geolocation_manager_.get());
  }

  bool IsUpdating() { return fake_geolocation_manager_->watching_position(); }

  // updates the position synchronously
  void FakeUpdatePosition(const mojom::Geoposition position) {
    fake_geolocation_manager_->FakePositionUpdated(position);
  }

  const mojom::Geoposition& GetLatestPosition() {
    return provider_->GetPosition();
  }

  base::test::TaskEnvironment task_environment_;
  const LocationProvider::LocationProviderUpdateCallback callback_;
  std::unique_ptr<FakeGeolocationManager> fake_geolocation_manager_;
};

TEST_F(CoreLocationProviderTest, CreateDestroy) {
  InitializeProvider();
  EXPECT_TRUE(provider_);
  provider_.reset();
}

TEST_F(CoreLocationProviderTest, StartAndStopUpdating) {
  InitializeProvider();
  fake_geolocation_manager_->SetSystemPermission(
      LocationSystemPermissionStatus::kAllowed);
  base::RunLoop().RunUntilIdle();
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_TRUE(IsUpdating());
  provider_->StopProvider();
  EXPECT_FALSE(IsUpdating());
  provider_.reset();
}

TEST_F(CoreLocationProviderTest, DontStartUpdatingIfPermissionDenied) {
  InitializeProvider();
  fake_geolocation_manager_->SetSystemPermission(
      LocationSystemPermissionStatus::kDenied);
  base::RunLoop().RunUntilIdle();
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_FALSE(IsUpdating());
  provider_.reset();
}

TEST_F(CoreLocationProviderTest, DontStartUpdatingUntilPermissionGranted) {
  InitializeProvider();
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_FALSE(IsUpdating());
  fake_geolocation_manager_->SetSystemPermission(
      LocationSystemPermissionStatus::kDenied);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsUpdating());
  fake_geolocation_manager_->SetSystemPermission(
      LocationSystemPermissionStatus::kAllowed);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsUpdating());
  provider_.reset();
}

TEST_F(CoreLocationProviderTest, GetPositionUpdates) {
  InitializeProvider();
  fake_geolocation_manager_->SetSystemPermission(
      LocationSystemPermissionStatus::kAllowed);
  base::RunLoop().RunUntilIdle();
  provider_->StartProvider(/*high_accuracy=*/true);
  EXPECT_TRUE(IsUpdating());

  // test info
  double latitude = 147.147;
  double longitude = 101.101;
  double altitude = 417.417;
  double accuracy = 10.5;
  double altitude_accuracy = 15.5;

  mojom::Geoposition test_position;
  test_position.latitude = latitude;
  test_position.longitude = longitude;
  test_position.altitude = altitude;
  test_position.accuracy = accuracy;
  test_position.altitude_accuracy = altitude_accuracy;
  test_position.timestamp = base::Time::Now();

  base::RunLoop loop;
  provider_->SetUpdateCallback(
      base::BindLambdaForTesting([&](const LocationProvider* provider,
                                     const mojom::Geoposition& position) {
        EXPECT_TRUE(test_position.Equals(position));
        loop.Quit();
      }));
  FakeUpdatePosition(test_position);
  loop.Run();

  EXPECT_TRUE(GetLatestPosition().Equals(test_position));

  provider_->StopProvider();
  EXPECT_FALSE(IsUpdating());
  provider_.reset();
}

}  // namespace device

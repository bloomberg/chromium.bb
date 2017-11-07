// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/device_service_test_base.h"
#include "services/device/generic_sensor/absolute_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_magnetometer.h"
#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"
#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace device {

using mojom::SensorType;

class PlatformSensorFusionTest : public DeviceServiceTestBase {
 public:
  PlatformSensorFusionTest() {
    provider_ = base::MakeUnique<FakePlatformSensorProvider>();
    PlatformSensorProvider::SetProviderForTesting(provider_.get());
  }

  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    // Default
    ON_CALL(*provider_, DoCreateSensorInternal(_, _, _))
        .WillByDefault(Invoke(
            [](mojom::SensorType, scoped_refptr<PlatformSensor> sensor,
               const PlatformSensorProvider::CreateSensorCallback& callback) {
              callback.Run(std::move(sensor));
            }));
  }

 protected:
  void AccelerometerCallback(scoped_refptr<PlatformSensor> sensor) {
    accelerometer_callback_called_ = true;
    accelerometer_ = std::move(sensor);
  }

  void MagnetometerCallback(scoped_refptr<PlatformSensor> sensor) {
    magnetometer_callback_called_ = true;
    magnetometer_ = std::move(sensor);
  }

  void PlatformSensorFusionCallback(scoped_refptr<PlatformSensor> sensor) {
    platform_sensor_fusion_callback_called_ = true;
    fusion_sensor_ = std::move(sensor);
  }

  void CreateAccelerometer() {
    auto callback = base::Bind(&PlatformSensorFusionTest::AccelerometerCallback,
                               base::Unretained(this));
    provider_->CreateSensor(SensorType::ACCELEROMETER, callback);
    EXPECT_TRUE(accelerometer_callback_called_);
    EXPECT_TRUE(accelerometer_);
    EXPECT_EQ(SensorType::ACCELEROMETER, accelerometer_->GetType());
  }

  void CreateMagnetometer() {
    auto callback = base::Bind(&PlatformSensorFusionTest::MagnetometerCallback,
                               base::Unretained(this));
    provider_->CreateSensor(SensorType::MAGNETOMETER, callback);
    EXPECT_TRUE(magnetometer_callback_called_);
    EXPECT_TRUE(magnetometer_);
    EXPECT_EQ(SensorType::MAGNETOMETER, magnetometer_->GetType());
  }

  void CreateLinearAccelerationFusionSensor() {
    auto callback =
        base::Bind(&PlatformSensorFusionTest::PlatformSensorFusionCallback,
                   base::Unretained(this));
    auto fusion_algorithm =
        base::MakeUnique<LinearAccelerationFusionAlgorithmUsingAccelerometer>();
    PlatformSensorFusion::Create(
        provider_->GetMapping(SensorType::LINEAR_ACCELERATION), provider_.get(),
        std::move(fusion_algorithm), callback);
    EXPECT_TRUE(platform_sensor_fusion_callback_called_);
  }

  void CreateAbsoluteOrientationEulerAnglesFusionSensor() {
    auto callback =
        base::Bind(&PlatformSensorFusionTest::PlatformSensorFusionCallback,
                   base::Unretained(this));
    auto fusion_algorithm = base::MakeUnique<
        AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer>();
    PlatformSensorFusion::Create(
        provider_->GetMapping(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES),
        provider_.get(), std::move(fusion_algorithm), callback);
    EXPECT_TRUE(platform_sensor_fusion_callback_called_);
  }

  std::unique_ptr<FakePlatformSensorProvider> provider_;
  bool accelerometer_callback_called_ = false;
  scoped_refptr<PlatformSensor> accelerometer_;
  bool magnetometer_callback_called_ = false;
  scoped_refptr<PlatformSensor> magnetometer_;
  bool platform_sensor_fusion_callback_called_ = false;
  scoped_refptr<PlatformSensor> fusion_sensor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformSensorFusionTest);
};

// The following code tests creating a fusion sensor that needs one source
// sensor.

TEST_F(PlatformSensorFusionTest, SourceSensorAlreadyExists) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  CreateAccelerometer();
  // Now the source sensor already exists.
  EXPECT_TRUE(provider_->GetSensor(SensorType::ACCELEROMETER));

  CreateLinearAccelerationFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::LINEAR_ACCELERATION, fusion_sensor_->GetType());
}

TEST_F(PlatformSensorFusionTest, SourceSensorWorksSeparately) {
  CreateAccelerometer();
  scoped_refptr<PlatformSensor> accel =
      provider_->GetSensor(SensorType::ACCELEROMETER);
  EXPECT_TRUE(accel);
  EXPECT_FALSE(accel->IsActiveForTesting());

  auto client = base::MakeUnique<testing::NiceMock<MockPlatformSensorClient>>();
  accel->AddClient(client.get());
  accel->StartListening(client.get(), PlatformSensorConfiguration(10));
  EXPECT_TRUE(accel->IsActiveForTesting());

  CreateLinearAccelerationFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::LINEAR_ACCELERATION, fusion_sensor_->GetType());
  EXPECT_FALSE(fusion_sensor_->IsActiveForTesting());

  fusion_sensor_->AddClient(client.get());
  fusion_sensor_->StartListening(client.get(), PlatformSensorConfiguration(10));
  EXPECT_TRUE(fusion_sensor_->IsActiveForTesting());

  fusion_sensor_->StopListening(client.get(), PlatformSensorConfiguration(10));
  EXPECT_FALSE(fusion_sensor_->IsActiveForTesting());

  EXPECT_TRUE(accel->IsActiveForTesting());

  accel->RemoveClient(client.get());
  EXPECT_FALSE(accel->IsActiveForTesting());

  fusion_sensor_->RemoveClient(client.get());
}

TEST_F(PlatformSensorFusionTest, SourceSensorNeedsToBeCreated) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));

  CreateLinearAccelerationFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::LINEAR_ACCELERATION, fusion_sensor_->GetType());
}

TEST_F(PlatformSensorFusionTest, SourceSensorIsNotAvailable) {
  // Accelerometer is not available.
  ON_CALL(*provider_, DoCreateSensorInternal(SensorType::ACCELEROMETER, _, _))
      .WillByDefault(Invoke(
          [](mojom::SensorType, scoped_refptr<PlatformSensor>,
             const FakePlatformSensorProvider::CreateSensorCallback& callback) {
            callback.Run(nullptr);
          }));

  CreateLinearAccelerationFusionSensor();
  EXPECT_FALSE(fusion_sensor_);
}

// The following code tests creating a fusion sensor that needs two source
// sensors.

TEST_F(PlatformSensorFusionTest, BothSourceSensorsAlreadyExist) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  CreateAccelerometer();
  EXPECT_TRUE(provider_->GetSensor(SensorType::ACCELEROMETER));

  EXPECT_FALSE(provider_->GetSensor(SensorType::MAGNETOMETER));
  CreateMagnetometer();
  EXPECT_TRUE(provider_->GetSensor(SensorType::MAGNETOMETER));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES,
            fusion_sensor_->GetType());
}

TEST_F(PlatformSensorFusionTest, BothSourceSensorsNeedToBeCreated) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  EXPECT_FALSE(provider_->GetSensor(SensorType::MAGNETOMETER));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES,
            fusion_sensor_->GetType());
}

TEST_F(PlatformSensorFusionTest, BothSourceSensorsAreNotAvailable) {
  // Failure.
  ON_CALL(*provider_, DoCreateSensorInternal(_, _, _))
      .WillByDefault(Invoke(
          [](mojom::SensorType, scoped_refptr<PlatformSensor>,
             const FakePlatformSensorProvider::CreateSensorCallback& callback) {
            callback.Run(nullptr);
          }));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_FALSE(fusion_sensor_);
}

TEST_F(PlatformSensorFusionTest,
       OneSourceSensorAlreadyExistsTheOtherSourceSensorNeedsToBeCreated) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  CreateAccelerometer();
  EXPECT_TRUE(provider_->GetSensor(SensorType::ACCELEROMETER));
  EXPECT_FALSE(provider_->GetSensor(SensorType::MAGNETOMETER));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_TRUE(fusion_sensor_);
  EXPECT_EQ(SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES,
            fusion_sensor_->GetType());
}

TEST_F(PlatformSensorFusionTest,
       OneSourceSensorAlreadyExistsTheOtherSourceSensorIsNotAvailable) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  CreateAccelerometer();
  EXPECT_TRUE(provider_->GetSensor(SensorType::ACCELEROMETER));

  // Magnetometer is not available.
  ON_CALL(*provider_, DoCreateSensorInternal(SensorType::MAGNETOMETER, _, _))
      .WillByDefault(Invoke(
          [](mojom::SensorType, scoped_refptr<PlatformSensor>,
             const FakePlatformSensorProvider::CreateSensorCallback& callback) {
            callback.Run(nullptr);
          }));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_FALSE(fusion_sensor_);
}

TEST_F(PlatformSensorFusionTest,
       OneSourceSensorNeedsToBeCreatedTheOtherSourceSensorIsNotAvailable) {
  EXPECT_FALSE(provider_->GetSensor(SensorType::ACCELEROMETER));
  // Magnetometer is not available.
  ON_CALL(*provider_, DoCreateSensorInternal(SensorType::MAGNETOMETER, _, _))
      .WillByDefault(Invoke(
          [](mojom::SensorType, scoped_refptr<PlatformSensor>,
             const FakePlatformSensorProvider::CreateSensorCallback& callback) {
            callback.Run(nullptr);
          }));

  CreateAbsoluteOrientationEulerAnglesFusionSensor();
  EXPECT_FALSE(fusion_sensor_);
}

}  //  namespace device

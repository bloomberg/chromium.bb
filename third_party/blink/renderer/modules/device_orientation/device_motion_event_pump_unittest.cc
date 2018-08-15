// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>

#include "base/run_loop.h"
#include "services/device/public/cpp/test/fake_sensor_and_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/platform_event_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_pump.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace blink {

using device::FakeSensorProvider;

class MockDeviceMotionController final
    : public GarbageCollectedFinalized<MockDeviceMotionController>,
      public PlatformEventController {
  USING_GARBAGE_COLLECTED_MIXIN(MockDeviceMotionController);

 public:
  explicit MockDeviceMotionController(DeviceMotionEventPump* motion_pump)
      : PlatformEventController(nullptr),
        did_change_device_motion_(false),
        motion_pump_(motion_pump) {}
  ~MockDeviceMotionController() override {}

  void Trace(Visitor* visitor) override {
    PlatformEventController::Trace(visitor);
    visitor->Trace(motion_pump_);
  }

  void DidUpdateData() override {
    did_change_device_motion_ = true;
    ++number_of_events_;
  }

  bool did_change_device_motion() const { return did_change_device_motion_; }

  int number_of_events() const { return number_of_events_; }

  void RegisterWithDispatcher() override { motion_pump_->AddController(this); }

  bool HasLastData() override { return motion_pump_->LatestDeviceMotionData(); }

  void UnregisterWithDispatcher() override {
    motion_pump_->RemoveController(this);
  }

  const DeviceMotionData* data() {
    return motion_pump_->LatestDeviceMotionData();
  };

  DeviceMotionEventPump* motion_pump() { return motion_pump_.Get(); }

 private:
  bool did_change_device_motion_;
  int number_of_events_;
  Member<DeviceMotionEventPump> motion_pump_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceMotionController);
};

class DeviceMotionEventPumpTest : public testing::Test {
 public:
  DeviceMotionEventPumpTest() = default;

 protected:
  void SetUp() override {
    device::mojom::SensorProviderPtrInfo sensor_provider_ptr_info;
    sensor_provider_.Bind(mojo::MakeRequest(&sensor_provider_ptr_info));
    auto* motion_pump =
        new DeviceMotionEventPump(base::ThreadTaskRunnerHandle::Get());
    motion_pump->SetSensorProviderForTesting(
        device::mojom::blink::SensorProviderPtr(
            device::mojom::blink::SensorProviderPtrInfo(
                sensor_provider_ptr_info.PassHandle(),
                device::mojom::SensorProvider::Version_)));

    controller_ = new MockDeviceMotionController(motion_pump);

    ExpectAllThreeSensorsStateToBe(
        DeviceMotionEventPump::SensorState::NOT_INITIALIZED);
    EXPECT_EQ(DeviceMotionEventPump::PumpState::STOPPED,
              controller_->motion_pump()->GetPumpStateForTesting());
  }

  void FireEvent() { controller_->motion_pump()->FireEvent(nullptr); }

  void ExpectAccelerometerStateToBe(
      DeviceMotionEventPump::SensorState expected_sensor_state) {
    EXPECT_EQ(expected_sensor_state,
              controller_->motion_pump()->accelerometer_.sensor_state);
  }

  void ExpectLinearAccelerationSensorStateToBe(
      DeviceMotionEventPump::SensorState expected_sensor_state) {
    EXPECT_EQ(
        expected_sensor_state,
        controller_->motion_pump()->linear_acceleration_sensor_.sensor_state);
  }

  void ExpectGyroscopeStateToBe(
      DeviceMotionEventPump::SensorState expected_sensor_state) {
    EXPECT_EQ(expected_sensor_state,
              controller_->motion_pump()->gyroscope_.sensor_state);
  }

  void ExpectAllThreeSensorsStateToBe(
      DeviceMotionEventPump::SensorState expected_sensor_state) {
    ExpectAccelerometerStateToBe(expected_sensor_state);
    ExpectLinearAccelerationSensorStateToBe(expected_sensor_state);
    ExpectGyroscopeStateToBe(expected_sensor_state);
  }

  MockDeviceMotionController* controller() { return controller_.Get(); }

  FakeSensorProvider* sensor_provider() { return &sensor_provider_; }

 private:
  Persistent<MockDeviceMotionController> controller_;

  FakeSensorProvider sensor_provider_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMotionEventPumpTest);
};

TEST_F(DeviceMotionEventPumpTest, MultipleStartAndStopWithWait) {
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::ACTIVE);
  EXPECT_EQ(DeviceMotionEventPump::PumpState::RUNNING,
            controller()->motion_pump()->GetPumpStateForTesting());

  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
  EXPECT_EQ(DeviceMotionEventPump::PumpState::STOPPED,
            controller()->motion_pump()->GetPumpStateForTesting());

  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::ACTIVE);
  EXPECT_EQ(DeviceMotionEventPump::PumpState::RUNNING,
            controller()->motion_pump()->GetPumpStateForTesting());

  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
  EXPECT_EQ(DeviceMotionEventPump::PumpState::STOPPED,
            controller()->motion_pump()->GetPumpStateForTesting());
}

TEST_F(DeviceMotionEventPumpTest, CallStop) {
  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(
      DeviceMotionEventPump::SensorState::NOT_INITIALIZED);
}

TEST_F(DeviceMotionEventPumpTest, CallStartAndStop) {
  controller()->motion_pump()->Start(nullptr);
  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, CallStartMultipleTimes) {
  controller()->motion_pump()->Start(nullptr);
  controller()->motion_pump()->Start(nullptr);
  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, CallStopMultipleTimes) {
  controller()->motion_pump()->Start(nullptr);
  controller()->motion_pump()->Stop();
  controller()->motion_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
}

// Test multiple DeviceSensorEventPump::Start() calls only bind sensor once.
TEST_F(DeviceMotionEventPumpTest, SensorOnlyBindOnce) {
  controller()->motion_pump()->Start(nullptr);
  controller()->motion_pump()->Stop();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::ACTIVE);

  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, AllSensorsAreActive) {
  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  sensor_provider()->UpdateLinearAccelerationSensorData(4, 5, 6);
  sensor_provider()->UpdateGyroscopeData(7, 8, 9);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  EXPECT_TRUE(received_data->GetAccelerationIncludingGravity()->CanProvideX());
  EXPECT_EQ(1, received_data->GetAccelerationIncludingGravity()->X());
  EXPECT_TRUE(received_data->GetAccelerationIncludingGravity()->CanProvideY());
  EXPECT_EQ(2, received_data->GetAccelerationIncludingGravity()->Y());
  EXPECT_TRUE(received_data->GetAccelerationIncludingGravity()->CanProvideZ());
  EXPECT_EQ(3, received_data->GetAccelerationIncludingGravity()->Z());

  EXPECT_TRUE(received_data->GetAcceleration()->CanProvideX());
  EXPECT_EQ(4, received_data->GetAcceleration()->X());
  EXPECT_TRUE(received_data->GetAcceleration()->CanProvideY());
  EXPECT_EQ(5, received_data->GetAcceleration()->Y());
  EXPECT_TRUE(received_data->GetAcceleration()->CanProvideZ());
  EXPECT_EQ(6, received_data->GetAcceleration()->Z());

  EXPECT_TRUE(received_data->GetRotationRate()->CanProvideAlpha());
  EXPECT_EQ(gfx::RadToDeg(7.0), received_data->GetRotationRate()->Alpha());
  EXPECT_TRUE(received_data->GetRotationRate()->CanProvideBeta());
  EXPECT_EQ(gfx::RadToDeg(8.0), received_data->GetRotationRate()->Beta());
  EXPECT_TRUE(received_data->GetRotationRate()->CanProvideGamma());
  EXPECT_EQ(gfx::RadToDeg(9.0), received_data->GetRotationRate()->Gamma());

  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, TwoSensorsAreActive) {
  sensor_provider()->set_linear_acceleration_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAccelerometerStateToBe(DeviceMotionEventPump::SensorState::ACTIVE);
  ExpectLinearAccelerationSensorStateToBe(
      DeviceMotionEventPump::SensorState::NOT_INITIALIZED);
  ExpectGyroscopeStateToBe(DeviceMotionEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  sensor_provider()->UpdateGyroscopeData(7, 8, 9);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  EXPECT_TRUE(received_data->GetAccelerationIncludingGravity()->CanProvideX());
  EXPECT_EQ(1, received_data->GetAccelerationIncludingGravity()->X());
  EXPECT_TRUE(received_data->GetAccelerationIncludingGravity()->CanProvideY());
  EXPECT_EQ(2, received_data->GetAccelerationIncludingGravity()->Y());
  EXPECT_TRUE(received_data->GetAccelerationIncludingGravity()->CanProvideZ());
  EXPECT_EQ(3, received_data->GetAccelerationIncludingGravity()->Z());

  EXPECT_FALSE(received_data->GetAcceleration()->CanProvideX());
  EXPECT_FALSE(received_data->GetAcceleration()->CanProvideY());
  EXPECT_FALSE(received_data->GetAcceleration()->CanProvideZ());

  EXPECT_TRUE(received_data->GetRotationRate()->CanProvideAlpha());
  EXPECT_EQ(gfx::RadToDeg(7.0), received_data->GetRotationRate()->Alpha());
  EXPECT_TRUE(received_data->GetRotationRate()->CanProvideBeta());
  EXPECT_EQ(gfx::RadToDeg(8.0), received_data->GetRotationRate()->Beta());
  EXPECT_TRUE(received_data->GetRotationRate()->CanProvideGamma());
  EXPECT_EQ(gfx::RadToDeg(9.0), received_data->GetRotationRate()->Gamma());

  controller()->motion_pump()->Stop();

  ExpectAccelerometerStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
  ExpectLinearAccelerationSensorStateToBe(
      DeviceMotionEventPump::SensorState::NOT_INITIALIZED);
  ExpectGyroscopeStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, SomeSensorDataFieldsNotAvailable) {
  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateAccelerometerData(NAN, 2, 3);
  sensor_provider()->UpdateLinearAccelerationSensorData(4, NAN, 6);
  sensor_provider()->UpdateGyroscopeData(7, 8, NAN);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  EXPECT_FALSE(received_data->GetAccelerationIncludingGravity()->CanProvideX());
  EXPECT_TRUE(received_data->GetAccelerationIncludingGravity()->CanProvideY());
  EXPECT_EQ(2, received_data->GetAccelerationIncludingGravity()->Y());
  EXPECT_TRUE(received_data->GetAccelerationIncludingGravity()->CanProvideZ());
  EXPECT_EQ(3, received_data->GetAccelerationIncludingGravity()->Z());

  EXPECT_TRUE(received_data->GetAcceleration()->CanProvideX());
  EXPECT_EQ(4, received_data->GetAcceleration()->X());
  EXPECT_FALSE(received_data->GetAcceleration()->CanProvideY());
  EXPECT_TRUE(received_data->GetAcceleration()->CanProvideZ());
  EXPECT_EQ(6, received_data->GetAcceleration()->Z());

  EXPECT_TRUE(received_data->GetRotationRate()->CanProvideAlpha());
  EXPECT_EQ(gfx::RadToDeg(7.0), received_data->GetRotationRate()->Alpha());
  EXPECT_TRUE(received_data->GetRotationRate()->CanProvideBeta());
  EXPECT_EQ(gfx::RadToDeg(8.0), received_data->GetRotationRate()->Beta());
  EXPECT_FALSE(received_data->GetRotationRate()->CanProvideGamma());

  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceMotionEventPumpTest, FireAllNullEvent) {
  // No active sensors.
  sensor_provider()->set_accelerometer_is_available(false);
  sensor_provider()->set_linear_acceleration_sensor_is_available(false);
  sensor_provider()->set_gyroscope_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(
      DeviceMotionEventPump::SensorState::NOT_INITIALIZED);

  FireEvent();

  const DeviceMotionData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_motion());

  EXPECT_FALSE(received_data->GetAcceleration()->CanProvideX());
  EXPECT_FALSE(received_data->GetAcceleration()->CanProvideY());
  EXPECT_FALSE(received_data->GetAcceleration()->CanProvideZ());

  EXPECT_FALSE(received_data->GetAccelerationIncludingGravity()->CanProvideX());
  EXPECT_FALSE(received_data->GetAccelerationIncludingGravity()->CanProvideY());
  EXPECT_FALSE(received_data->GetAccelerationIncludingGravity()->CanProvideZ());

  EXPECT_FALSE(received_data->GetRotationRate()->CanProvideAlpha());
  EXPECT_FALSE(received_data->GetRotationRate()->CanProvideBeta());
  EXPECT_FALSE(received_data->GetRotationRate()->CanProvideGamma());

  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(
      DeviceMotionEventPump::SensorState::NOT_INITIALIZED);
}

TEST_F(DeviceMotionEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZero) {
  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::ACTIVE);

  FireEvent();
  EXPECT_FALSE(controller()->did_change_device_motion());

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  FireEvent();
  EXPECT_FALSE(controller()->did_change_device_motion());

  sensor_provider()->UpdateLinearAccelerationSensorData(4, 5, 6);
  FireEvent();
  EXPECT_FALSE(controller()->did_change_device_motion());

  sensor_provider()->UpdateGyroscopeData(7, 8, 9);
  FireEvent();
  // Event is fired only after all the available sensors have data.
  EXPECT_TRUE(controller()->did_change_device_motion());

  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);
}

// Confirm that the frequency of pumping events is not greater than 60Hz.
// A rate above 60Hz would allow for the detection of keystrokes.
// (crbug.com/421691)
TEST_F(DeviceMotionEventPumpTest, PumpThrottlesEventRate) {
  // Confirm that the delay for pumping events is 60 Hz.
  EXPECT_GE(60, WTF::Time::kMicrosecondsPerSecond /
                    DeviceMotionEventPump::kDefaultPumpDelayMicroseconds);

  controller()->RegisterWithDispatcher();
  controller()->motion_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateAccelerometerData(1, 2, 3);
  sensor_provider()->UpdateLinearAccelerationSensorData(4, 5, 6);
  sensor_provider()->UpdateGyroscopeData(7, 8, 9);

  base::RunLoop loop;
  blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
      FROM_HERE, loop.QuitWhenIdleClosure(),
      WTF::TimeDelta::FromMilliseconds(100));
  loop.Run();
  controller()->motion_pump()->Stop();

  ExpectAllThreeSensorsStateToBe(DeviceMotionEventPump::SensorState::SUSPENDED);

  // Check that the PlatformEventController does not receive excess
  // events.
  EXPECT_TRUE(controller()->did_change_device_motion());
  EXPECT_GE(6, controller()->number_of_events());
}

}  // namespace blink

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
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event_pump.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace {

constexpr double kEpsilon = 1e-8;

}  // namespace

namespace blink {

using device::FakeSensorProvider;

class MockDeviceOrientationController final
    : public GarbageCollectedFinalized<MockDeviceOrientationController>,
      public PlatformEventController {
  USING_GARBAGE_COLLECTED_MIXIN(MockDeviceOrientationController);

 public:
  explicit MockDeviceOrientationController(
      DeviceOrientationEventPump* orientation_pump)
      : PlatformEventController(nullptr),
        did_change_device_orientation_(false),
        orientation_pump_(orientation_pump) {}
  ~MockDeviceOrientationController() override {}

  void Trace(Visitor* visitor) override {
    PlatformEventController::Trace(visitor);
    visitor->Trace(orientation_pump_);
  }

  void DidUpdateData() override { did_change_device_orientation_ = true; }

  bool did_change_device_orientation() const {
    return did_change_device_orientation_;
  }
  void set_did_change_device_orientation(bool value) {
    did_change_device_orientation_ = value;
  }

  void RegisterWithDispatcher() override {
    orientation_pump_->AddController(this);
  }

  bool HasLastData() override {
    return orientation_pump_->LatestDeviceOrientationData();
  }

  void UnregisterWithDispatcher() override {
    orientation_pump_->RemoveController(this);
  }

  const DeviceOrientationData* data() {
    return orientation_pump_->LatestDeviceOrientationData();
  };

  DeviceOrientationEventPump* orientation_pump() {
    return orientation_pump_.Get();
  }

 private:
  bool did_change_device_orientation_;
  Member<DeviceOrientationEventPump> orientation_pump_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceOrientationController);
};

class DeviceOrientationEventPumpTest : public testing::Test {
 public:
  DeviceOrientationEventPumpTest() = default;

 protected:
  void SetUp() override {
    device::mojom::SensorProviderPtrInfo sensor_provider_ptr_info;
    sensor_provider_.Bind(mojo::MakeRequest(&sensor_provider_ptr_info));
    auto* orientation_pump = MakeGarbageCollected<DeviceOrientationEventPump>(
        base::ThreadTaskRunnerHandle::Get(), false /* absolute */);
    orientation_pump->SetSensorProviderForTesting(
        device::mojom::blink::SensorProviderPtr(
            device::mojom::blink::SensorProviderPtrInfo(
                sensor_provider_ptr_info.PassHandle(),
                device::mojom::SensorProvider::Version_)));

    controller_ =
        MakeGarbageCollected<MockDeviceOrientationController>(orientation_pump);

    ExpectRelativeOrientationSensorStateToBe(
        DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
    ExpectAbsoluteOrientationSensorStateToBe(
        DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
    EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
              controller_->orientation_pump()->GetPumpStateForTesting());
  }

  void FireEvent() { controller_->orientation_pump()->FireEvent(nullptr); }

  void ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState expected_sensor_state) {
    EXPECT_EQ(expected_sensor_state,
              controller_->orientation_pump()
                  ->relative_orientation_sensor_.sensor_state);
  }

  void ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState expected_sensor_state) {
    EXPECT_EQ(expected_sensor_state,
              controller_->orientation_pump()
                  ->absolute_orientation_sensor_.sensor_state);
  }

  MockDeviceOrientationController* controller() { return controller_.Get(); }

  FakeSensorProvider* sensor_provider() { return &sensor_provider_; }

 private:
  Persistent<MockDeviceOrientationController> controller_;
  FakeSensorProvider sensor_provider_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOrientationEventPumpTest);
};

TEST_F(DeviceOrientationEventPumpTest, MultipleStartAndStopWithWait) {
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());
}

TEST_F(DeviceOrientationEventPumpTest,
       MultipleStartAndStopWithWaitWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());
}

TEST_F(DeviceOrientationEventPumpTest, CallStop) {
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
}

TEST_F(DeviceOrientationEventPumpTest, CallStopWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
}

TEST_F(DeviceOrientationEventPumpTest, CallStartAndStop) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, CallStartAndStopWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, CallStartMultipleTimes) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest,
       CallStartMultipleTimesWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, CallStopMultipleTimes) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest,
       CallStopMultipleTimesWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

// Test a sequence of Start(), Stop(), Start() calls only bind sensor once.
TEST_F(DeviceOrientationEventPumpTest, SensorOnlyBindOnce) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

// Test when using fallback from relative orientation to absolute orientation,
// a sequence of Start(), Stop(), Start() calls only bind sensor once.
TEST_F(DeviceOrientationEventPumpTest, SensorOnlyBindOnceWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, SensorIsActive) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateRelativeOrientationSensorData(
      1 /* alpha */, 2 /* beta */, 3 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  // DeviceOrientation Event provides relative orientation data when it is
  // available.
  EXPECT_DOUBLE_EQ(1, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(2, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(3, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, SensorIsActiveWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, 5 /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  // DeviceOrientation Event provides absolute orientation data when relative
  // orientation data is not available but absolute orientation data is
  // available.
  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());

  // Since no relative orientation data is available, DeviceOrientationEvent
  // fallback to provide absolute orientation data.
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, SomeSensorDataFieldsNotAvailable) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateRelativeOrientationSensorData(
      NAN /* alpha */, 2 /* beta */, 3 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_FALSE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(2, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(3, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest,
       SomeSensorDataFieldsNotAvailableWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, NAN /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  // DeviceOrientation Event provides absolute orientation data when relative
  // orientation data is not available but absolute orientation data is
  // available.
  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_FALSE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  // Since no relative orientation data is available, DeviceOrientationEvent
  // fallback to provide absolute orientation data.
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, FireAllNullEvent) {
  // No active sensors.
  sensor_provider()->set_relative_orientation_sensor_is_available(false);
  sensor_provider()->set_absolute_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_FALSE(received_data->CanProvideAlpha());
  EXPECT_FALSE(received_data->CanProvideBeta());
  EXPECT_FALSE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
}

TEST_F(DeviceOrientationEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZero) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  FireEvent();

  EXPECT_FALSE(controller()->did_change_device_orientation());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZeroWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  FireEvent();

  EXPECT_FALSE(controller()->did_change_device_orientation());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest, UpdateRespectsOrientationThreshold) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateRelativeOrientationSensorData(
      1 /* alpha */, 2 /* beta */, 3 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  // DeviceOrientation Event provides relative orientation data when it is
  // available.
  EXPECT_DOUBLE_EQ(1, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(2, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(3, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateRelativeOrientationSensorData(
      1 + DeviceOrientationEventPump::kOrientationThreshold / 2.0 /* alpha */,
      2 /* beta */, 3 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_FALSE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(1, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(2, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(3, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateRelativeOrientationSensorData(
      1 + DeviceOrientationEventPump::kOrientationThreshold /* alpha */,
      2 /* beta */, 3 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(1 + DeviceOrientationEventPump::kOrientationThreshold,
                   received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(2, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(3, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_FALSE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceOrientationEventPumpTest,
       UpdateRespectsOrientationThresholdWithSensorFallback) {
  sensor_provider()->set_relative_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, 5 /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  // DeviceOrientation Event provides absolute orientation data when relative
  // orientation data is not available but absolute orientation data is
  // available.
  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  // Since no relative orientation data is available, DeviceOrientationEvent
  // fallback to provide absolute orientation data.
  EXPECT_TRUE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */,
      5 + DeviceOrientationEventPump::kOrientationThreshold / 2.0 /* beta */,
      6 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_FALSE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */,
      5 + DeviceOrientationEventPump::kOrientationThreshold +
          kEpsilon /* beta */,
      6 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(
      5 + DeviceOrientationEventPump::kOrientationThreshold + kEpsilon,
      received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectRelativeOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

class DeviceAbsoluteOrientationEventPumpTest : public testing::Test {
 public:
  DeviceAbsoluteOrientationEventPumpTest() = default;

 protected:
  void SetUp() override {
    device::mojom::SensorProviderPtrInfo sensor_provider_ptr_info;
    sensor_provider_.Bind(mojo::MakeRequest(&sensor_provider_ptr_info));
    auto* absolute_orientation_pump =
        MakeGarbageCollected<DeviceOrientationEventPump>(
            base::ThreadTaskRunnerHandle::Get(), true /* absolute */);
    absolute_orientation_pump->SetSensorProviderForTesting(
        device::mojom::blink::SensorProviderPtr(
            device::mojom::blink::SensorProviderPtrInfo(
                sensor_provider_ptr_info.PassHandle(),
                device::mojom::SensorProvider::Version_)));
    controller_ = MakeGarbageCollected<MockDeviceOrientationController>(
        absolute_orientation_pump);

    ExpectAbsoluteOrientationSensorStateToBe(
        DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
    EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
              controller_->orientation_pump()->GetPumpStateForTesting());
  }

  void FireEvent() { controller_->orientation_pump()->FireEvent(nullptr); }

  void ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState expected_sensor_state) {
    EXPECT_EQ(expected_sensor_state,
              controller_->orientation_pump()
                  ->absolute_orientation_sensor_.sensor_state);
  }

  MockDeviceOrientationController* controller() { return controller_.Get(); }

  FakeSensorProvider* sensor_provider() { return &sensor_provider_; }

 private:
  Persistent<MockDeviceOrientationController> controller_;
  FakeSensorProvider sensor_provider_;

  DISALLOW_COPY_AND_ASSIGN(DeviceAbsoluteOrientationEventPumpTest);
};

TEST_F(DeviceAbsoluteOrientationEventPumpTest, MultipleStartAndStopWithWait) {
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::RUNNING,
            controller()->orientation_pump()->GetPumpStateForTesting());

  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
  EXPECT_EQ(DeviceOrientationEventPump::PumpState::STOPPED,
            controller()->orientation_pump()->GetPumpStateForTesting());
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, CallStop) {
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, CallStartAndStop) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, CallStartMultipleTimes) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, CallStopMultipleTimes) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Stop();
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

// Test multiple DeviceSensorEventPump::Start() calls only bind sensor once.
TEST_F(DeviceAbsoluteOrientationEventPumpTest, SensorOnlyBindOnce) {
  controller()->orientation_pump()->Start(nullptr);
  controller()->orientation_pump()->Stop();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, SensorIsActive) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, 5 /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest,
       SomeSensorDataFieldsNotAvailable) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, NAN /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_FALSE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest, FireAllNullEvent) {
  // No active sensor.
  sensor_provider()->set_absolute_orientation_sensor_is_available(false);

  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_FALSE(received_data->CanProvideAlpha());
  EXPECT_FALSE(received_data->CanProvideBeta());
  EXPECT_FALSE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::NOT_INITIALIZED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest,
       NotFireEventWhenSensorReadingTimeStampIsZero) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  FireEvent();

  EXPECT_FALSE(controller()->did_change_device_orientation());

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

TEST_F(DeviceAbsoluteOrientationEventPumpTest,
       UpdateRespectsOrientationThreshold) {
  controller()->RegisterWithDispatcher();
  controller()->orientation_pump()->Start(nullptr);
  base::RunLoop().RunUntilIdle();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::ACTIVE);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */, 5 /* beta */, 6 /* gamma */);

  FireEvent();

  const DeviceOrientationData* received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */,
      5 + DeviceOrientationEventPump::kOrientationThreshold / 2.0 /* beta */,
      6 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_FALSE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(5, received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->set_did_change_device_orientation(false);

  sensor_provider()->UpdateAbsoluteOrientationSensorData(
      4 /* alpha */,
      5 + DeviceOrientationEventPump::kOrientationThreshold +
          kEpsilon /* beta */,
      6 /* gamma */);

  FireEvent();

  received_data = controller()->data();
  EXPECT_TRUE(controller()->did_change_device_orientation());

  EXPECT_DOUBLE_EQ(4, received_data->Alpha());
  EXPECT_TRUE(received_data->CanProvideAlpha());
  EXPECT_DOUBLE_EQ(
      5 + DeviceOrientationEventPump::kOrientationThreshold + kEpsilon,
      received_data->Beta());
  EXPECT_TRUE(received_data->CanProvideBeta());
  EXPECT_DOUBLE_EQ(6, received_data->Gamma());
  EXPECT_TRUE(received_data->CanProvideGamma());
  EXPECT_TRUE(received_data->Absolute());

  controller()->orientation_pump()->Stop();

  ExpectAbsoluteOrientationSensorStateToBe(
      DeviceOrientationEventPump::SensorState::SUSPENDED);
}

}  // namespace blink

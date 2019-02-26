// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/auto_reset.h"
#include "services/device/public/mojom/sensor.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/platform_event_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_acceleration.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_pump.h"
#include "third_party/blink/renderer/modules/device_orientation/device_rotation_rate.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace {

constexpr double kDefaultPumpDelayMilliseconds =
    blink::DeviceMotionEventPump::kDefaultPumpDelayMicroseconds / 1000;

}  // namespace

namespace blink {

DeviceMotionEventPump::DeviceMotionEventPump(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : DeviceSensorEventPump(task_runner),
      accelerometer_(this, device::mojom::blink::SensorType::ACCELEROMETER),
      linear_acceleration_sensor_(
          this,
          device::mojom::blink::SensorType::LINEAR_ACCELERATION),
      gyroscope_(this, device::mojom::blink::SensorType::GYROSCOPE) {}

DeviceMotionEventPump::~DeviceMotionEventPump() {
  StopIfObserving();
}

void DeviceMotionEventPump::SetController(PlatformEventController* controller) {
  DCHECK(controller);
  DCHECK(!controller_);

  controller_ = controller;
  StartListening(controller_->GetDocument()
                     ? controller_->GetDocument()->GetFrame()
                     : nullptr);
}

void DeviceMotionEventPump::RemoveController() {
  controller_ = nullptr;
  StopListening();
}

DeviceMotionData* DeviceMotionEventPump::LatestDeviceMotionData() {
  return data_.Get();
}

void DeviceMotionEventPump::Trace(blink::Visitor* visitor) {
  visitor->Trace(data_);
  visitor->Trace(controller_);
}

void DeviceMotionEventPump::StartListening(LocalFrame* frame) {
  // TODO(crbug.com/850619): ensure a valid frame is passed
  if (!frame)
    return;
  Start(frame);
}

void DeviceMotionEventPump::SendStartMessage(LocalFrame* frame) {
  if (!sensor_provider_) {
    DCHECK(frame);

    frame->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&sensor_provider_));
    sensor_provider_.set_connection_error_handler(
        WTF::Bind(&DeviceSensorEventPump::HandleSensorProviderError,
                  WrapPersistent(this)));
  }

  accelerometer_.Start(sensor_provider_.get());
  linear_acceleration_sensor_.Start(sensor_provider_.get());
  gyroscope_.Start(sensor_provider_.get());
}

void DeviceMotionEventPump::StopListening() {
  Stop();
  data_.Clear();
}

void DeviceMotionEventPump::SendStopMessage() {
  // SendStopMessage() gets called both when the page visibility changes and if
  // all device motion event listeners are unregistered. Since removing the
  // event listener is more rare than the page visibility changing,
  // Sensor::Suspend() is used to optimize this case for not doing extra work.

  accelerometer_.Stop();
  linear_acceleration_sensor_.Stop();
  gyroscope_.Stop();
}

void DeviceMotionEventPump::NotifyController() {
  DCHECK(controller_);
  controller_->DidUpdateData();
}

void DeviceMotionEventPump::FireEvent(TimerBase*) {
  DeviceMotionData* data = GetDataFromSharedMemory();

  // data is null if not all sensors are active
  if (data) {
    data_ = data;
    NotifyController();
  }
}

bool DeviceMotionEventPump::SensorsReadyOrErrored() const {
  return accelerometer_.ReadyOrErrored() &&
         linear_acceleration_sensor_.ReadyOrErrored() &&
         gyroscope_.ReadyOrErrored();
}

DeviceMotionData* DeviceMotionEventPump::GetDataFromSharedMemory() {
  DeviceAcceleration* acceleration = nullptr;
  DeviceAcceleration* acceleration_including_gravity = nullptr;
  DeviceRotationRate* rotation_rate = nullptr;

  if (accelerometer_.SensorReadingCouldBeRead()) {
    if (accelerometer_.reading.timestamp() == 0.0)
      return nullptr;

    acceleration_including_gravity = DeviceAcceleration::Create(
        accelerometer_.reading.accel.x, accelerometer_.reading.accel.y,
        accelerometer_.reading.accel.z);
  } else {
    acceleration_including_gravity = DeviceAcceleration::Create(NAN, NAN, NAN);
  }

  if (linear_acceleration_sensor_.SensorReadingCouldBeRead()) {
    if (linear_acceleration_sensor_.reading.timestamp() == 0.0)
      return nullptr;

    acceleration =
        DeviceAcceleration::Create(linear_acceleration_sensor_.reading.accel.x,
                                   linear_acceleration_sensor_.reading.accel.y,
                                   linear_acceleration_sensor_.reading.accel.z);
  } else {
    acceleration = DeviceAcceleration::Create(NAN, NAN, NAN);
  }

  if (gyroscope_.SensorReadingCouldBeRead()) {
    if (gyroscope_.reading.timestamp() == 0.0)
      return nullptr;

    rotation_rate =
        DeviceRotationRate::Create(gfx::RadToDeg(gyroscope_.reading.gyro.x),
                                   gfx::RadToDeg(gyroscope_.reading.gyro.y),
                                   gfx::RadToDeg(gyroscope_.reading.gyro.z));
  } else {
    rotation_rate = DeviceRotationRate::Create(NAN, NAN, NAN);
  }

  // The device orientation spec states that interval should be in
  // milliseconds.
  // https://w3c.github.io/deviceorientation/spec-source-orientation.html#devicemotion
  return DeviceMotionData::Create(acceleration, acceleration_including_gravity,
                                  rotation_rate, kDefaultPumpDelayMilliseconds);
}
}  // namespace blink

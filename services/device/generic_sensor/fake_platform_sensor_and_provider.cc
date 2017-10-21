// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/fake_platform_sensor_and_provider.h"

namespace device {

FakePlatformSensor::FakePlatformSensor(mojom::SensorType type,
                                       mojo::ScopedSharedBufferMapping mapping,
                                       PlatformSensorProvider* provider)
    : PlatformSensor(type, std::move(mapping), provider) {}

bool FakePlatformSensor::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  SensorReading reading;
  // Only mocking the shared memory update for AMBIENT_LIGHT type is enough.
  if (GetType() == mojom::SensorType::AMBIENT_LIGHT) {
    // Set the shared buffer value as frequency for testing purpose.
    reading.als.value = configuration.frequency();
    UpdateSensorReading(reading);
  }
  return true;
}

bool FakePlatformSensor::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  return configuration.frequency() <= GetMaximumSupportedFrequency() &&
         configuration.frequency() >= GetMinimumSupportedFrequency();
}

PlatformSensorConfiguration FakePlatformSensor::GetDefaultConfiguration() {
  return PlatformSensorConfiguration(30.0);
}

mojom::ReportingMode FakePlatformSensor::GetReportingMode() {
  // Set the ReportingMode as ON_CHANGE, so we can test the
  // SensorReadingChanged() mojo interface.
  return mojom::ReportingMode::ON_CHANGE;
}

double FakePlatformSensor::GetMaximumSupportedFrequency() {
  return 50.0;
}
double FakePlatformSensor::GetMinimumSupportedFrequency() {
  return 1.0;
}

FakePlatformSensorProvider::FakePlatformSensorProvider() = default;
FakePlatformSensorProvider::~FakePlatformSensorProvider() = default;

void FakePlatformSensorProvider::CreateSensorInternal(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  DCHECK(type >= mojom::SensorType::FIRST && type <= mojom::SensorType::LAST);
  if (request_result_ == kFailure) {
    request_result_ = kSuccess;
    callback.Run(nullptr);
    return;
  }

  if (request_result_ == kPending) {
    request_result_ = kSuccess;
    return;
  }

  auto sensor =
      base::MakeRefCounted<FakePlatformSensor>(type, std::move(mapping), this);
  callback.Run(sensor);
}

}  // namespace device

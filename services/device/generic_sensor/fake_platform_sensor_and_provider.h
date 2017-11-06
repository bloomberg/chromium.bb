// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_AND_PROVIDER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_AND_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class FakePlatformSensor : public PlatformSensor {
 public:
  FakePlatformSensor(mojom::SensorType type,
                     mojo::ScopedSharedBufferMapping mapping,
                     PlatformSensorProvider* provider);

  bool StartSensor(const PlatformSensorConfiguration& configuration) override;

  void StopSensor() override {}

  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;

  PlatformSensorConfiguration GetDefaultConfiguration() override;

  mojom::ReportingMode GetReportingMode() override;

  double GetMaximumSupportedFrequency() override;
  double GetMinimumSupportedFrequency() override;

 protected:
  ~FakePlatformSensor() override = default;

  DISALLOW_COPY_AND_ASSIGN(FakePlatformSensor);
};

class FakePlatformSensorProvider : public PlatformSensorProvider {
 public:
  FakePlatformSensorProvider();
  ~FakePlatformSensorProvider() override;

  MOCK_METHOD0(FreeResources, void());
  MOCK_METHOD3(DoCreateSensorInternal,
               void(mojom::SensorType,
                    scoped_refptr<PlatformSensor>,
                    const CreateSensorCallback&));

 private:
  void CreateSensorInternal(mojom::SensorType type,
                            mojo::ScopedSharedBufferMapping mapping,
                            const CreateSensorCallback& callback) override;

  DISALLOW_COPY_AND_ASSIGN(FakePlatformSensorProvider);
};

// Mock for PlatformSensor's client interface that is used to deliver
// error and data changes notifications.
class MockPlatformSensorClient : public PlatformSensor::Client {
 public:
  explicit MockPlatformSensorClient(scoped_refptr<PlatformSensor> sensor);
  ~MockPlatformSensorClient() override;

  // PlatformSensor::Client:
  MOCK_METHOD1(OnSensorReadingChanged, void(mojom::SensorType type));
  MOCK_METHOD0(OnSensorError, void());
  MOCK_METHOD0(IsSuspended, bool());

 private:
  scoped_refptr<PlatformSensor> sensor_;

  DISALLOW_COPY_AND_ASSIGN(MockPlatformSensorClient);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_AND_PROVIDER_H_

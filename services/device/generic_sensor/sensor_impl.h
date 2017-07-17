// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_SENSOR_IMPL_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_SENSOR_IMPL_H_

#include "base/macros.h"
#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/public/interfaces/sensor.mojom.h"

namespace device {

// Implementation of Sensor mojo interface.
// Instances of this class are created by SensorProviderImpl.
class SensorImpl final : public mojom::Sensor, public PlatformSensor::Client {
 public:
  explicit SensorImpl(scoped_refptr<PlatformSensor> sensor);
  ~SensorImpl() override;

  mojom::SensorClientRequest GetClient();

 private:
  // Sensor implementation.
  void AddConfiguration(const PlatformSensorConfiguration& configuration,
                        AddConfigurationCallback callback) override;
  void GetDefaultConfiguration(
      GetDefaultConfigurationCallback callback) override;
  void RemoveConfiguration(const PlatformSensorConfiguration& configuration,
                           RemoveConfigurationCallback callback) override;
  void Suspend() override;
  void Resume() override;
  void ConfigureReadingChangeNotifications(bool enabled) override;

  // device::Sensor::Client implementation.
  void OnSensorReadingChanged() override;
  void OnSensorError() override;
  bool IsSuspended() override;

 private:
  scoped_refptr<PlatformSensor> sensor_;
  mojom::SensorClientPtr client_;
  bool reading_notification_enabled_;
  bool suspended_;

  DISALLOW_COPY_AND_ASSIGN(SensorImpl);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_SENSOR_IMPL_H_

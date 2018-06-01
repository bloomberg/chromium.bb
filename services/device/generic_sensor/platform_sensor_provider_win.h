// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_

#include <SensorsApi.h>
#include <wrl/client.h>

#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace device {

class PlatformSensorReaderWin;

// Implementation of PlatformSensorProvider for Windows platform.
// PlatformSensorProviderWin is responsible for following tasks:
// - Starts sensor thread and stops it when there are no active sensors.
// - Initialises ISensorManager and creates sensor reader on sensor thread.
// - Constructs PlatformSensorWin on IPC thread and returns it to requester.
class PlatformSensorProviderWin final : public PlatformSensorProvider {
 public:
  static PlatformSensorProviderWin* GetInstance();

  // Overrides ISensorManager COM interface provided by the system, used
  // only for testing purposes.
  void SetSensorManagerForTesting(
      Microsoft::WRL::ComPtr<ISensorManager> sensor_manager);

 protected:
  ~PlatformSensorProviderWin() override;

  // PlatformSensorProvider interface implementation.
  void FreeResources() override;
  void CreateSensorInternal(mojom::SensorType type,
                            SensorReadingSharedBuffer* reading_buffer,
                            const CreateSensorCallback& callback) override;

 private:
  friend struct base::DefaultSingletonTraits<PlatformSensorProviderWin>;

  class SensorThread;

  PlatformSensorProviderWin();

  void CreateSensorThread();
  bool StartSensorThread();
  void StopSensorThread();
  std::unique_ptr<PlatformSensorReaderWin> CreateSensorReader(
      mojom::SensorType type);
  void SensorReaderCreated(
      mojom::SensorType type,
      SensorReadingSharedBuffer* reading_buffer,
      const CreateSensorCallback& callback,
      std::unique_ptr<PlatformSensorReaderWin> sensor_reader);

  std::unique_ptr<SensorThread> sensor_thread_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_

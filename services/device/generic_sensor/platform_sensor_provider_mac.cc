// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_mac.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "services/device/generic_sensor/orientation_quaternion_fusion_algorithm_using_euler_angles.h"
#include "services/device/generic_sensor/platform_sensor_accelerometer_mac.h"
#include "services/device/generic_sensor/platform_sensor_ambient_light_mac.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer.h"

namespace device {

// static
PlatformSensorProviderMac* PlatformSensorProviderMac::GetInstance() {
  return base::Singleton<
      PlatformSensorProviderMac,
      base::LeakySingletonTraits<PlatformSensorProviderMac>>::get();
}

PlatformSensorProviderMac::PlatformSensorProviderMac() = default;

PlatformSensorProviderMac::~PlatformSensorProviderMac() = default;

void PlatformSensorProviderMac::CreateSensorInternal(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  // Create Sensors here.
  switch (type) {
    case mojom::SensorType::AMBIENT_LIGHT: {
      scoped_refptr<PlatformSensor> sensor =
          new PlatformSensorAmbientLightMac(std::move(mapping), this);
      callback.Run(std::move(sensor));
      break;
    }
    case mojom::SensorType::ACCELEROMETER: {
      callback.Run(base::MakeRefCounted<PlatformSensorAccelerometerMac>(
          std::move(mapping), this));
      break;
    }
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES: {
      auto fusion_algorithm = base::MakeUnique<
          RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer>();
      // If this PlatformSensorFusion object is successfully initialized,
      // |callback| will be run with a reference to this object.
      PlatformSensorFusion::Create(std::move(mapping), this,
                                   std::move(fusion_algorithm), callback);
      break;
    }
    case mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION: {
      auto orientation_quaternion_fusion_algorithm_using_euler_angles =
          base::MakeUnique<
              OrientationQuaternionFusionAlgorithmUsingEulerAngles>(
              false /* absolute */);
      // If this PlatformSensorFusion object is successfully initialized,
      // |callback| will be run with a reference to this object.
      PlatformSensorFusion::Create(
          std::move(mapping), this,
          std::move(orientation_quaternion_fusion_algorithm_using_euler_angles),
          callback);
      break;
    }
    default:
      callback.Run(nullptr);
  }
}

}  // namespace device

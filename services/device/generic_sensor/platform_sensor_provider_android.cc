// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_android.h"

#include <utility>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "jni/PlatformSensorProvider_jni.h"
#include "services/device/generic_sensor/absolute_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_magnetometer.h"
#include "services/device/generic_sensor/orientation_euler_angles_fusion_algorithm_using_quaternion.h"
#include "services/device/generic_sensor/orientation_quaternion_fusion_algorithm_using_euler_angles.h"
#include "services/device/generic_sensor/platform_sensor_android.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace device {

// static
PlatformSensorProviderAndroid* PlatformSensorProviderAndroid::GetInstance() {
  return base::Singleton<
      PlatformSensorProviderAndroid,
      base::LeakySingletonTraits<PlatformSensorProviderAndroid>>::get();
}

PlatformSensorProviderAndroid::PlatformSensorProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  j_object_.Reset(Java_PlatformSensorProvider_create(env));
}

PlatformSensorProviderAndroid::~PlatformSensorProviderAndroid() = default;

void PlatformSensorProviderAndroid::SetSensorManagerToNullForTesting() {
  JNIEnv* env = AttachCurrentThread();
  Java_PlatformSensorProvider_setSensorManagerToNullForTesting(env, j_object_);
}

void PlatformSensorProviderAndroid::CreateSensorInternal(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  JNIEnv* env = AttachCurrentThread();

  // Some of the sensors may not be available depending on the device and
  // Android version, so the fallback ensures selection of the best possible
  // option.
  switch (type) {
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      CreateAbsoluteOrientationEulerAnglesSensor(env, std::move(mapping),
                                                 callback);
      break;
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      CreateAbsoluteOrientationQuaternionSensor(env, std::move(mapping),
                                                callback);
      break;
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      CreateRelativeOrientationEulerAnglesSensor(env, std::move(mapping),
                                                 callback);
      break;
    default: {
      ScopedJavaLocalRef<jobject> sensor =
          Java_PlatformSensorProvider_createSensor(env, j_object_,
                                                   static_cast<jint>(type));

      if (!sensor.obj()) {
        callback.Run(nullptr);
        return;
      }

      auto concrete_sensor = base::MakeRefCounted<PlatformSensorAndroid>(
          type, std::move(mapping), this, sensor);
      callback.Run(concrete_sensor);
      break;
    }
  }
}

// For ABSOLUTE_ORIENTATION_EULER_ANGLES we use a 3-way fallback approach
// where up to 3 different sets of sensors are attempted if necessary. The
// sensors to be used are determined in the following order:
//   A: ABSOLUTE_ORIENTATION_QUATERNION (if it uses TYPE_ROTATION_VECTOR
//      directly)
//   B: TODO(juncai): Combination of ACCELEROMETER, MAGNETOMETER and
//      GYROSCOPE
//   C: Combination of ACCELEROMETER and MAGNETOMETER
void PlatformSensorProviderAndroid::CreateAbsoluteOrientationEulerAnglesSensor(
    JNIEnv* env,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  if (static_cast<bool>(Java_PlatformSensorProvider_hasSensorType(
          env, j_object_,
          static_cast<jint>(
              mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION)))) {
    auto sensor_fusion_algorithm =
        base::MakeUnique<OrientationEulerAnglesFusionAlgorithmUsingQuaternion>(
            true /* absolute */);

    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    base::MakeRefCounted<PlatformSensorFusion>(
        std::move(mapping), this, callback, std::move(sensor_fusion_algorithm));
  } else {
    auto sensor_fusion_algorithm = base::MakeUnique<
        AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer>();

    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    base::MakeRefCounted<PlatformSensorFusion>(
        std::move(mapping), this, callback, std::move(sensor_fusion_algorithm));
  }
}

// For ABSOLUTE_ORIENTATION_QUATERNION we use a 2-way fallback approach
// where up to 2 different sets of sensors are attempted if necessary. The
// sensors to be used are determined in the following order:
//   A: Use TYPE_ROTATION_VECTOR directly
//   B: ABSOLUTE_ORIENTATION_EULER_ANGLES
void PlatformSensorProviderAndroid::CreateAbsoluteOrientationQuaternionSensor(
    JNIEnv* env,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  ScopedJavaLocalRef<jobject> sensor = Java_PlatformSensorProvider_createSensor(
      env, j_object_,
      static_cast<jint>(mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION));

  if (sensor.obj()) {
    auto concrete_sensor = base::MakeRefCounted<PlatformSensorAndroid>(
        mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION, std::move(mapping),
        this, sensor);

    callback.Run(concrete_sensor);
  } else {
    auto sensor_fusion_algorithm =
        base::MakeUnique<OrientationQuaternionFusionAlgorithmUsingEulerAngles>(
            true /* absolute */);

    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    base::MakeRefCounted<PlatformSensorFusion>(
        std::move(mapping), this, callback, std::move(sensor_fusion_algorithm));
  }
}

// For RELATIVE_ORIENTATION_EULER_ANGLES we use RELATIVE_ORIENTATION_QUATERNION
// (if it uses TYPE_GAME_ROTATION_VECTOR directly).
void PlatformSensorProviderAndroid::CreateRelativeOrientationEulerAnglesSensor(
    JNIEnv* env,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  if (static_cast<bool>(Java_PlatformSensorProvider_hasSensorType(
          env, j_object_,
          static_cast<jint>(
              mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION)))) {
    auto sensor_fusion_algorithm =
        base::MakeUnique<OrientationEulerAnglesFusionAlgorithmUsingQuaternion>(
            false /* absolute */);

    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    base::MakeRefCounted<PlatformSensorFusion>(
        std::move(mapping), this, callback, std::move(sensor_fusion_algorithm));
  } else {
    callback.Run(nullptr);
  }
}

}  // namespace device

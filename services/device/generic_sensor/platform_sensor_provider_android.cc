// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_android.h"

#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"
#include "jni/PlatformSensorProvider_jni.h"
#include "services/device/generic_sensor/platform_sensor_android.h"

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
  Java_PlatformSensorProvider_setSensorManagerToNullForTesting(env,
                                                               j_object_.obj());
}

void PlatformSensorProviderAndroid::CreateSensorInternal(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> sensor = Java_PlatformSensorProvider_createSensor(
      env, j_object_.obj(), static_cast<jint>(type));

  if (!sensor.obj()) {
    callback.Run(nullptr);
    return;
  }

  scoped_refptr<PlatformSensorAndroid> concrete_sensor =
      new PlatformSensorAndroid(type, std::move(mapping), this, sensor);
  callback.Run(concrete_sensor);
}

}  // namespace device

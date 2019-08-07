// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ANDROID_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "services/device/generic_sensor/platform_sensor.h"

namespace device {

class PlatformSensorAndroid : public PlatformSensor {
 public:
  PlatformSensorAndroid(mojom::SensorType type,
                        SensorReadingSharedBuffer* reading_buffer,
                        PlatformSensorProvider* provider,
                        const base::android::JavaRef<jobject>& java_sensor);

  mojom::ReportingMode GetReportingMode() override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;
  double GetMaximumSupportedFrequency() override;

  void NotifyPlatformSensorError(JNIEnv*,
                                 const base::android::JavaRef<jobject>& caller);

  void UpdatePlatformSensorReading(
      JNIEnv*,
      const base::android::JavaRef<jobject>& caller,
      jdouble timestamp,
      jdouble value1,
      jdouble value2,
      jdouble value3,
      jdouble value4);

 protected:
  ~PlatformSensorAndroid() override;
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;

 private:
  // Java object org.chromium.device.sensors.PlatformSensor
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
  DISALLOW_COPY_AND_ASSIGN(PlatformSensorAndroid);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ANDROID_H_

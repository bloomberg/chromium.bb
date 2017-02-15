// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_TIME_ZONE_MONITOR_ANDROID_TIME_ZONE_MONITOR_JNI_REGISTRAR_H_
#define SERVICES_DEVICE_TIME_ZONE_MONITOR_ANDROID_TIME_ZONE_MONITOR_JNI_REGISTRAR_H_

#include <jni.h>

namespace device {
namespace android {

bool RegisterTimeZoneMonitorJni(JNIEnv* env);

}  // namespace android
}  // namespace device

#endif  // SERVICES_DEVICE_TIME_ZONE_MONITOR_ANDROID_TIME_ZONE_MONITOR_JNI_REGISTRAR_H_

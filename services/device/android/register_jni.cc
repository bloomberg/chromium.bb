// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/android/register_jni.h"

#include "base/android/jni_android.h"
#include "services/device/time_zone_monitor/android/time_zone_monitor_jni_registrar.h"

namespace device {

bool EnsureJniRegistered() {
  static bool g_jni_init_done = false;

  if (!g_jni_init_done) {
    JNIEnv* env = base::android::AttachCurrentThread();

    if (!android::RegisterTimeZoneMonitorJni(env))
      return false;

    g_jni_init_done = true;
  }

  return true;
}

}  // namespace device

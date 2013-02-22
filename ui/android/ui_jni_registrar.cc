// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/ui_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/android/window_android.h"

namespace ui {
namespace android {

static base::android::RegistrationMethod kUiRegisteredMethods[] = {
  { "DeviceDisplayInfo", gfx::DeviceDisplayInfo::RegisterDeviceDisplayInfo },
  { "JavaBitmap", gfx::JavaBitmap::RegisterJavaBitmap },
  { "NativeWindow", ui::WindowAndroid::RegisterWindowAndroid },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kUiRegisteredMethods,
                               arraysize(kUiRegisteredMethods));
}

}  // namespace android
}  // namespace ui

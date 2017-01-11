// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/ui_android_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/screen_android.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

namespace ui {

static base::android::RegistrationMethod kAndroidRegisteredMethods[] = {
    {"DisplayAndroidManager", ui::RegisterScreenAndroid},
    {"ResourceManager", ui::ResourceManagerImpl::RegisterResourceManager},
    {"WindowAndroid", WindowAndroid::RegisterWindowAndroid},
};

bool RegisterUIAndroidJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kAndroidRegisteredMethods,
                               arraysize(kAndroidRegisteredMethods));
}

}  // namespace ui

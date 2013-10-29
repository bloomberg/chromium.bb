// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "net/android/net_jni_registrar.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "ui/gfx/android/gfx_jni_registrar.h"

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);

  JNIEnv* env = base::android::AttachCurrentThread();
  static base::android::RegistrationMethod kRemotingRegisteredMethods[] = {
      {"base", base::android::RegisterJni},
      {"gfx", gfx::android::RegisterJni},
      {"net", net::android::RegisterJni},
      {"remoting", remoting::RegisterJni},
  };
  if (!base::android::RegisterNativeMethods(
      env, kRemotingRegisteredMethods, arraysize(kRemotingRegisteredMethods))) {
    return -1;
  }

  return JNI_VERSION_1_4;
}

}  // extern "C"

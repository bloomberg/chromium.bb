// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_onload.h"
#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_utils.h"
#include "net/android/net_jni_registrar.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "ui/gfx/android/gfx_jni_registrar.h"

namespace {

base::android::RegistrationMethod kRemotingRegisteredMethods[] = {
  {"base", base::android::RegisterJni},
  {"gfx", gfx::android::RegisterJni},
  {"net", net::android::RegisterJni},
  {"remoting", remoting::RegisterJni},
};

bool RegisterJNI(JNIEnv* env) {
  return base::android::RegisterNativeMethods(env,
      kRemotingRegisteredMethods, arraysize(kRemotingRegisteredMethods));
}

}  // namespace

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  std::vector<base::android::RegisterCallback> register_callbacks;
  register_callbacks.push_back(base::Bind(&RegisterJNI));
  std::vector<base::android::InitCallback> init_callbacks;
  if (!base::android::OnJNIOnLoadRegisterJNI(vm, register_callbacks) ||
      !base::android::OnJNIOnLoadInit(init_callbacks)) {
    return -1;
  }
  return JNI_VERSION_1_4;
}

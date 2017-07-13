// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_utils.h"
#include "base/bind.h"
#include "base/macros.h"
#include "net/android/net_jni_registrar.h"
#include "remoting/client/jni/remoting_jni_registrar.h"
#include "remoting/client/remoting_jni_registration.h"
#include "ui/gfx/android/gfx_jni_registrar.h"

namespace {

base::android::RegistrationMethod kRemotingRegisteredMethods[] = {
  {"gfx", gfx::android::RegisterJni},
  {"net", net::android::RegisterJni},
  {"remoting", remoting::RegisterJni},
};

bool RegisterJNI(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kRemotingRegisteredMethods, arraysize(kRemotingRegisteredMethods));
}

}  // namespace

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!RegisterMainDexNatives(env) || !RegisterNonMainDexNatives(env)) {
    return -1;
  }

  // TODO(agrieve): Delete this block, this is a no-op now.
  // https://crbug.com/683256.
  if (!RegisterJNI(env) || !base::android::OnJNIOnLoadInit()) {
    return -1;
  }
  return JNI_VERSION_1_4;
}

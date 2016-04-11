// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/android/remoting_host_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "remoting/host/android/jni_host.h"

namespace remoting {

namespace {
const base::android::RegistrationMethod kRemotingRegisteredMethods[] = {
    {"JniHost", JniHost::RegisterJni},
};
}  // namespace

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kRemotingRegisteredMethods,
                               arraysize(kRemotingRegisteredMethods));
}

}  // namespace remoting

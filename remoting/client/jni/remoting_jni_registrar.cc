// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "remoting/client/jni/remoting_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/jni/jni_client.h"
#include "remoting/client/jni/jni_gl_display_handler.h"

namespace remoting {

namespace {
const base::android::RegistrationMethod kRemotingRegisteredMethods[] = {
  {"JniClient", JniClient::RegisterJni},
  {"ChromotingJniRuntime", RegisterChromotingJniRuntime},
  {"JniGlDisplay", JniGlDisplayHandler::RegisterJni},
};
}  // namespace

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kRemotingRegisteredMethods,
                               arraysize(kRemotingRegisteredMethods));
}

}  // namespace remoting

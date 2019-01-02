// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/register_jni.h"
#include "jni/VrModuleProvider_jni.h"

namespace vr {

static void JNI_VrModuleProvider_RegisterJni(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz) {
  CHECK(RegisterJni(env));
}

}  // namespace vr

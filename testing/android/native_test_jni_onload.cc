// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/android/jni_onload_delegate.h"
#include "testing/android/native_test_launcher.h"

namespace {

class NativeTestJNIOnLoadDelegate : public base::android::JNIOnLoadDelegate {
 public:
  bool RegisterJNI(JNIEnv* env) override;
  bool Init() override;
};

bool NativeTestJNIOnLoadDelegate::RegisterJNI(JNIEnv* env) {
  return RegisterNativeTestJNI(env);
}

bool NativeTestJNIOnLoadDelegate::Init() {
  InstallHandlers();
  return true;
}

}  // namespace


// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  NativeTestJNIOnLoadDelegate delegate;
  std::vector<base::android::JNIOnLoadDelegate*> delegates;
  delegates.push_back(&delegate);

  if (!base::android::OnJNIOnLoad(vm, &delegates))
    return -1;

  return JNI_VERSION_1_4;
}

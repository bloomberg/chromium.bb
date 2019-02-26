// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_MODULE_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_VR_VR_MODULE_PROVIDER_H_

#include <jni.h>
#include <queue>

#include "base/android/jni_android.h"
#include "device/vr/android/gvr/vr_module_delegate.h"

namespace vr {

// Installs the VR module.
class VrModuleProvider : public device::VrModuleDelegate {
 public:
  VrModuleProvider();
  ~VrModuleProvider() override;
  bool ModuleInstalled() override;
  void InstallModule(base::OnceCallback<void(bool)> on_finished) override;

  // Called by Java.
  void OnInstalledModule(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         bool success);

 private:
  std::queue<base::OnceCallback<void(bool)>> on_finished_callbacks_;
  base::android::ScopedJavaGlobalRef<jobject> j_vr_module_provider_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_MODULE_PROVIDER_H_

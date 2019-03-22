// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_VR_MODULE_DELEGATE_H_
#define DEVICE_VR_ANDROID_GVR_VR_MODULE_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/vr_export.h"

namespace device {

// Delegates installation of the VR module.
class DEVICE_VR_EXPORT VrModuleDelegate {
 public:
  // Returns the global module delegate.
  static VrModuleDelegate* Get();
  // Sets the global module delegate.
  static void Set(std::unique_ptr<VrModuleDelegate> delegate);

  VrModuleDelegate() = default;
  virtual ~VrModuleDelegate() = default;
  // Returns true if the VR module is installed.
  virtual bool ModuleInstalled() = 0;
  // Asynchronously requests to install the VR module. |on_finished| is called
  // after the module install is completed. If |success| is false the module
  // install failed.
  virtual void InstallModule(
      base::OnceCallback<void(bool success)> on_finished) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VrModuleDelegate);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_VR_MODULE_DELEGATE_H_

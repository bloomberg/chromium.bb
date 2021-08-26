// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_STATICS_H_
#define DEVICE_VR_OPENXR_OPENXR_STATICS_H_

#include <d3d11.h>

#include "build/build_config.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"

#if defined(OS_WIN)
#include "base/win/windows_types.h"
#endif

namespace device {

// OpenXrStatics must outlive all other OpenXR objects. It owns the XrInstance
// and will destroy it in the destructor.
class DEVICE_VR_EXPORT OpenXrStatics {
 public:
  OpenXrStatics();
  ~OpenXrStatics();

  const OpenXrExtensionEnumeration* GetExtensionEnumeration() const {
    return &extension_enumeration_;
  }

  XrInstance GetXrInstance();

  bool IsHardwareAvailable();
  bool IsApiAvailable();

#if defined(OS_WIN)
  CHROME_LUID GetLuid(const OpenXrExtensionHelper& extension_helper);
#endif

 private:
  XrInstance instance_;
  OpenXrExtensionEnumeration extension_enumeration_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_STATICS_H_

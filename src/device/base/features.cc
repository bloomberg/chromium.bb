// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/features.h"

#include "build/build_config.h"

namespace device {

#if defined(OS_WIN)
const base::Feature kNewUsbBackend{"NewUsbBackend",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kNewBLEWinImplementation{"NewBLEWinImplementation",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_VR)
// Controls whether the orientation sensor based device is enabled.
const base::Feature kWebXrOrientationSensorDevice {
  "WebXROrientationSensorDevice",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      // TODO(https://crbug.com/820308, https://crbug.com/773829): Enable once
      // platform specific bugs have been fixed.
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};
#endif  // BUILDFLAG(ENABLE_VR)
namespace features {
#if BUILDFLAG(ENABLE_OCULUS_VR)
// Controls WebXR support for the Oculus Runtime.
const base::Feature kOculusVR{"OculusVR", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // ENABLE_OCULUS_VR

#if BUILDFLAG(ENABLE_OPENVR)
// Controls WebXR support for the OpenVR Runtime.
const base::Feature kOpenVR{"OpenVR", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // ENABLE_OPENVR

#if BUILDFLAG(ENABLE_OPENXR)
// Controls WebXR support for the OpenXR Runtime.
const base::Feature kOpenXR{"OpenXR", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // ENABLE_OPENXR

#if BUILDFLAG(ENABLE_WINDOWS_MR)
// Controls WebXR support for the Windows Mixed Reality Runtime.
const base::Feature kWindowsMixedReality{"WindowsMixedReality",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // ENABLE_WINDOWS_MR
}  // namespace features
}  // namespace device

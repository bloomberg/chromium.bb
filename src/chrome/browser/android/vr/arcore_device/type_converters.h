// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_TYPE_CONVERTERS_H_

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/arcore-android-sdk/src/libraries/include/arcore_c_api.h"

namespace mojo {

template <>
struct TypeConverter<device::mojom::XRPlaneOrientation, ArPlaneType> {
  static device::mojom::XRPlaneOrientation Convert(ArPlaneType plane_type);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_TYPE_CONVERTERS_H_

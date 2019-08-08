// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/type_converters.h"

namespace mojo {

device::mojom::XRPlaneOrientation
TypeConverter<device::mojom::XRPlaneOrientation, ArPlaneType>::Convert(
    ArPlaneType plane_type) {
  switch (plane_type) {
    case ArPlaneType::AR_PLANE_HORIZONTAL_DOWNWARD_FACING:
    case ArPlaneType::AR_PLANE_HORIZONTAL_UPWARD_FACING:
      return device::mojom::XRPlaneOrientation::HORIZONTAL;
    case ArPlaneType::AR_PLANE_VERTICAL:
      return device::mojom::XRPlaneOrientation::VERTICAL;
  }
}

}  // namespace mojo

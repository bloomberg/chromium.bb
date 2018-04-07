// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_device_memory.h"

#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"

namespace blink {

float NavigatorDeviceMemory::deviceMemory() const {
  return ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
}

}  // namespace blink

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_device_lost_info.h"

namespace blink {

GPUDeviceLostInfo::GPUDeviceLostInfo(const String& message) {
  message_ = message;
}

const String& GPUDeviceLostInfo::message() const {
  return message_;
}

}  // namespace blink

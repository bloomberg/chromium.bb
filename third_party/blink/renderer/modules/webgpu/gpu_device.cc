// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device_descriptor.h"

namespace blink {

// static
GPUDevice* GPUDevice::Create(GPUAdapter* adapter,
                             const GPUDeviceDescriptor* descriptor) {
  return MakeGarbageCollected<GPUDevice>(adapter, descriptor);
}

// TODO(enga): Handle adapter options and device descriptor
GPUDevice::GPUDevice(GPUAdapter* adapter, const GPUDeviceDescriptor* descriptor)
    : adapter_(adapter) {}

GPUAdapter* GPUDevice::adapter() const {
  return adapter_;
}

void GPUDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(adapter_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

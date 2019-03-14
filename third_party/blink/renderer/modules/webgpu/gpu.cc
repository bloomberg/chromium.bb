// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter_descriptor.h"

namespace blink {

// static
GPU* GPU::Create() {
  return MakeGarbageCollected<GPU>();
}

GPUAdapter* GPU::getAdapter(const GPUAdapterDescriptor* descriptor) {
  return GPUAdapter::Create(descriptor->powerPreference());
}

GPU::GPU() {}

}  // namespace blink

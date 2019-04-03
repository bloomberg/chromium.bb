// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUBuffer* GPUBuffer::Create(GPUDevice* device, DawnBuffer buffer) {
  return MakeGarbageCollected<GPUBuffer>(device, buffer);
}

GPUBuffer::GPUBuffer(GPUDevice* device, DawnBuffer buffer)
    : DawnObject<DawnBuffer>(device, buffer) {}

GPUBuffer::~GPUBuffer() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().bufferRelease(GetHandle());
}

}  // namespace blink

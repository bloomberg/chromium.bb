// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUQueue* GPUQueue::Create(GPUDevice* device, DawnQueue queue) {
  return MakeGarbageCollected<GPUQueue>(device, queue);
}

GPUQueue::GPUQueue(GPUDevice* device, DawnQueue queue)
    : DawnObject<DawnQueue>(device, queue) {}

GPUQueue::~GPUQueue() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().queueRelease(GetHandle());
}

}  // namespace blink

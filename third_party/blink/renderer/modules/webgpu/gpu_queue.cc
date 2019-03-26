// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_fence.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_fence_descriptor.h"

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

void GPUQueue::signal(GPUFence* fence, uint64_t signal_value) {
  GetProcs().queueSignal(GetHandle(), fence->GetHandle(), signal_value);
  device_->GetInterface()->FlushCommands();
}

GPUFence* GPUQueue::createFence(const GPUFenceDescriptor* descriptor) {
  DCHECK(descriptor);

  DawnFenceDescriptor desc;
  desc.nextInChain = nullptr;
  desc.initialValue = descriptor->initialValue();

  return GPUFence::Create(device_,
                          GetProcs().queueCreateFence(GetHandle(), &desc));
}

}  // namespace blink

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUBindGroup* GPUBindGroup::Create(GPUDevice* device,
                                   DawnBindGroup bind_group) {
  return MakeGarbageCollected<GPUBindGroup>(device, bind_group);
}

GPUBindGroup::GPUBindGroup(GPUDevice* device, DawnBindGroup bind_group)
    : DawnObject<DawnBindGroup>(device, bind_group) {}

GPUBindGroup::~GPUBindGroup() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().bindGroupRelease(GetHandle());
}

}  // namespace blink

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUBindGroupLayout* GPUBindGroupLayout::Create(
    GPUDevice* device,
    DawnBindGroupLayout bind_group_layout) {
  return MakeGarbageCollected<GPUBindGroupLayout>(device, bind_group_layout);
}

GPUBindGroupLayout::GPUBindGroupLayout(GPUDevice* device,
                                       DawnBindGroupLayout bind_group_layout)
    : DawnObject<DawnBindGroupLayout>(device, bind_group_layout) {}

GPUBindGroupLayout::~GPUBindGroupLayout() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().bindGroupLayoutRelease(GetHandle());
}

}  // namespace blink

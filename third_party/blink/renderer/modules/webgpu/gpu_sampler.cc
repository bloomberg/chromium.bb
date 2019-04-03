// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUSampler* GPUSampler::Create(GPUDevice* device, DawnSampler sampler) {
  return MakeGarbageCollected<GPUSampler>(device, sampler);
}

GPUSampler::GPUSampler(GPUDevice* device, DawnSampler sampler)
    : DawnObject<DawnSampler>(device, sampler) {}

GPUSampler::~GPUSampler() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().samplerRelease(GetHandle());
}

}  // namespace blink

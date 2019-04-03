// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pass_encoder.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUComputePassEncoder* GPUComputePassEncoder::Create(
    GPUDevice* device,
    DawnComputePassEncoder compute_pass_encoder) {
  return MakeGarbageCollected<GPUComputePassEncoder>(device,
                                                     compute_pass_encoder);
}

GPUComputePassEncoder::GPUComputePassEncoder(
    GPUDevice* device,
    DawnComputePassEncoder compute_pass_encoder)
    : DawnObject<DawnComputePassEncoder>(device, compute_pass_encoder) {}

GPUComputePassEncoder::~GPUComputePassEncoder() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().computePassEncoderRelease(GetHandle());
}

}  // namespace blink

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUComputePipeline* GPUComputePipeline::Create(
    GPUDevice* device,
    DawnComputePipeline compute_pipeline) {
  return MakeGarbageCollected<GPUComputePipeline>(device, compute_pipeline);
}

GPUComputePipeline::GPUComputePipeline(GPUDevice* device,
                                       DawnComputePipeline compute_pipeline)
    : DawnObject<DawnComputePipeline>(device, compute_pipeline) {}

GPUComputePipeline::~GPUComputePipeline() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().computePipelineRelease(GetHandle());
}

}  // namespace blink

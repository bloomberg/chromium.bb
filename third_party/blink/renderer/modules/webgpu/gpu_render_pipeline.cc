// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPURenderPipeline* GPURenderPipeline::Create(
    GPUDevice* device,
    const GPURenderPipelineDescriptor* webgpu_desc) {
  NOTIMPLEMENTED();
  return nullptr;
}

GPURenderPipeline::GPURenderPipeline(GPUDevice* device,
                                     DawnRenderPipeline render_pipeline)
    : DawnObject<DawnRenderPipeline>(device, render_pipeline) {}

GPURenderPipeline::~GPURenderPipeline() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().renderPipelineRelease(GetHandle());
}

}  // namespace blink

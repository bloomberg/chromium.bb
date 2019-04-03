// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUPipelineLayout* GPUPipelineLayout::Create(
    GPUDevice* device,
    DawnPipelineLayout pipeline_layout) {
  return MakeGarbageCollected<GPUPipelineLayout>(device, pipeline_layout);
}

GPUPipelineLayout::GPUPipelineLayout(GPUDevice* device,
                                     DawnPipelineLayout pipeline_layout)
    : DawnObject<DawnPipelineLayout>(device, pipeline_layout) {}

GPUPipelineLayout::~GPUPipelineLayout() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().pipelineLayoutRelease(GetHandle());
}

}  // namespace blink

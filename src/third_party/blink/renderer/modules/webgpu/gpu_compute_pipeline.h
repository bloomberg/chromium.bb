// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PIPELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PIPELINE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUBindGroupLayout;
class GPUComputePipelineDescriptor;

WGPUComputePipelineDescriptor AsDawnType(
    const GPUComputePipelineDescriptor* webgpu_desc,
    std::string* label,
    OwnedProgrammableStageDescriptor* computeStageDescriptor,
    GPUDevice* device);

class GPUComputePipeline : public DawnObject<WGPUComputePipeline> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUComputePipeline* Create(
      GPUDevice* device,
      const GPUComputePipelineDescriptor* webgpu_desc);
  explicit GPUComputePipeline(GPUDevice* device,
                              WGPUComputePipeline compute_pipeline);

  GPUComputePipeline(const GPUComputePipeline&) = delete;
  GPUComputePipeline& operator=(const GPUComputePipeline&) = delete;

  GPUBindGroupLayout* getBindGroupLayout(uint32_t index);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PIPELINE_H_

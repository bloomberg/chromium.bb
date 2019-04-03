// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PASS_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PASS_ENCODER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUComputePassEncoder : public DawnObject<DawnComputePassEncoder> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUComputePassEncoder* Create(
      GPUDevice* device,
      DawnComputePassEncoder compute_pass_encoder);
  explicit GPUComputePassEncoder(GPUDevice* device,
                                 DawnComputePassEncoder compute_pass_encoder);
  ~GPUComputePassEncoder() override;

  // gpu_compute_pass_encoder.idl
  // TODO(crbug.com/877147): implement GPUComputePassEncoder.

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUComputePassEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPUTE_PASS_ENCODER_H_

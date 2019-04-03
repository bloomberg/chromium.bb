// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PASS_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PASS_ENCODER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPURenderPassEncoder : public DawnObject<DawnRenderPassEncoder> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPURenderPassEncoder* Create(
      GPUDevice* device,
      DawnRenderPassEncoder render_pass_encoder);
  explicit GPURenderPassEncoder(GPUDevice* device,
                                DawnRenderPassEncoder render_pass_encoder);
  ~GPURenderPassEncoder() override;

  // gpu_render_pass_encoder.idl
  // TODO(crbug.com/877147): implement GPURenderPassEncoder.

 private:
  DISALLOW_COPY_AND_ASSIGN(GPURenderPassEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RENDER_PASS_ENCODER_H_

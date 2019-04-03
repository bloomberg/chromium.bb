// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUCommandEncoder : public DawnObject<DawnCommandEncoder> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUCommandEncoder* Create(GPUDevice* device,
                                   DawnCommandEncoder command_encoder);
  explicit GPUCommandEncoder(GPUDevice* device,
                             DawnCommandEncoder command_encoder);
  ~GPUCommandEncoder() override;

  // gpu_command_encoder.idl
  // TODO(crbug.com/877147): implement GPUCommandEncoder.

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUCommandEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUBufferDescriptor;

class GPUBuffer : public DawnObject<DawnBuffer> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUBuffer* Create(GPUDevice* device,
                           const GPUBufferDescriptor* webgpu_desc);
  explicit GPUBuffer(GPUDevice* device, DawnBuffer buffer);
  ~GPUBuffer() override;

  // gpu_buffer.idl
  // TODO(crbug.com/877147): implement GPUBuffer.

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUBuffer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_H_

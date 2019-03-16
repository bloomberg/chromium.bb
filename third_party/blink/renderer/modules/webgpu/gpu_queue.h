// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUQueue : public DawnObject<DawnQueue> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUQueue* Create(GPUDevice* device, DawnQueue queue);
  explicit GPUQueue(GPUDevice* device, DawnQueue queue);
  ~GPUQueue() override;
  
  DISALLOW_COPY_AND_ASSIGN(GPUQueue);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_

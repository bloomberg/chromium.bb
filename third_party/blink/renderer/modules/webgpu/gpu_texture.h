// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUTexture : public DawnObject<DawnTexture> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUTexture* Create(GPUDevice* device, DawnTexture texture);
  explicit GPUTexture(GPUDevice* device, DawnTexture texture);
  ~GPUTexture() override;

  // gpu_texture.idl
  // TODO(crbug.com/877147): implement GPUTexture.

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUTexture);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_H_

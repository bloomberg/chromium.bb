// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUTexture* GPUTexture::Create(GPUDevice* device, DawnTexture texture) {
  return MakeGarbageCollected<GPUTexture>(device, texture);
}

GPUTexture::GPUTexture(GPUDevice* device, DawnTexture texture)
    : DawnObject<DawnTexture>(device, texture) {}

GPUTexture::~GPUTexture() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().textureRelease(GetHandle());
}

}  // namespace blink

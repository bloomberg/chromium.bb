// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_encoder.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPURenderPassEncoder* GPURenderPassEncoder::Create(
    GPUDevice* device,
    DawnRenderPassEncoder render_pass_encoder) {
  return MakeGarbageCollected<GPURenderPassEncoder>(device,
                                                    render_pass_encoder);
}

GPURenderPassEncoder::GPURenderPassEncoder(
    GPUDevice* device,
    DawnRenderPassEncoder render_pass_encoder)
    : DawnObject<DawnRenderPassEncoder>(device, render_pass_encoder) {}

GPURenderPassEncoder::~GPURenderPassEncoder() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().renderPassEncoderRelease(GetHandle());
}

}  // namespace blink

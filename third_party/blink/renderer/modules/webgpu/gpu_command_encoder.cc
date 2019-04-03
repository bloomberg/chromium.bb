// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_command_encoder.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUCommandEncoder* GPUCommandEncoder::Create(
    GPUDevice* device,
    DawnCommandEncoder command_encoder) {
  return MakeGarbageCollected<GPUCommandEncoder>(device, command_encoder);
}

GPUCommandEncoder::GPUCommandEncoder(GPUDevice* device,
                                     DawnCommandEncoder command_encoder)
    : DawnObject<DawnCommandEncoder>(device, command_encoder) {}

GPUCommandEncoder::~GPUCommandEncoder() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().commandEncoderRelease(GetHandle());
}

}  // namespace blink

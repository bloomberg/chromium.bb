// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUShaderModule* GPUShaderModule::Create(GPUDevice* device,
                                         DawnShaderModule shader_module) {
  return MakeGarbageCollected<GPUShaderModule>(device, shader_module);
}

GPUShaderModule::GPUShaderModule(GPUDevice* device,
                                 DawnShaderModule shader_module)
    : DawnObject<DawnShaderModule>(device, shader_module) {}

GPUShaderModule::~GPUShaderModule() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().shaderModuleRelease(GetHandle());
}

}  // namespace blink

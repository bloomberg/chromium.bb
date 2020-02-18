// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module_descriptor.h"

namespace blink {

namespace {

DawnShaderModuleDescriptor AsDawnType(
    const GPUShaderModuleDescriptor* webgpu_desc) {
  DawnShaderModuleDescriptor dawn_desc = {};

  dawn_desc.nextInChain = nullptr;
  dawn_desc.code = webgpu_desc->code().View()->Data();
  dawn_desc.codeSize = webgpu_desc->code().View()->length();

  return dawn_desc;
}

}  // anonymous namespace

// static
GPUShaderModule* GPUShaderModule::Create(
    GPUDevice* device,
    const GPUShaderModuleDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);
  DawnShaderModuleDescriptor dawn_desc = AsDawnType(webgpu_desc);
  return MakeGarbageCollected<GPUShaderModule>(
      device, device->GetProcs().deviceCreateShaderModule(device->GetHandle(),
                                                          &dawn_desc));
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

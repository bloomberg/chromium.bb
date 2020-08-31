// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_bind_group_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_bind_group_entry.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

WGPUBindGroupEntry AsDawnType(const GPUBindGroupEntry* webgpu_binding) {
  WGPUBindGroupEntry dawn_binding = {};

  dawn_binding.binding = webgpu_binding->binding();

  if (webgpu_binding->resource().IsGPUBufferBinding()) {
    GPUBufferBinding* buffer =
        webgpu_binding->resource().GetAsGPUBufferBinding();
    dawn_binding.offset = buffer->offset();
    dawn_binding.size = buffer->hasSize() ? buffer->size() : WGPU_WHOLE_SIZE;
    dawn_binding.buffer = AsDawnType(buffer->buffer());

  } else if (webgpu_binding->resource().IsGPUSampler()) {
    GPUSampler* sampler = webgpu_binding->resource().GetAsGPUSampler();
    dawn_binding.sampler = AsDawnType(sampler);

  } else if (webgpu_binding->resource().IsGPUTextureView()) {
    GPUTextureView* texture_view =
        webgpu_binding->resource().GetAsGPUTextureView();
    dawn_binding.textureView = AsDawnType(texture_view);

  } else {
    NOTREACHED();
  }

  return dawn_binding;
}

// static
GPUBindGroup* GPUBindGroup::Create(GPUDevice* device,
                                   const GPUBindGroupDescriptor* webgpu_desc,
                                   ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  if (webgpu_desc->hasBindings()) {
    device->AddConsoleWarning(
        "GPUBindGroupDescriptor.bindings is deprecated: renamed to entries");
  }

  uint32_t entry_count = 0;
  std::unique_ptr<WGPUBindGroupEntry[]> entries;
  if (webgpu_desc->hasEntries()) {
    entry_count = static_cast<uint32_t>(webgpu_desc->entries().size());
    entries = entry_count != 0 ? AsDawnType(webgpu_desc->entries()) : nullptr;
  } else {
    if (!webgpu_desc->hasBindings()) {
      exception_state.ThrowTypeError("required member entries is undefined.");
      return nullptr;
    }

    entry_count = static_cast<uint32_t>(webgpu_desc->bindings().size());
    entries = entry_count != 0 ? AsDawnType(webgpu_desc->bindings()) : nullptr;
  }

  WGPUBindGroupDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.layout = AsDawnType(webgpu_desc->layout());
  dawn_desc.entryCount = entry_count;
  dawn_desc.entries = entries.get();
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  return MakeGarbageCollected<GPUBindGroup>(
      device, device->GetProcs().deviceCreateBindGroup(device->GetHandle(),
                                                       &dawn_desc));
}

GPUBindGroup::GPUBindGroup(GPUDevice* device, WGPUBindGroup bind_group)
    : DawnObject<WGPUBindGroup>(device, bind_group) {}

GPUBindGroup::~GPUBindGroup() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().bindGroupRelease(GetHandle());
}

}  // namespace blink

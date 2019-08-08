// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_control_client_holder.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"

namespace blink {

// static
GPUDevice* GPUDevice::Create(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    GPUAdapter* adapter,
    const GPUDeviceDescriptor* descriptor) {
  return MakeGarbageCollected<GPUDevice>(std::move(dawn_control_client),
                                         adapter, descriptor);
}

// TODO(enga): Handle adapter options and device descriptor
GPUDevice::GPUDevice(scoped_refptr<DawnControlClientHolder> dawn_control_client,
                     GPUAdapter* adapter,
                     const GPUDeviceDescriptor* descriptor)
    : DawnObject(dawn_control_client,
                 dawn_control_client->GetInterface()->GetDefaultDevice()),
      adapter_(adapter),
      queue_(
          GPUQueue::Create(this, GetProcs().deviceCreateQueue(GetHandle()))) {}

GPUDevice::~GPUDevice() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().deviceRelease(GetHandle());
}

GPUAdapter* GPUDevice::adapter() const {
  return adapter_;
}

GPUBuffer* GPUDevice::createBuffer(const GPUBufferDescriptor* descriptor) {
  return GPUBuffer::Create(this, descriptor);
}

GPUTexture* GPUDevice::createTexture(const GPUTextureDescriptor* descriptor) {
  return GPUTexture::Create(this, descriptor);
}

GPUSampler* GPUDevice::createSampler(const GPUSamplerDescriptor* descriptor) {
  return GPUSampler::Create(this, descriptor);
}

GPUBindGroup* GPUDevice::createBindGroup(
    const GPUBindGroupDescriptor* descriptor) {
  return GPUBindGroup::Create(this, descriptor);
}

GPUBindGroupLayout* GPUDevice::createBindGroupLayout(
    const GPUBindGroupLayoutDescriptor* descriptor) {
  return GPUBindGroupLayout::Create(this, descriptor);
}

GPUPipelineLayout* GPUDevice::createPipelineLayout(
    const GPUPipelineLayoutDescriptor* descriptor) {
  return GPUPipelineLayout::Create(this, descriptor);
}

GPUShaderModule* GPUDevice::createShaderModule(
    const GPUShaderModuleDescriptor* descriptor) {
  return GPUShaderModule::Create(this, descriptor);
}

GPURenderPipeline* GPUDevice::createRenderPipeline(
    const GPURenderPipelineDescriptor* descriptor) {
  return GPURenderPipeline::Create(this, descriptor);
}

GPUComputePipeline* GPUDevice::createComputePipeline(
    const GPUComputePipelineDescriptor* descriptor) {
  return GPUComputePipeline::Create(this, descriptor);
}

GPUCommandEncoder* GPUDevice::createCommandEncoder(
    const GPUCommandEncoderDescriptor* descriptor) {
  return GPUCommandEncoder::Create(this, descriptor);
}

GPUQueue* GPUDevice::getQueue() {
  return queue_;
}

void GPUDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(adapter_);
  visitor->Trace(queue_);
  DawnObject<DawnDevice>::Trace(visitor);
}

}  // namespace blink

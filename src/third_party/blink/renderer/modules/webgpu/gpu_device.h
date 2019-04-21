// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUAdapter;
class GPUAdapter;
class GPUBuffer;
class GPUBufferDescriptor;
class GPUCommandEncoder;
class GPUCommandEncoderDescriptor;
class GPUBindGroup;
class GPUBindGroupDescriptor;
class GPUBindGroupLayout;
class GPUBindGroupLayoutDescriptor;
class GPUComputePipeline;
class GPUComputePipelineDescriptor;
class GPUDeviceDescriptor;
class GPUPipelineLayout;
class GPUPipelineLayoutDescriptor;
class GPUQueue;
class GPURenderPipeline;
class GPURenderPipelineDescriptor;
class GPUSampler;
class GPUSamplerDescriptor;
class GPUShaderModule;
class GPUShaderModuleDescriptor;
class GPUTexture;
class GPUTextureDescriptor;

class GPUDevice final : public DawnObject<DawnDevice> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUDevice* Create(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      GPUAdapter* adapter,
      const GPUDeviceDescriptor* descriptor);
  explicit GPUDevice(scoped_refptr<DawnControlClientHolder> dawn_control_client,
                     GPUAdapter* adapter,
                     const GPUDeviceDescriptor* descriptor);
  ~GPUDevice() override;

  void Trace(blink::Visitor* visitor) override;

  // gpu_device.idl
  GPUAdapter* adapter() const;

  GPUBuffer* createBuffer(const GPUBufferDescriptor* descriptor);
  GPUTexture* createTexture(const GPUTextureDescriptor* descriptor);
  GPUSampler* createSampler(const GPUSamplerDescriptor* descriptor);

  GPUBindGroup* createBindGroup(const GPUBindGroupDescriptor* descriptor);
  GPUBindGroupLayout* createBindGroupLayout(
      const GPUBindGroupLayoutDescriptor* descriptor);
  GPUPipelineLayout* createPipelineLayout(
      const GPUPipelineLayoutDescriptor* descriptor);

  GPUShaderModule* createShaderModule(
      const GPUShaderModuleDescriptor* descriptor);
  GPURenderPipeline* createRenderPipeline(
      const GPURenderPipelineDescriptor* descriptor);
  GPUComputePipeline* createComputePipeline(
      const GPUComputePipelineDescriptor* descriptor);

  GPUCommandEncoder* createCommandEncoder(
      const GPUCommandEncoderDescriptor* descriptor);

  GPUQueue* getQueue();

 private:
  Member<GPUAdapter> adapter_;
  Member<GPUQueue> queue_;

  DISALLOW_COPY_AND_ASSIGN(GPUDevice);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_

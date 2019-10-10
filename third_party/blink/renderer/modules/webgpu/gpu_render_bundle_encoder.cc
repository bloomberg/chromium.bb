// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle_encoder.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle_encoder_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"

namespace blink {

// static
GPURenderBundleEncoder* GPURenderBundleEncoder::Create(
    GPUDevice* device,
    const GPURenderBundleEncoderDescriptor* webgpu_desc) {
  uint32_t color_formats_count =
      static_cast<uint32_t>(webgpu_desc->colorFormats().size());

  std::unique_ptr<DawnTextureFormat[]> color_formats =
      AsDawnEnum<DawnTextureFormat>(webgpu_desc->colorFormats());

  DawnTextureFormat depth_stencil_format = DAWN_TEXTURE_FORMAT_UNDEFINED;
  if (webgpu_desc->hasDepthStencilFormat()) {
    depth_stencil_format =
        AsDawnEnum<DawnTextureFormat>(webgpu_desc->depthStencilFormat());
  }

  DawnRenderBundleEncoderDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.colorFormatsCount = color_formats_count;
  dawn_desc.colorFormats = color_formats.get();
  dawn_desc.depthStencilFormat = depth_stencil_format;
  dawn_desc.sampleCount = webgpu_desc->sampleCount();
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  return MakeGarbageCollected<GPURenderBundleEncoder>(
      device, device->GetProcs().deviceCreateRenderBundleEncoder(
                  device->GetHandle(), &dawn_desc));
}

GPURenderBundleEncoder::GPURenderBundleEncoder(
    GPUDevice* device,
    DawnRenderBundleEncoder render_bundle_encoder)
    : DawnObject<DawnRenderBundleEncoder>(device, render_bundle_encoder) {}

GPURenderBundleEncoder::~GPURenderBundleEncoder() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().renderBundleEncoderRelease(GetHandle());
}

void GPURenderBundleEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint64_t>& dynamicOffsets) {
  GetProcs().renderBundleEncoderSetBindGroup(
      GetHandle(), index, bindGroup->GetHandle(), dynamicOffsets.size(),
      dynamicOffsets.data());
}

void GPURenderBundleEncoder::pushDebugGroup(String groupLabel) {
  GetProcs().renderBundleEncoderPushDebugGroup(GetHandle(),
                                               groupLabel.Utf8().data());
}

void GPURenderBundleEncoder::popDebugGroup() {
  GetProcs().renderBundleEncoderPopDebugGroup(GetHandle());
}

void GPURenderBundleEncoder::insertDebugMarker(String markerLabel) {
  GetProcs().renderBundleEncoderInsertDebugMarker(GetHandle(),
                                                  markerLabel.Utf8().data());
}

void GPURenderBundleEncoder::setPipeline(GPURenderPipeline* pipeline) {
  GetProcs().renderBundleEncoderSetPipeline(GetHandle(), pipeline->GetHandle());
}

void GPURenderBundleEncoder::setIndexBuffer(GPUBuffer* buffer,
                                            uint64_t offset) {
  GetProcs().renderBundleEncoderSetIndexBuffer(GetHandle(), buffer->GetHandle(),
                                               offset);
}

void GPURenderBundleEncoder::setVertexBuffer(uint32_t slot,
                                             const GPUBuffer* buffer,
                                             uint64_t offset) {
  GetProcs().renderBundleEncoderSetVertexBuffer(GetHandle(), slot,
                                                buffer->GetHandle(), offset);
}

void GPURenderBundleEncoder::draw(uint32_t vertexCount,
                                  uint32_t instanceCount,
                                  uint32_t firstVertex,
                                  uint32_t firstInstance) {
  GetProcs().renderBundleEncoderDraw(GetHandle(), vertexCount, instanceCount,
                                     firstVertex, firstInstance);
}

void GPURenderBundleEncoder::drawIndexed(uint32_t indexCount,
                                         uint32_t instanceCount,
                                         uint32_t firstIndex,
                                         int32_t baseVertex,
                                         uint32_t firstInstance) {
  GetProcs().renderBundleEncoderDrawIndexed(GetHandle(), indexCount,
                                            instanceCount, firstIndex,
                                            baseVertex, firstInstance);
}

void GPURenderBundleEncoder::drawIndirect(GPUBuffer* indirectBuffer,
                                          uint64_t indirectOffset) {
  GetProcs().renderBundleEncoderDrawIndirect(
      GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
}

void GPURenderBundleEncoder::drawIndexedIndirect(GPUBuffer* indirectBuffer,
                                                 uint64_t indirectOffset) {
  GetProcs().renderBundleEncoderDrawIndexedIndirect(
      GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
}

GPURenderBundle* GPURenderBundleEncoder::finish(
    const GPURenderBundleDescriptor* webgpu_desc) {
  DawnRenderBundleDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  DawnRenderBundle render_bundle =
      GetProcs().renderBundleEncoderFinish(GetHandle(), &dawn_desc);
  return GPURenderBundle::Create(device_, render_bundle);
}

}  // namespace blink

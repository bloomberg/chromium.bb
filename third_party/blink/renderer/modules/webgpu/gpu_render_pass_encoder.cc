// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_encoder.h"

#include "third_party/blink/renderer/bindings/modules/v8/double_sequence_or_gpu_color_dict.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"

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

void GPURenderPassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint64_t>& dynamicOffsets) {
  GetProcs().renderPassEncoderSetBindGroup(
      GetHandle(), index, bindGroup->GetHandle(), dynamicOffsets.size(),
      dynamicOffsets.data());
}

void GPURenderPassEncoder::pushDebugGroup(String groupLabel) {
  GetProcs().renderPassEncoderPushDebugGroup(GetHandle(),
                                             groupLabel.Utf8().data());
}

void GPURenderPassEncoder::popDebugGroup() {
  GetProcs().renderPassEncoderPopDebugGroup(GetHandle());
}

void GPURenderPassEncoder::insertDebugMarker(String markerLabel) {
  GetProcs().renderPassEncoderInsertDebugMarker(GetHandle(),
                                                markerLabel.Utf8().data());
}

void GPURenderPassEncoder::setPipeline(GPURenderPipeline* pipeline) {
  GetProcs().renderPassEncoderSetPipeline(GetHandle(), pipeline->GetHandle());
}

void GPURenderPassEncoder::setBlendColor(DoubleSequenceOrGPUColorDict& color,
                                         ExceptionState& exception_state) {
  if (color.IsDoubleSequence() && color.GetAsDoubleSequence().size() != 4) {
    exception_state.ThrowRangeError("color size must be 4");
    return;
  }

  DawnColor dawn_color = AsDawnType(&color);
  GetProcs().renderPassEncoderSetBlendColor(GetHandle(), &dawn_color);
}

void GPURenderPassEncoder::setStencilReference(uint32_t reference) {
  GetProcs().renderPassEncoderSetStencilReference(GetHandle(), reference);
}

void GPURenderPassEncoder::setViewport(float x,
                                       float y,
                                       float width,
                                       float height,
                                       float minDepth,
                                       float maxDepth) {
  GetProcs().renderPassEncoderSetViewport(GetHandle(), x, y, width, height,
                                          minDepth, maxDepth);
}

void GPURenderPassEncoder::setScissorRect(uint32_t x,
                                          uint32_t y,
                                          uint32_t width,
                                          uint32_t height) {
  GetProcs().renderPassEncoderSetScissorRect(GetHandle(), x, y, width, height);
}

void GPURenderPassEncoder::setIndexBuffer(GPUBuffer* buffer, uint64_t offset) {
  GetProcs().renderPassEncoderSetIndexBuffer(GetHandle(), buffer->GetHandle(),
                                             offset);
}

void GPURenderPassEncoder::setVertexBuffer(uint32_t slot,
                                           const GPUBuffer* buffer,
                                           const uint64_t offset) {
  GetProcs().renderPassEncoderSetVertexBuffer(GetHandle(), slot,
                                              buffer->GetHandle(), offset);
}

void GPURenderPassEncoder::draw(uint32_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t firstInstance) {
  GetProcs().renderPassEncoderDraw(GetHandle(), vertexCount, instanceCount,
                                   firstVertex, firstInstance);
}

void GPURenderPassEncoder::drawIndexed(uint32_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t baseVertex,
                                       uint32_t firstInstance) {
  GetProcs().renderPassEncoderDrawIndexed(GetHandle(), indexCount,
                                          instanceCount, firstIndex, baseVertex,
                                          firstInstance);
}

void GPURenderPassEncoder::drawIndirect(GPUBuffer* indirectBuffer,
                                        uint64_t indirectOffset) {
  GetProcs().renderPassEncoderDrawIndirect(
      GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
}

void GPURenderPassEncoder::drawIndexedIndirect(GPUBuffer* indirectBuffer,
                                               uint64_t indirectOffset) {
  GetProcs().renderPassEncoderDrawIndexedIndirect(
      GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
}

void GPURenderPassEncoder::executeBundles(
    const HeapVector<Member<GPURenderBundle>>& bundles) {
  std::unique_ptr<DawnRenderBundle[]> dawn_bundles = AsDawnType(bundles);

  GetProcs().renderPassEncoderExecuteBundles(GetHandle(), bundles.size(),
                                             dawn_bundles.get());
}

void GPURenderPassEncoder::endPass() {
  GetProcs().renderPassEncoderEndPass(GetHandle());
}

}  // namespace blink

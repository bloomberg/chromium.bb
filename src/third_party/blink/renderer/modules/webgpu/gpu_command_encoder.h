// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class ExceptionState;
class GPUImageCopyBuffer;
class GPUCommandBuffer;
class GPUCommandBufferDescriptor;
class GPUCommandEncoderDescriptor;
class GPUComputePassDescriptor;
class GPUComputePassEncoder;
class GPURenderPassDescriptor;
class GPURenderPassEncoder;
class GPUImageCopyTexture;
class UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict;

class GPUCommandEncoder : public DawnObject<WGPUCommandEncoder> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUCommandEncoder* Create(
      GPUDevice* device,
      const GPUCommandEncoderDescriptor* webgpu_desc);
  explicit GPUCommandEncoder(GPUDevice* device,
                             WGPUCommandEncoder command_encoder);

  // gpu_command_encoder.idl
  GPURenderPassEncoder* beginRenderPass(
      const GPURenderPassDescriptor* descriptor,
      ExceptionState& exception_state);
  GPUComputePassEncoder* beginComputePass(
      const GPUComputePassDescriptor* descriptor);
  void copyBufferToBuffer(DawnObject<WGPUBuffer>* src,
                          uint64_t src_offset,
                          DawnObject<WGPUBuffer>* dst,
                          uint64_t dst_offset,
                          uint64_t size) {
    DCHECK(src);
    DCHECK(dst);
    GetProcs().commandEncoderCopyBufferToBuffer(GetHandle(), src->GetHandle(),
                                                src_offset, dst->GetHandle(),
                                                dst_offset, size);
  }
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void copyBufferToTexture(GPUImageCopyBuffer* source,
                           GPUImageCopyTexture* destination,
                           const V8GPUExtent3D* copy_size);
  void copyTextureToBuffer(GPUImageCopyTexture* source,
                           GPUImageCopyBuffer* destination,
                           const V8GPUExtent3D* copy_size);
  void copyTextureToTexture(GPUImageCopyTexture* source,
                            GPUImageCopyTexture* destination,
                            const V8GPUExtent3D* copy_size);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void copyBufferToTexture(
      GPUImageCopyBuffer* source,
      GPUImageCopyTexture* destination,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& copy_size);
  void copyTextureToBuffer(
      GPUImageCopyTexture* source,
      GPUImageCopyBuffer* destination,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& copy_size);
  void copyTextureToTexture(
      GPUImageCopyTexture* source,
      GPUImageCopyTexture* destination,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& copy_size);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void pushDebugGroup(String groupLabel) {
    std::string label = groupLabel.Utf8();
    GetProcs().commandEncoderPushDebugGroup(GetHandle(), label.c_str());
  }
  void popDebugGroup() { GetProcs().commandEncoderPopDebugGroup(GetHandle()); }
  void insertDebugMarker(String markerLabel) {
    std::string label = markerLabel.Utf8();
    GetProcs().commandEncoderInsertDebugMarker(GetHandle(), label.c_str());
  }
  void resolveQuerySet(DawnObject<WGPUQuerySet>* querySet,
                       uint32_t firstQuery,
                       uint32_t queryCount,
                       DawnObject<WGPUBuffer>* destination,
                       uint64_t destinationOffset) {
    GetProcs().commandEncoderResolveQuerySet(
        GetHandle(), querySet->GetHandle(), firstQuery, queryCount,
        destination->GetHandle(), destinationOffset);
  }
  void writeTimestamp(DawnObject<WGPUQuerySet>* querySet, uint32_t queryIndex) {
    GetProcs().commandEncoderWriteTimestamp(GetHandle(), querySet->GetHandle(),
                                            queryIndex);
  }
  GPUCommandBuffer* finish(const GPUCommandBufferDescriptor* descriptor);

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUCommandEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_

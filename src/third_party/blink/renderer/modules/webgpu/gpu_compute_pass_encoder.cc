// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pass_encoder.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

GPUComputePassEncoder::GPUComputePassEncoder(
    GPUDevice* device,
    WGPUComputePassEncoder compute_pass_encoder)
    : DawnObject<WGPUComputePassEncoder>(device, compute_pass_encoder) {}

GPUComputePassEncoder::~GPUComputePassEncoder() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().computePassEncoderRelease(GetHandle());
}

void GPUComputePassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint32_t>& dynamicOffsets) {
  GetProcs().computePassEncoderSetBindGroup(
      GetHandle(), index, bindGroup->GetHandle(), dynamicOffsets.size(),
      dynamicOffsets.data());
}

void GPUComputePassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bind_group,
    const FlexibleUint32Array& dynamic_offsets_data,
    uint64_t dynamic_offsets_data_start,
    uint32_t dynamic_offsets_data_length,
    ExceptionState& exception_state) {
  if (!ValidateSetBindGroupDynamicOffsets(
          dynamic_offsets_data, dynamic_offsets_data_start,
          dynamic_offsets_data_length, exception_state)) {
    return;
  }

  const uint32_t* data =
      dynamic_offsets_data.DataMaybeOnStack() + dynamic_offsets_data_start;

  GetProcs().computePassEncoderSetBindGroup(GetHandle(), index,
                                            bind_group->GetHandle(),
                                            dynamic_offsets_data_length, data);
}

void GPUComputePassEncoder::pushDebugGroup(String groupLabel) {
  GetProcs().computePassEncoderPushDebugGroup(GetHandle(),
                                              groupLabel.Utf8().data());
}

void GPUComputePassEncoder::popDebugGroup() {
  GetProcs().computePassEncoderPopDebugGroup(GetHandle());
}

void GPUComputePassEncoder::insertDebugMarker(String markerLabel) {
  GetProcs().computePassEncoderInsertDebugMarker(GetHandle(),
                                                 markerLabel.Utf8().data());
}

void GPUComputePassEncoder::setPipeline(GPUComputePipeline* pipeline) {
  GetProcs().computePassEncoderSetPipeline(GetHandle(), pipeline->GetHandle());
}

void GPUComputePassEncoder::dispatch(uint32_t x, uint32_t y, uint32_t z) {
  GetProcs().computePassEncoderDispatch(GetHandle(), x, y, z);
}

void GPUComputePassEncoder::dispatchIndirect(GPUBuffer* indirectBuffer,
                                             uint64_t indirectOffset) {
  GetProcs().computePassEncoderDispatchIndirect(
      GetHandle(), indirectBuffer->GetHandle(), indirectOffset);
}

void GPUComputePassEncoder::endPass() {
  GetProcs().computePassEncoderEndPass(GetHandle());
}

}  // namespace blink

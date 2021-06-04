// Copyright 2019 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dawn_native/RenderEncoderBase.h"

#include "common/Constants.h"
#include "common/Log.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/ValidationUtils_autogen.h"

#include <math.h>
#include <cstring>

namespace dawn_native {

    RenderEncoderBase::RenderEncoderBase(DeviceBase* device,
                                         EncodingContext* encodingContext,
                                         Ref<AttachmentState> attachmentState)
        : ProgrammablePassEncoder(device, encodingContext),
          mAttachmentState(std::move(attachmentState)),
          mDisableBaseVertex(device->IsToggleEnabled(Toggle::DisableBaseVertex)),
          mDisableBaseInstance(device->IsToggleEnabled(Toggle::DisableBaseInstance)) {
    }

    RenderEncoderBase::RenderEncoderBase(DeviceBase* device,
                                         EncodingContext* encodingContext,
                                         ErrorTag errorTag)
        : ProgrammablePassEncoder(device, encodingContext, errorTag),
          mDisableBaseVertex(device->IsToggleEnabled(Toggle::DisableBaseVertex)),
          mDisableBaseInstance(device->IsToggleEnabled(Toggle::DisableBaseInstance)) {
    }

    const AttachmentState* RenderEncoderBase::GetAttachmentState() const {
        ASSERT(!IsError());
        ASSERT(mAttachmentState != nullptr);
        return mAttachmentState.Get();
    }

    Ref<AttachmentState> RenderEncoderBase::AcquireAttachmentState() {
        return std::move(mAttachmentState);
    }

    void RenderEncoderBase::APIDraw(uint32_t vertexCount,
                                    uint32_t instanceCount,
                                    uint32_t firstVertex,
                                    uint32_t firstInstance) {
        mEncodingContext->TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (IsValidationEnabled()) {
                DAWN_TRY(mCommandBufferState.ValidateCanDraw());

                if (mDisableBaseInstance && firstInstance != 0) {
                    return DAWN_VALIDATION_ERROR("Non-zero first instance not supported");
                }
            }

            DrawCmd* draw = allocator->Allocate<DrawCmd>(Command::Draw);
            draw->vertexCount = vertexCount;
            draw->instanceCount = instanceCount;
            draw->firstVertex = firstVertex;
            draw->firstInstance = firstInstance;

            return {};
        });
    }

    void RenderEncoderBase::APIDrawIndexed(uint32_t indexCount,
                                           uint32_t instanceCount,
                                           uint32_t firstIndex,
                                           int32_t baseVertex,
                                           uint32_t firstInstance) {
        mEncodingContext->TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (IsValidationEnabled()) {
                DAWN_TRY(mCommandBufferState.ValidateCanDrawIndexed());

                if (mDisableBaseInstance && firstInstance != 0) {
                    return DAWN_VALIDATION_ERROR("Non-zero first instance not supported");
                }
                if (mDisableBaseVertex && baseVertex != 0) {
                    return DAWN_VALIDATION_ERROR("Non-zero base vertex not supported");
                }
            }

            if (static_cast<uint64_t>(firstIndex) + indexCount >
                mCommandBufferState.GetIndexBufferSize() /
                    IndexFormatSize(mCommandBufferState.GetIndexFormat())) {
                // Index range is out of bounds
                // Treat as no-op and skip issuing draw call
                dawn::WarningLog() << "Index range is out of bounds";
                return {};
            }
            DrawIndexedCmd* draw = allocator->Allocate<DrawIndexedCmd>(Command::DrawIndexed);
            draw->indexCount = indexCount;
            draw->instanceCount = instanceCount;
            draw->firstIndex = firstIndex;
            draw->baseVertex = baseVertex;
            draw->firstInstance = firstInstance;

            return {};
        });
    }

    void RenderEncoderBase::APIDrawIndirect(BufferBase* indirectBuffer, uint64_t indirectOffset) {
        mEncodingContext->TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (IsValidationEnabled()) {
                DAWN_TRY(GetDevice()->ValidateObject(indirectBuffer));
                DAWN_TRY(ValidateCanUseAs(indirectBuffer, wgpu::BufferUsage::Indirect));
                DAWN_TRY(mCommandBufferState.ValidateCanDraw());

                if (indirectOffset % 4 != 0) {
                    return DAWN_VALIDATION_ERROR("Indirect offset must be a multiple of 4");
                }

                if (indirectOffset >= indirectBuffer->GetSize() ||
                    indirectOffset + kDrawIndirectSize > indirectBuffer->GetSize()) {
                    return DAWN_VALIDATION_ERROR("Indirect offset out of bounds");
                }
            }

            DrawIndirectCmd* cmd = allocator->Allocate<DrawIndirectCmd>(Command::DrawIndirect);
            cmd->indirectBuffer = indirectBuffer;
            cmd->indirectOffset = indirectOffset;

            mUsageTracker.BufferUsedAs(indirectBuffer, wgpu::BufferUsage::Indirect);

            return {};
        });
    }

    void RenderEncoderBase::APIDrawIndexedIndirect(BufferBase* indirectBuffer,
                                                   uint64_t indirectOffset) {
        mEncodingContext->TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (IsValidationEnabled()) {
                DAWN_TRY(GetDevice()->ValidateObject(indirectBuffer));
                DAWN_TRY(ValidateCanUseAs(indirectBuffer, wgpu::BufferUsage::Indirect));
                DAWN_TRY(mCommandBufferState.ValidateCanDrawIndexed());

                // Indexed indirect draws need a compute-shader based validation check that the
                // range of indices is contained inside the index buffer on Metal. Disallow them as
                // unsafe until the validation is implemented.
                if (GetDevice()->IsToggleEnabled(Toggle::DisallowUnsafeAPIs)) {
                    return DAWN_VALIDATION_ERROR(
                        "DrawIndexedIndirect is disallowed because it doesn't validate that the "
                        "index "
                        "range is valid yet.");
                }

                if (indirectOffset % 4 != 0) {
                    return DAWN_VALIDATION_ERROR("Indirect offset must be a multiple of 4");
                }

                if ((indirectOffset >= indirectBuffer->GetSize() ||
                     indirectOffset + kDrawIndexedIndirectSize > indirectBuffer->GetSize())) {
                    return DAWN_VALIDATION_ERROR("Indirect offset out of bounds");
                }
            }

            DrawIndexedIndirectCmd* cmd =
                allocator->Allocate<DrawIndexedIndirectCmd>(Command::DrawIndexedIndirect);
            cmd->indirectBuffer = indirectBuffer;
            cmd->indirectOffset = indirectOffset;

            mUsageTracker.BufferUsedAs(indirectBuffer, wgpu::BufferUsage::Indirect);

            return {};
        });
    }

    void RenderEncoderBase::APISetPipeline(RenderPipelineBase* pipeline) {
        mEncodingContext->TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (IsValidationEnabled()) {
                DAWN_TRY(GetDevice()->ValidateObject(pipeline));

                if (pipeline->GetAttachmentState() != mAttachmentState.Get()) {
                    return DAWN_VALIDATION_ERROR(
                        "Pipeline attachment state is not compatible with render encoder "
                        "attachment state");
                }
            }

            mCommandBufferState.SetRenderPipeline(pipeline);

            SetRenderPipelineCmd* cmd =
                allocator->Allocate<SetRenderPipelineCmd>(Command::SetRenderPipeline);
            cmd->pipeline = pipeline;

            return {};
        });
    }

    void RenderEncoderBase::APISetIndexBuffer(BufferBase* buffer,
                                              wgpu::IndexFormat format,
                                              uint64_t offset,
                                              uint64_t size) {
        mEncodingContext->TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (IsValidationEnabled()) {
                DAWN_TRY(GetDevice()->ValidateObject(buffer));
                DAWN_TRY(ValidateCanUseAs(buffer, wgpu::BufferUsage::Index));

                DAWN_TRY(ValidateIndexFormat(format));
                if (format == wgpu::IndexFormat::Undefined) {
                    return DAWN_VALIDATION_ERROR("Index format must be specified");
                }

                uint64_t bufferSize = buffer->GetSize();
                if (offset > bufferSize) {
                    return DAWN_VALIDATION_ERROR("Offset larger than the buffer size");
                }
                uint64_t remainingSize = bufferSize - offset;

                if (size == 0) {
                    size = remainingSize;
                } else {
                    if (size > remainingSize) {
                        return DAWN_VALIDATION_ERROR("Size + offset larger than the buffer size");
                    }
                }
            } else {
                if (size == 0) {
                    size = buffer->GetSize() - offset;
                }
            }

            mCommandBufferState.SetIndexBuffer(format, size);

            SetIndexBufferCmd* cmd =
                allocator->Allocate<SetIndexBufferCmd>(Command::SetIndexBuffer);
            cmd->buffer = buffer;
            cmd->format = format;
            cmd->offset = offset;
            cmd->size = size;

            mUsageTracker.BufferUsedAs(buffer, wgpu::BufferUsage::Index);

            return {};
        });
    }

    void RenderEncoderBase::APISetVertexBuffer(uint32_t slot,
                                               BufferBase* buffer,
                                               uint64_t offset,
                                               uint64_t size) {
        mEncodingContext->TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (IsValidationEnabled()) {
                DAWN_TRY(GetDevice()->ValidateObject(buffer));
                DAWN_TRY(ValidateCanUseAs(buffer, wgpu::BufferUsage::Vertex));

                if (slot >= kMaxVertexBuffers) {
                    return DAWN_VALIDATION_ERROR("Vertex buffer slot out of bounds");
                }

                uint64_t bufferSize = buffer->GetSize();
                if (offset > bufferSize) {
                    return DAWN_VALIDATION_ERROR("Offset larger than the buffer size");
                }
                uint64_t remainingSize = bufferSize - offset;

                if (size == 0) {
                    size = remainingSize;
                } else {
                    if (size > remainingSize) {
                        return DAWN_VALIDATION_ERROR("Size + offset larger than the buffer size");
                    }
                }
            } else {
                if (size == 0) {
                    size = buffer->GetSize() - offset;
                }
            }

            mCommandBufferState.SetVertexBuffer(VertexBufferSlot(uint8_t(slot)));

            SetVertexBufferCmd* cmd =
                allocator->Allocate<SetVertexBufferCmd>(Command::SetVertexBuffer);
            cmd->slot = VertexBufferSlot(static_cast<uint8_t>(slot));
            cmd->buffer = buffer;
            cmd->offset = offset;
            cmd->size = size;

            mUsageTracker.BufferUsedAs(buffer, wgpu::BufferUsage::Vertex);

            return {};
        });
    }

    void RenderEncoderBase::APISetBindGroup(uint32_t groupIndexIn,
                                            BindGroupBase* group,
                                            uint32_t dynamicOffsetCount,
                                            const uint32_t* dynamicOffsets) {
        mEncodingContext->TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            BindGroupIndex groupIndex(groupIndexIn);

            if (IsValidationEnabled()) {
                DAWN_TRY(
                    ValidateSetBindGroup(groupIndex, group, dynamicOffsetCount, dynamicOffsets));
            }

            RecordSetBindGroup(allocator, groupIndex, group, dynamicOffsetCount, dynamicOffsets);
            mCommandBufferState.SetBindGroup(groupIndex, group);
            mUsageTracker.AddBindGroup(group);

            return {};
        });
    }

}  // namespace dawn_native

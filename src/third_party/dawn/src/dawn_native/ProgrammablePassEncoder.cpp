// Copyright 2018 The Dawn Authors
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

#include "dawn_native/ProgrammablePassEncoder.h"

#include "common/BitSetIterator.h"
#include "common/ityp_array.h"
#include "dawn_native/BindGroup.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBuffer.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/ObjectType_autogen.h"
#include "dawn_native/ValidationUtils_autogen.h"

#include <cstring>

namespace dawn_native {

    ProgrammablePassEncoder::ProgrammablePassEncoder(DeviceBase* device,
                                                     EncodingContext* encodingContext)
        : ApiObjectBase(device, kLabelNotImplemented),
          mEncodingContext(encodingContext),
          mValidationEnabled(device->IsValidationEnabled()) {
    }

    ProgrammablePassEncoder::ProgrammablePassEncoder(DeviceBase* device,
                                                     EncodingContext* encodingContext,
                                                     ErrorTag errorTag)
        : ApiObjectBase(device, errorTag),
          mEncodingContext(encodingContext),
          mValidationEnabled(device->IsValidationEnabled()) {
    }

    bool ProgrammablePassEncoder::IsValidationEnabled() const {
        return mValidationEnabled;
    }

    MaybeError ProgrammablePassEncoder::ValidateProgrammableEncoderEnd() const {
        if (mDebugGroupStackSize != 0) {
            return DAWN_VALIDATION_ERROR("Each Push must be balanced by a corresponding Pop.");
        }
        return {};
    }

    void ProgrammablePassEncoder::APIInsertDebugMarker(const char* groupLabel) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                InsertDebugMarkerCmd* cmd =
                    allocator->Allocate<InsertDebugMarkerCmd>(Command::InsertDebugMarker);
                cmd->length = strlen(groupLabel);

                char* label = allocator->AllocateData<char>(cmd->length + 1);
                memcpy(label, groupLabel, cmd->length + 1);

                return {};
            },
            "encoding InsertDebugMarker(\"%s\")", groupLabel);
    }

    void ProgrammablePassEncoder::APIPopDebugGroup() {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    if (mDebugGroupStackSize == 0) {
                        return DAWN_VALIDATION_ERROR(
                            "Pop must be balanced by a corresponding Push.");
                    }
                }
                allocator->Allocate<PopDebugGroupCmd>(Command::PopDebugGroup);
                mDebugGroupStackSize--;
                mEncodingContext->PopDebugGroupLabel();

                return {};
            },
            "encoding PopDebugGroup()");
    }

    void ProgrammablePassEncoder::APIPushDebugGroup(const char* groupLabel) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                PushDebugGroupCmd* cmd =
                    allocator->Allocate<PushDebugGroupCmd>(Command::PushDebugGroup);
                cmd->length = strlen(groupLabel);

                char* label = allocator->AllocateData<char>(cmd->length + 1);
                memcpy(label, groupLabel, cmd->length + 1);

                mDebugGroupStackSize++;
                mEncodingContext->PushDebugGroupLabel(groupLabel);

                return {};
            },
            "encoding PushDebugGroup(\"%s\")", groupLabel);
    }

    MaybeError ProgrammablePassEncoder::ValidateSetBindGroup(
        BindGroupIndex index,
        BindGroupBase* group,
        uint32_t dynamicOffsetCountIn,
        const uint32_t* dynamicOffsetsIn) const {
        DAWN_TRY(GetDevice()->ValidateObject(group));

        if (index >= kMaxBindGroupsTyped) {
            return DAWN_VALIDATION_ERROR("Setting bind group over the max");
        }

        ityp::span<BindingIndex, const uint32_t> dynamicOffsets(dynamicOffsetsIn,
                                                                BindingIndex(dynamicOffsetCountIn));

        // Dynamic offsets count must match the number required by the layout perfectly.
        const BindGroupLayoutBase* layout = group->GetLayout();
        if (layout->GetDynamicBufferCount() != dynamicOffsets.size()) {
            return DAWN_VALIDATION_ERROR("dynamicOffset count mismatch");
        }

        for (BindingIndex i{0}; i < dynamicOffsets.size(); ++i) {
            const BindingInfo& bindingInfo = layout->GetBindingInfo(i);

            // BGL creation sorts bindings such that the dynamic buffer bindings are first.
            // ASSERT that this true.
            ASSERT(bindingInfo.bindingType == BindingInfoType::Buffer);
            ASSERT(bindingInfo.buffer.hasDynamicOffset);

            uint64_t requiredAlignment;
            switch (bindingInfo.buffer.type) {
                case wgpu::BufferBindingType::Uniform:
                    requiredAlignment = kMinUniformBufferOffsetAlignment;
                    break;
                case wgpu::BufferBindingType::Storage:
                case wgpu::BufferBindingType::ReadOnlyStorage:
                case kInternalStorageBufferBinding:
                    requiredAlignment = kMinStorageBufferOffsetAlignment;
                    requiredAlignment = kMinStorageBufferOffsetAlignment;
                    break;
                case wgpu::BufferBindingType::Undefined:
                    UNREACHABLE();
            }

            if (!IsAligned(dynamicOffsets[i], requiredAlignment)) {
                return DAWN_VALIDATION_ERROR("Dynamic Buffer Offset need to be aligned");
            }

            BufferBinding bufferBinding = group->GetBindingAsBufferBinding(i);

            // During BindGroup creation, validation ensures binding offset + binding size
            // <= buffer size.
            ASSERT(bufferBinding.buffer->GetSize() >= bufferBinding.size);
            ASSERT(bufferBinding.buffer->GetSize() - bufferBinding.size >= bufferBinding.offset);

            if ((dynamicOffsets[i] >
                 bufferBinding.buffer->GetSize() - bufferBinding.offset - bufferBinding.size)) {
                if ((bufferBinding.buffer->GetSize() - bufferBinding.offset) ==
                    bufferBinding.size) {
                    return DAWN_VALIDATION_ERROR(
                        "Dynamic offset out of bounds. The binding goes to the end of the "
                        "buffer even with a dynamic offset of 0. Did you forget to specify "
                        "the binding's size?");
                } else {
                    return DAWN_VALIDATION_ERROR("Dynamic offset out of bounds");
                }
            }
        }

        return {};
    }

    void ProgrammablePassEncoder::RecordSetBindGroup(CommandAllocator* allocator,
                                                     BindGroupIndex index,
                                                     BindGroupBase* group,
                                                     uint32_t dynamicOffsetCount,
                                                     const uint32_t* dynamicOffsets) const {
        SetBindGroupCmd* cmd = allocator->Allocate<SetBindGroupCmd>(Command::SetBindGroup);
        cmd->index = index;
        cmd->group = group;
        cmd->dynamicOffsetCount = dynamicOffsetCount;
        if (dynamicOffsetCount > 0) {
            uint32_t* offsets = allocator->AllocateData<uint32_t>(cmd->dynamicOffsetCount);
            memcpy(offsets, dynamicOffsets, dynamicOffsetCount * sizeof(uint32_t));
        }
    }

}  // namespace dawn_native

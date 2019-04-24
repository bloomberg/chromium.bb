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

#include "dawn_native/BindGroup.h"
#include "dawn_native/CommandBuffer.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"

#include <string.h>

namespace dawn_native {

    ProgrammablePassEncoder::ProgrammablePassEncoder(DeviceBase* device,
                                                     CommandEncoderBase* topLevelEncoder,
                                                     CommandAllocator* allocator)
        : ObjectBase(device), mTopLevelEncoder(topLevelEncoder), mAllocator(allocator) {
        DAWN_ASSERT(allocator != nullptr);
    }

    ProgrammablePassEncoder::ProgrammablePassEncoder(DeviceBase* device,
                                                     CommandEncoderBase* topLevelEncoder,
                                                     ErrorTag errorTag)
        : ObjectBase(device, errorTag), mTopLevelEncoder(topLevelEncoder), mAllocator(nullptr) {
    }

    void ProgrammablePassEncoder::EndPass() {
        if (mTopLevelEncoder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        mTopLevelEncoder->PassEnded();
        mAllocator = nullptr;
    }

    void ProgrammablePassEncoder::InsertDebugMarker(const char* groupLabel) {
        if (mTopLevelEncoder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        InsertDebugMarkerCmd* cmd =
            mAllocator->Allocate<InsertDebugMarkerCmd>(Command::InsertDebugMarker);
        new (cmd) InsertDebugMarkerCmd;
        cmd->length = strlen(groupLabel);

        char* label = mAllocator->AllocateData<char>(cmd->length + 1);
        memcpy(label, groupLabel, cmd->length + 1);
    }

    void ProgrammablePassEncoder::PopDebugGroup() {
        if (mTopLevelEncoder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        PopDebugGroupCmd* cmd = mAllocator->Allocate<PopDebugGroupCmd>(Command::PopDebugGroup);
        new (cmd) PopDebugGroupCmd;
    }

    void ProgrammablePassEncoder::PushDebugGroup(const char* groupLabel) {
        if (mTopLevelEncoder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        PushDebugGroupCmd* cmd = mAllocator->Allocate<PushDebugGroupCmd>(Command::PushDebugGroup);
        new (cmd) PushDebugGroupCmd;
        cmd->length = strlen(groupLabel);

        char* label = mAllocator->AllocateData<char>(cmd->length + 1);
        memcpy(label, groupLabel, cmd->length + 1);
    }

    void ProgrammablePassEncoder::SetBindGroup(uint32_t groupIndex, BindGroupBase* group) {
        if (mTopLevelEncoder->ConsumedError(ValidateCanRecordCommands()) ||
            mTopLevelEncoder->ConsumedError(GetDevice()->ValidateObject(group))) {
            return;
        }

        if (groupIndex >= kMaxBindGroups) {
            mTopLevelEncoder->HandleError("Setting bind group over the max");
            return;
        }

        SetBindGroupCmd* cmd = mAllocator->Allocate<SetBindGroupCmd>(Command::SetBindGroup);
        new (cmd) SetBindGroupCmd;
        cmd->index = groupIndex;
        cmd->group = group;
    }

    void ProgrammablePassEncoder::SetPushConstants(dawn::ShaderStageBit stages,
                                                   uint32_t offset,
                                                   uint32_t count,
                                                   const void* data) {
        if (mTopLevelEncoder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        // TODO(cwallez@chromium.org): check for overflows
        if (offset + count > kMaxPushConstants) {
            mTopLevelEncoder->HandleError("Setting too many push constants");
            return;
        }

        SetPushConstantsCmd* cmd =
            mAllocator->Allocate<SetPushConstantsCmd>(Command::SetPushConstants);
        new (cmd) SetPushConstantsCmd;
        cmd->stages = stages;
        cmd->offset = offset;
        cmd->count = count;

        uint32_t* values = mAllocator->AllocateData<uint32_t>(count);
        memcpy(values, data, count * sizeof(uint32_t));
    }

    MaybeError ProgrammablePassEncoder::ValidateCanRecordCommands() const {
        if (mAllocator == nullptr) {
            return DAWN_VALIDATION_ERROR("Recording in an error or already ended pass encoder");
        }

        return nullptr;
    }

}  // namespace dawn_native

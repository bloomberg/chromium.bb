// Copyright 2017 The Dawn Authors
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

#include "dawn_native/PipelineLayout.h"

#include "common/Assert.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/Device.h"

namespace dawn_native {

    MaybeError ValidatePipelineLayoutDescriptor(DeviceBase* device,
                                                const PipelineLayoutDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        if (descriptor->bindGroupLayoutCount > kMaxBindGroups) {
            return DAWN_VALIDATION_ERROR("too many bind group layouts");
        }

        for (uint32_t i = 0; i < descriptor->bindGroupLayoutCount; ++i) {
            DAWN_TRY(device->ValidateObject(descriptor->bindGroupLayouts[i]));
        }
        return {};
    }

    // PipelineLayoutBase

    PipelineLayoutBase::PipelineLayoutBase(DeviceBase* device,
                                           const PipelineLayoutDescriptor* descriptor)
        : ObjectBase(device) {
        ASSERT(descriptor->bindGroupLayoutCount <= kMaxBindGroups);
        for (uint32_t group = 0; group < descriptor->bindGroupLayoutCount; ++group) {
            mBindGroupLayouts[group] = descriptor->bindGroupLayouts[group];
            mMask.set(group);
        }
    }

    PipelineLayoutBase::PipelineLayoutBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ObjectBase(device, tag) {
    }

    // static
    PipelineLayoutBase* PipelineLayoutBase::MakeError(DeviceBase* device) {
        return new PipelineLayoutBase(device, ObjectBase::kError);
    }

    const BindGroupLayoutBase* PipelineLayoutBase::GetBindGroupLayout(size_t group) const {
        ASSERT(!IsError());
        ASSERT(group < kMaxBindGroups);
        ASSERT(mMask[group]);
        return mBindGroupLayouts[group].Get();
    }

    const std::bitset<kMaxBindGroups> PipelineLayoutBase::GetBindGroupLayoutsMask() const {
        ASSERT(!IsError());
        return mMask;
    }

    std::bitset<kMaxBindGroups> PipelineLayoutBase::InheritedGroupsMask(
        const PipelineLayoutBase* other) const {
        ASSERT(!IsError());
        return {(1 << GroupsInheritUpTo(other)) - 1u};
    }

    uint32_t PipelineLayoutBase::GroupsInheritUpTo(const PipelineLayoutBase* other) const {
        ASSERT(!IsError());

        for (uint32_t i = 0; i < kMaxBindGroups; ++i) {
            if (!mMask[i] || mBindGroupLayouts[i].Get() != other->mBindGroupLayouts[i].Get()) {
                return i;
            }
        }
        return kMaxBindGroups;
    }

}  // namespace dawn_native

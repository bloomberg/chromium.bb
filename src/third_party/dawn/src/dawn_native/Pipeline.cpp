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

#include "dawn_native/Pipeline.h"

#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/Device.h"
#include "dawn_native/ObjectContentHasher.h"
#include "dawn_native/PipelineLayout.h"
#include "dawn_native/ShaderModule.h"

namespace dawn_native {

    MaybeError ValidateProgrammableStage(DeviceBase* device,
                                         const ShaderModuleBase* module,
                                         const std::string& entryPoint,
                                         const PipelineLayoutBase* layout,
                                         SingleShaderStage stage) {
        DAWN_TRY(device->ValidateObject(module));

        if (!module->HasEntryPoint(entryPoint)) {
            return DAWN_VALIDATION_ERROR("Entry point doesn't exist in the module");
        }

        const EntryPointMetadata& metadata = module->GetEntryPoint(entryPoint);

        if (metadata.stage != stage) {
            return DAWN_VALIDATION_ERROR("Entry point isn't for the correct stage");
        }

        if (layout != nullptr) {
            DAWN_TRY(ValidateCompatibilityWithPipelineLayout(device, metadata, layout));
        }

        return {};
    }

    // PipelineBase

    PipelineBase::PipelineBase(DeviceBase* device,
                               PipelineLayoutBase* layout,
                               std::vector<StageAndDescriptor> stages)
        : CachedObject(device), mLayout(layout) {
        ASSERT(!stages.empty());

        for (const StageAndDescriptor& stage : stages) {
            // Extract argument for this stage.
            SingleShaderStage shaderStage = stage.shaderStage;
            ShaderModuleBase* module = stage.module;
            const char* entryPointName = stage.entryPoint.c_str();

            const EntryPointMetadata& metadata = module->GetEntryPoint(entryPointName);
            ASSERT(metadata.stage == shaderStage);

            // Record them internally.
            bool isFirstStage = mStageMask == wgpu::ShaderStage::None;
            mStageMask |= StageBit(shaderStage);
            mStages[shaderStage] = {module, entryPointName, &metadata};

            // Compute the max() of all minBufferSizes across all stages.
            RequiredBufferSizes stageMinBufferSizes =
                ComputeRequiredBufferSizesForLayout(metadata, layout);

            if (isFirstStage) {
                mMinBufferSizes = std::move(stageMinBufferSizes);
            } else {
                for (BindGroupIndex group(0); group < mMinBufferSizes.size(); ++group) {
                    ASSERT(stageMinBufferSizes[group].size() == mMinBufferSizes[group].size());

                    for (size_t i = 0; i < stageMinBufferSizes[group].size(); ++i) {
                        mMinBufferSizes[group][i] =
                            std::max(mMinBufferSizes[group][i], stageMinBufferSizes[group][i]);
                    }
                }
            }
        }
    }

    PipelineBase::PipelineBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : CachedObject(device, tag) {
    }

    PipelineLayoutBase* PipelineBase::GetLayout() {
        ASSERT(!IsError());
        return mLayout.Get();
    }

    const PipelineLayoutBase* PipelineBase::GetLayout() const {
        ASSERT(!IsError());
        return mLayout.Get();
    }

    const RequiredBufferSizes& PipelineBase::GetMinBufferSizes() const {
        ASSERT(!IsError());
        return mMinBufferSizes;
    }

    const ProgrammableStage& PipelineBase::GetStage(SingleShaderStage stage) const {
        ASSERT(!IsError());
        return mStages[stage];
    }

    const PerStage<ProgrammableStage>& PipelineBase::GetAllStages() const {
        return mStages;
    }

    MaybeError PipelineBase::ValidateGetBindGroupLayout(uint32_t groupIndex) {
        DAWN_TRY(GetDevice()->ValidateIsAlive());
        DAWN_TRY(GetDevice()->ValidateObject(this));
        DAWN_TRY(GetDevice()->ValidateObject(mLayout.Get()));
        if (groupIndex >= kMaxBindGroups) {
            return DAWN_VALIDATION_ERROR("Bind group layout index out of bounds");
        }
        return {};
    }

    BindGroupLayoutBase* PipelineBase::APIGetBindGroupLayout(uint32_t groupIndexIn) {
        if (GetDevice()->ConsumedError(ValidateGetBindGroupLayout(groupIndexIn))) {
            return BindGroupLayoutBase::MakeError(GetDevice());
        }

        BindGroupIndex groupIndex(groupIndexIn);

        BindGroupLayoutBase* bgl = nullptr;
        if (!mLayout->GetBindGroupLayoutsMask()[groupIndex]) {
            bgl = GetDevice()->GetEmptyBindGroupLayout();
        } else {
            bgl = mLayout->GetBindGroupLayout(groupIndex);
        }
        bgl->Reference();
        return bgl;
    }

    size_t PipelineBase::ComputeContentHash() {
        ObjectContentHasher recorder;
        recorder.Record(mLayout->GetContentHash());

        recorder.Record(mStageMask);
        for (SingleShaderStage stage : IterateStages(mStageMask)) {
            recorder.Record(mStages[stage].module->GetContentHash());
            recorder.Record(mStages[stage].entryPoint);
        }

        return recorder.GetContentHash();
    }

    // static
    bool PipelineBase::EqualForCache(const PipelineBase* a, const PipelineBase* b) {
        // The layout is deduplicated so it can be compared by pointer.
        if (a->mLayout.Get() != b->mLayout.Get() || a->mStageMask != b->mStageMask) {
            return false;
        }

        for (SingleShaderStage stage : IterateStages(a->mStageMask)) {
            // The module is deduplicated so it can be compared by pointer.
            if (a->mStages[stage].module.Get() != b->mStages[stage].module.Get() ||
                a->mStages[stage].entryPoint != b->mStages[stage].entryPoint) {
                return false;
            }
        }

        return true;
    }

}  // namespace dawn_native

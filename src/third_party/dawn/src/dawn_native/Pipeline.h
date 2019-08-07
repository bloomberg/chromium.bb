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

#ifndef DAWNNATIVE_PIPELINE_H_
#define DAWNNATIVE_PIPELINE_H_

#include "dawn_native/Forward.h"
#include "dawn_native/ObjectBase.h"
#include "dawn_native/PerStage.h"
#include "dawn_native/PipelineLayout.h"
#include "dawn_native/ShaderModule.h"

#include "dawn_native/dawn_platform.h"

#include <array>
#include <bitset>

namespace dawn_native {

    enum PushConstantType : uint8_t {
        Int,
        UInt,
        Float,
    };

    MaybeError ValidatePipelineStageDescriptor(DeviceBase* device,
                                               const PipelineStageDescriptor* descriptor,
                                               const PipelineLayoutBase* layout,
                                               dawn::ShaderStage stage);

    class PipelineBase : public ObjectBase {
      public:
        struct PushConstantInfo {
            std::bitset<kMaxPushConstants> mask;
            std::array<PushConstantType, kMaxPushConstants> types;
        };
        const PushConstantInfo& GetPushConstants(dawn::ShaderStage stage) const;
        dawn::ShaderStageBit GetStageMask() const;
        PipelineLayoutBase* GetLayout();

      protected:
        PipelineBase(DeviceBase* device, PipelineLayoutBase* layout, dawn::ShaderStageBit stages);
        PipelineBase(DeviceBase* device, ObjectBase::ErrorTag tag);

        void ExtractModuleData(dawn::ShaderStage stage, ShaderModuleBase* module);

      private:
        dawn::ShaderStageBit mStageMask;
        Ref<PipelineLayoutBase> mLayout;
        PerStage<PushConstantInfo> mPushConstants;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_PIPELINE_H_

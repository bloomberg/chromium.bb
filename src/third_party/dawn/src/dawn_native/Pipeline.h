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

    MaybeError ValidatePipelineStageDescriptor(const DeviceBase* device,
                                               const PipelineStageDescriptor* descriptor,
                                               const PipelineLayoutBase* layout,
                                               SingleShaderStage stage);

    class PipelineBase : public ObjectBase {
      public:
        dawn::ShaderStage GetStageMask() const;
        PipelineLayoutBase* GetLayout();
        const PipelineLayoutBase* GetLayout() const;

      protected:
        PipelineBase(DeviceBase* device, PipelineLayoutBase* layout, dawn::ShaderStage stages);
        PipelineBase(DeviceBase* device, ObjectBase::ErrorTag tag);

      private:
        dawn::ShaderStage mStageMask;
        Ref<PipelineLayoutBase> mLayout;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_PIPELINE_H_

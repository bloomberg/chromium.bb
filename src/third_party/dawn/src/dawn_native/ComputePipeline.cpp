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

#include "dawn_native/ComputePipeline.h"

#include "dawn_native/Device.h"
#include "dawn_native/ObjectContentHasher.h"

namespace dawn_native {

    MaybeError ValidateComputePipelineDescriptor(DeviceBase* device,
                                                 const ComputePipelineDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        if (descriptor->layout != nullptr) {
            DAWN_TRY(device->ValidateObject(descriptor->layout));
        }

        if (descriptor->compute.module != nullptr) {
            DAWN_TRY(ValidateProgrammableStage(device, descriptor->compute.module,
                                               descriptor->compute.entryPoint, descriptor->layout,
                                               SingleShaderStage::Compute));
        } else {
            // TODO(dawn:800): Remove after deprecation period.
            device->EmitDeprecationWarning(
                "computeStage has been deprecated. Please begin using compute instead.");
            DAWN_TRY(ValidateProgrammableStage(device, descriptor->computeStage.module,
                                               descriptor->computeStage.entryPoint,
                                               descriptor->layout, SingleShaderStage::Compute));
        }

        return {};
    }

    // ComputePipelineBase

    ComputePipelineBase::ComputePipelineBase(DeviceBase* device,
                                             const ComputePipelineDescriptor* descriptor)
        : PipelineBase(device,
                       descriptor->layout,
                       {{SingleShaderStage::Compute, descriptor->compute.module,
                         descriptor->compute.entryPoint}}) {
    }

    ComputePipelineBase::ComputePipelineBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : PipelineBase(device, tag) {
    }

    ComputePipelineBase::~ComputePipelineBase() {
        // Do not uncache the actual cached object if we are a blueprint
        if (IsCachedReference()) {
            GetDevice()->UncacheComputePipeline(this);
        }
    }

    MaybeError ComputePipelineBase::Initialize(const ComputePipelineDescriptor* descriptor) {
        return {};
    }

    // static
    ComputePipelineBase* ComputePipelineBase::MakeError(DeviceBase* device) {
        return new ComputePipelineBase(device, ObjectBase::kError);
    }

    bool ComputePipelineBase::EqualityFunc::operator()(const ComputePipelineBase* a,
                                                       const ComputePipelineBase* b) const {
        return PipelineBase::EqualForCache(a, b);
    }

}  // namespace dawn_native

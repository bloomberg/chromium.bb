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

#include "dawn_native/ShaderModule.h"

#include "common/HashUtils.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/Device.h"
#include "dawn_native/Pipeline.h"
#include "dawn_native/PipelineLayout.h"

#include <spirv-tools/libspirv.hpp>
#include <spirv_cross.hpp>

#include <sstream>

namespace dawn_native {

    MaybeError ValidateShaderModuleDescriptor(DeviceBase*,
                                              const ShaderModuleDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_1);

        std::ostringstream errorStream;
        errorStream << "SPIRV Validation failure:" << std::endl;

        spirvTools.SetMessageConsumer([&errorStream](spv_message_level_t level, const char*,
                                                     const spv_position_t& position,
                                                     const char* message) {
            switch (level) {
                case SPV_MSG_FATAL:
                case SPV_MSG_INTERNAL_ERROR:
                case SPV_MSG_ERROR:
                    errorStream << "error: line " << position.index << ": " << message << std::endl;
                    break;
                case SPV_MSG_WARNING:
                    errorStream << "warning: line " << position.index << ": " << message
                                << std::endl;
                    break;
                case SPV_MSG_INFO:
                    errorStream << "info: line " << position.index << ": " << message << std::endl;
                    break;
                default:
                    break;
            }
        });

        if (!spirvTools.Validate(descriptor->code, descriptor->codeSize)) {
            return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
        }

        return {};
    }

    // ShaderModuleBase

    ShaderModuleBase::ShaderModuleBase(DeviceBase* device,
                                       const ShaderModuleDescriptor* descriptor,
                                       bool blueprint)
        : ObjectBase(device),
          mCode(descriptor->code, descriptor->code + descriptor->codeSize),
          mIsBlueprint(blueprint) {
    }

    ShaderModuleBase::ShaderModuleBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ObjectBase(device, tag) {
    }

    ShaderModuleBase::~ShaderModuleBase() {
        // Do not uncache the actual cached object if we are a blueprint
        if (!mIsBlueprint && !IsError()) {
            GetDevice()->UncacheShaderModule(this);
        }
    }

    // static
    ShaderModuleBase* ShaderModuleBase::MakeError(DeviceBase* device) {
        return new ShaderModuleBase(device, ObjectBase::kError);
    }

    void ShaderModuleBase::ExtractSpirvInfo(const spirv_cross::Compiler& compiler) {
        ASSERT(!IsError());

        DeviceBase* device = GetDevice();
        // TODO(cwallez@chromium.org): make errors here creation errors
        // currently errors here do not prevent the shadermodule from being used
        const auto& resources = compiler.get_shader_resources();

        switch (compiler.get_execution_model()) {
            case spv::ExecutionModelVertex:
                mExecutionModel = SingleShaderStage::Vertex;
                break;
            case spv::ExecutionModelFragment:
                mExecutionModel = SingleShaderStage::Fragment;
                break;
            case spv::ExecutionModelGLCompute:
                mExecutionModel = SingleShaderStage::Compute;
                break;
            default:
                UNREACHABLE();
        }

        if (resources.push_constant_buffers.size() > 0) {
            GetDevice()->HandleError(dawn::ErrorType::Validation,
                                     "Push constants aren't supported.");
        }

        // Fill in bindingInfo with the SPIRV bindings
        auto ExtractResourcesBinding = [this](const spirv_cross::SmallVector<spirv_cross::Resource>&
                                                  resources,
                                              const spirv_cross::Compiler& compiler,
                                              dawn::BindingType bindingType) {
            for (const auto& resource : resources) {
                ASSERT(compiler.get_decoration_bitset(resource.id).get(spv::DecorationBinding));
                ASSERT(
                    compiler.get_decoration_bitset(resource.id).get(spv::DecorationDescriptorSet));

                uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
                uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

                if (binding >= kMaxBindingsPerGroup || set >= kMaxBindGroups) {
                    GetDevice()->HandleError(dawn::ErrorType::Validation,
                                             "Binding over limits in the SPIRV");
                    continue;
                }

                auto& info = mBindingInfo[set][binding];
                info.used = true;
                info.id = resource.id;
                info.base_type_id = resource.base_type_id;
                info.type = bindingType;
            }
        };

        ExtractResourcesBinding(resources.uniform_buffers, compiler,
                                dawn::BindingType::UniformBuffer);
        ExtractResourcesBinding(resources.separate_images, compiler,
                                dawn::BindingType::SampledTexture);
        ExtractResourcesBinding(resources.separate_samplers, compiler, dawn::BindingType::Sampler);
        ExtractResourcesBinding(resources.storage_buffers, compiler,
                                dawn::BindingType::StorageBuffer);

        // Extract the vertex attributes
        if (mExecutionModel == SingleShaderStage::Vertex) {
            for (const auto& attrib : resources.stage_inputs) {
                ASSERT(compiler.get_decoration_bitset(attrib.id).get(spv::DecorationLocation));
                uint32_t location = compiler.get_decoration(attrib.id, spv::DecorationLocation);

                if (location >= kMaxVertexAttributes) {
                    device->HandleError(dawn::ErrorType::Validation,
                                        "Attribute location over limits in the SPIRV");
                    return;
                }

                mUsedVertexAttributes.set(location);
            }

            // Without a location qualifier on vertex outputs, spirv_cross::CompilerMSL gives them
            // all the location 0, causing a compile error.
            for (const auto& attrib : resources.stage_outputs) {
                if (!compiler.get_decoration_bitset(attrib.id).get(spv::DecorationLocation)) {
                    device->HandleError(dawn::ErrorType::Validation,
                                        "Need location qualifier on vertex output");
                    return;
                }
            }
        }

        if (mExecutionModel == SingleShaderStage::Fragment) {
            // Without a location qualifier on vertex inputs, spirv_cross::CompilerMSL gives them
            // all the location 0, causing a compile error.
            for (const auto& attrib : resources.stage_inputs) {
                if (!compiler.get_decoration_bitset(attrib.id).get(spv::DecorationLocation)) {
                    device->HandleError(dawn::ErrorType::Validation,
                                        "Need location qualifier on fragment input");
                    return;
                }
            }
        }
    }

    const ShaderModuleBase::ModuleBindingInfo& ShaderModuleBase::GetBindingInfo() const {
        ASSERT(!IsError());
        return mBindingInfo;
    }

    const std::bitset<kMaxVertexAttributes>& ShaderModuleBase::GetUsedVertexAttributes() const {
        ASSERT(!IsError());
        return mUsedVertexAttributes;
    }

    SingleShaderStage ShaderModuleBase::GetExecutionModel() const {
        ASSERT(!IsError());
        return mExecutionModel;
    }

    bool ShaderModuleBase::IsCompatibleWithPipelineLayout(const PipelineLayoutBase* layout) {
        ASSERT(!IsError());

        for (uint32_t group : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
            if (!IsCompatibleWithBindGroupLayout(group, layout->GetBindGroupLayout(group))) {
                return false;
            }
        }

        for (uint32_t group : IterateBitSet(~layout->GetBindGroupLayoutsMask())) {
            for (size_t i = 0; i < kMaxBindingsPerGroup; ++i) {
                if (mBindingInfo[group][i].used) {
                    return false;
                }
            }
        }

        return true;
    }

    bool ShaderModuleBase::IsCompatibleWithBindGroupLayout(size_t group,
                                                           const BindGroupLayoutBase* layout) {
        ASSERT(!IsError());

        const auto& layoutInfo = layout->GetBindingInfo();
        for (size_t i = 0; i < kMaxBindingsPerGroup; ++i) {
            const auto& moduleInfo = mBindingInfo[group][i];
            const auto& layoutBindingType = layoutInfo.types[i];

            if (!moduleInfo.used) {
                continue;
            }

            if (layoutBindingType != moduleInfo.type) {
                return false;
            }

            if ((layoutInfo.visibilities[i] & StageBit(mExecutionModel)) == 0) {
                return false;
            }
        }

        return true;
    }

    size_t ShaderModuleBase::HashFunc::operator()(const ShaderModuleBase* module) const {
        size_t hash = 0;

        for (uint32_t word : module->mCode) {
            HashCombine(&hash, word);
        }

        return hash;
    }

    bool ShaderModuleBase::EqualityFunc::operator()(const ShaderModuleBase* a,
                                                    const ShaderModuleBase* b) const {
        return a->mCode == b->mCode;
    }

}  // namespace dawn_native

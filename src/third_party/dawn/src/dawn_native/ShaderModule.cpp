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
#include "common/VertexFormatUtils.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/ChainUtils_autogen.h"
#include "dawn_native/CompilationMessages.h"
#include "dawn_native/Device.h"
#include "dawn_native/ObjectContentHasher.h"
#include "dawn_native/Pipeline.h"
#include "dawn_native/PipelineLayout.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/SpirvUtils.h"
#include "dawn_native/TintUtils.h"

#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/optimizer.hpp>
#include <spirv_cross.hpp>

// Tint include must be after spirv_cross.hpp, because spirv-cross has its own
// version of spirv_headers. We also need to undef SPV_REVISION because SPIRV-Cross
// is at 3 while spirv-headers is at 4.
#undef SPV_REVISION
#include <tint/tint.h>

#include <sstream>

namespace dawn_native {

    namespace {

        std::string GetShaderDeclarationString(BindGroupIndex group, BindingNumber binding) {
            std::ostringstream ostream;
            ostream << "the shader module declaration at set " << static_cast<uint32_t>(group)
                    << " binding " << static_cast<uint32_t>(binding);
            return ostream.str();
        }

        tint::transform::VertexFormat ToTintVertexFormat(wgpu::VertexFormat format) {
            format = dawn::NormalizeVertexFormat(format);
            switch (format) {
                case wgpu::VertexFormat::Uint8x2:
                    return tint::transform::VertexFormat::kVec2U8;
                case wgpu::VertexFormat::Uint8x4:
                    return tint::transform::VertexFormat::kVec4U8;
                case wgpu::VertexFormat::Sint8x2:
                    return tint::transform::VertexFormat::kVec2I8;
                case wgpu::VertexFormat::Sint8x4:
                    return tint::transform::VertexFormat::kVec4I8;
                case wgpu::VertexFormat::Unorm8x2:
                    return tint::transform::VertexFormat::kVec2U8Norm;
                case wgpu::VertexFormat::Unorm8x4:
                    return tint::transform::VertexFormat::kVec4U8Norm;
                case wgpu::VertexFormat::Snorm8x2:
                    return tint::transform::VertexFormat::kVec2I8Norm;
                case wgpu::VertexFormat::Snorm8x4:
                    return tint::transform::VertexFormat::kVec4I8Norm;
                case wgpu::VertexFormat::Uint16x2:
                    return tint::transform::VertexFormat::kVec2U16;
                case wgpu::VertexFormat::Uint16x4:
                    return tint::transform::VertexFormat::kVec4U16;
                case wgpu::VertexFormat::Sint16x2:
                    return tint::transform::VertexFormat::kVec2I16;
                case wgpu::VertexFormat::Sint16x4:
                    return tint::transform::VertexFormat::kVec4I16;
                case wgpu::VertexFormat::Unorm16x2:
                    return tint::transform::VertexFormat::kVec2U16Norm;
                case wgpu::VertexFormat::Unorm16x4:
                    return tint::transform::VertexFormat::kVec4U16Norm;
                case wgpu::VertexFormat::Snorm16x2:
                    return tint::transform::VertexFormat::kVec2I16Norm;
                case wgpu::VertexFormat::Snorm16x4:
                    return tint::transform::VertexFormat::kVec4I16Norm;
                case wgpu::VertexFormat::Float16x2:
                    return tint::transform::VertexFormat::kVec2F16;
                case wgpu::VertexFormat::Float16x4:
                    return tint::transform::VertexFormat::kVec4F16;
                case wgpu::VertexFormat::Float32:
                    return tint::transform::VertexFormat::kF32;
                case wgpu::VertexFormat::Float32x2:
                    return tint::transform::VertexFormat::kVec2F32;
                case wgpu::VertexFormat::Float32x3:
                    return tint::transform::VertexFormat::kVec3F32;
                case wgpu::VertexFormat::Float32x4:
                    return tint::transform::VertexFormat::kVec4F32;
                case wgpu::VertexFormat::Uint32:
                    return tint::transform::VertexFormat::kU32;
                case wgpu::VertexFormat::Uint32x2:
                    return tint::transform::VertexFormat::kVec2U32;
                case wgpu::VertexFormat::Uint32x3:
                    return tint::transform::VertexFormat::kVec3U32;
                case wgpu::VertexFormat::Uint32x4:
                    return tint::transform::VertexFormat::kVec4U32;
                case wgpu::VertexFormat::Sint32:
                    return tint::transform::VertexFormat::kI32;
                case wgpu::VertexFormat::Sint32x2:
                    return tint::transform::VertexFormat::kVec2I32;
                case wgpu::VertexFormat::Sint32x3:
                    return tint::transform::VertexFormat::kVec3I32;
                case wgpu::VertexFormat::Sint32x4:
                    return tint::transform::VertexFormat::kVec4I32;

                case wgpu::VertexFormat::Undefined:
                    break;

                // Deprecated formats (should be unreachable after NormalizeVertexFormat call)
                case wgpu::VertexFormat::UChar2:
                case wgpu::VertexFormat::UChar4:
                case wgpu::VertexFormat::Char2:
                case wgpu::VertexFormat::Char4:
                case wgpu::VertexFormat::UChar2Norm:
                case wgpu::VertexFormat::UChar4Norm:
                case wgpu::VertexFormat::Char2Norm:
                case wgpu::VertexFormat::Char4Norm:
                case wgpu::VertexFormat::UShort2:
                case wgpu::VertexFormat::UShort4:
                case wgpu::VertexFormat::UShort2Norm:
                case wgpu::VertexFormat::UShort4Norm:
                case wgpu::VertexFormat::Short2:
                case wgpu::VertexFormat::Short4:
                case wgpu::VertexFormat::Short2Norm:
                case wgpu::VertexFormat::Short4Norm:
                case wgpu::VertexFormat::Half2:
                case wgpu::VertexFormat::Half4:
                case wgpu::VertexFormat::Float:
                case wgpu::VertexFormat::Float2:
                case wgpu::VertexFormat::Float3:
                case wgpu::VertexFormat::Float4:
                case wgpu::VertexFormat::UInt:
                case wgpu::VertexFormat::UInt2:
                case wgpu::VertexFormat::UInt3:
                case wgpu::VertexFormat::UInt4:
                case wgpu::VertexFormat::Int:
                case wgpu::VertexFormat::Int2:
                case wgpu::VertexFormat::Int3:
                case wgpu::VertexFormat::Int4:
                    break;
            }
            UNREACHABLE();
        }

        tint::transform::InputStepMode ToTintInputStepMode(wgpu::InputStepMode mode) {
            switch (mode) {
                case wgpu::InputStepMode::Vertex:
                    return tint::transform::InputStepMode::kVertex;
                case wgpu::InputStepMode::Instance:
                    return tint::transform::InputStepMode::kInstance;
            }
        }

        ResultOrError<SingleShaderStage> TintPipelineStageToShaderStage(
            tint::ast::PipelineStage stage) {
            switch (stage) {
                case tint::ast::PipelineStage::kVertex:
                    return SingleShaderStage::Vertex;
                case tint::ast::PipelineStage::kFragment:
                    return SingleShaderStage::Fragment;
                case tint::ast::PipelineStage::kCompute:
                    return SingleShaderStage::Compute;
                case tint::ast::PipelineStage::kNone:
                    UNREACHABLE();
            }
        }

        BindingInfoType TintResourceTypeToBindingInfoType(
            tint::inspector::ResourceBinding::ResourceType type) {
            switch (type) {
                case tint::inspector::ResourceBinding::ResourceType::kUniformBuffer:
                case tint::inspector::ResourceBinding::ResourceType::kStorageBuffer:
                case tint::inspector::ResourceBinding::ResourceType::kReadOnlyStorageBuffer:
                    return BindingInfoType::Buffer;
                case tint::inspector::ResourceBinding::ResourceType::kSampler:
                case tint::inspector::ResourceBinding::ResourceType::kComparisonSampler:
                    return BindingInfoType::Sampler;
                case tint::inspector::ResourceBinding::ResourceType::kSampledTexture:
                case tint::inspector::ResourceBinding::ResourceType::kMultisampledTexture:
                case tint::inspector::ResourceBinding::ResourceType::kDepthTexture:
                    return BindingInfoType::Texture;
                case tint::inspector::ResourceBinding::ResourceType::kReadOnlyStorageTexture:
                case tint::inspector::ResourceBinding::ResourceType::kWriteOnlyStorageTexture:
                    return BindingInfoType::StorageTexture;

                default:
                    UNREACHABLE();
                    return BindingInfoType::Buffer;
            }
        }

        wgpu::TextureFormat TintImageFormatToTextureFormat(
            tint::inspector::ResourceBinding::ImageFormat format) {
            switch (format) {
                case tint::inspector::ResourceBinding::ImageFormat::kR8Unorm:
                    return wgpu::TextureFormat::R8Unorm;
                case tint::inspector::ResourceBinding::ImageFormat::kR8Snorm:
                    return wgpu::TextureFormat::R8Snorm;
                case tint::inspector::ResourceBinding::ImageFormat::kR8Uint:
                    return wgpu::TextureFormat::R8Uint;
                case tint::inspector::ResourceBinding::ImageFormat::kR8Sint:
                    return wgpu::TextureFormat::R8Sint;
                case tint::inspector::ResourceBinding::ImageFormat::kR16Uint:
                    return wgpu::TextureFormat::R16Uint;
                case tint::inspector::ResourceBinding::ImageFormat::kR16Sint:
                    return wgpu::TextureFormat::R16Sint;
                case tint::inspector::ResourceBinding::ImageFormat::kR16Float:
                    return wgpu::TextureFormat::R16Float;
                case tint::inspector::ResourceBinding::ImageFormat::kRg8Unorm:
                    return wgpu::TextureFormat::RG8Unorm;
                case tint::inspector::ResourceBinding::ImageFormat::kRg8Snorm:
                    return wgpu::TextureFormat::RG8Snorm;
                case tint::inspector::ResourceBinding::ImageFormat::kRg8Uint:
                    return wgpu::TextureFormat::RG8Uint;
                case tint::inspector::ResourceBinding::ImageFormat::kRg8Sint:
                    return wgpu::TextureFormat::RG8Sint;
                case tint::inspector::ResourceBinding::ImageFormat::kR32Uint:
                    return wgpu::TextureFormat::R32Uint;
                case tint::inspector::ResourceBinding::ImageFormat::kR32Sint:
                    return wgpu::TextureFormat::R32Sint;
                case tint::inspector::ResourceBinding::ImageFormat::kR32Float:
                    return wgpu::TextureFormat::R32Float;
                case tint::inspector::ResourceBinding::ImageFormat::kRg16Uint:
                    return wgpu::TextureFormat::RG16Uint;
                case tint::inspector::ResourceBinding::ImageFormat::kRg16Sint:
                    return wgpu::TextureFormat::RG16Sint;
                case tint::inspector::ResourceBinding::ImageFormat::kRg16Float:
                    return wgpu::TextureFormat::RG16Float;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba8Unorm:
                    return wgpu::TextureFormat::RGBA8Unorm;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba8UnormSrgb:
                    return wgpu::TextureFormat::RGBA8UnormSrgb;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba8Snorm:
                    return wgpu::TextureFormat::RGBA8Snorm;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba8Uint:
                    return wgpu::TextureFormat::RGBA8Uint;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba8Sint:
                    return wgpu::TextureFormat::RGBA8Sint;
                case tint::inspector::ResourceBinding::ImageFormat::kBgra8Unorm:
                    return wgpu::TextureFormat::BGRA8Unorm;
                case tint::inspector::ResourceBinding::ImageFormat::kBgra8UnormSrgb:
                    return wgpu::TextureFormat::BGRA8UnormSrgb;
                case tint::inspector::ResourceBinding::ImageFormat::kRgb10A2Unorm:
                    return wgpu::TextureFormat::RGB10A2Unorm;
                case tint::inspector::ResourceBinding::ImageFormat::kRg11B10Float:
                    return wgpu::TextureFormat::RG11B10Ufloat;
                case tint::inspector::ResourceBinding::ImageFormat::kRg32Uint:
                    return wgpu::TextureFormat::RG32Uint;
                case tint::inspector::ResourceBinding::ImageFormat::kRg32Sint:
                    return wgpu::TextureFormat::RG32Sint;
                case tint::inspector::ResourceBinding::ImageFormat::kRg32Float:
                    return wgpu::TextureFormat::RG32Float;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba16Uint:
                    return wgpu::TextureFormat::RGBA16Uint;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba16Sint:
                    return wgpu::TextureFormat::RGBA16Sint;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba16Float:
                    return wgpu::TextureFormat::RGBA16Float;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba32Uint:
                    return wgpu::TextureFormat::RGBA32Uint;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba32Sint:
                    return wgpu::TextureFormat::RGBA32Sint;
                case tint::inspector::ResourceBinding::ImageFormat::kRgba32Float:
                    return wgpu::TextureFormat::RGBA32Float;
                case tint::inspector::ResourceBinding::ImageFormat::kNone:
                    return wgpu::TextureFormat::Undefined;
            }
        }

        wgpu::TextureViewDimension TintTextureDimensionToTextureViewDimension(
            tint::inspector::ResourceBinding::TextureDimension dim) {
            switch (dim) {
                case tint::inspector::ResourceBinding::TextureDimension::k1d:
                    return wgpu::TextureViewDimension::e1D;
                case tint::inspector::ResourceBinding::TextureDimension::k2d:
                    return wgpu::TextureViewDimension::e2D;
                case tint::inspector::ResourceBinding::TextureDimension::k2dArray:
                    return wgpu::TextureViewDimension::e2DArray;
                case tint::inspector::ResourceBinding::TextureDimension::k3d:
                    return wgpu::TextureViewDimension::e3D;
                case tint::inspector::ResourceBinding::TextureDimension::kCube:
                    return wgpu::TextureViewDimension::Cube;
                case tint::inspector::ResourceBinding::TextureDimension::kCubeArray:
                    return wgpu::TextureViewDimension::CubeArray;
                case tint::inspector::ResourceBinding::TextureDimension::kNone:
                    return wgpu::TextureViewDimension::Undefined;
            }
        }

        wgpu::TextureSampleType TintSampledKindToTextureSampleType(
            tint::inspector::ResourceBinding::SampledKind s) {
            switch (s) {
                case tint::inspector::ResourceBinding::SampledKind::kSInt:
                    return wgpu::TextureSampleType::Sint;
                case tint::inspector::ResourceBinding::SampledKind::kUInt:
                    return wgpu::TextureSampleType::Uint;
                case tint::inspector::ResourceBinding::SampledKind::kFloat:
                    return wgpu::TextureSampleType::Float;
                case tint::inspector::ResourceBinding::SampledKind::kUnknown:
                    return wgpu::TextureSampleType::Undefined;
            }
        }

        ResultOrError<wgpu::TextureComponentType> TintComponentTypeToTextureComponentType(
            tint::inspector::ComponentType type) {
            switch (type) {
                case tint::inspector::ComponentType::kFloat:
                    return wgpu::TextureComponentType::Float;
                case tint::inspector::ComponentType::kSInt:
                    return wgpu::TextureComponentType::Sint;
                case tint::inspector::ComponentType::kUInt:
                    return wgpu::TextureComponentType::Uint;
                case tint::inspector::ComponentType::kUnknown:
                    return DAWN_VALIDATION_ERROR(
                        "Attempted to convert 'Unknown' component type from Tint");
            }
        }

        ResultOrError<wgpu::BufferBindingType> TintResourceTypeToBufferBindingType(
            tint::inspector::ResourceBinding::ResourceType resource_type) {
            switch (resource_type) {
                case tint::inspector::ResourceBinding::ResourceType::kUniformBuffer:
                    return wgpu::BufferBindingType::Uniform;
                case tint::inspector::ResourceBinding::ResourceType::kStorageBuffer:
                    return wgpu::BufferBindingType::Storage;
                case tint::inspector::ResourceBinding::ResourceType::kReadOnlyStorageBuffer:
                    return wgpu::BufferBindingType::ReadOnlyStorage;
                default:
                    return DAWN_VALIDATION_ERROR("Attempted to convert non-buffer resource type");
            }
        }

        ResultOrError<wgpu::StorageTextureAccess> TintResourceTypeToStorageTextureAccess(
            tint::inspector::ResourceBinding::ResourceType resource_type) {
            switch (resource_type) {
                case tint::inspector::ResourceBinding::ResourceType::kReadOnlyStorageTexture:
                    return wgpu::StorageTextureAccess::ReadOnly;
                case tint::inspector::ResourceBinding::ResourceType::kWriteOnlyStorageTexture:
                    return wgpu::StorageTextureAccess::WriteOnly;
                default:
                    return DAWN_VALIDATION_ERROR(
                        "Attempted to convert non-storage texture resource type");
            }
        }

        MaybeError ValidateSpirv(const uint32_t* code, uint32_t codeSize) {
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
                        errorStream << "error: line " << position.index << ": " << message
                                    << std::endl;
                        break;
                    case SPV_MSG_WARNING:
                        errorStream << "warning: line " << position.index << ": " << message
                                    << std::endl;
                        break;
                    case SPV_MSG_INFO:
                        errorStream << "info: line " << position.index << ": " << message
                                    << std::endl;
                        break;
                    default:
                        break;
                }
            });

            if (!spirvTools.Validate(code, codeSize)) {
                std::string disassembly;
                if (spirvTools.Disassemble(std::vector<uint32_t>(code, code + codeSize),
                                           &disassembly)) {
                    errorStream << "disassembly:" << std::endl << disassembly;
                }

                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }

            return {};
        }

        ResultOrError<tint::Program> ParseWGSL(const tint::Source::File* file,
                                               OwnedCompilationMessages* outMessages) {
            std::ostringstream errorStream;
            errorStream << "Tint WGSL reader failure:" << std::endl;

            tint::Program program = tint::reader::wgsl::Parse(file);
            if (outMessages != nullptr) {
                outMessages->AddMessages(program.Diagnostics());
            }
            if (!program.IsValid()) {
                auto err = program.Diagnostics().str();
                errorStream << "Parser: " << err << std::endl
                            << "Shader: " << std::endl
                            << file->content << std::endl;
                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }

            return std::move(program);
        }

        ResultOrError<tint::Program> ParseSPIRV(const std::vector<uint32_t>& spirv,
                                                OwnedCompilationMessages* outMessages) {
            std::ostringstream errorStream;
            errorStream << "Tint SPIRV reader failure:" << std::endl;

            tint::Program program = tint::reader::spirv::Parse(spirv);
            if (outMessages != nullptr) {
                outMessages->AddMessages(program.Diagnostics());
            }
            if (!program.IsValid()) {
                auto err = program.Diagnostics().str();
                errorStream << "Parser: " << err << std::endl;
                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }

            return std::move(program);
        }

        ResultOrError<std::vector<uint32_t>> ModuleToSPIRV(const tint::Program* program) {
            std::ostringstream errorStream;
            errorStream << "Tint SPIR-V writer failure:" << std::endl;

            tint::writer::spirv::Generator generator(program);
            if (!generator.Generate()) {
                errorStream << "Generator: " << generator.error() << std::endl;
                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }

            std::vector<uint32_t> spirv = generator.result();
            return std::move(spirv);
        }

        std::vector<uint64_t> GetBindGroupMinBufferSizes(
            const EntryPointMetadata::BindingGroupInfoMap& shaderBindings,
            const BindGroupLayoutBase* layout) {
            std::vector<uint64_t> requiredBufferSizes(layout->GetUnverifiedBufferCount());
            uint32_t packedIdx = 0;

            for (BindingIndex bindingIndex{0}; bindingIndex < layout->GetBufferCount();
                 ++bindingIndex) {
                const BindingInfo& bindingInfo = layout->GetBindingInfo(bindingIndex);
                if (bindingInfo.buffer.minBindingSize != 0) {
                    // Skip bindings that have minimum buffer size set in the layout
                    continue;
                }

                ASSERT(packedIdx < requiredBufferSizes.size());
                const auto& shaderInfo = shaderBindings.find(bindingInfo.binding);
                if (shaderInfo != shaderBindings.end()) {
                    requiredBufferSizes[packedIdx] = shaderInfo->second.buffer.minBindingSize;
                } else {
                    // We have to include buffers if they are included in the bind group's
                    // packed vector. We don't actually need to check these at draw time, so
                    // if this is a problem in the future we can optimize it further.
                    requiredBufferSizes[packedIdx] = 0;
                }
                ++packedIdx;
            }

            return requiredBufferSizes;
        }

        ResultOrError<std::vector<uint32_t>> RunRobustBufferAccessPass(
            const std::vector<uint32_t>& spirv) {
            spvtools::Optimizer opt(SPV_ENV_VULKAN_1_1);

            std::ostringstream errorStream;
            errorStream << "SPIRV Optimizer failure:" << std::endl;
            opt.SetMessageConsumer([&errorStream](spv_message_level_t level, const char*,
                                                  const spv_position_t& position,
                                                  const char* message) {
                switch (level) {
                    case SPV_MSG_FATAL:
                    case SPV_MSG_INTERNAL_ERROR:
                    case SPV_MSG_ERROR:
                        errorStream << "error: line " << position.index << ": " << message
                                    << std::endl;
                        break;
                    case SPV_MSG_WARNING:
                        errorStream << "warning: line " << position.index << ": " << message
                                    << std::endl;
                        break;
                    case SPV_MSG_INFO:
                        errorStream << "info: line " << position.index << ": " << message
                                    << std::endl;
                        break;
                    default:
                        break;
                }
            });
            opt.RegisterPass(spvtools::CreateGraphicsRobustAccessPass());

            std::vector<uint32_t> result;
            if (!opt.Run(spirv.data(), spirv.size(), &result, spvtools::ValidatorOptions(),
                         false)) {
                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }
            return std::move(result);
        }

        MaybeError ValidateCompatibilityWithBindGroupLayout(DeviceBase*,
                                                            BindGroupIndex group,
                                                            const EntryPointMetadata& entryPoint,
                                                            const BindGroupLayoutBase* layout) {
            const BindGroupLayoutBase::BindingMap& layoutBindings = layout->GetBindingMap();

            // Iterate over all bindings used by this group in the shader, and find the
            // corresponding binding in the BindGroupLayout, if it exists.
            for (const auto& it : entryPoint.bindings[group]) {
                BindingNumber bindingNumber = it.first;
                const EntryPointMetadata::ShaderBindingInfo& shaderInfo = it.second;

                const auto& bindingIt = layoutBindings.find(bindingNumber);
                if (bindingIt == layoutBindings.end()) {
                    return DAWN_VALIDATION_ERROR("Missing bind group layout entry for " +
                                                 GetShaderDeclarationString(group, bindingNumber));
                }
                BindingIndex bindingIndex(bindingIt->second);
                const BindingInfo& layoutInfo = layout->GetBindingInfo(bindingIndex);

                if (layoutInfo.bindingType != shaderInfo.bindingType) {
                    return DAWN_VALIDATION_ERROR(
                        "The binding type of the bind group layout entry conflicts " +
                        GetShaderDeclarationString(group, bindingNumber));
                }

                if ((layoutInfo.visibility & StageBit(entryPoint.stage)) == 0) {
                    return DAWN_VALIDATION_ERROR("The bind group layout entry for " +
                                                 GetShaderDeclarationString(group, bindingNumber) +
                                                 " is not visible for the shader stage");
                }

                switch (layoutInfo.bindingType) {
                    case BindingInfoType::Texture: {
                        if (layoutInfo.texture.multisampled != shaderInfo.texture.multisampled) {
                            return DAWN_VALIDATION_ERROR(
                                "The texture multisampled flag of the bind group layout entry is "
                                "different from " +
                                GetShaderDeclarationString(group, bindingNumber));
                        }

                        if (layoutInfo.texture.sampleType != shaderInfo.texture.sampleType) {
                            return DAWN_VALIDATION_ERROR(
                                "The texture sampleType of the bind group layout entry is "
                                "different from " +
                                GetShaderDeclarationString(group, bindingNumber));
                        }

                        if (layoutInfo.texture.viewDimension != shaderInfo.texture.viewDimension) {
                            return DAWN_VALIDATION_ERROR(
                                "The texture viewDimension of the bind group layout entry is "
                                "different "
                                "from " +
                                GetShaderDeclarationString(group, bindingNumber));
                        }
                        break;
                    }

                    case BindingInfoType::StorageTexture: {
                        ASSERT(layoutInfo.storageTexture.format != wgpu::TextureFormat::Undefined);
                        ASSERT(shaderInfo.storageTexture.format != wgpu::TextureFormat::Undefined);

                        if (layoutInfo.storageTexture.access != shaderInfo.storageTexture.access) {
                            return DAWN_VALIDATION_ERROR(
                                "The storageTexture access mode of the bind group layout entry is "
                                "different from " +
                                GetShaderDeclarationString(group, bindingNumber));
                        }

                        if (layoutInfo.storageTexture.format != shaderInfo.storageTexture.format) {
                            return DAWN_VALIDATION_ERROR(
                                "The storageTexture format of the bind group layout entry is "
                                "different from " +
                                GetShaderDeclarationString(group, bindingNumber));
                        }
                        if (layoutInfo.storageTexture.viewDimension !=
                            shaderInfo.storageTexture.viewDimension) {
                            return DAWN_VALIDATION_ERROR(
                                "The storageTexture viewDimension of the bind group layout entry "
                                "is different from " +
                                GetShaderDeclarationString(group, bindingNumber));
                        }
                        break;
                    }

                    case BindingInfoType::Buffer: {
                        // Binding mismatch between shader and bind group is invalid. For example, a
                        // writable binding in the shader with a readonly storage buffer in the bind
                        // group layout is invalid. However, a readonly binding in the shader with a
                        // writable storage buffer in the bind group layout is valid.
                        bool validBindingConversion =
                            layoutInfo.buffer.type == wgpu::BufferBindingType::Storage &&
                            shaderInfo.buffer.type == wgpu::BufferBindingType::ReadOnlyStorage;

                        if (layoutInfo.buffer.type != shaderInfo.buffer.type &&
                            !validBindingConversion) {
                            return DAWN_VALIDATION_ERROR(
                                "The buffer type of the bind group layout entry conflicts " +
                                GetShaderDeclarationString(group, bindingNumber));
                        }

                        if (layoutInfo.buffer.minBindingSize != 0 &&
                            shaderInfo.buffer.minBindingSize > layoutInfo.buffer.minBindingSize) {
                            return DAWN_VALIDATION_ERROR(
                                "The minimum buffer size of the bind group layout entry is smaller "
                                "than " +
                                GetShaderDeclarationString(group, bindingNumber));
                        }
                        break;
                    }

                    case BindingInfoType::Sampler:
                        // TODO(crbug.com/dawn/367): Temporarily allow using either a sampler or a
                        // comparison sampler until we can perform the proper shader analysis of
                        // what type is used in the shader module.
                        break;
                }
            }

            return {};
        }

        ResultOrError<std::unique_ptr<EntryPointMetadata>> ExtractSpirvInfo(
            const DeviceBase* device,
            const spirv_cross::Compiler& compiler,
            const std::string& entryPointName,
            SingleShaderStage stage) {
            std::unique_ptr<EntryPointMetadata> metadata = std::make_unique<EntryPointMetadata>();
            metadata->stage = stage;

            // TODO(cwallez@chromium.org): make errors here creation errors
            // currently errors here do not prevent the shadermodule from being used
            const auto& resources = compiler.get_shader_resources();

            if (resources.push_constant_buffers.size() > 0) {
                return DAWN_VALIDATION_ERROR("Push constants aren't supported.");
            }

            if (resources.sampled_images.size() > 0) {
                return DAWN_VALIDATION_ERROR("Combined images and samplers aren't supported.");
            }

            // Fill in bindingInfo with the SPIRV bindings
            auto ExtractResourcesBinding =
                [](const DeviceBase* device,
                   const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                   const spirv_cross::Compiler& compiler, BindingInfoType bindingType,
                   EntryPointMetadata::BindingInfoArray* metadataBindings,
                   bool isStorageBuffer = false) -> MaybeError {
                for (const auto& resource : resources) {
                    if (!compiler.get_decoration_bitset(resource.id).get(spv::DecorationBinding)) {
                        return DAWN_VALIDATION_ERROR("No Binding decoration set for resource");
                    }

                    if (!compiler.get_decoration_bitset(resource.id)
                             .get(spv::DecorationDescriptorSet)) {
                        return DAWN_VALIDATION_ERROR("No Descriptor Decoration set for resource");
                    }

                    BindingNumber bindingNumber(
                        compiler.get_decoration(resource.id, spv::DecorationBinding));
                    BindGroupIndex bindGroupIndex(
                        compiler.get_decoration(resource.id, spv::DecorationDescriptorSet));

                    if (bindGroupIndex >= kMaxBindGroupsTyped) {
                        return DAWN_VALIDATION_ERROR("Bind group index over limits in the SPIRV");
                    }

                    const auto& it = (*metadataBindings)[bindGroupIndex].emplace(
                        bindingNumber, EntryPointMetadata::ShaderBindingInfo{});
                    if (!it.second) {
                        return DAWN_VALIDATION_ERROR("Shader has duplicate bindings");
                    }

                    EntryPointMetadata::ShaderBindingInfo* info = &it.first->second;
                    info->id = resource.id;
                    info->base_type_id = resource.base_type_id;
                    info->bindingType = bindingType;

                    switch (bindingType) {
                        case BindingInfoType::Texture: {
                            spirv_cross::SPIRType::ImageType imageType =
                                compiler.get_type(info->base_type_id).image;
                            spirv_cross::SPIRType::BaseType textureComponentType =
                                compiler.get_type(imageType.type).basetype;

                            info->texture.viewDimension =
                                SpirvDimToTextureViewDimension(imageType.dim, imageType.arrayed);
                            info->texture.sampleType =
                                SpirvBaseTypeToTextureSampleType(textureComponentType);
                            info->texture.multisampled = imageType.ms;

                            if (imageType.depth) {
                                if (imageType.ms) {
                                    return DAWN_VALIDATION_ERROR(
                                        "Multisampled depth textures aren't supported");
                                }
                                if (info->texture.sampleType != wgpu::TextureSampleType::Float) {
                                    return DAWN_VALIDATION_ERROR(
                                        "Depth textures must have a float type");
                                }
                                info->texture.sampleType = wgpu::TextureSampleType::Depth;
                            }
                            if (imageType.ms && imageType.arrayed) {
                                return DAWN_VALIDATION_ERROR(
                                    "Multisampled array textures aren't supported");
                            }
                            break;
                        }
                        case BindingInfoType::Buffer: {
                            // Determine buffer size, with a minimum of 1 element in the runtime
                            // array
                            spirv_cross::SPIRType type = compiler.get_type(info->base_type_id);
                            info->buffer.minBindingSize =
                                compiler.get_declared_struct_size_runtime_array(type, 1);

                            // Differentiate between readonly storage bindings and writable ones
                            // based on the NonWritable decoration.
                            // TODO(dawn:527): Could isStorageBuffer be determined by calling
                            // compiler.get_storage_class(resource.id)?
                            if (isStorageBuffer) {
                                spirv_cross::Bitset flags =
                                    compiler.get_buffer_block_flags(resource.id);
                                if (flags.get(spv::DecorationNonWritable)) {
                                    info->buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
                                } else {
                                    info->buffer.type = wgpu::BufferBindingType::Storage;
                                }
                            } else {
                                info->buffer.type = wgpu::BufferBindingType::Uniform;
                            }
                            break;
                        }
                        case BindingInfoType::StorageTexture: {
                            spirv_cross::Bitset flags = compiler.get_decoration_bitset(resource.id);
                            if (flags.get(spv::DecorationNonReadable)) {
                                info->storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
                            } else if (flags.get(spv::DecorationNonWritable)) {
                                info->storageTexture.access = wgpu::StorageTextureAccess::ReadOnly;
                            } else {
                                return DAWN_VALIDATION_ERROR(
                                    "Read-write storage textures are not supported");
                            }

                            spirv_cross::SPIRType::ImageType imageType =
                                compiler.get_type(info->base_type_id).image;
                            wgpu::TextureFormat storageTextureFormat =
                                SpirvImageFormatToTextureFormat(imageType.format);
                            if (storageTextureFormat == wgpu::TextureFormat::Undefined) {
                                return DAWN_VALIDATION_ERROR(
                                    "Invalid image format declaration on storage image");
                            }
                            const Format& format =
                                device->GetValidInternalFormat(storageTextureFormat);
                            if (!format.supportsStorageUsage) {
                                return DAWN_VALIDATION_ERROR(
                                    "The storage texture format is not supported");
                            }
                            if (imageType.ms) {
                                return DAWN_VALIDATION_ERROR(
                                    "Multisampled storage textures aren't supported");
                            }
                            if (imageType.depth) {
                                return DAWN_VALIDATION_ERROR(
                                    "Depth storage textures aren't supported");
                            }
                            info->storageTexture.format = storageTextureFormat;
                            info->storageTexture.viewDimension =
                                SpirvDimToTextureViewDimension(imageType.dim, imageType.arrayed);
                            break;
                        }
                        case BindingInfoType::Sampler: {
                            info->sampler.type = wgpu::SamplerBindingType::Filtering;
                        }
                    }
                }
                return {};
            };

            DAWN_TRY(ExtractResourcesBinding(device, resources.uniform_buffers, compiler,
                                             BindingInfoType::Buffer, &metadata->bindings));
            DAWN_TRY(ExtractResourcesBinding(device, resources.separate_images, compiler,
                                             BindingInfoType::Texture, &metadata->bindings));
            DAWN_TRY(ExtractResourcesBinding(device, resources.separate_samplers, compiler,
                                             BindingInfoType::Sampler, &metadata->bindings));
            DAWN_TRY(ExtractResourcesBinding(device, resources.storage_buffers, compiler,
                                             BindingInfoType::Buffer, &metadata->bindings, true));
            // ReadonlyStorageTexture is used as a tag to do general storage texture handling.
            DAWN_TRY(ExtractResourcesBinding(device, resources.storage_images, compiler,
                                             BindingInfoType::StorageTexture, &metadata->bindings));

            // Extract the vertex attributes
            if (stage == SingleShaderStage::Vertex) {
                for (const auto& attrib : resources.stage_inputs) {
                    if (!(compiler.get_decoration_bitset(attrib.id).get(spv::DecorationLocation))) {
                        return DAWN_VALIDATION_ERROR(
                            "Unable to find Location decoration for Vertex input");
                    }
                    uint32_t location = compiler.get_decoration(attrib.id, spv::DecorationLocation);

                    if (location >= kMaxVertexAttributes) {
                        return DAWN_VALIDATION_ERROR("Attribute location over limits in the SPIRV");
                    }

                    metadata->usedVertexAttributes.set(location);
                }

                // Without a location qualifier on vertex outputs, spirv_cross::CompilerMSL gives
                // them all the location 0, causing a compile error.
                for (const auto& attrib : resources.stage_outputs) {
                    if (!compiler.get_decoration_bitset(attrib.id).get(spv::DecorationLocation)) {
                        return DAWN_VALIDATION_ERROR("Need location qualifier on vertex output");
                    }
                }
            }

            if (stage == SingleShaderStage::Fragment) {
                // Without a location qualifier on vertex inputs, spirv_cross::CompilerMSL gives
                // them all the location 0, causing a compile error.
                for (const auto& attrib : resources.stage_inputs) {
                    if (!compiler.get_decoration_bitset(attrib.id).get(spv::DecorationLocation)) {
                        return DAWN_VALIDATION_ERROR("Need location qualifier on fragment input");
                    }
                }

                for (const auto& fragmentOutput : resources.stage_outputs) {
                    if (!compiler.get_decoration_bitset(fragmentOutput.id)
                             .get(spv::DecorationLocation)) {
                        return DAWN_VALIDATION_ERROR(
                            "Unable to find Location decoration for Fragment output");
                    }
                    uint32_t unsanitizedAttachment =
                        compiler.get_decoration(fragmentOutput.id, spv::DecorationLocation);
                    if (unsanitizedAttachment >= kMaxColorAttachments) {
                        return DAWN_VALIDATION_ERROR(
                            "Fragment output index must be less than max number of color "
                            "attachments");
                    }
                    ColorAttachmentIndex attachment(static_cast<uint8_t>(unsanitizedAttachment));

                    spirv_cross::SPIRType::BaseType shaderFragmentOutputBaseType =
                        compiler.get_type(fragmentOutput.base_type_id).basetype;
                    metadata->fragmentOutputFormatBaseTypes[attachment] =
                        SpirvBaseTypeToTextureComponentType(shaderFragmentOutputBaseType);
                    metadata->fragmentOutputsWritten.set(attachment);
                }
            }

            if (stage == SingleShaderStage::Compute) {
                const spirv_cross::SPIREntryPoint& spirEntryPoint =
                    compiler.get_entry_point(entryPointName, spv::ExecutionModelGLCompute);
                metadata->localWorkgroupSize.x = spirEntryPoint.workgroup_size.x;
                metadata->localWorkgroupSize.y = spirEntryPoint.workgroup_size.y;
                metadata->localWorkgroupSize.z = spirEntryPoint.workgroup_size.z;
            }

            return {std::move(metadata)};
        }

        ResultOrError<EntryPointMetadataTable> ReflectShaderUsingTint(
            DeviceBase*,
            const tint::Program* program) {
            ASSERT(program->IsValid());

            EntryPointMetadataTable result;
            std::ostringstream errorStream;
            errorStream << "Tint Reflection failure:" << std::endl;

            tint::inspector::Inspector inspector(program);
            auto entryPoints = inspector.GetEntryPoints();
            if (inspector.has_error()) {
                errorStream << "Inspector: " << inspector.error() << std::endl;
                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }

            for (auto& entryPoint : entryPoints) {
                ASSERT(result.count(entryPoint.name) == 0);

                auto metadata = std::make_unique<EntryPointMetadata>();

                DAWN_TRY_ASSIGN(metadata->stage, TintPipelineStageToShaderStage(entryPoint.stage));
                if (metadata->stage == SingleShaderStage::Vertex) {
                    for (auto& stage_input : entryPoint.input_variables) {
                        if (!stage_input.has_location_decoration) {
                            return DAWN_VALIDATION_ERROR(
                                "Need Location decoration on Vertex input");
                        }
                        uint32_t location = stage_input.location_decoration;
                        if (location >= kMaxVertexAttributes) {
                            return DAWN_VALIDATION_ERROR("Attribute location over limits");
                        }
                        metadata->usedVertexAttributes.set(location);
                    }

                    for (auto& stage_output : entryPoint.output_variables) {
                        if (!stage_output.has_location_decoration) {
                            return DAWN_VALIDATION_ERROR(
                                "Need Location decoration on Vertex output");
                        }
                    }
                }

                if (metadata->stage == SingleShaderStage::Compute) {
                    metadata->localWorkgroupSize.x = entryPoint.workgroup_size_x;
                    metadata->localWorkgroupSize.y = entryPoint.workgroup_size_y;
                    metadata->localWorkgroupSize.z = entryPoint.workgroup_size_z;
                }

                if (metadata->stage == SingleShaderStage::Vertex) {
                    for (const auto& input_var : entryPoint.input_variables) {
                        uint32_t location = 0;
                        if (input_var.has_location_decoration) {
                            location = input_var.location_decoration;
                        }

                        if (DAWN_UNLIKELY(location >= kMaxVertexAttributes)) {
                            std::stringstream ss;
                            ss << "Attribute location (" << location << ") over limits";
                            return DAWN_VALIDATION_ERROR(ss.str());
                        }
                        metadata->usedVertexAttributes.set(location);
                    }

                    for (const auto& output_var : entryPoint.output_variables) {
                        if (DAWN_UNLIKELY(!output_var.has_location_decoration)) {
                            std::stringstream ss;
                            ss << "Missing location qualifier on vertex output, "
                               << output_var.name;
                            return DAWN_VALIDATION_ERROR(ss.str());
                        }
                    }
                }

                if (metadata->stage == SingleShaderStage::Fragment) {
                    for (const auto& input_var : entryPoint.input_variables) {
                        if (!input_var.has_location_decoration) {
                            return DAWN_VALIDATION_ERROR(
                                "Need location decoration on fragment input");
                        }
                    }

                    for (const auto& output_var : entryPoint.output_variables) {
                        if (!output_var.has_location_decoration) {
                            return DAWN_VALIDATION_ERROR(
                                "Need location decoration on fragment output");
                        }

                        uint32_t unsanitizedAttachment = output_var.location_decoration;
                        if (unsanitizedAttachment >= kMaxColorAttachments) {
                            return DAWN_VALIDATION_ERROR(
                                "Fragment output index must be less than max number of color "
                                "attachments");
                        }
                        ColorAttachmentIndex attachment(
                            static_cast<uint8_t>(unsanitizedAttachment));
                        DAWN_TRY_ASSIGN(
                            metadata->fragmentOutputFormatBaseTypes[attachment],
                            TintComponentTypeToTextureComponentType(output_var.component_type));
                        metadata->fragmentOutputsWritten.set(attachment);
                    }
                }

                for (auto& resource : inspector.GetResourceBindings(entryPoint.name)) {
                    BindingNumber bindingNumber(resource.binding);
                    BindGroupIndex bindGroupIndex(resource.bind_group);
                    if (bindGroupIndex >= kMaxBindGroupsTyped) {
                        return DAWN_VALIDATION_ERROR("Shader has bind group index over limits");
                    }

                    const auto& it = metadata->bindings[bindGroupIndex].emplace(
                        bindingNumber, EntryPointMetadata::ShaderBindingInfo{});
                    if (!it.second) {
                        return DAWN_VALIDATION_ERROR("Shader has duplicate bindings");
                    }

                    EntryPointMetadata::ShaderBindingInfo* info = &it.first->second;
                    info->bindingType = TintResourceTypeToBindingInfoType(resource.resource_type);

                    switch (info->bindingType) {
                        case BindingInfoType::Buffer:
                            info->buffer.minBindingSize = resource.size_no_padding;
                            DAWN_TRY_ASSIGN(info->buffer.type, TintResourceTypeToBufferBindingType(
                                                                   resource.resource_type));
                            break;
                        case BindingInfoType::Sampler:
                            info->sampler.type = wgpu::SamplerBindingType::Filtering;
                            break;
                        case BindingInfoType::Texture:
                            info->texture.viewDimension =
                                TintTextureDimensionToTextureViewDimension(resource.dim);
                            if (resource.resource_type ==
                                tint::inspector::ResourceBinding::ResourceType::kDepthTexture) {
                                info->texture.sampleType = wgpu::TextureSampleType::Depth;
                            } else {
                                info->texture.sampleType =
                                    TintSampledKindToTextureSampleType(resource.sampled_kind);
                            }
                            info->texture.multisampled = resource.resource_type ==
                                                         tint::inspector::ResourceBinding::
                                                             ResourceType::kMultisampledTexture;

                            break;
                        case BindingInfoType::StorageTexture:
                            DAWN_TRY_ASSIGN(
                                info->storageTexture.access,
                                TintResourceTypeToStorageTextureAccess(resource.resource_type));
                            info->storageTexture.format =
                                TintImageFormatToTextureFormat(resource.image_format);
                            info->storageTexture.viewDimension =
                                TintTextureDimensionToTextureViewDimension(resource.dim);

                            break;
                        default:
                            return DAWN_VALIDATION_ERROR("Unknown binding type in Shader");
                    }
                }

                result[entryPoint.name] = std::move(metadata);
            }
            return std::move(result);
        }
    }  // anonymous namespace

    ShaderModuleParseResult::ShaderModuleParseResult()
        : compilationMessages(new OwnedCompilationMessages()) {
    }
    ShaderModuleParseResult::~ShaderModuleParseResult() = default;

    ShaderModuleParseResult::ShaderModuleParseResult(ShaderModuleParseResult&& rhs) = default;

    ShaderModuleParseResult& ShaderModuleParseResult::operator=(ShaderModuleParseResult&& rhs) =
        default;

    bool ShaderModuleParseResult::HasParsedShader() const {
        return tintProgram != nullptr || spirv.size() > 0;
    }

    // TintSource is a PIMPL container for a tint::Source::File, which needs to be kept alive for as
    // long as tint diagnostics are inspected / printed.
    class TintSource {
      public:
        template <typename... ARGS>
        TintSource(ARGS&&... args) : file(std::forward<ARGS>(args)...) {
        }

        tint::Source::File file;
    };

    MaybeError ValidateShaderModuleDescriptor(DeviceBase* device,
                                              const ShaderModuleDescriptor* descriptor,
                                              ShaderModuleParseResult* parseResult) {
        ASSERT(parseResult != nullptr);

        const ChainedStruct* chainedDescriptor = descriptor->nextInChain;
        if (chainedDescriptor == nullptr) {
            return DAWN_VALIDATION_ERROR("Shader module descriptor missing chained descriptor");
        }
        // For now only a single SPIRV or WGSL subdescriptor is allowed.
        DAWN_TRY(ValidateSingleSType(chainedDescriptor, wgpu::SType::ShaderModuleSPIRVDescriptor,
                                     wgpu::SType::ShaderModuleWGSLDescriptor));

        OwnedCompilationMessages* outMessages = parseResult->compilationMessages.get();

        ScopedTintICEHandler scopedICEHandler(device);

        const ShaderModuleSPIRVDescriptor* spirvDesc = nullptr;
        FindInChain(chainedDescriptor, &spirvDesc);
        const ShaderModuleWGSLDescriptor* wgslDesc = nullptr;
        FindInChain(chainedDescriptor, &wgslDesc);

        if (spirvDesc) {
            std::vector<uint32_t> spirv(spirvDesc->code, spirvDesc->code + spirvDesc->codeSize);
            if (device->IsToggleEnabled(Toggle::UseTintGenerator)) {
                tint::Program program;
                DAWN_TRY_ASSIGN(program, ParseSPIRV(spirv, outMessages));
                parseResult->tintProgram = std::make_unique<tint::Program>(std::move(program));
            } else {
                if (device->IsValidationEnabled()) {
                    DAWN_TRY(ValidateSpirv(spirv.data(), spirv.size()));
                }
                parseResult->spirv = std::move(spirv);
            }
        } else if (wgslDesc) {
            auto tintSource = std::make_unique<TintSource>("", wgslDesc->source);

            tint::Program program;
            DAWN_TRY_ASSIGN(program, ParseWGSL(&tintSource->file, outMessages));

            if (device->IsToggleEnabled(Toggle::UseTintGenerator)) {
                parseResult->tintProgram = std::make_unique<tint::Program>(std::move(program));
                parseResult->tintSource = std::move(tintSource);
            } else {
                tint::transform::Manager transformManager;
                transformManager.Add<tint::transform::Spirv>();

                tint::transform::DataMap transformInputs;

                tint::transform::Spirv::Config spirv_cfg;
                spirv_cfg.emit_vertex_point_size = true;
                transformInputs.Add<tint::transform::Spirv::Config>(spirv_cfg);

                DAWN_TRY_ASSIGN(program, RunTransforms(&transformManager, &program, transformInputs,
                                                       nullptr, outMessages));

                std::vector<uint32_t> spirv;
                DAWN_TRY_ASSIGN(spirv, ModuleToSPIRV(&program));
                DAWN_TRY(ValidateSpirv(spirv.data(), spirv.size()));

                parseResult->spirv = std::move(spirv);
            }
        }

        return {};
    }

    RequiredBufferSizes ComputeRequiredBufferSizesForLayout(const EntryPointMetadata& entryPoint,
                                                            const PipelineLayoutBase* layout) {
        RequiredBufferSizes bufferSizes;
        for (BindGroupIndex group : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
            bufferSizes[group] = GetBindGroupMinBufferSizes(entryPoint.bindings[group],
                                                            layout->GetBindGroupLayout(group));
        }

        return bufferSizes;
    }

    ResultOrError<tint::Program> RunTransforms(tint::transform::Transform* transform,
                                               const tint::Program* program,
                                               const tint::transform::DataMap& inputs,
                                               tint::transform::DataMap* outputs,
                                               OwnedCompilationMessages* outMessages) {
        tint::transform::Output output = transform->Run(program, inputs);
        if (outMessages != nullptr) {
            outMessages->AddMessages(output.program.Diagnostics());
        }
        if (!output.program.IsValid()) {
            std::string err = "Tint program failure: " + output.program.Diagnostics().str();
            return DAWN_VALIDATION_ERROR(err.c_str());
        }
        if (outputs != nullptr) {
            *outputs = std::move(output.data);
        }
        return std::move(output.program);
    }

    void AddVertexPullingTransformConfig(const VertexState& vertexState,
                                         const std::string& entryPoint,
                                         BindGroupIndex pullingBufferBindingSet,
                                         tint::transform::DataMap* transformInputs) {
        tint::transform::VertexPulling::Config cfg;
        cfg.entry_point_name = entryPoint;
        cfg.pulling_group = static_cast<uint32_t>(pullingBufferBindingSet);
        for (uint32_t i = 0; i < vertexState.bufferCount; ++i) {
            const auto& vertexBuffer = vertexState.buffers[i];
            tint::transform::VertexBufferLayoutDescriptor layout;
            layout.array_stride = vertexBuffer.arrayStride;
            layout.step_mode = ToTintInputStepMode(vertexBuffer.stepMode);

            for (uint32_t j = 0; j < vertexBuffer.attributeCount; ++j) {
                const auto& attribute = vertexBuffer.attributes[j];
                tint::transform::VertexAttributeDescriptor attr;
                attr.format = ToTintVertexFormat(attribute.format);
                attr.offset = attribute.offset;
                attr.shader_location = attribute.shaderLocation;

                layout.attributes.push_back(std::move(attr));
            }

            cfg.vertex_state.push_back(std::move(layout));
        }
        transformInputs->Add<tint::transform::VertexPulling::Config>(cfg);
    }

    MaybeError ValidateCompatibilityWithPipelineLayout(DeviceBase* device,
                                                       const EntryPointMetadata& entryPoint,
                                                       const PipelineLayoutBase* layout) {
        for (BindGroupIndex group : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
            DAWN_TRY(ValidateCompatibilityWithBindGroupLayout(device, group, entryPoint,
                                                              layout->GetBindGroupLayout(group)));
        }

        for (BindGroupIndex group : IterateBitSet(~layout->GetBindGroupLayoutsMask())) {
            if (entryPoint.bindings[group].size() > 0) {
                std::ostringstream ostream;
                ostream << "No bind group layout entry matches the declaration set "
                        << static_cast<uint32_t>(group) << " in the shader module";
                return DAWN_VALIDATION_ERROR(ostream.str());
            }
        }

        return {};
    }

    // ShaderModuleBase

    ShaderModuleBase::ShaderModuleBase(DeviceBase* device, const ShaderModuleDescriptor* descriptor)
        : CachedObject(device), mType(Type::Undefined) {
        ASSERT(descriptor->nextInChain != nullptr);
        const ShaderModuleSPIRVDescriptor* spirvDesc = nullptr;
        FindInChain(descriptor->nextInChain, &spirvDesc);
        const ShaderModuleWGSLDescriptor* wgslDesc = nullptr;
        FindInChain(descriptor->nextInChain, &wgslDesc);
        ASSERT(spirvDesc || wgslDesc);

        if (spirvDesc) {
            mType = Type::Spirv;
            mOriginalSpirv.assign(spirvDesc->code, spirvDesc->code + spirvDesc->codeSize);
        } else if (wgslDesc) {
            mType = Type::Wgsl;
            mWgsl = std::string(wgslDesc->source);
        }
    }

    ShaderModuleBase::ShaderModuleBase(
        DeviceBase* device,
        ObjectBase::ErrorTag tag,
        std::unique_ptr<OwnedCompilationMessages> compilationMessages)
        : CachedObject(device, tag),
          mType(Type::Undefined),
          mCompilationMessages(std::move(compilationMessages)) {
    }

    ShaderModuleBase::~ShaderModuleBase() {
        if (IsCachedReference()) {
            GetDevice()->UncacheShaderModule(this);
        }
    }

    // static
    ShaderModuleBase* ShaderModuleBase::MakeError(
        DeviceBase* device,
        std::unique_ptr<OwnedCompilationMessages> compilationMessages) {
        return new ShaderModuleBase(device, ObjectBase::kError, std::move(compilationMessages));
    }

    bool ShaderModuleBase::HasEntryPoint(const std::string& entryPoint) const {
        return mEntryPoints.count(entryPoint) > 0;
    }

    const EntryPointMetadata& ShaderModuleBase::GetEntryPoint(const std::string& entryPoint) const {
        ASSERT(HasEntryPoint(entryPoint));
        return *mEntryPoints.at(entryPoint);
    }

    size_t ShaderModuleBase::ComputeContentHash() {
        ObjectContentHasher recorder;
        recorder.Record(mType);
        recorder.Record(mOriginalSpirv);
        recorder.Record(mWgsl);
        return recorder.GetContentHash();
    }

    bool ShaderModuleBase::EqualityFunc::operator()(const ShaderModuleBase* a,
                                                    const ShaderModuleBase* b) const {
        return a->mType == b->mType && a->mOriginalSpirv == b->mOriginalSpirv &&
               a->mWgsl == b->mWgsl;
    }

    const std::vector<uint32_t>& ShaderModuleBase::GetSpirv() const {
        ASSERT(!GetDevice()->IsToggleEnabled(Toggle::UseTintGenerator));
        return mSpirv;
    }

    const tint::Program* ShaderModuleBase::GetTintProgram() const {
        ASSERT(GetDevice()->IsToggleEnabled(Toggle::UseTintGenerator));
        return mTintProgram.get();
    }

    void ShaderModuleBase::APIGetCompilationInfo(wgpu::CompilationInfoCallback callback,
                                                 void* userdata) {
        if (callback == nullptr) {
            return;
        }

        callback(WGPUCompilationInfoRequestStatus_Success,
                 mCompilationMessages->GetCompilationInfo(), userdata);
    }

    ResultOrError<std::vector<uint32_t>> ShaderModuleBase::GeneratePullingSpirv(
        const std::vector<uint32_t>& spirv,
        const VertexState& vertexState,
        const std::string& entryPoint,
        BindGroupIndex pullingBufferBindingSet) const {
        tint::Program program;
        DAWN_TRY_ASSIGN(program, ParseSPIRV(spirv, nullptr));

        return GeneratePullingSpirv(&program, vertexState, entryPoint, pullingBufferBindingSet);
    }

    ResultOrError<std::vector<uint32_t>> ShaderModuleBase::GeneratePullingSpirv(
        const tint::Program* programIn,
        const VertexState& vertexState,
        const std::string& entryPoint,
        BindGroupIndex pullingBufferBindingSet) const {
        std::ostringstream errorStream;
        errorStream << "Tint vertex pulling failure:" << std::endl;

        tint::transform::Manager transformManager;
        transformManager.Add<tint::transform::VertexPulling>();
        transformManager.Add<tint::transform::Spirv>();
        if (GetDevice()->IsRobustnessEnabled()) {
            transformManager.Add<tint::transform::BoundArrayAccessors>();
        }

        tint::transform::DataMap transformInputs;

        tint::transform::Spirv::Config spirv_cfg;
        spirv_cfg.emit_vertex_point_size = true;
        transformInputs.Add<tint::transform::Spirv::Config>(spirv_cfg);

        AddVertexPullingTransformConfig(vertexState, entryPoint, pullingBufferBindingSet,
                                        &transformInputs);

        // A nullptr is passed in for the CompilationMessages here since this method is called
        // during RenderPipeline creation, by which point the shader module's CompilationInfo
        // may have already been queried.
        tint::Program program;
        DAWN_TRY_ASSIGN(program, RunTransforms(&transformManager, programIn, transformInputs,
                                               nullptr, nullptr));

        tint::writer::spirv::Generator generator(&program);
        if (!generator.Generate()) {
            errorStream << "Generator: " << generator.error() << std::endl;
            return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
        }

        std::vector<uint32_t> spirv = generator.result();
        DAWN_TRY(ValidateSpirv(spirv.data(), spirv.size()));
        return std::move(spirv);
    }

    MaybeError ShaderModuleBase::InitializeBase(ShaderModuleParseResult* parseResult) {
        mTintProgram = std::move(parseResult->tintProgram);
        mTintSource = std::move(parseResult->tintSource);
        mSpirv = std::move(parseResult->spirv);
        mCompilationMessages = std::move(parseResult->compilationMessages);

        if (GetDevice()->IsToggleEnabled(Toggle::UseTintGenerator)) {
            DAWN_TRY_ASSIGN(mEntryPoints, ReflectShaderUsingTint(GetDevice(), mTintProgram.get()));
        } else {
            // If not using Tint to generate backend code, run the robust buffer access pass now
            // since all backends will use this SPIR-V. If Tint is used, the robustness pass should
            // be run per-backend.
            if (GetDevice()->IsRobustnessEnabled()) {
                DAWN_TRY_ASSIGN(mSpirv, RunRobustBufferAccessPass(mSpirv));
            }
            DAWN_TRY_ASSIGN(mEntryPoints, ReflectShaderUsingSPIRVCross(GetDevice(), mSpirv));
        }

        return {};
    }

    ResultOrError<EntryPointMetadataTable> ShaderModuleBase::ReflectShaderUsingSPIRVCross(
        DeviceBase* device,
        const std::vector<uint32_t>& spirv) {
        EntryPointMetadataTable result;
        spirv_cross::Compiler compiler(spirv);
        for (const spirv_cross::EntryPoint& entryPoint : compiler.get_entry_points_and_stages()) {
            ASSERT(result.count(entryPoint.name) == 0);

            SingleShaderStage stage = ExecutionModelToShaderStage(entryPoint.execution_model);
            compiler.set_entry_point(entryPoint.name, entryPoint.execution_model);

            std::unique_ptr<EntryPointMetadata> metadata;
            DAWN_TRY_ASSIGN(metadata, ExtractSpirvInfo(device, compiler, entryPoint.name, stage));
            result[entryPoint.name] = std::move(metadata);
        }
        return std::move(result);
    }

    size_t PipelineLayoutEntryPointPairHashFunc::operator()(
        const PipelineLayoutEntryPointPair& pair) const {
        size_t hash = 0;
        HashCombine(&hash, pair.first, pair.second);
        return hash;
    }

}  // namespace dawn_native

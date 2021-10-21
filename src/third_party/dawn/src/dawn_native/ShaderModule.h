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

#ifndef DAWNNATIVE_SHADERMODULE_H_
#define DAWNNATIVE_SHADERMODULE_H_

#include "common/Constants.h"
#include "common/ityp_array.h"
#include "dawn_native/BindingInfo.h"
#include "dawn_native/CachedObject.h"
#include "dawn_native/CompilationMessages.h"
#include "dawn_native/Error.h"
#include "dawn_native/Format.h"
#include "dawn_native/Forward.h"
#include "dawn_native/IntegerTypes.h"
#include "dawn_native/ObjectBase.h"
#include "dawn_native/PerStage.h"
#include "dawn_native/VertexFormat.h"
#include "dawn_native/dawn_platform.h"

#include <bitset>
#include <map>
#include <unordered_map>
#include <vector>

namespace tint {

    class Program;

    namespace transform {
        class DataMap;
        class Transform;
        class VertexPulling;
    }  // namespace transform

}  // namespace tint

namespace dawn_native {

    struct EntryPointMetadata;

    // Base component type of an inter-stage variable
    enum class InterStageComponentType {
        Sint,
        Uint,
        Float,
    };

    enum class InterpolationType {
        Perspective,
        Linear,
        Flat,
    };

    enum class InterpolationSampling {
        None,
        Center,
        Centroid,
        Sample,
    };

    using PipelineLayoutEntryPointPair = std::pair<PipelineLayoutBase*, std::string>;
    struct PipelineLayoutEntryPointPairHashFunc {
        size_t operator()(const PipelineLayoutEntryPointPair& pair) const;
    };

    // A map from name to EntryPointMetadata.
    using EntryPointMetadataTable =
        std::unordered_map<std::string, std::unique_ptr<EntryPointMetadata>>;

    // Source for a tint program
    class TintSource;

    struct ShaderModuleParseResult {
        ShaderModuleParseResult();
        ~ShaderModuleParseResult();
        ShaderModuleParseResult(ShaderModuleParseResult&& rhs);
        ShaderModuleParseResult& operator=(ShaderModuleParseResult&& rhs);

        bool HasParsedShader() const;

        std::unique_ptr<tint::Program> tintProgram;
        std::unique_ptr<TintSource> tintSource;
    };

    MaybeError ValidateShaderModuleDescriptor(DeviceBase* device,
                                              const ShaderModuleDescriptor* descriptor,
                                              ShaderModuleParseResult* parseResult,
                                              OwnedCompilationMessages* outMessages);
    MaybeError ValidateCompatibilityWithPipelineLayout(DeviceBase* device,
                                                       const EntryPointMetadata& entryPoint,
                                                       const PipelineLayoutBase* layout);

    RequiredBufferSizes ComputeRequiredBufferSizesForLayout(const EntryPointMetadata& entryPoint,
                                                            const PipelineLayoutBase* layout);
    ResultOrError<tint::Program> RunTransforms(tint::transform::Transform* transform,
                                               const tint::Program* program,
                                               const tint::transform::DataMap& inputs,
                                               tint::transform::DataMap* outputs,
                                               OwnedCompilationMessages* messages);

    /// Creates and adds the tint::transform::VertexPulling::Config to transformInputs.
    void AddVertexPullingTransformConfig(const RenderPipelineBase& renderPipeline,
                                         const std::string& entryPoint,
                                         BindGroupIndex pullingBufferBindingSet,
                                         tint::transform::DataMap* transformInputs);

    // Mirrors wgpu::SamplerBindingLayout but instead stores a single boolean
    // for isComparison instead of a wgpu::SamplerBindingType enum.
    struct ShaderSamplerBindingInfo {
        bool isComparison;
    };

    // Mirrors wgpu::TextureBindingLayout but instead has a set of compatible sampleTypes
    // instead of a single enum.
    struct ShaderTextureBindingInfo {
        SampleTypeBit compatibleSampleTypes;
        wgpu::TextureViewDimension viewDimension;
        bool multisampled;
    };

    // Per-binding shader metadata contains some SPIRV specific information in addition to
    // most of the frontend per-binding information.
    struct ShaderBindingInfo {
        // The SPIRV ID of the resource.
        uint32_t id;
        uint32_t base_type_id;

        BindingNumber binding;
        BindingInfoType bindingType;

        BufferBindingLayout buffer;
        ShaderSamplerBindingInfo sampler;
        ShaderTextureBindingInfo texture;
        StorageTextureBindingLayout storageTexture;
    };

    using BindingGroupInfoMap = std::map<BindingNumber, ShaderBindingInfo>;
    using BindingInfoArray = ityp::array<BindGroupIndex, BindingGroupInfoMap, kMaxBindGroups>;

    // Contains all the reflection data for a valid (ShaderModule, entryPoint, stage). They are
    // stored in the ShaderModuleBase and destroyed only when the shader program is destroyed so
    // pointers to EntryPointMetadata are safe to store as long as you also keep a Ref to the
    // ShaderModuleBase.
    struct EntryPointMetadata {
        // bindings[G][B] is the reflection data for the binding defined with
        // [[group=G, binding=B]] in WGSL / SPIRV.
        BindingInfoArray bindings;

        struct SamplerTexturePair {
            BindingSlot sampler;
            BindingSlot texture;
        };
        std::vector<SamplerTexturePair> samplerTexturePairs;

        // The set of vertex attributes this entryPoint uses.
        ityp::array<VertexAttributeLocation, VertexFormatBaseType, kMaxVertexAttributes>
            vertexInputBaseTypes;
        ityp::bitset<VertexAttributeLocation, kMaxVertexAttributes> usedVertexInputs;

        // An array to record the basic types (float, int and uint) of the fragment shader outputs.
        struct FragmentOutputVariableInfo {
            wgpu::TextureComponentType baseType;
            uint8_t componentCount;
        };
        ityp::array<ColorAttachmentIndex, FragmentOutputVariableInfo, kMaxColorAttachments>
            fragmentOutputVariables;
        ityp::bitset<ColorAttachmentIndex, kMaxColorAttachments> fragmentOutputsWritten;

        struct InterStageVariableInfo {
            InterStageComponentType baseType;
            uint32_t componentCount;
            InterpolationType interpolationType;
            InterpolationSampling interpolationSampling;
        };
        // Now that we only support vertex and fragment stages, there can't be both inter-stage
        // inputs and outputs in one shader stage.
        std::bitset<kMaxInterStageShaderVariables> usedInterStageVariables;
        std::array<InterStageVariableInfo, kMaxInterStageShaderVariables> interStageVariables;

        // The local workgroup size declared for a compute entry point (or 0s otehrwise).
        Origin3D localWorkgroupSize;

        // The shader stage for this binding.
        SingleShaderStage stage;

        struct OverridableConstant {
            uint32_t id;
            // Match tint::inspector::OverridableConstant::Type
            // Bool is defined as a macro on linux X11 and cannot compile
            enum class Type { Boolean, Float32, Uint32, Int32 } type;
        };

        // Store overridableConstants from tint program
        std::unordered_map<std::string, OverridableConstant> overridableConstants;
    };

    class ShaderModuleBase : public ApiObjectBase, public CachedObject {
      public:
        ShaderModuleBase(DeviceBase* device, const ShaderModuleDescriptor* descriptor);
        ~ShaderModuleBase() override;

        static Ref<ShaderModuleBase> MakeError(DeviceBase* device);

        ObjectType GetType() const override;

        // Return true iff the program has an entrypoint called `entryPoint`.
        bool HasEntryPoint(const std::string& entryPoint) const;

        // Returns the metadata for the given `entryPoint`. HasEntryPoint with the same argument
        // must be true.
        const EntryPointMetadata& GetEntryPoint(const std::string& entryPoint) const;

        // Functions necessary for the unordered_set<ShaderModuleBase*>-based cache.
        size_t ComputeContentHash() override;

        struct EqualityFunc {
            bool operator()(const ShaderModuleBase* a, const ShaderModuleBase* b) const;
        };

        const tint::Program* GetTintProgram() const;

        void APIGetCompilationInfo(wgpu::CompilationInfoCallback callback, void* userdata);

        void InjectCompilationMessages(
            std::unique_ptr<OwnedCompilationMessages> compilationMessages);

        OwnedCompilationMessages* GetCompilationMessages() const;

      protected:
        MaybeError InitializeBase(ShaderModuleParseResult* parseResult);

      private:
        ShaderModuleBase(DeviceBase* device, ObjectBase::ErrorTag tag);

        // The original data in the descriptor for caching.
        enum class Type { Undefined, Spirv, Wgsl };
        Type mType;
        std::vector<uint32_t> mOriginalSpirv;
        std::string mWgsl;

        EntryPointMetadataTable mEntryPoints;
        std::unique_ptr<tint::Program> mTintProgram;
        std::unique_ptr<TintSource> mTintSource;  // Keep the tint::Source::File alive

        std::unique_ptr<OwnedCompilationMessages> mCompilationMessages;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_SHADERMODULE_H_

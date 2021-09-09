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
#include "dawn_native/PerStage.h"
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

namespace spirv_cross {
    class Compiler;
}

namespace dawn_native {

    struct EntryPointMetadata;

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
        std::vector<uint32_t> spirv;
        std::unique_ptr<OwnedCompilationMessages> compilationMessages;
    };

    MaybeError ValidateShaderModuleDescriptor(DeviceBase* device,
                                              const ShaderModuleDescriptor* descriptor,
                                              ShaderModuleParseResult* parseResult);
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
    void AddVertexPullingTransformConfig(const VertexState& vertexState,
                                         const std::string& entryPoint,
                                         BindGroupIndex pullingBufferBindingSet,
                                         tint::transform::DataMap* transformInputs);

    // Contains all the reflection data for a valid (ShaderModule, entryPoint, stage). They are
    // stored in the ShaderModuleBase and destroyed only when the shader program is destroyed so
    // pointers to EntryPointMetadata are safe to store as long as you also keep a Ref to the
    // ShaderModuleBase.
    struct EntryPointMetadata {
        // Per-binding shader metadata contains some SPIRV specific information in addition to
        // most of the frontend per-binding information.
        struct ShaderBindingInfo : BindingInfo {
            // The SPIRV ID of the resource.
            uint32_t id;
            uint32_t base_type_id;

          private:
            // Disallow access to unused members.
            using BindingInfo::visibility;
        };

        // bindings[G][B] is the reflection data for the binding defined with
        // [[group=G, binding=B]] in WGSL / SPIRV.
        using BindingGroupInfoMap = std::map<BindingNumber, ShaderBindingInfo>;
        using BindingInfoArray = ityp::array<BindGroupIndex, BindingGroupInfoMap, kMaxBindGroups>;
        BindingInfoArray bindings;

        // The set of vertex attributes this entryPoint uses.
        std::bitset<kMaxVertexAttributes> usedVertexAttributes;

        // An array to record the basic types (float, int and uint) of the fragment shader outputs.
        ityp::array<ColorAttachmentIndex, wgpu::TextureComponentType, kMaxColorAttachments>
            fragmentOutputFormatBaseTypes;
        ityp::bitset<ColorAttachmentIndex, kMaxColorAttachments> fragmentOutputsWritten;

        // The local workgroup size declared for a compute entry point (or 0s otehrwise).
        Origin3D localWorkgroupSize;

        // The shader stage for this binding.
        SingleShaderStage stage;
    };

    class ShaderModuleBase : public CachedObject {
      public:
        ShaderModuleBase(DeviceBase* device, const ShaderModuleDescriptor* descriptor);
        ~ShaderModuleBase() override;

        static ShaderModuleBase* MakeError(
            DeviceBase* device,
            std::unique_ptr<OwnedCompilationMessages> compilationMessages);

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

        const std::vector<uint32_t>& GetSpirv() const;
        const tint::Program* GetTintProgram() const;

        void APIGetCompilationInfo(wgpu::CompilationInfoCallback callback, void* userdata);

        ResultOrError<std::vector<uint32_t>> GeneratePullingSpirv(
            const std::vector<uint32_t>& spirv,
            const VertexState& vertexState,
            const std::string& entryPoint,
            BindGroupIndex pullingBufferBindingSet) const;

        ResultOrError<std::vector<uint32_t>> GeneratePullingSpirv(
            const tint::Program* program,
            const VertexState& vertexState,
            const std::string& entryPoint,
            BindGroupIndex pullingBufferBindingSet) const;

        OwnedCompilationMessages* GetCompilationMessages() {
            return mCompilationMessages.get();
        }

      protected:
        MaybeError InitializeBase(ShaderModuleParseResult* parseResult);
        static ResultOrError<EntryPointMetadataTable> ReflectShaderUsingSPIRVCross(
            DeviceBase* device,
            const std::vector<uint32_t>& spirv);

      private:
        ShaderModuleBase(DeviceBase* device,
                         ObjectBase::ErrorTag tag,
                         std::unique_ptr<OwnedCompilationMessages> compilationMessages);

        // The original data in the descriptor for caching.
        enum class Type { Undefined, Spirv, Wgsl };
        Type mType;
        std::vector<uint32_t> mOriginalSpirv;
        std::string mWgsl;

        // Data computed from what is in the descriptor. mSpirv is set iff !UseTintGenerator while
        // mTintProgram is set iff UseTintGenerator.
        EntryPointMetadataTable mEntryPoints;
        std::vector<uint32_t> mSpirv;
        std::unique_ptr<tint::Program> mTintProgram;
        std::unique_ptr<TintSource> mTintSource;  // Keep the tint::Source::File alive

        std::unique_ptr<OwnedCompilationMessages> mCompilationMessages;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_SHADERMODULE_H_

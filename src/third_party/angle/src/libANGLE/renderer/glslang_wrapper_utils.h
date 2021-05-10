//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Wrapper for Khronos glslang compiler. This file is used by Vulkan and Metal backends.
//

#ifndef LIBANGLE_RENDERER_GLSLANG_WRAPPER_UTILS_H_
#define LIBANGLE_RENDERER_GLSLANG_WRAPPER_UTILS_H_

#include <functional>

#include "libANGLE/renderer/ProgramImpl.h"
#include "libANGLE/renderer/renderer_utils.h"

namespace rx
{
enum class GlslangError
{
    InvalidShader,
    InvalidSpirv,
};

constexpr gl::ShaderMap<const char *> kDefaultUniformNames = {
    {gl::ShaderType::Vertex, sh::vk::kDefaultUniformsNameVS},
    {gl::ShaderType::TessControl, sh::vk::kDefaultUniformsNameTCS},
    {gl::ShaderType::TessEvaluation, sh::vk::kDefaultUniformsNameTES},
    {gl::ShaderType::Geometry, sh::vk::kDefaultUniformsNameGS},
    {gl::ShaderType::Fragment, sh::vk::kDefaultUniformsNameFS},
    {gl::ShaderType::Compute, sh::vk::kDefaultUniformsNameCS},
};

struct GlslangProgramInterfaceInfo
{
    // Uniforms set index:
    uint32_t uniformsAndXfbDescriptorSetIndex;
    uint32_t currentUniformBindingIndex;
    // Textures set index:
    uint32_t textureDescriptorSetIndex;
    uint32_t currentTextureBindingIndex;
    // Other shader resources set index:
    uint32_t shaderResourceDescriptorSetIndex;
    uint32_t currentShaderResourceBindingIndex;
    // ANGLE driver uniforms set index:
    uint32_t driverUniformsDescriptorSetIndex;

    uint32_t locationsUsedForXfbExtension;
};

struct GlslangSourceOptions
{
    bool supportsTransformFeedbackExtension = false;
    bool supportsTransformFeedbackEmulation = false;
    bool enableTransformFeedbackEmulation   = false;
    bool emulateBresenhamLines              = false;
};

struct GlslangSpirvOptions
{
    gl::ShaderType shaderType                 = gl::ShaderType::InvalidEnum;
    SurfaceRotation preRotation               = SurfaceRotation::Identity;
    bool negativeViewportSupported            = false;
    bool transformPositionToVulkanClipSpace   = false;
    bool removeEarlyFragmentTestsOptimization = false;
    bool removeDebugInfo                      = false;
    bool isTransformFeedbackStage             = false;
};

using SpirvBlob = std::vector<uint32_t>;

using GlslangErrorCallback = std::function<angle::Result(GlslangError)>;

struct ShaderInterfaceVariableXfbInfo
{
    static constexpr uint32_t kInvalid = std::numeric_limits<uint32_t>::max();

    uint32_t buffer = kInvalid;
    uint32_t offset = kInvalid;
    uint32_t stride = kInvalid;
};

// Information for each shader interface variable.  Not all fields are relevant to each shader
// interface variable.  For example opaque uniforms require a set and binding index, while vertex
// attributes require a location.
struct ShaderInterfaceVariableInfo
{
    ShaderInterfaceVariableInfo();

    static constexpr uint32_t kInvalid = std::numeric_limits<uint32_t>::max();

    // Used for interface blocks and opaque uniforms.
    uint32_t descriptorSet = kInvalid;
    uint32_t binding       = kInvalid;
    // Used for vertex attributes, fragment shader outputs and varyings.  There could be different
    // variables that share the same name, such as a vertex attribute and a fragment output.  They
    // will share this object since they have the same name, but will find possibly different
    // locations in their respective slots.
    uint32_t location  = kInvalid;
    uint32_t component = kInvalid;
    uint32_t index     = kInvalid;
    // The stages this shader interface variable is active.
    gl::ShaderBitSet activeStages;
    // Used for transform feedback extension to decorate vertex shader output.
    ShaderInterfaceVariableXfbInfo xfb;
    std::vector<ShaderInterfaceVariableXfbInfo> fieldXfb;
    // Indicates that the precision needs to be modified in the generated SPIR-V
    // to support only transferring medium precision data when there's a precision
    // mismatch between the shaders. For example, either the VS casts highp->mediump
    // or the FS casts mediump->highp.
    bool useRelaxedPrecision = false;
    // Indicate if varying is input or output, or both (in case of for example gl_Position in a
    // geometry shader)
    bool varyingIsInput  = false;
    bool varyingIsOutput = false;
    // For vertex attributes, this is the number of components / locations.  These are used by the
    // vertex attribute aliasing transformation only.
    uint8_t attributeComponentCount = 0;
    uint8_t attributeLocationCount  = 0;
};

// TODO: http://anglebug.com/4524: Need a different hash key than a string, since that's slow to
// calculate.
class ShaderInterfaceVariableInfoMap final : angle::NonCopyable
{
  public:
    ShaderInterfaceVariableInfoMap();
    ~ShaderInterfaceVariableInfoMap();

    void clear();
    bool contains(gl::ShaderType shaderType, const std::string &variableName) const;
    const ShaderInterfaceVariableInfo &get(gl::ShaderType shaderType,
                                           const std::string &variableName) const;
    ShaderInterfaceVariableInfo &get(gl::ShaderType shaderType, const std::string &variableName);
    ShaderInterfaceVariableInfo &add(gl::ShaderType shaderType, const std::string &variableName);
    ShaderInterfaceVariableInfo &addOrGet(gl::ShaderType shaderType,
                                          const std::string &variableName);
    size_t variableCount(gl::ShaderType shaderType) const { return mData[shaderType].size(); }

    using VariableNameToInfoMap = angle::HashMap<std::string, ShaderInterfaceVariableInfo>;

    class Iterator final
    {
      public:
        Iterator(VariableNameToInfoMap::const_iterator beginIt,
                 VariableNameToInfoMap::const_iterator endIt)
            : mBeginIt(beginIt), mEndIt(endIt)
        {}
        VariableNameToInfoMap::const_iterator begin() { return mBeginIt; }
        VariableNameToInfoMap::const_iterator end() { return mEndIt; }

      private:
        VariableNameToInfoMap::const_iterator mBeginIt;
        VariableNameToInfoMap::const_iterator mEndIt;
    };

    Iterator getIterator(gl::ShaderType shaderType) const;

  private:
    gl::ShaderMap<VariableNameToInfoMap> mData;
};

void GlslangInitialize();
void GlslangRelease();

bool GetImageNameWithoutIndices(std::string *name);

// Get the mapped sampler name after the source is transformed by GlslangGetShaderSource()
std::string GlslangGetMappedSamplerName(const std::string &originalName);
std::string GetXfbBufferName(const uint32_t bufferIndex);

// NOTE: options.emulateTransformFeedback is ignored in this case. It is assumed to be always true.
void GlslangGenTransformFeedbackEmulationOutputs(
    const GlslangSourceOptions &options,
    const gl::ProgramState &programState,
    GlslangProgramInterfaceInfo *programInterfaceInfo,
    std::string *vertexShader,
    ShaderInterfaceVariableInfoMap *variableInfoMapOut);

void GlslangAssignLocations(const GlslangSourceOptions &options,
                            const gl::ProgramState &programState,
                            const gl::ProgramVaryingPacking &varyingPacking,
                            const gl::ShaderType shaderType,
                            const gl::ShaderType frontShaderType,
                            bool isTransformFeedbackStage,
                            GlslangProgramInterfaceInfo *programInterfaceInfo,
                            ShaderInterfaceVariableInfoMap *variableInfoMapOut);

// Transform the source to include actual binding points for various shader resources (textures,
// buffers, xfb, etc).  For some variables, these values are instead output to the variableInfoMap
// to be set during a SPIR-V transformation.  This is a transitory step towards moving all variables
// to this map, at which point GlslangGetShaderSpirvCode will also be called by this function.
void GlslangGetShaderSource(const GlslangSourceOptions &options,
                            const gl::ProgramState &programState,
                            const gl::ProgramLinkedResources &resources,
                            GlslangProgramInterfaceInfo *programInterfaceInfo,
                            gl::ShaderMap<std::string> *shaderSourcesOut,
                            ShaderInterfaceVariableInfoMap *variableInfoMapOut);

angle::Result GlslangTransformSpirvCode(const GlslangErrorCallback &callback,
                                        const GlslangSpirvOptions &options,
                                        const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                        const SpirvBlob &initialSpirvBlob,
                                        SpirvBlob *spirvBlobOut);

angle::Result GlslangGetShaderSpirvCode(const GlslangErrorCallback &callback,
                                        const gl::ShaderBitSet &linkedShaderStages,
                                        const gl::Caps &glCaps,
                                        const gl::ShaderMap<std::string> &shaderSources,
                                        gl::ShaderMap<SpirvBlob> *spirvBlobsOut);

angle::Result GlslangCompileShaderOneOff(const GlslangErrorCallback &callback,
                                         gl::ShaderType shaderType,
                                         const std::string &shaderSource,
                                         SpirvBlob *spirvBlobOut);

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GLSLANG_WRAPPER_UTILS_H_

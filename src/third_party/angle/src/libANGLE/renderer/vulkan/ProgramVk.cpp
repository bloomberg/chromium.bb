//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramVk.cpp:
//    Implements the class methods for ProgramVk.
//

#include "libANGLE/renderer/vulkan/ProgramVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/GlslangWrapper.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"

namespace rx
{

namespace
{

constexpr size_t kUniformBlockDynamicBufferMinSize = 256 * 128;

void InitDefaultUniformBlock(const std::vector<sh::Uniform> &uniforms,
                             gl::Shader *shader,
                             sh::BlockLayoutMap *blockLayoutMapOut,
                             size_t *blockSizeOut)
{
    if (uniforms.empty())
    {
        *blockSizeOut = 0;
        return;
    }

    sh::Std140BlockEncoder blockEncoder;
    sh::GetUniformBlockInfo(uniforms, "", &blockEncoder, blockLayoutMapOut);

    size_t blockSize = blockEncoder.getCurrentOffset();

    // TODO(jmadill): I think we still need a valid block for the pipeline even if zero sized.
    if (blockSize == 0)
    {
        *blockSizeOut = 0;
        return;
    }

    *blockSizeOut = blockSize;
    return;
}

template <typename T>
void UpdateDefaultUniformBlock(GLsizei count,
                               uint32_t arrayIndex,
                               int componentCount,
                               const T *v,
                               const sh::BlockMemberInfo &layoutInfo,
                               angle::MemoryBuffer *uniformData)
{
    const int elementSize = sizeof(T) * componentCount;

    uint8_t *dst = uniformData->data() + layoutInfo.offset;
    if (layoutInfo.arrayStride == 0 || layoutInfo.arrayStride == elementSize)
    {
        uint32_t arrayOffset = arrayIndex * layoutInfo.arrayStride;
        uint8_t *writePtr    = dst + arrayOffset;
        ASSERT(writePtr + (elementSize * count) <= uniformData->data() + uniformData->size());
        memcpy(writePtr, v, elementSize * count);
    }
    else
    {
        // Have to respect the arrayStride between each element of the array.
        int maxIndex = arrayIndex + count;
        for (int writeIndex = arrayIndex, readIndex = 0; writeIndex < maxIndex;
             writeIndex++, readIndex++)
        {
            const int arrayOffset = writeIndex * layoutInfo.arrayStride;
            uint8_t *writePtr     = dst + arrayOffset;
            const T *readPtr      = v + (readIndex * componentCount);
            ASSERT(writePtr + elementSize <= uniformData->data() + uniformData->size());
            memcpy(writePtr, readPtr, elementSize);
        }
    }
}

template <typename T>
void ReadFromDefaultUniformBlock(int componentCount,
                                 uint32_t arrayIndex,
                                 T *dst,
                                 const sh::BlockMemberInfo &layoutInfo,
                                 const angle::MemoryBuffer *uniformData)
{
    ASSERT(layoutInfo.offset != -1);

    const int elementSize = sizeof(T) * componentCount;
    const uint8_t *source = uniformData->data() + layoutInfo.offset;

    if (layoutInfo.arrayStride == 0 || layoutInfo.arrayStride == elementSize)
    {
        const uint8_t *readPtr = source + arrayIndex * layoutInfo.arrayStride;
        memcpy(dst, readPtr, elementSize);
    }
    else
    {
        // Have to respect the arrayStride between each element of the array.
        const int arrayOffset  = arrayIndex * layoutInfo.arrayStride;
        const uint8_t *readPtr = source + arrayOffset;
        memcpy(dst, readPtr, elementSize);
    }
}

angle::Result SyncDefaultUniformBlock(ContextVk *contextVk,
                                      vk::DynamicBuffer *dynamicBuffer,
                                      const angle::MemoryBuffer &bufferData,
                                      uint32_t *outOffset,
                                      bool *outBufferModified)
{
    dynamicBuffer->releaseRetainedBuffers(contextVk);

    ASSERT(!bufferData.empty());
    uint8_t *data       = nullptr;
    VkBuffer *outBuffer = nullptr;
    VkDeviceSize offset = 0;
    ANGLE_TRY(dynamicBuffer->allocate(contextVk, bufferData.size(), &data, outBuffer, &offset,
                                      outBufferModified));
    *outOffset = static_cast<uint32_t>(offset);
    memcpy(data, bufferData.data(), bufferData.size());
    ANGLE_TRY(dynamicBuffer->flush(contextVk));
    return angle::Result::Continue;
}

uint32_t GetUniformBlockArraySize(const std::vector<gl::InterfaceBlock> &uniformBlocks,
                                  uint32_t bufferIndex)
{
    const gl::InterfaceBlock &uniformBlock = uniformBlocks[bufferIndex];

    if (!uniformBlock.isArray)
    {
        return 1;
    }

    ASSERT(uniformBlock.arrayElement == 0);

    // Search consecutively until all array indices of this block are visited.
    uint32_t arraySize;
    for (arraySize = 1; bufferIndex + arraySize < uniformBlocks.size(); ++arraySize)
    {
        const gl::InterfaceBlock &nextBlock = uniformBlocks[bufferIndex + arraySize];

        if (nextBlock.arrayElement != arraySize)
        {
            break;
        }

        // It's unexpected for an array to start at a non-zero array size, so we can always rely on
        // the sequential `arrayElement`s to belong to the same block.
        ASSERT(nextBlock.name == uniformBlock.name);
        ASSERT(nextBlock.isArray);
    }

    return arraySize;
}

class Std140BlockLayoutEncoderFactory : public gl::CustomBlockLayoutEncoderFactory
{
  public:
    sh::BlockLayoutEncoder *makeEncoder() override { return new sh::Std140BlockEncoder(); }
};
}  // anonymous namespace

// ProgramVk::ShaderInfo implementation.
ProgramVk::ShaderInfo::ShaderInfo() {}

ProgramVk::ShaderInfo::~ShaderInfo() = default;

angle::Result ProgramVk::ShaderInfo::initShaders(ContextVk *contextVk,
                                                 const std::string &vertexSource,
                                                 const std::string &fragmentSource,
                                                 bool enableLineRasterEmulation)
{
    ASSERT(!valid());

    std::vector<uint32_t> vertexCode;
    std::vector<uint32_t> fragmentCode;
    ANGLE_TRY(GlslangWrapper::GetShaderCode(contextVk, contextVk->getCaps(),
                                            enableLineRasterEmulation, vertexSource, fragmentSource,
                                            &vertexCode, &fragmentCode));

    ANGLE_TRY(vk::InitShaderAndSerial(contextVk, &mShaders[gl::ShaderType::Vertex].get(),
                                      vertexCode.data(), vertexCode.size() * sizeof(uint32_t)));
    ANGLE_TRY(vk::InitShaderAndSerial(contextVk, &mShaders[gl::ShaderType::Fragment].get(),
                                      fragmentCode.data(), fragmentCode.size() * sizeof(uint32_t)));

    mProgramHelper.setShader(gl::ShaderType::Vertex, &mShaders[gl::ShaderType::Vertex]);
    mProgramHelper.setShader(gl::ShaderType::Fragment, &mShaders[gl::ShaderType::Fragment]);

    return angle::Result::Continue;
}

void ProgramVk::ShaderInfo::release(ContextVk *contextVk)
{
    mProgramHelper.release(contextVk);

    for (vk::RefCounted<vk::ShaderAndSerial> &shader : mShaders)
    {
        shader.get().destroy(contextVk->getDevice());
    }
}

// ProgramVk implementation.
ProgramVk::DefaultUniformBlock::DefaultUniformBlock()
    : storage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              kUniformBlockDynamicBufferMinSize,
              true)
{}

ProgramVk::DefaultUniformBlock::~DefaultUniformBlock() = default;

ProgramVk::ProgramVk(const gl::ProgramState &state) : ProgramImpl(state), mUniformBlocksOffsets{} {}

ProgramVk::~ProgramVk() = default;

void ProgramVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    reset(contextVk);
}

void ProgramVk::reset(ContextVk *contextVk)
{
    for (auto &descriptorSetLayout : mDescriptorSetLayouts)
    {
        descriptorSetLayout.reset();
    }
    mPipelineLayout.reset();

    for (auto &uniformBlock : mDefaultUniformBlocks)
    {
        uniformBlock.storage.release(contextVk);
    }

    mDefaultShaderInfo.release(contextVk);
    mLineRasterShaderInfo.release(contextVk);

    mEmptyUniformBlockStorage.release(contextVk);

    mDescriptorSets.clear();
    mEmptyDescriptorSets.fill(VK_NULL_HANDLE);

    for (vk::RefCountedDescriptorPoolBinding &binding : mDescriptorPoolBindings)
    {
        binding.reset();
    }
}

std::unique_ptr<rx::LinkEvent> ProgramVk::load(const gl::Context *context,
                                               gl::BinaryInputStream *stream,
                                               gl::InfoLog &infoLog)
{
    UNIMPLEMENTED();
    return std::make_unique<LinkEventDone>(angle::Result::Stop);
}

void ProgramVk::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    UNIMPLEMENTED();
}

void ProgramVk::setBinaryRetrievableHint(bool retrievable)
{
    UNIMPLEMENTED();
}

void ProgramVk::setSeparable(bool separable)
{
    UNIMPLEMENTED();
}

std::unique_ptr<LinkEvent> ProgramVk::link(const gl::Context *context,
                                           const gl::ProgramLinkedResources &resources,
                                           gl::InfoLog &infoLog)
{
    // TODO(jie.a.chen@intel.com): Parallelize linking.
    // http://crbug.com/849576
    return std::make_unique<LinkEventDone>(linkImpl(context, resources, infoLog));
}

angle::Result ProgramVk::linkImpl(const gl::Context *glContext,
                                  const gl::ProgramLinkedResources &resources,
                                  gl::InfoLog &infoLog)
{
    ContextVk *contextVk = vk::GetImpl(glContext);
    RendererVk *renderer = contextVk->getRenderer();

    reset(contextVk);

    // Link resources before calling GetShaderSource to make sure they are ready for the set/binding
    // assignment done in that function.
    linkResources(resources);

    GlslangWrapper::GetShaderSource(mState, resources, &mVertexSource, &mFragmentSource);

    ANGLE_TRY(initDefaultUniformBlocks(glContext));

    // Store a reference to the pipeline and descriptor set layouts. This will create them if they
    // don't already exist in the cache.
    vk::DescriptorSetLayoutDesc uniformsSetDesc;
    uniformsSetDesc.update(kVertexUniformsBindingIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                           1, VK_SHADER_STAGE_VERTEX_BIT);
    uniformsSetDesc.update(kFragmentUniformsBindingIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                           1, VK_SHADER_STAGE_FRAGMENT_BIT);

    ANGLE_TRY(renderer->getDescriptorSetLayout(
        contextVk, uniformsSetDesc, &mDescriptorSetLayouts[kUniformsDescriptorSetIndex]));

    vk::DescriptorSetLayoutDesc uniformBlocksSetDesc;

    const std::vector<gl::InterfaceBlock> &uniformBlocks = mState.getUniformBlocks();
    for (uint32_t bufferIndex = 0; bufferIndex < uniformBlocks.size();)
    {
        const uint32_t arraySize = GetUniformBlockArraySize(uniformBlocks, bufferIndex);

        uniformBlocksSetDesc.update(bufferIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, arraySize,
                                    VK_SHADER_STAGE_ALL_GRAPHICS);

        bufferIndex += arraySize;
    }

    ANGLE_TRY(renderer->getDescriptorSetLayout(
        contextVk, uniformBlocksSetDesc, &mDescriptorSetLayouts[kUniformBlockDescriptorSetIndex]));

    vk::DescriptorSetLayoutDesc texturesSetDesc;

    for (uint32_t textureIndex = 0; textureIndex < mState.getSamplerBindings().size();
         ++textureIndex)
    {
        const gl::SamplerBinding &samplerBinding = mState.getSamplerBindings()[textureIndex];

        // The front-end always binds array sampler units sequentially.
        const uint32_t count = static_cast<uint32_t>(samplerBinding.boundTextureUnits.size());
        texturesSetDesc.update(textureIndex, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count,
                               VK_SHADER_STAGE_ALL_GRAPHICS);
    }

    ANGLE_TRY(renderer->getDescriptorSetLayout(contextVk, texturesSetDesc,
                                               &mDescriptorSetLayouts[kTextureDescriptorSetIndex]));

    vk::DescriptorSetLayoutDesc driverUniformsSetDesc;
    driverUniformsSetDesc.update(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                 VK_SHADER_STAGE_ALL_GRAPHICS);
    ANGLE_TRY(renderer->getDescriptorSetLayout(
        contextVk, driverUniformsSetDesc,
        &mDescriptorSetLayouts[kDriverUniformsDescriptorSetIndex]));

    vk::PipelineLayoutDesc pipelineLayoutDesc;
    pipelineLayoutDesc.updateDescriptorSetLayout(kUniformsDescriptorSetIndex, uniformsSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(kUniformBlockDescriptorSetIndex,
                                                 uniformBlocksSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(kTextureDescriptorSetIndex, texturesSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(kDriverUniformsDescriptorSetIndex,
                                                 driverUniformsSetDesc);

    ANGLE_TRY(renderer->getPipelineLayout(contextVk, pipelineLayoutDesc, mDescriptorSetLayouts,
                                          &mPipelineLayout));

    return angle::Result::Continue;
}

void ProgramVk::linkResources(const gl::ProgramLinkedResources &resources)
{
    Std140BlockLayoutEncoderFactory std140EncoderFactory;
    gl::ProgramLinkedResourcesLinker linker(&std140EncoderFactory);

    linker.linkResources(mState, resources);
}

angle::Result ProgramVk::initDefaultUniformBlocks(const gl::Context *glContext)
{
    ContextVk *contextVk = vk::GetImpl(glContext);
    RendererVk *renderer = contextVk->getRenderer();

    // Process vertex and fragment uniforms into std140 packing.
    gl::ShaderMap<sh::BlockLayoutMap> layoutMap;
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);

    for (gl::ShaderType shaderType : gl::AllGLES2ShaderTypes())
    {
        gl::Shader *shader                       = mState.getAttachedShader(shaderType);
        const std::vector<sh::Uniform> &uniforms = shader->getUniforms();
        InitDefaultUniformBlock(uniforms, shader, &layoutMap[shaderType],
                                &requiredBufferSize[shaderType]);
    }

    // Init the default block layout info.
    const auto &uniforms = mState.getUniforms();
    for (const gl::VariableLocation &location : mState.getUniformLocations())
    {
        gl::ShaderMap<sh::BlockMemberInfo> layoutInfo;

        if (location.used() && !location.ignored)
        {
            const auto &uniform = uniforms[location.index];
            if (uniform.isInDefaultBlock() && !uniform.isSampler())
            {
                std::string uniformName = uniform.name;
                if (uniform.isArray())
                {
                    // Gets the uniform name without the [0] at the end.
                    uniformName = gl::ParseResourceName(uniformName, nullptr);
                }

                bool found = false;

                for (gl::ShaderType shaderType : gl::AllGLES2ShaderTypes())
                {
                    auto it = layoutMap[shaderType].find(uniformName);
                    if (it != layoutMap[shaderType].end())
                    {
                        found                  = true;
                        layoutInfo[shaderType] = it->second;
                    }
                }

                ASSERT(found);
            }
        }

        for (gl::ShaderType shaderType : gl::AllGLES2ShaderTypes())
        {
            mDefaultUniformBlocks[shaderType].uniformLayout.push_back(layoutInfo[shaderType]);
        }
    }

    for (gl::ShaderType shaderType : gl::AllGLES2ShaderTypes())
    {
        if (requiredBufferSize[shaderType] > 0)
        {
            if (!mDefaultUniformBlocks[shaderType].uniformData.resize(
                    requiredBufferSize[shaderType]))
            {
                ANGLE_VK_CHECK(contextVk, false, VK_ERROR_OUT_OF_HOST_MEMORY);
            }
            size_t minAlignment = static_cast<size_t>(
                renderer->getPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment);

            mDefaultUniformBlocks[shaderType].storage.init(minAlignment, renderer);

            // Initialize uniform buffer memory to zero by default.
            mDefaultUniformBlocks[shaderType].uniformData.fill(0);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }

    if (mDefaultUniformBlocksDirty.any())
    {
        // Initialize the "empty" uniform block if necessary.
        if (!mDefaultUniformBlocksDirty.all())
        {
            VkBufferCreateInfo uniformBufferInfo    = {};
            uniformBufferInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            uniformBufferInfo.flags                 = 0;
            uniformBufferInfo.size                  = 1;
            uniformBufferInfo.usage                 = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            uniformBufferInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
            uniformBufferInfo.queueFamilyIndexCount = 0;
            uniformBufferInfo.pQueueFamilyIndices   = nullptr;

            constexpr VkMemoryPropertyFlags kMemoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            ANGLE_TRY(mEmptyUniformBlockStorage.init(contextVk, uniformBufferInfo, kMemoryType));
        }
    }

    return angle::Result::Continue;
}

GLboolean ProgramVk::validate(const gl::Caps &caps, gl::InfoLog *infoLog)
{
    // No-op. The spec is very vague about the behavior of validation.
    return GL_TRUE;
}

template <typename T>
void ProgramVk::setUniformImpl(GLint location, GLsizei count, const T *v, GLenum entryPointType)
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    if (linkedUniform.isSampler())
    {
        // We could potentially cache some indexing here. For now this is a no-op since the mapping
        // is handled entirely in ContextVk.
        return;
    }

    if (linkedUniform.typeInfo->type == entryPointType)
    {
        for (gl::ShaderType shaderType : gl::AllGLES2ShaderTypes())
        {
            DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.typeInfo->componentCount;
            UpdateDefaultUniformBlock(count, locationInfo.arrayIndex, componentCount, v, layoutInfo,
                                      &uniformBlock.uniformData);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
    else
    {
        for (gl::ShaderType shaderType : gl::AllGLES2ShaderTypes())
        {
            DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
            const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

            // Assume an offset of -1 means the block is unused.
            if (layoutInfo.offset == -1)
            {
                continue;
            }

            const GLint componentCount = linkedUniform.typeInfo->componentCount;

            ASSERT(linkedUniform.typeInfo->type == gl::VariableBoolVectorType(entryPointType));

            GLint initialArrayOffset =
                locationInfo.arrayIndex * layoutInfo.arrayStride + layoutInfo.offset;
            for (GLint i = 0; i < count; i++)
            {
                GLint elementOffset = i * layoutInfo.arrayStride + initialArrayOffset;
                GLint *dest =
                    reinterpret_cast<GLint *>(uniformBlock.uniformData.data() + elementOffset);
                const T *source = v + i * componentCount;

                for (int c = 0; c < componentCount; c++)
                {
                    dest[c] = (source[c] == static_cast<T>(0)) ? GL_FALSE : GL_TRUE;
                }
            }

            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
}

template <typename T>
void ProgramVk::getUniformImpl(GLint location, T *v, GLenum entryPointType) const
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    ASSERT(!linkedUniform.isSampler());

    const gl::ShaderType shaderType = linkedUniform.getFirstShaderTypeWhereActive();
    ASSERT(shaderType != gl::ShaderType::InvalidEnum);

    const DefaultUniformBlock &uniformBlock = mDefaultUniformBlocks[shaderType];
    const sh::BlockMemberInfo &layoutInfo   = uniformBlock.uniformLayout[location];

    ASSERT(linkedUniform.typeInfo->componentType == entryPointType ||
           linkedUniform.typeInfo->componentType == gl::VariableBoolVectorType(entryPointType));

    if (gl::IsMatrixType(linkedUniform.type))
    {
        const uint8_t *ptrToElement = uniformBlock.uniformData.data() + layoutInfo.offset +
                                      (locationInfo.arrayIndex * layoutInfo.arrayStride);
        GetMatrixUniform(linkedUniform.type, v, reinterpret_cast<const T *>(ptrToElement), false);
    }
    else
    {
        ReadFromDefaultUniformBlock(linkedUniform.typeInfo->componentCount, locationInfo.arrayIndex,
                                    v, layoutInfo, &uniformBlock.uniformData);
    }
}

void ProgramVk::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT);
}

void ProgramVk::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC2);
}

void ProgramVk::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC3);
}

void ProgramVk::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    setUniformImpl(location, count, v, GL_FLOAT_VEC4);
}

void ProgramVk::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT);
}

void ProgramVk::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC2);
}

void ProgramVk::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC3);
}

void ProgramVk::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    setUniformImpl(location, count, v, GL_INT_VEC4);
}

void ProgramVk::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT);
}

void ProgramVk::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC2);
}

void ProgramVk::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC3);
}

void ProgramVk::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    setUniformImpl(location, count, v, GL_UNSIGNED_INT_VEC4);
}

template <int cols, int rows>
void ProgramVk::setUniformMatrixfv(GLint location,
                                   GLsizei count,
                                   GLboolean transpose,
                                   const GLfloat *value)
{
    const gl::VariableLocation &locationInfo = mState.getUniformLocations()[location];
    const gl::LinkedUniform &linkedUniform   = mState.getUniforms()[locationInfo.index];

    for (gl::ShaderType shaderType : gl::AllGLES2ShaderTypes())
    {
        DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
        const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

        // Assume an offset of -1 means the block is unused.
        if (layoutInfo.offset == -1)
        {
            continue;
        }

        bool updated = SetFloatUniformMatrixGLSL<cols, rows>(
            locationInfo.arrayIndex, linkedUniform.getArraySizeProduct(), count, transpose, value,
            uniformBlock.uniformData.data() + layoutInfo.offset);

        // If the uniformsDirty flag was true, we don't want to flip it to false here if the
        // setter did not update any data. We still want the uniform to be included when we'll
        // update the descriptor sets.
        if (updated)
        {
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }
}

void ProgramVk::setUniformMatrix2fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    setUniformMatrixfv<2, 2>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix3fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    setUniformMatrixfv<3, 3>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix4fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    setUniformMatrixfv<4, 4>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix2x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<2, 3>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix3x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<3, 2>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix2x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<2, 4>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix4x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<4, 2>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix3x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<3, 4>(location, count, transpose, value);
}

void ProgramVk::setUniformMatrix4x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    setUniformMatrixfv<4, 3>(location, count, transpose, value);
}

void ProgramVk::setPathFragmentInputGen(const std::string &inputName,
                                        GLenum genMode,
                                        GLint components,
                                        const GLfloat *coeffs)
{
    UNIMPLEMENTED();
}

angle::Result ProgramVk::allocateDescriptorSet(ContextVk *contextVk, uint32_t descriptorSetIndex)
{
    // Write out to a new a descriptor set.
    vk::DynamicDescriptorPool *dynamicDescriptorPool =
        contextVk->getDynamicDescriptorPool(descriptorSetIndex);

    uint32_t potentialNewCount = descriptorSetIndex + 1;
    if (potentialNewCount > mDescriptorSets.size())
    {
        mDescriptorSets.resize(potentialNewCount, VK_NULL_HANDLE);
    }

    const vk::DescriptorSetLayout &descriptorSetLayout =
        mDescriptorSetLayouts[descriptorSetIndex].get();
    ANGLE_TRY(dynamicDescriptorPool->allocateSets(contextVk, descriptorSetLayout.ptr(), 1,
                                                  &mDescriptorPoolBindings[descriptorSetIndex],
                                                  &mDescriptorSets[descriptorSetIndex]));
    mEmptyDescriptorSets[descriptorSetIndex] = VK_NULL_HANDLE;

    return angle::Result::Continue;
}

void ProgramVk::getUniformfv(const gl::Context *context, GLint location, GLfloat *params) const
{
    getUniformImpl(location, params, GL_FLOAT);
}

void ProgramVk::getUniformiv(const gl::Context *context, GLint location, GLint *params) const
{
    getUniformImpl(location, params, GL_INT);
}

void ProgramVk::getUniformuiv(const gl::Context *context, GLint location, GLuint *params) const
{
    getUniformImpl(location, params, GL_UNSIGNED_INT);
}

angle::Result ProgramVk::updateUniforms(ContextVk *contextVk)
{
    ASSERT(dirtyUniforms());

    // Update buffer memory by immediate mapping. This immediate update only works once.
    bool anyNewBufferAllocated = false;
    for (gl::ShaderType shaderType : gl::AllGLES2ShaderTypes())
    {
        DefaultUniformBlock &uniformBlock = mDefaultUniformBlocks[shaderType];

        if (mDefaultUniformBlocksDirty[shaderType])
        {
            bool bufferModified = false;
            ANGLE_TRY(SyncDefaultUniformBlock(contextVk, &uniformBlock.storage,
                                              uniformBlock.uniformData,
                                              &mUniformBlocksOffsets[shaderType], &bufferModified));
            mDefaultUniformBlocksDirty.reset(shaderType);

            if (bufferModified)
            {
                anyNewBufferAllocated = true;
            }
        }
    }

    if (anyNewBufferAllocated)
    {
        // We need to reinitialize the descriptor sets if we newly allocated buffers since we can't
        // modify the descriptor sets once initialized.
        ANGLE_TRY(allocateDescriptorSet(contextVk, kUniformsDescriptorSetIndex));
        ANGLE_TRY(updateDefaultUniformsDescriptorSet(contextVk));
    }

    return angle::Result::Continue;
}

angle::Result ProgramVk::updateDefaultUniformsDescriptorSet(ContextVk *contextVk)
{
    gl::ShaderMap<VkDescriptorBufferInfo> descriptorBufferInfo;
    gl::ShaderMap<VkWriteDescriptorSet> writeDescriptorInfo;

    // Write default uniforms for each shader type.
    for (gl::ShaderType shaderType : gl::AllGLES2ShaderTypes())
    {
        DefaultUniformBlock &uniformBlock  = mDefaultUniformBlocks[shaderType];
        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[shaderType];
        VkWriteDescriptorSet &writeInfo    = writeDescriptorInfo[shaderType];

        if (!uniformBlock.uniformData.empty())
        {
            const vk::BufferHelper *bufferHelper = uniformBlock.storage.getCurrentBuffer();
            bufferInfo.buffer                    = bufferHelper->getBuffer().getHandle();
        }
        else
        {
            bufferInfo.buffer = mEmptyUniformBlockStorage.getBuffer().getHandle();
        }

        bufferInfo.offset = 0;
        bufferInfo.range  = VK_WHOLE_SIZE;

        writeInfo.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeInfo.pNext            = nullptr;
        writeInfo.dstSet           = mDescriptorSets[kUniformsDescriptorSetIndex];
        writeInfo.dstBinding       = static_cast<uint32_t>(shaderType);
        writeInfo.dstArrayElement  = 0;
        writeInfo.descriptorCount  = 1;
        writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        writeInfo.pImageInfo       = nullptr;
        writeInfo.pBufferInfo      = &bufferInfo;
        writeInfo.pTexelBufferView = nullptr;
    }

    VkDevice device = contextVk->getDevice();

    constexpr uint32_t kShaderTypeMin    = static_cast<uint32_t>(gl::kGLES2ShaderTypeMin);
    constexpr uint32_t kShaderTypeMax    = static_cast<uint32_t>(gl::kGLES2ShaderTypeMax);
    constexpr uint32_t kGLES2ShaderCount = kShaderTypeMax - kShaderTypeMin + 1;
    vkUpdateDescriptorSets(device, kGLES2ShaderCount, writeDescriptorInfo.data(), 0, nullptr);

    return angle::Result::Continue;
}

angle::Result ProgramVk::updateUniformBuffersDescriptorSet(ContextVk *contextVk,
                                                           vk::FramebufferHelper *framebuffer)
{
    ASSERT(hasUniformBuffers());
    ANGLE_TRY(allocateDescriptorSet(contextVk, kUniformBlockDescriptorSetIndex));

    VkDescriptorSet descriptorSet = mDescriptorSets[kUniformBlockDescriptorSetIndex];

    gl::UniformBuffersArray<VkDescriptorBufferInfo> descriptorBufferInfo;
    gl::UniformBuffersArray<VkWriteDescriptorSet> writeDescriptorInfo;
    uint32_t writeCount     = 0;
    uint32_t currentBinding = 0;

    // Write uniform buffers.
    const gl::State &glState                             = contextVk->getState();
    const std::vector<gl::InterfaceBlock> &uniformBlocks = mState.getUniformBlocks();
    for (uint32_t bufferIndex = 0; bufferIndex < uniformBlocks.size(); ++bufferIndex)
    {
        if (glState.getIndexedUniformBuffer(uniformBlocks[bufferIndex].binding).get() == nullptr)
        {
            continue;
        }

        VkWriteDescriptorSet &writeInfo    = writeDescriptorInfo[writeCount];
        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[writeCount];

        const gl::InterfaceBlock &uniformBlock = uniformBlocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedUniformBuffer(uniformBlock.binding);
        gl::Buffer *buffer = bufferBinding.get();
        ASSERT(buffer != nullptr);

        // Make sure there's no possible under/overflow with binding size.
        static_assert(sizeof(VkDeviceSize) >= sizeof(bufferBinding.getSize()),
                      "VkDeviceSize too small");
        ASSERT(bufferBinding.getSize() >= 0);

        BufferVk *bufferVk             = vk::GetImpl(buffer);
        GLintptr offset                = bufferBinding.getOffset();
        VkDeviceSize size              = bufferBinding.getSize();
        VkDeviceSize blockSize         = uniformBlock.dataSize;
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        ANGLE_TRY(bufferVk->onRead(contextVk, framebuffer, VK_ACCESS_UNIFORM_READ_BIT));

        // If size is 0, we can't always use VK_WHOLE_SIZE (or bufferHelper.getSize()), as the
        // backing buffer may be larger than maxUniformBufferRange.  In that case, we use the
        // minimum of the backing buffer size (what's left after offset) and the uniform buffer
        // size as defined by the shader.
        size = std::min(size > 0 ? size : (bufferHelper.getSize() - offset), blockSize);

        bufferInfo.buffer = bufferHelper.getBuffer().getHandle();
        bufferInfo.offset = offset;
        bufferInfo.range  = size;

        if (!uniformBlock.isArray || uniformBlock.arrayElement == 0)
        {
            // Array indices of the same buffer binding are placed sequentially in `uniformBlocks`.
            // Thus, the uniform block binding is updated only when array index 0 is encountered.
            currentBinding = bufferIndex;
        }

        writeInfo.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeInfo.pNext            = nullptr;
        writeInfo.dstSet           = descriptorSet;
        writeInfo.dstBinding       = currentBinding;
        writeInfo.dstArrayElement  = uniformBlock.isArray ? uniformBlock.arrayElement : 0;
        writeInfo.descriptorCount  = 1;
        writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeInfo.pImageInfo       = nullptr;
        writeInfo.pBufferInfo      = &bufferInfo;
        writeInfo.pTexelBufferView = nullptr;
        ASSERT(writeInfo.pBufferInfo[0].buffer != VK_NULL_HANDLE);

        ++writeCount;
    }

    VkDevice device = contextVk->getDevice();

    vkUpdateDescriptorSets(device, writeCount, writeDescriptorInfo.data(), 0, nullptr);

    return angle::Result::Continue;
}

angle::Result ProgramVk::updateTexturesDescriptorSet(ContextVk *contextVk,
                                                     vk::FramebufferHelper *framebuffer)
{
    ASSERT(hasTextures());
    ANGLE_TRY(allocateDescriptorSet(contextVk, kTextureDescriptorSetIndex));

    VkDescriptorSet descriptorSet = mDescriptorSets[kTextureDescriptorSetIndex];

    gl::ActiveTextureArray<VkDescriptorImageInfo> descriptorImageInfo;
    gl::ActiveTextureArray<VkWriteDescriptorSet> writeDescriptorInfo;
    uint32_t writeCount = 0;

    const gl::ActiveTextureArray<TextureVk *> &activeTextures = contextVk->getActiveTextures();

    for (uint32_t textureIndex = 0; textureIndex < mState.getSamplerBindings().size();
         ++textureIndex)
    {
        const gl::SamplerBinding &samplerBinding = mState.getSamplerBindings()[textureIndex];

        ASSERT(!samplerBinding.unreferenced);

        for (uint32_t arrayElement = 0; arrayElement < samplerBinding.boundTextureUnits.size();
             ++arrayElement)
        {
            GLuint textureUnit   = samplerBinding.boundTextureUnits[arrayElement];
            TextureVk *textureVk = activeTextures[textureUnit];

            // Ensure any writes to the textures are flushed before we read from them.
            ANGLE_TRY(textureVk->ensureImageInitialized(contextVk));
            vk::ImageHelper &image = textureVk->getImage();

            // Ensure the image is in read-only layout
            if (image.isLayoutChangeNecessary(vk::ImageLayout::FragmentShaderReadOnly))
            {
                vk::CommandBuffer *srcLayoutChange;
                ANGLE_TRY(image.recordCommands(contextVk, &srcLayoutChange));

                VkImageAspectFlags aspectFlags = image.getAspectFlags();
                ASSERT(aspectFlags != 0);
                image.changeLayout(aspectFlags, vk::ImageLayout::FragmentShaderReadOnly,
                                   srcLayoutChange);
            }

            image.addReadDependency(framebuffer);

            VkDescriptorImageInfo &imageInfo = descriptorImageInfo[writeCount];

            imageInfo.sampler     = textureVk->getSampler().getHandle();
            imageInfo.imageView   = textureVk->getReadImageView().getHandle();
            imageInfo.imageLayout = image.getCurrentLayout();

            VkWriteDescriptorSet &writeInfo = writeDescriptorInfo[writeCount];

            writeInfo.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.pNext            = nullptr;
            writeInfo.dstSet           = descriptorSet;
            writeInfo.dstBinding       = textureIndex;
            writeInfo.dstArrayElement  = arrayElement;
            writeInfo.descriptorCount  = 1;
            writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeInfo.pImageInfo       = &imageInfo;
            writeInfo.pBufferInfo      = nullptr;
            writeInfo.pTexelBufferView = nullptr;

            writeCount++;
        }
    }

    VkDevice device = contextVk->getDevice();

    ASSERT(writeCount > 0);
    vkUpdateDescriptorSets(device, writeCount, writeDescriptorInfo.data(), 0, nullptr);

    return angle::Result::Continue;
}

void ProgramVk::setDefaultUniformBlocksMinSizeForTesting(size_t minSize)
{
    for (DefaultUniformBlock &block : mDefaultUniformBlocks)
    {
        block.storage.setMinimumSizeForTesting(minSize);
    }
}

angle::Result ProgramVk::updateDescriptorSets(ContextVk *contextVk,
                                              vk::CommandBuffer *commandBuffer)
{
    // Can probably use better dirty bits here.

    if (mDescriptorSets.empty())
        return angle::Result::Continue;

    // Find the maximum non-null descriptor set.  This is used in conjunction with a driver
    // workaround to bind empty descriptor sets only for gaps in between 0 and max and avoid
    // binding unnecessary empty descriptor sets for the sets beyond max.
    size_t descriptorSetRange = 0;
    for (size_t descriptorSetIndex = 0; descriptorSetIndex < mDescriptorSets.size();
         ++descriptorSetIndex)
    {
        if (mDescriptorSets[descriptorSetIndex] != VK_NULL_HANDLE)
        {
            descriptorSetRange = descriptorSetIndex + 1;
        }
    }

    for (size_t descriptorSetIndex = 0; descriptorSetIndex < descriptorSetRange;
         ++descriptorSetIndex)
    {
        VkDescriptorSet descSet = mDescriptorSets[descriptorSetIndex];
        if (descSet == VK_NULL_HANDLE)
        {
            if (!contextVk->getRenderer()->getFeatures().bindEmptyForUnusedDescriptorSets.enabled)
            {
                continue;
            }

            // Workaround a driver bug where missing (though unused) descriptor sets indices cause
            // later sets to misbehave.
            if (mEmptyDescriptorSets[descriptorSetIndex] == VK_NULL_HANDLE)
            {
                vk::DynamicDescriptorPool *dynamicDescriptorPool =
                    contextVk->getDynamicDescriptorPool(descriptorSetIndex);
                const vk::DescriptorSetLayout &descriptorSetLayout =
                    mDescriptorSetLayouts[descriptorSetIndex].get();

                ANGLE_TRY(dynamicDescriptorPool->allocateSets(
                    contextVk, descriptorSetLayout.ptr(), 1,
                    &mDescriptorPoolBindings[descriptorSetIndex],
                    &mEmptyDescriptorSets[descriptorSetIndex]));
            }
            descSet = mEmptyDescriptorSets[descriptorSetIndex];
        }

        constexpr uint32_t kShaderTypeMin   = static_cast<uint32_t>(gl::kGLES2ShaderTypeMin);
        constexpr uint32_t kShaderTypeMax   = static_cast<uint32_t>(gl::kGLES2ShaderTypeMax);
        constexpr uint32_t kShaderTypeCount = kShaderTypeMax - kShaderTypeMin + 1;

        // Default uniforms are encompassed in a block per shader stage, and they are assigned
        // through dynamic uniform buffers (requiring dynamic offsets).  No other descriptor
        // requires a dynamic offset.
        const uint32_t uniformBlockOffsetCount =
            descriptorSetIndex == kUniformsDescriptorSetIndex ? kShaderTypeCount : 0;

        commandBuffer->bindGraphicsDescriptorSets(mPipelineLayout.get(), descriptorSetIndex, 1,
                                                  &descSet, uniformBlockOffsetCount,
                                                  mUniformBlocksOffsets.data() + kShaderTypeMin);
    }

    return angle::Result::Continue;
}
}  // namespace rx

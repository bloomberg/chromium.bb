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
#include "libANGLE/renderer/vulkan/GlslangWrapper.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"

namespace rx
{

namespace
{

constexpr size_t kUniformBlockDynamicBufferMinSize = 256 * 128;

void InitDefaultUniformBlock(const std::vector<sh::Uniform> &uniforms,
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
    dynamicBuffer->releaseInFlightBuffers(contextVk);

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

uint32_t GetInterfaceBlockArraySize(const std::vector<gl::InterfaceBlock> &blocks,
                                    uint32_t bufferIndex)
{
    const gl::InterfaceBlock &block = blocks[bufferIndex];

    if (!block.isArray)
    {
        return 1;
    }

    ASSERT(block.arrayElement == 0);

    // Search consecutively until all array indices of this block are visited.
    uint32_t arraySize;
    for (arraySize = 1; bufferIndex + arraySize < blocks.size(); ++arraySize)
    {
        const gl::InterfaceBlock &nextBlock = blocks[bufferIndex + arraySize];

        if (nextBlock.arrayElement != arraySize)
        {
            break;
        }

        // It's unexpected for an array to start at a non-zero array size, so we can always rely on
        // the sequential `arrayElement`s to belong to the same block.
        ASSERT(nextBlock.name == block.name);
        ASSERT(nextBlock.isArray);
    }

    return arraySize;
}

void AddInterfaceBlockDescriptorSetDesc(const std::vector<gl::InterfaceBlock> &blocks,
                                        uint32_t bindingStart,
                                        VkDescriptorType descType,
                                        vk::DescriptorSetLayoutDesc *descOut)
{
    uint32_t bindingIndex = 0;
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size();)
    {
        const uint32_t arraySize = GetInterfaceBlockArraySize(blocks, bufferIndex);
        VkShaderStageFlags activeStages =
            gl_vk::GetShaderStageFlags(blocks[bufferIndex].activeShaders());

        descOut->update(bindingStart + bindingIndex, descType, arraySize, activeStages);

        bufferIndex += arraySize;
        ++bindingIndex;
    }
}

void AddAtomicCounterBufferDescriptorSetDesc(
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
    uint32_t bindingStart,
    vk::DescriptorSetLayoutDesc *descOut)
{
    uint32_t bindingIndex = 0;
    for (uint32_t bufferIndex = 0; bufferIndex < atomicCounterBuffers.size(); ++bufferIndex)
    {
        VkShaderStageFlags activeStages =
            gl_vk::GetShaderStageFlags(atomicCounterBuffers[bufferIndex].activeShaders());

        descOut->update(bindingStart + bindingIndex, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                        activeStages);
    }
}

void WriteBufferDescriptorSetBinding(const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding,
                                     VkDeviceSize maxSize,
                                     VkDescriptorSet descSet,
                                     VkDescriptorType descType,
                                     uint32_t bindingIndex,
                                     uint32_t arrayElement,
                                     VkDescriptorBufferInfo *bufferInfoOut,
                                     VkWriteDescriptorSet *writeInfoOut)
{
    gl::Buffer *buffer = bufferBinding.get();
    ASSERT(buffer != nullptr);

    // Make sure there's no possible under/overflow with binding size.
    static_assert(sizeof(VkDeviceSize) >= sizeof(bufferBinding.getSize()),
                  "VkDeviceSize too small");
    ASSERT(bufferBinding.getSize() >= 0);

    BufferVk *bufferVk             = vk::GetImpl(buffer);
    GLintptr offset                = bufferBinding.getOffset();
    VkDeviceSize size              = bufferBinding.getSize();
    vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

    // If size is 0, we can't always use VK_WHOLE_SIZE (or bufferHelper.getSize()), as the
    // backing buffer may be larger than max*BufferRange.  In that case, we use the minimum of
    // the backing buffer size (what's left after offset) and the buffer size as defined by the
    // shader.  That latter is only valid for UBOs, as SSBOs may have variable length arrays.
    size = size > 0 ? size : (bufferHelper.getSize() - offset);
    if (maxSize > 0)
    {
        size = std::min(size, maxSize);
    }

    bufferInfoOut->buffer = bufferHelper.getBuffer().getHandle();
    bufferInfoOut->offset = offset;
    bufferInfoOut->range  = size;

    writeInfoOut->sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfoOut->pNext            = nullptr;
    writeInfoOut->dstSet           = descSet;
    writeInfoOut->dstBinding       = bindingIndex;
    writeInfoOut->dstArrayElement  = arrayElement;
    writeInfoOut->descriptorCount  = 1;
    writeInfoOut->descriptorType   = descType;
    writeInfoOut->pImageInfo       = nullptr;
    writeInfoOut->pBufferInfo      = bufferInfoOut;
    writeInfoOut->pTexelBufferView = nullptr;
    ASSERT(writeInfoOut->pBufferInfo[0].buffer != VK_NULL_HANDLE);
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
                                                 const gl::ShaderMap<std::string> &shaderSources,
                                                 bool enableLineRasterEmulation)
{
    ASSERT(!valid());

    gl::ShaderMap<std::vector<uint32_t>> shaderCodes;
    ANGLE_TRY(GlslangWrapper::GetShaderCode(
        contextVk, contextVk->getCaps(), enableLineRasterEmulation, shaderSources, &shaderCodes));

    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (!shaderSources[shaderType].empty())
        {
            ANGLE_TRY(vk::InitShaderAndSerial(contextVk, &mShaders[shaderType].get(),
                                              shaderCodes[shaderType].data(),
                                              shaderCodes[shaderType].size() * sizeof(uint32_t)));

            mProgramHelper.setShader(shaderType, &mShaders[shaderType]);
        }
    }

    return angle::Result::Continue;
}

angle::Result ProgramVk::loadShaderSource(ContextVk *contextVk, gl::BinaryInputStream *stream)
{
    // Read in shader sources for all shader types
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mShaderSources[shaderType] = stream->readString();
    }

    return angle::Result::Continue;
}

void ProgramVk::saveShaderSource(gl::BinaryOutputStream *stream)
{
    // Write out shader sources for all shader types
    for (const gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeString(mShaderSources[shaderType]);
    }
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
ProgramVk::DefaultUniformBlock::DefaultUniformBlock() {}

ProgramVk::DefaultUniformBlock::~DefaultUniformBlock() = default;

ProgramVk::ProgramVk(const gl::ProgramState &state)
    : ProgramImpl(state),
      mDynamicBufferOffsets{},
      mStorageBlockBindingsOffset(0),
      mAtomicCounterBufferBindingsOffset(0)
{}

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

    for (vk::DynamicDescriptorPool &descriptorPool : mDynamicDescriptorPools)
    {
        descriptorPool.release(contextVk);
    }

    mTextureDescriptorsCache.clear();
}

std::unique_ptr<rx::LinkEvent> ProgramVk::load(const gl::Context *context,
                                               gl::BinaryInputStream *stream,
                                               gl::InfoLog &infoLog)
{
    ContextVk *contextVk = vk::GetImpl(context);
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);

    angle::Result status = loadShaderSource(contextVk, stream);
    if (status != angle::Result::Continue)
    {
        return std::make_unique<LinkEventDone>(status);
    }

    // Deserializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const size_t uniformCount = stream->readInt<size_t>();
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
        {
            sh::BlockMemberInfo blockInfo;
            gl::LoadBlockMemberInfo(stream, &blockInfo);
            mDefaultUniformBlocks[shaderType].uniformLayout.push_back(blockInfo);
        }
    }

    // Deserializes required uniform block memory sizes
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        requiredBufferSize[shaderType] = stream->readInt<size_t>();
    }

    reset(contextVk);

    // Initialize and resize the mDefaultUniformBlocks' memory
    status = resizeUniformBlockMemory(contextVk, requiredBufferSize);
    if (status != angle::Result::Continue)
    {
        return std::make_unique<LinkEventDone>(status);
    }

    return std::make_unique<LinkEventDone>(linkImpl(context, infoLog));
}

void ProgramVk::save(const gl::Context *context, gl::BinaryOutputStream *stream)
{
    // (geofflang): Look into saving shader modules in ShaderInfo objects (keep in mind that we
    // compile shaders lazily)
    saveShaderSource(stream);

    // Serializes the uniformLayout data of mDefaultUniformBlocks
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const size_t uniformCount = mDefaultUniformBlocks[shaderType].uniformLayout.size();
        stream->writeInt<size_t>(uniformCount);
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
        {
            sh::BlockMemberInfo &blockInfo =
                mDefaultUniformBlocks[shaderType].uniformLayout[uniformIndex];
            gl::WriteBlockMemberInfo(stream, blockInfo);
        }
    }

    // Serializes required uniform block memory sizes
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        stream->writeInt(mDefaultUniformBlocks[shaderType].uniformData.size());
    }
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
    ContextVk *contextVk = vk::GetImpl(context);
    // Link resources before calling GetShaderSource to make sure they are ready for the set/binding
    // assignment done in that function.
    linkResources(resources);

    GlslangWrapper::GetShaderSource(mState, resources, &mShaderSources);

    reset(contextVk);

    angle::Result status = initDefaultUniformBlocks(context);
    if (status != angle::Result::Continue)
    {
        return std::make_unique<LinkEventDone>(status);
    }

    // TODO(jie.a.chen@intel.com): Parallelize linking.
    // http://crbug.com/849576
    return std::make_unique<LinkEventDone>(linkImpl(context, infoLog));
}

angle::Result ProgramVk::linkImpl(const gl::Context *glContext, gl::InfoLog &infoLog)
{
    const gl::State &glState                 = glContext->getState();
    ContextVk *contextVk                     = vk::GetImpl(glContext);
    RendererVk *renderer                     = contextVk->getRenderer();
    gl::TransformFeedback *transformFeedback = glState.getCurrentTransformFeedback();

    updateBindingOffsets();

    // Store a reference to the pipeline and descriptor set layouts. This will create them if they
    // don't already exist in the cache.

    // Default uniforms and transform feedback:
    vk::DescriptorSetLayoutDesc uniformsAndXfbSetDesc;
    uint32_t uniformBindingIndex = 0;
    for (const gl::ShaderType shaderType : mState.getLinkedShaderStages())
    {
        uniformsAndXfbSetDesc.update(uniformBindingIndex++,
                                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                                     gl_vk::kShaderStageMap[shaderType]);
    }
    if (mState.hasLinkedShaderStage(gl::ShaderType::Vertex) && transformFeedback &&
        !mState.getLinkedTransformFeedbackVaryings().empty())
    {
        vk::GetImpl(transformFeedback)->updateDescriptorSetLayout(mState, &uniformsAndXfbSetDesc);
    }

    ANGLE_TRY(renderer->getDescriptorSetLayout(
        contextVk, uniformsAndXfbSetDesc,
        &mDescriptorSetLayouts[kUniformsAndXfbDescriptorSetIndex]));

    // Uniform and storage buffers:
    vk::DescriptorSetLayoutDesc buffersSetDesc;

    AddInterfaceBlockDescriptorSetDesc(mState.getUniformBlocks(), getUniformBlockBindingsOffset(),
                                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffersSetDesc);
    AddInterfaceBlockDescriptorSetDesc(mState.getShaderStorageBlocks(),
                                       getStorageBlockBindingsOffset(),
                                       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &buffersSetDesc);
    AddAtomicCounterBufferDescriptorSetDesc(
        mState.getAtomicCounterBuffers(), getAtomicCounterBufferBindingsOffset(), &buffersSetDesc);

    ANGLE_TRY(renderer->getDescriptorSetLayout(
        contextVk, buffersSetDesc, &mDescriptorSetLayouts[kShaderResourceDescriptorSetIndex]));

    // Textures:
    vk::DescriptorSetLayoutDesc texturesSetDesc;

    for (uint32_t textureIndex = 0; textureIndex < mState.getSamplerBindings().size();
         ++textureIndex)
    {
        const gl::SamplerBinding &samplerBinding = mState.getSamplerBindings()[textureIndex];

        uint32_t uniformIndex = mState.getUniformIndexFromSamplerIndex(textureIndex);
        const gl::LinkedUniform &samplerUniform = mState.getUniforms()[uniformIndex];

        // The front-end always binds array sampler units sequentially.
        const uint32_t arraySize = static_cast<uint32_t>(samplerBinding.boundTextureUnits.size());
        VkShaderStageFlags activeStages =
            gl_vk::GetShaderStageFlags(samplerUniform.activeShaders());

        texturesSetDesc.update(textureIndex, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arraySize,
                               activeStages);
    }

    ANGLE_TRY(renderer->getDescriptorSetLayout(contextVk, texturesSetDesc,
                                               &mDescriptorSetLayouts[kTextureDescriptorSetIndex]));

    vk::DescriptorSetLayoutDesc driverUniformsSetDesc =
        contextVk->getDriverUniformsDescriptorSetDesc();
    ANGLE_TRY(renderer->getDescriptorSetLayout(
        contextVk, driverUniformsSetDesc,
        &mDescriptorSetLayouts[kDriverUniformsDescriptorSetIndex]));

    vk::PipelineLayoutDesc pipelineLayoutDesc;
    pipelineLayoutDesc.updateDescriptorSetLayout(kUniformsAndXfbDescriptorSetIndex,
                                                 uniformsAndXfbSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(kShaderResourceDescriptorSetIndex, buffersSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(kTextureDescriptorSetIndex, texturesSetDesc);
    pipelineLayoutDesc.updateDescriptorSetLayout(kDriverUniformsDescriptorSetIndex,
                                                 driverUniformsSetDesc);

    ANGLE_TRY(renderer->getPipelineLayout(contextVk, pipelineLayoutDesc, mDescriptorSetLayouts,
                                          &mPipelineLayout));

    std::array<VkDescriptorPoolSize, 2> uniformAndXfbSetSize = {
        {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
          static_cast<uint32_t>(mState.getLinkedShaderStageCount())},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS}}};

    uint32_t uniformBlockCount = static_cast<uint32_t>(mState.getUniformBlocks().size());
    uint32_t storageBlockCount = static_cast<uint32_t>(mState.getShaderStorageBlocks().size());
    uint32_t atomicCounterBufferCount =
        static_cast<uint32_t>(mState.getAtomicCounterBuffers().size());
    uint32_t textureCount      = static_cast<uint32_t>(mState.getSamplerBindings().size());

    if (renderer->getFeatures().bindEmptyForUnusedDescriptorSets.enabled)
    {
        // For this workaround, we have to create an empty descriptor set for each descriptor set
        // index, so make sure their pools are initialized.
        uniformBlockCount = std::max(uniformBlockCount, 1u);
        textureCount      = std::max(textureCount, 1u);
    }

    angle::FixedVector<VkDescriptorPoolSize, 2> bufferSetSize;
    if (uniformBlockCount > 0)
    {
        bufferSetSize.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBlockCount});
    }
    if (storageBlockCount > 0 || atomicCounterBufferCount > 0)
    {
        bufferSetSize.push_back(
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storageBlockCount + atomicCounterBufferCount});
    }

    VkDescriptorPoolSize textureSetSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textureCount};

    ANGLE_TRY(mDynamicDescriptorPools[kUniformsAndXfbDescriptorSetIndex].init(
        contextVk, uniformAndXfbSetSize.data(), uniformAndXfbSetSize.size()));
    if (bufferSetSize.size() > 0)
    {
        ANGLE_TRY(mDynamicDescriptorPools[kShaderResourceDescriptorSetIndex].init(
            contextVk, bufferSetSize.data(), bufferSetSize.size()));
    }
    if (textureCount > 0)
    {
        ANGLE_TRY(mDynamicDescriptorPools[kTextureDescriptorSetIndex].init(contextVk,
                                                                           &textureSetSize, 1));
    }

    mDynamicBufferOffsets.resize(mState.getLinkedShaderStageCount());

    return angle::Result::Continue;
}

void ProgramVk::updateBindingOffsets()
{
    mStorageBlockBindingsOffset = mState.getUniqueUniformBlockCount();
    mAtomicCounterBufferBindingsOffset =
        mStorageBlockBindingsOffset + mState.getUniqueStorageBlockCount();
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

    // Process vertex and fragment uniforms into std140 packing.
    gl::ShaderMap<sh::BlockLayoutMap> layoutMap;
    gl::ShaderMap<size_t> requiredBufferSize;
    requiredBufferSize.fill(0);

    generateUniformLayoutMapping(layoutMap, requiredBufferSize);
    initDefaultUniformLayoutMapping(layoutMap);

    // All uniform initializations are complete, now resize the buffers accordingly and return
    return resizeUniformBlockMemory(contextVk, requiredBufferSize);
}

void ProgramVk::generateUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> &layoutMap,
                                             gl::ShaderMap<size_t> &requiredBufferSize)
{
    for (const gl::ShaderType shaderType : mState.getLinkedShaderStages())
    {
        gl::Shader *shader = mState.getAttachedShader(shaderType);

        if (shader)
        {
            const std::vector<sh::Uniform> &uniforms = shader->getUniforms();
            InitDefaultUniformBlock(uniforms, &layoutMap[shaderType],
                                    &requiredBufferSize[shaderType]);
        }
    }
}

void ProgramVk::initDefaultUniformLayoutMapping(gl::ShaderMap<sh::BlockLayoutMap> &layoutMap)
{
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

                for (const gl::ShaderType shaderType : mState.getLinkedShaderStages())
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

        for (const gl::ShaderType shaderType : mState.getLinkedShaderStages())
        {
            mDefaultUniformBlocks[shaderType].uniformLayout.push_back(layoutInfo[shaderType]);
        }
    }
}

angle::Result ProgramVk::resizeUniformBlockMemory(ContextVk *contextVk,
                                                  gl::ShaderMap<size_t> &requiredBufferSize)
{
    RendererVk *renderer = contextVk->getRenderer();
    for (const gl::ShaderType shaderType : mState.getLinkedShaderStages())
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

            mDefaultUniformBlocks[shaderType].storage.init(
                renderer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                minAlignment, kUniformBlockDynamicBufferMinSize, true);

            // Initialize uniform buffer memory to zero by default.
            mDefaultUniformBlocks[shaderType].uniformData.fill(0);
            mDefaultUniformBlocksDirty.set(shaderType);
        }
    }

    if (mDefaultUniformBlocksDirty.any() || mState.getTransformFeedbackBufferCount() > 0)
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
        for (const gl::ShaderType shaderType : mState.getLinkedShaderStages())
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
        for (const gl::ShaderType shaderType : mState.getLinkedShaderStages())
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

    for (const gl::ShaderType shaderType : mState.getLinkedShaderStages())
    {
        DefaultUniformBlock &uniformBlock     = mDefaultUniformBlocks[shaderType];
        const sh::BlockMemberInfo &layoutInfo = uniformBlock.uniformLayout[location];

        // Assume an offset of -1 means the block is unused.
        if (layoutInfo.offset == -1)
        {
            continue;
        }

        bool updated = SetFloatUniformMatrixGLSL<cols, rows>::Run(
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
    bool ignoreNewPoolAllocated;
    return allocateDescriptorSetAndGetInfo(contextVk, descriptorSetIndex, &ignoreNewPoolAllocated);
}

angle::Result ProgramVk::allocateDescriptorSetAndGetInfo(ContextVk *contextVk,
                                                         uint32_t descriptorSetIndex,
                                                         bool *newPoolAllocatedOut)
{
    vk::DynamicDescriptorPool &dynamicDescriptorPool = mDynamicDescriptorPools[descriptorSetIndex];

    uint32_t potentialNewCount = descriptorSetIndex + 1;
    if (potentialNewCount > mDescriptorSets.size())
    {
        mDescriptorSets.resize(potentialNewCount, VK_NULL_HANDLE);
    }

    const vk::DescriptorSetLayout &descriptorSetLayout =
        mDescriptorSetLayouts[descriptorSetIndex].get();
    ANGLE_TRY(dynamicDescriptorPool.allocateSetsAndGetInfo(
        contextVk, descriptorSetLayout.ptr(), 1, &mDescriptorPoolBindings[descriptorSetIndex],
        &mDescriptorSets[descriptorSetIndex], newPoolAllocatedOut));
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

    bool anyNewBufferAllocated = false;
    uint32_t offsetIndex       = 0;

    // Update buffer memory by immediate mapping. This immediate update only works once.
    for (gl::ShaderType shaderType : mState.getLinkedShaderStages())
    {
        DefaultUniformBlock &uniformBlock = mDefaultUniformBlocks[shaderType];

        if (mDefaultUniformBlocksDirty[shaderType])
        {
            bool bufferModified = false;
            ANGLE_TRY(
                SyncDefaultUniformBlock(contextVk, &uniformBlock.storage, uniformBlock.uniformData,
                                        &mDynamicBufferOffsets[offsetIndex], &bufferModified));
            mDefaultUniformBlocksDirty.reset(shaderType);

            if (bufferModified)
            {
                anyNewBufferAllocated = true;
            }
        }

        ++offsetIndex;
    }

    if (anyNewBufferAllocated)
    {
        // We need to reinitialize the descriptor sets if we newly allocated buffers since we can't
        // modify the descriptor sets once initialized.
        ANGLE_TRY(allocateDescriptorSet(contextVk, kUniformsAndXfbDescriptorSetIndex));
        updateDefaultUniformsDescriptorSet(contextVk);
        updateTransformFeedbackDescriptorSetImpl(contextVk);
    }

    return angle::Result::Continue;
}

void ProgramVk::updateDefaultUniformsDescriptorSet(ContextVk *contextVk)
{
    size_t shaderStageCount = mState.getLinkedShaderStageCount();

    gl::ShaderVector<VkDescriptorBufferInfo> descriptorBufferInfo(shaderStageCount);
    gl::ShaderVector<VkWriteDescriptorSet> writeDescriptorInfo(shaderStageCount);

    uint32_t bindingIndex = 0;

    // Write default uniforms for each shader type.
    for (const gl::ShaderType shaderType : mState.getLinkedShaderStages())
    {
        DefaultUniformBlock &uniformBlock  = mDefaultUniformBlocks[shaderType];
        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[bindingIndex];
        VkWriteDescriptorSet &writeInfo    = writeDescriptorInfo[bindingIndex];

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
        writeInfo.dstSet           = mDescriptorSets[kUniformsAndXfbDescriptorSetIndex];
        writeInfo.dstBinding       = bindingIndex;
        writeInfo.dstArrayElement  = 0;
        writeInfo.descriptorCount  = 1;
        writeInfo.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        writeInfo.pImageInfo       = nullptr;
        writeInfo.pBufferInfo      = &bufferInfo;
        writeInfo.pTexelBufferView = nullptr;

        ++bindingIndex;
    }

    VkDevice device = contextVk->getDevice();

    ASSERT(bindingIndex == shaderStageCount);
    ASSERT(shaderStageCount <= kReservedDefaultUniformBindingCount);

    vkUpdateDescriptorSets(device, shaderStageCount, writeDescriptorInfo.data(), 0, nullptr);
}

void ProgramVk::updateBuffersDescriptorSet(ContextVk *contextVk,
                                           vk::CommandGraphResource *recorder,
                                           const std::vector<gl::InterfaceBlock> &blocks,
                                           VkDescriptorType descriptorType)
{
    VkDescriptorSet descriptorSet = mDescriptorSets[kShaderResourceDescriptorSetIndex];

    ASSERT(descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
           descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const bool isStorageBuffer = descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const uint32_t bindingStart =
        isStorageBuffer ? getStorageBlockBindingsOffset() : getUniformBlockBindingsOffset();

    static_assert(
        gl::IMPLEMENTATION_MAX_SHADER_STORAGE_BUFFER_BINDINGS >=
            gl::IMPLEMENTATION_MAX_UNIFORM_BUFFER_BINDINGS,
        "The descriptor arrays here would have inadequate size for uniform buffer objects");

    gl::StorageBuffersArray<VkDescriptorBufferInfo> descriptorBufferInfo;
    gl::StorageBuffersArray<VkWriteDescriptorSet> writeDescriptorInfo;
    uint32_t writeCount = 0;
    // The binding is incremented every time arrayElement 0 is encountered, which means there will
    // be an increment right at the start.  Start from -1 to get 0 as the first binding.
    int32_t currentBinding = -1;

    // Write uniform or storage buffers.
    const gl::State &glState = contextVk->getState();
    for (uint32_t bufferIndex = 0; bufferIndex < blocks.size(); ++bufferIndex)
    {
        const gl::InterfaceBlock &block = blocks[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            isStorageBuffer ? glState.getIndexedShaderStorageBuffer(block.binding)
                            : glState.getIndexedUniformBuffer(block.binding);

        if (!block.isArray || block.arrayElement == 0)
        {
            // Array indices of the same buffer binding are placed sequentially in `blocks`.
            // Thus, the block binding is updated only when array index 0 is encountered.
            ++currentBinding;
        }

        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        uint32_t binding          = bindingStart + currentBinding;
        uint32_t arrayElement     = block.isArray ? block.arrayElement : 0;
        VkDeviceSize maxBlockSize = isStorageBuffer ? 0 : block.dataSize;

        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[writeCount];
        VkWriteDescriptorSet &writeInfo    = writeDescriptorInfo[writeCount];

        WriteBufferDescriptorSetBinding(bufferBinding, maxBlockSize, descriptorSet, descriptorType,
                                        binding, arrayElement, &bufferInfo, &writeInfo);

        BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        if (isStorageBuffer)
        {
            bufferHelper.onWrite(contextVk, recorder, VK_ACCESS_SHADER_READ_BIT,
                                 VK_ACCESS_SHADER_WRITE_BIT);
        }
        else
        {
            bufferHelper.onRead(recorder, VK_ACCESS_UNIFORM_READ_BIT);
        }

        ++writeCount;
    }

    VkDevice device = contextVk->getDevice();

    vkUpdateDescriptorSets(device, writeCount, writeDescriptorInfo.data(), 0, nullptr);
}

void ProgramVk::updateAtomicCounterBuffersDescriptorSet(ContextVk *contextVk,
                                                        vk::CommandGraphResource *recorder)
{
    VkDescriptorSet descriptorSet = mDescriptorSets[kShaderResourceDescriptorSetIndex];

    const uint32_t bindingStart = getAtomicCounterBufferBindingsOffset();

    gl::AtomicCounterBuffersArray<VkDescriptorBufferInfo> descriptorBufferInfo;
    gl::AtomicCounterBuffersArray<VkWriteDescriptorSet> writeDescriptorInfo;
    uint32_t writeCount = 0;

    // Write atomic counter buffers.
    const gl::State &glState = contextVk->getState();
    const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers =
        mState.getAtomicCounterBuffers();

    for (uint32_t bufferIndex = 0; bufferIndex < atomicCounterBuffers.size(); ++bufferIndex)
    {
        const gl::AtomicCounterBuffer &atomicCounterBuffer = atomicCounterBuffers[bufferIndex];
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding =
            glState.getIndexedAtomicCounterBuffer(atomicCounterBuffer.binding);

        if (bufferBinding.get() == nullptr)
        {
            continue;
        }

        uint32_t binding = bindingStart + bufferIndex;

        VkDescriptorBufferInfo &bufferInfo = descriptorBufferInfo[writeCount];
        VkWriteDescriptorSet &writeInfo    = writeDescriptorInfo[writeCount];

        WriteBufferDescriptorSetBinding(bufferBinding, 0, descriptorSet,
                                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, binding, 0, &bufferInfo,
                                        &writeInfo);

        BufferVk *bufferVk             = vk::GetImpl(bufferBinding.get());
        vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

        bufferHelper.onWrite(contextVk, recorder, VK_ACCESS_SHADER_READ_BIT,
                             VK_ACCESS_SHADER_WRITE_BIT);

        ++writeCount;
    }

    VkDevice device = contextVk->getDevice();

    vkUpdateDescriptorSets(device, writeCount, writeDescriptorInfo.data(), 0, nullptr);
}

angle::Result ProgramVk::updateShaderResourcesDescriptorSet(ContextVk *contextVk,
                                                            vk::CommandGraphResource *recorder)
{
    ANGLE_TRY(allocateDescriptorSet(contextVk, kShaderResourceDescriptorSetIndex));

    updateBuffersDescriptorSet(contextVk, recorder, mState.getUniformBlocks(),
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    updateBuffersDescriptorSet(contextVk, recorder, mState.getShaderStorageBlocks(),
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    updateAtomicCounterBuffersDescriptorSet(contextVk, recorder);

    return angle::Result::Continue;
}

angle::Result ProgramVk::updateTransformFeedbackDescriptorSet(ContextVk *contextVk,
                                                              vk::FramebufferHelper *framebuffer)
{
    const gl::State &glState = contextVk->getState();
    ASSERT(hasTransformFeedbackOutput());

    TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(glState.getCurrentTransformFeedback());
    transformFeedbackVk->addFramebufferDependency(contextVk, mState, framebuffer);

    ANGLE_TRY(allocateDescriptorSet(contextVk, kUniformsAndXfbDescriptorSetIndex));

    updateDefaultUniformsDescriptorSet(contextVk);
    updateTransformFeedbackDescriptorSetImpl(contextVk);

    return angle::Result::Continue;
}

void ProgramVk::updateTransformFeedbackDescriptorSetImpl(ContextVk *contextVk)
{
    const gl::State &glState = contextVk->getState();
    if (!hasTransformFeedbackOutput())
    {
        // NOTE(syoussefi): a possible optimization is to skip this if transform feedback is
        // paused.  However, even if paused, |updateDescriptorSet| must be called at least once for
        // the sake of validation.
        return;
    }

    TransformFeedbackVk *transformFeedbackVk = vk::GetImpl(glState.getCurrentTransformFeedback());
    transformFeedbackVk->updateDescriptorSet(contextVk, mState,
                                             mDescriptorSets[kUniformsAndXfbDescriptorSetIndex]);
}

angle::Result ProgramVk::updateTexturesDescriptorSet(ContextVk *contextVk)
{
    const vk::TextureDescriptorDesc &texturesDesc = contextVk->getActiveTexturesDesc();

    auto iter = mTextureDescriptorsCache.find(texturesDesc);
    if (iter != mTextureDescriptorsCache.end())
    {
        mDescriptorSets[kTextureDescriptorSetIndex] = iter->second;
        return angle::Result::Continue;
    }

    ASSERT(hasTextures());
    bool newPoolAllocated;
    ANGLE_TRY(
        allocateDescriptorSetAndGetInfo(contextVk, kTextureDescriptorSetIndex, &newPoolAllocated));

    // Clear descriptor set cache. It may no longer be valid.
    if (newPoolAllocated)
    {
        mTextureDescriptorsCache.clear();
    }

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

            vk::ImageHelper &image = textureVk->getImage();

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

            ++writeCount;
        }
    }

    VkDevice device = contextVk->getDevice();

    ASSERT(writeCount > 0);

    vkUpdateDescriptorSets(device, writeCount, writeDescriptorInfo.data(), 0, nullptr);

    mTextureDescriptorsCache.emplace(texturesDesc, descriptorSet);

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

    const VkPipelineBindPoint pipelineBindPoint =
        mState.isCompute() ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

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
                const vk::DescriptorSetLayout &descriptorSetLayout =
                    mDescriptorSetLayouts[descriptorSetIndex].get();

                ANGLE_TRY(mDynamicDescriptorPools[descriptorSetIndex].allocateSets(
                    contextVk, descriptorSetLayout.ptr(), 1,
                    &mDescriptorPoolBindings[descriptorSetIndex],
                    &mEmptyDescriptorSets[descriptorSetIndex]));
            }
            descSet = mEmptyDescriptorSets[descriptorSetIndex];
        }

        // Default uniforms are encompassed in a block per shader stage, and they are assigned
        // through dynamic uniform buffers (requiring dynamic offsets).  No other descriptor
        // requires a dynamic offset.
        const uint32_t uniformBlockOffsetCount =
            descriptorSetIndex == kUniformsAndXfbDescriptorSetIndex ? mDynamicBufferOffsets.size()
                                                                    : 0;

        commandBuffer->bindDescriptorSets(mPipelineLayout.get(), pipelineBindPoint,
                                          descriptorSetIndex, 1, &descSet, uniformBlockOffsetCount,
                                          mDynamicBufferOffsets.data());
    }

    return angle::Result::Continue;
}
}  // namespace rx

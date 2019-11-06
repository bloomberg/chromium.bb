//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextVk.cpp:
//    Implements the class methods for ContextVk.
//

#include "libANGLE/renderer/vulkan/ContextVk.h"

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/Semaphore.h"
#include "libANGLE/Surface.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/CompilerVk.h"
#include "libANGLE/renderer/vulkan/FenceNVVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/MemoryObjectVk.h"
#include "libANGLE/renderer/vulkan/ProgramPipelineVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/QueryVk.h"
#include "libANGLE/renderer/vulkan/RenderbufferVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SamplerVk.h"
#include "libANGLE/renderer/vulkan/SemaphoreVk.h"
#include "libANGLE/renderer/vulkan/ShaderVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"
#include "libANGLE/renderer/vulkan/SyncVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"
#include "libANGLE/renderer/vulkan/VertexArrayVk.h"

#include "libANGLE/trace.h"

namespace rx
{

namespace
{
GLenum DefaultGLErrorCode(VkResult result)
{
    switch (result)
    {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        case VK_ERROR_TOO_MANY_OBJECTS:
            return GL_OUT_OF_MEMORY;
        default:
            return GL_INVALID_OPERATION;
    }
}

constexpr VkColorComponentFlags kAllColorChannelsMask =
    (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
     VK_COLOR_COMPONENT_A_BIT);

constexpr VkBufferUsageFlags kVertexBufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
constexpr size_t kDefaultValueSize              = sizeof(gl::VertexAttribCurrentValueData::Values);
constexpr size_t kDefaultBufferSize             = kDefaultValueSize * 16;
constexpr size_t kDefaultPoolAllocatorPageSize  = 16 * 1024;
constexpr size_t kDriverUniformsBufferSize      = 64;

// Wait a maximum of 10s.  If that times out, we declare it a failure.
constexpr uint64_t kMaxFenceWaitTimeNs = 10'000'000'000llu;

constexpr size_t kInFlightCommandsLimit = 100u;

// Initially dumping the command graphs is disabled.
constexpr bool kEnableCommandGraphDiagnostics = false;

void InitializeSubmitInfo(VkSubmitInfo *submitInfo,
                          const vk::PrimaryCommandBuffer &commandBuffer,
                          const std::vector<VkSemaphore> &waitSemaphores,
                          std::vector<VkPipelineStageFlags> *waitSemaphoreStageMasks,
                          const vk::Semaphore *signalSemaphore)
{
    // Verify that the submitInfo has been zero'd out.
    ASSERT(submitInfo->signalSemaphoreCount == 0);

    submitInfo->sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo->commandBufferCount = commandBuffer.valid() ? 1 : 0;
    submitInfo->pCommandBuffers    = commandBuffer.ptr();

    if (waitSemaphoreStageMasks->size() < waitSemaphores.size())
    {
        waitSemaphoreStageMasks->resize(waitSemaphores.size(),
                                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }

    submitInfo->waitSemaphoreCount = waitSemaphores.size();
    submitInfo->pWaitSemaphores    = waitSemaphores.data();
    submitInfo->pWaitDstStageMask  = waitSemaphoreStageMasks->data();

    if (signalSemaphore)
    {
        submitInfo->signalSemaphoreCount = 1;
        submitInfo->pSignalSemaphores    = signalSemaphore->ptr();
    }
}

uint32_t GetCoverageSampleCount(const gl::State &glState, FramebufferVk *drawFramebuffer)
{
    if (!glState.isSampleCoverageEnabled())
    {
        return 0;
    }

    // Get a fraction of the samples based on the coverage parameters.
    return static_cast<uint32_t>(
        std::round(glState.getSampleCoverageValue() * drawFramebuffer->getSamples()));
}

void ApplySampleCoverage(const gl::State &glState,
                         uint32_t coverageSampleCount,
                         uint32_t maskNumber,
                         uint32_t *maskOut)
{
    if (!glState.isSampleCoverageEnabled())
    {
        return;
    }

    uint32_t maskBitOffset = maskNumber * 32;
    uint32_t coverageMask  = coverageSampleCount >= (maskBitOffset + 32)
                                ? std::numeric_limits<uint32_t>::max()
                                : (1u << (coverageSampleCount - maskBitOffset)) - 1;

    if (glState.getSampleCoverageInvert())
    {
        coverageMask = ~coverageMask;
    }

    *maskOut &= coverageMask;
}

}  // anonymous namespace

// CommandBatch implementation.
ContextVk::CommandBatch::CommandBatch() = default;

ContextVk::CommandBatch::~CommandBatch() = default;

ContextVk::CommandBatch::CommandBatch(CommandBatch &&other)
{
    *this = std::move(other);
}

ContextVk::CommandBatch &ContextVk::CommandBatch::operator=(CommandBatch &&other)
{
    std::swap(commandPool, other.commandPool);
    std::swap(fence, other.fence);
    std::swap(serial, other.serial);
    return *this;
}

void ContextVk::CommandBatch::destroy(VkDevice device)
{
    commandPool.destroy(device);
    fence.reset(device);
}

ContextVk::ContextVk(const gl::State &state, gl::ErrorSet *errorSet, RendererVk *renderer)
    : ContextImpl(state, errorSet),
      vk::Context(renderer),
      mCurrentGraphicsPipeline(nullptr),
      mCurrentComputePipeline(nullptr),
      mCurrentDrawMode(gl::PrimitiveMode::InvalidEnum),
      mCurrentWindowSurface(nullptr),
      mVertexArray(nullptr),
      mDrawFramebuffer(nullptr),
      mProgram(nullptr),
      mLastIndexBufferOffset(0),
      mCurrentDrawElementsType(gl::DrawElementsType::InvalidEnum),
      mXfbBaseVertex(0),
      mClearColorMask(kAllColorChannelsMask),
      mFlipYForCurrentSurface(false),
      mIsAnyHostVisibleBufferWritten(false),
      mDriverUniformsDescriptorSet(VK_NULL_HANDLE),
      mDriverUniformsDynamicOffset(0),
      mLastCompletedQueueSerial(renderer->nextSerial()),
      mCurrentQueueSerial(renderer->nextSerial()),
      mPoolAllocator(kDefaultPoolAllocatorPageSize, 1),
      mCommandGraph(kEnableCommandGraphDiagnostics, &mPoolAllocator),
      mGpuEventsEnabled(false),
      mGpuClockSync{std::numeric_limits<double>::max(), std::numeric_limits<double>::max()},
      mGpuEventTimestampOrigin(0)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::ContextVk");
    memset(&mClearColorValue, 0, sizeof(mClearColorValue));
    memset(&mClearDepthStencilValue, 0, sizeof(mClearDepthStencilValue));

    mNonIndexedDirtyBitsMask.set();
    mNonIndexedDirtyBitsMask.reset(DIRTY_BIT_INDEX_BUFFER);

    mIndexedDirtyBitsMask.set();

    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_PIPELINE);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_TEXTURES);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS);
    mNewGraphicsCommandBufferDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);

    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_PIPELINE);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_TEXTURES);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);
    mNewComputeCommandBufferDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);

    mGraphicsDirtyBitHandlers[DIRTY_BIT_DEFAULT_ATTRIBS] =
        &ContextVk::handleDirtyGraphicsDefaultAttribs;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_PIPELINE] = &ContextVk::handleDirtyGraphicsPipeline;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_TEXTURES] = &ContextVk::handleDirtyGraphicsTextures;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_VERTEX_BUFFERS] =
        &ContextVk::handleDirtyGraphicsVertexBuffers;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_INDEX_BUFFER] = &ContextVk::handleDirtyGraphicsIndexBuffer;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DRIVER_UNIFORMS] =
        &ContextVk::handleDirtyGraphicsDriverUniforms;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_SHADER_RESOURCES] =
        &ContextVk::handleDirtyGraphicsShaderResources;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS] =
        &ContextVk::handleDirtyGraphicsTransformFeedbackBuffers;
    mGraphicsDirtyBitHandlers[DIRTY_BIT_DESCRIPTOR_SETS] =
        &ContextVk::handleDirtyGraphicsDescriptorSets;

    mComputeDirtyBitHandlers[DIRTY_BIT_PIPELINE] = &ContextVk::handleDirtyComputePipeline;
    mComputeDirtyBitHandlers[DIRTY_BIT_TEXTURES] = &ContextVk::handleDirtyComputeTextures;
    mComputeDirtyBitHandlers[DIRTY_BIT_SHADER_RESOURCES] =
        &ContextVk::handleDirtyComputeShaderResources;
    mComputeDirtyBitHandlers[DIRTY_BIT_DESCRIPTOR_SETS] =
        &ContextVk::handleDirtyComputeDescriptorSets;

    mGraphicsDirtyBits = mNewGraphicsCommandBufferDirtyBits;
    mComputeDirtyBits  = mNewComputeCommandBufferDirtyBits;

    mActiveTextures.fill(nullptr);

    mPipelineDirtyBitsMask.set();
    mPipelineDirtyBitsMask.reset(gl::State::DIRTY_BIT_TEXTURE_BINDINGS);
}

ContextVk::~ContextVk() = default;

void ContextVk::onDestroy(const gl::Context *context)
{
    // Force a flush on destroy.
    (void)finishImpl();

    VkDevice device = getDevice();

    mDriverUniformsSetLayout.reset();
    mIncompleteTextures.onDestroy(context);
    mDriverUniformsBuffer.destroy(device);
    mDriverUniformsDescriptorPoolBinding.reset();
    mDriverUniformsDescriptorPool.destroy(device);

    for (vk::DynamicBuffer &defaultBuffer : mDefaultAttribBuffers)
    {
        defaultBuffer.destroy(device);
    }

    for (vk::DynamicQueryPool &queryPool : mQueryPools)
    {
        queryPool.destroy(device);
    }

    if (!mInFlightCommands.empty() || !mGarbage.empty())
    {
        (void)finishImpl();
    }

    mUtils.destroy(device);

    mRenderPassCache.destroy(device);
    mSubmitFence.reset(device);
    mShaderLibrary.destroy(device);
    mGpuEventQueryPool.destroy(device);
    mCommandPool.destroy(device);

    for (vk::CommandPool &pool : mCommandPoolFreeList)
    {
        pool.destroy(device);
    }
}

angle::Result ContextVk::getIncompleteTexture(const gl::Context *context,
                                              gl::TextureType type,
                                              gl::Texture **textureOut)
{
    // At some point, we'll need to support multisample and we'll pass "this" instead of nullptr
    // and implement the necessary interface.
    return mIncompleteTextures.getIncompleteTexture(context, type, nullptr, textureOut);
}

angle::Result ContextVk::initialize()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::initialize");

    VkDescriptorPoolSize driverSetSize = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1};
    ANGLE_TRY(mDriverUniformsDescriptorPool.init(this, &driverSetSize, 1));

    ANGLE_TRY(mQueryPools[gl::QueryType::AnySamples].init(this, VK_QUERY_TYPE_OCCLUSION,
                                                          vk::kDefaultOcclusionQueryPoolSize));
    ANGLE_TRY(mQueryPools[gl::QueryType::AnySamplesConservative].init(
        this, VK_QUERY_TYPE_OCCLUSION, vk::kDefaultOcclusionQueryPoolSize));
    ANGLE_TRY(mQueryPools[gl::QueryType::Timestamp].init(this, VK_QUERY_TYPE_TIMESTAMP,
                                                         vk::kDefaultTimestampQueryPoolSize));
    ANGLE_TRY(mQueryPools[gl::QueryType::TimeElapsed].init(this, VK_QUERY_TYPE_TIMESTAMP,
                                                           vk::kDefaultTimestampQueryPoolSize));

    size_t minAlignment = static_cast<size_t>(
        mRenderer->getPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment);
    mDriverUniformsBuffer.init(mRenderer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, minAlignment,
                               sizeof(DriverUniforms) * kDriverUniformsBufferSize, true);

    // Get the descriptor set layout.
    vk::DescriptorSetLayoutDesc desc = getDriverUniformsDescriptorSetDesc();
    ANGLE_TRY(mRenderer->getDescriptorSetLayout(this, desc, &mDriverUniformsSetLayout));

    mGraphicsPipelineDesc.reset(new vk::GraphicsPipelineDesc());
    mGraphicsPipelineDesc->initDefaults();

    // Initialize current value/default attribute buffers.
    for (vk::DynamicBuffer &buffer : mDefaultAttribBuffers)
    {
        buffer.init(mRenderer, kVertexBufferUsage, 1, kDefaultBufferSize, true);
    }

    // Initialize the command pool now that we know the queue family index.
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolInfo.queueFamilyIndex        = getRenderer()->getQueueFamilyIndex();

    VkDevice device = getDevice();
    ANGLE_VK_TRY(this, mCommandPool.init(device, commandPoolInfo));

#if ANGLE_ENABLE_VULKAN_GPU_TRACE_EVENTS
    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    // GPU tracing workaround for anglebug.com/2927.  The renderer should not emit gpu events during
    // platform discovery.
    const unsigned char *gpuEventsEnabled =
        platform->getTraceCategoryEnabledFlag(platform, "gpu.angle.gpu");
    mGpuEventsEnabled = gpuEventsEnabled && *gpuEventsEnabled;
#endif

    if (mGpuEventsEnabled)
    {
        // Calculate the difference between CPU and GPU clocks for GPU event reporting.
        ANGLE_TRY(mGpuEventQueryPool.init(this, VK_QUERY_TYPE_TIMESTAMP,
                                          vk::kDefaultTimestampQueryPoolSize));
        ANGLE_TRY(synchronizeCpuGpuTime());
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::flush(const gl::Context *context)
{
    return flushImpl(nullptr);
}

angle::Result ContextVk::finish(const gl::Context *context)
{
    return finishImpl();
}

angle::Result ContextVk::setupDraw(const gl::Context *context,
                                   gl::PrimitiveMode mode,
                                   GLint firstVertex,
                                   GLsizei vertexOrIndexCount,
                                   GLsizei instanceCount,
                                   gl::DrawElementsType indexTypeOrNone,
                                   const void *indices,
                                   DirtyBits dirtyBitMask,
                                   vk::CommandBuffer **commandBufferOut)
{
    // Set any dirty bits that depend on draw call parameters or other objects.
    if (mode != mCurrentDrawMode)
    {
        invalidateCurrentGraphicsPipeline();
        mCurrentDrawMode = mode;
        mGraphicsPipelineDesc->updateTopology(&mGraphicsPipelineTransition, mCurrentDrawMode);
    }

    // Must be called before the command buffer is started. Can call finish.
    if (context->getStateCache().hasAnyActiveClientAttrib())
    {
        ANGLE_TRY(mVertexArray->updateClientAttribs(context, firstVertex, vertexOrIndexCount,
                                                    instanceCount, indexTypeOrNone, indices));
        mGraphicsDirtyBits.set(DIRTY_BIT_VERTEX_BUFFERS);
    }

    // This could be improved using a dirty bit. But currently it's slower to use a handler
    // function than an inlined if. We should probably replace the dirty bit dispatch table
    // with a switch with inlined handler functions.
    // TODO(jmadill): Use dirty bit. http://anglebug.com/3014
    if (!mRenderPassCommandBuffer)
    {
        mGraphicsDirtyBits |= mNewGraphicsCommandBufferDirtyBits;

        gl::Rectangle scissoredRenderArea = mDrawFramebuffer->getScissoredRenderArea(this);
        if (!mDrawFramebuffer->appendToStartedRenderPass(
                getCurrentQueueSerial(), scissoredRenderArea, &mRenderPassCommandBuffer))
        {
            ANGLE_TRY(mDrawFramebuffer->startNewRenderPass(this, scissoredRenderArea,
                                                           &mRenderPassCommandBuffer));
        }
    }

    // We keep a local copy of the command buffer. It's possible that some state changes could
    // trigger a command buffer invalidation. The local copy ensures we retain the reference.
    // Command buffers are pool allocated and only deleted after submit. Thus we know the
    // command buffer will still be valid for the duration of this API call.
    *commandBufferOut = mRenderPassCommandBuffer;
    ASSERT(*commandBufferOut);

    if (mProgram->dirtyUniforms())
    {
        ANGLE_TRY(mProgram->updateUniforms(this));
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }

    // Update transform feedback offsets on every draw call.
    if (mState.isTransformFeedbackActiveUnpaused())
    {
        mXfbBaseVertex = firstVertex;
        invalidateDriverUniforms();
    }

    DirtyBits dirtyBits = mGraphicsDirtyBits & dirtyBitMask;

    if (dirtyBits.none())
        return angle::Result::Continue;

    // Flush any relevant dirty bits.
    for (size_t dirtyBit : dirtyBits)
    {
        ANGLE_TRY((this->*mGraphicsDirtyBitHandlers[dirtyBit])(context, *commandBufferOut));
    }

    mGraphicsDirtyBits &= ~dirtyBitMask;

    return angle::Result::Continue;
}

angle::Result ContextVk::setupIndexedDraw(const gl::Context *context,
                                          gl::PrimitiveMode mode,
                                          GLsizei indexCount,
                                          GLsizei instanceCount,
                                          gl::DrawElementsType indexType,
                                          const void *indices,
                                          vk::CommandBuffer **commandBufferOut)
{
    ASSERT(mode != gl::PrimitiveMode::LineLoop);

    if (indexType != mCurrentDrawElementsType)
    {
        mCurrentDrawElementsType = indexType;
        setIndexBufferDirty();
    }

    const gl::Buffer *elementArrayBuffer = mVertexArray->getState().getElementArrayBuffer();
    if (!elementArrayBuffer)
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
        ANGLE_TRY(mVertexArray->convertIndexBufferCPU(this, indexType, indexCount, indices));
    }
    else
    {
        if (indices != mLastIndexBufferOffset)
        {
            mGraphicsDirtyBits.set(DIRTY_BIT_INDEX_BUFFER);
            mLastIndexBufferOffset = indices;
            mVertexArray->updateCurrentElementArrayBufferOffset(mLastIndexBufferOffset);
        }

        if (indexType == gl::DrawElementsType::UnsignedByte &&
            mGraphicsDirtyBits[DIRTY_BIT_INDEX_BUFFER])
        {
            BufferVk *bufferVk = vk::GetImpl(elementArrayBuffer);
            ANGLE_TRY(mVertexArray->convertIndexBufferGPU(this, bufferVk, indices));
        }
    }

    return setupDraw(context, mode, 0, indexCount, instanceCount, indexType, indices,
                     mIndexedDirtyBitsMask, commandBufferOut);
}

angle::Result ContextVk::setupLineLoopDraw(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           GLint firstVertex,
                                           GLsizei vertexOrIndexCount,
                                           gl::DrawElementsType indexTypeOrInvalid,
                                           const void *indices,
                                           vk::CommandBuffer **commandBufferOut,
                                           size_t *numIndicesOut)
{
    ANGLE_TRY(mVertexArray->handleLineLoop(this, firstVertex, vertexOrIndexCount,
                                           indexTypeOrInvalid, indices, numIndicesOut));
    setIndexBufferDirty();
    mCurrentDrawElementsType = indexTypeOrInvalid != gl::DrawElementsType::InvalidEnum
                                   ? indexTypeOrInvalid
                                   : gl::DrawElementsType::UnsignedInt;
    return setupDraw(context, mode, firstVertex, vertexOrIndexCount, 1, indexTypeOrInvalid, indices,
                     mIndexedDirtyBitsMask, commandBufferOut);
}

angle::Result ContextVk::setupDispatch(const gl::Context *context,
                                       vk::CommandBuffer **commandBufferOut)
{
    ANGLE_TRY(mDispatcher.recordCommands(this, commandBufferOut));

    if (mProgram->dirtyUniforms())
    {
        ANGLE_TRY(mProgram->updateUniforms(this));
        mComputeDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }

    DirtyBits dirtyBits = mComputeDirtyBits;

    // Flush any relevant dirty bits.
    for (size_t dirtyBit : dirtyBits)
    {
        ANGLE_TRY((this->*mComputeDirtyBitHandlers[dirtyBit])(context, *commandBufferOut));
    }

    mComputeDirtyBits.reset();

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDefaultAttribs(const gl::Context *context,
                                                           vk::CommandBuffer *commandBuffer)
{
    ASSERT(mDirtyDefaultAttribsMask.any());

    for (size_t attribIndex : mDirtyDefaultAttribsMask)
    {
        ANGLE_TRY(updateDefaultAttribute(attribIndex));
    }

    mDirtyDefaultAttribsMask.reset();
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsPipeline(const gl::Context *context,
                                                     vk::CommandBuffer *commandBuffer)
{
    if (!mCurrentGraphicsPipeline)
    {
        const vk::GraphicsPipelineDesc *descPtr;

        // Draw call shader patching, shader compilation, and pipeline cache query.
        ANGLE_TRY(
            mProgram->getGraphicsPipeline(this, mCurrentDrawMode, *mGraphicsPipelineDesc,
                                          mProgram->getState().getNonBuiltinAttribLocationsMask(),
                                          &descPtr, &mCurrentGraphicsPipeline));
        mGraphicsPipelineTransition.reset();
    }
    else if (mGraphicsPipelineTransition.any())
    {
        if (!mCurrentGraphicsPipeline->findTransition(
                mGraphicsPipelineTransition, *mGraphicsPipelineDesc, &mCurrentGraphicsPipeline))
        {
            vk::PipelineHelper *oldPipeline = mCurrentGraphicsPipeline;

            const vk::GraphicsPipelineDesc *descPtr;

            ANGLE_TRY(mProgram->getGraphicsPipeline(
                this, mCurrentDrawMode, *mGraphicsPipelineDesc,
                mProgram->getState().getNonBuiltinAttribLocationsMask(), &descPtr,
                &mCurrentGraphicsPipeline));

            oldPipeline->addTransition(mGraphicsPipelineTransition, descPtr,
                                       mCurrentGraphicsPipeline);
        }

        mGraphicsPipelineTransition.reset();
    }
    commandBuffer->bindGraphicsPipeline(mCurrentGraphicsPipeline->getPipeline());
    // Update the queue serial for the pipeline object.
    ASSERT(mCurrentGraphicsPipeline && mCurrentGraphicsPipeline->valid());
    mCurrentGraphicsPipeline->updateSerial(getCurrentQueueSerial());
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyComputePipeline(const gl::Context *context,
                                                    vk::CommandBuffer *commandBuffer)
{
    if (!mCurrentComputePipeline)
    {
        ANGLE_TRY(mProgram->getComputePipeline(this, &mCurrentComputePipeline));
    }

    commandBuffer->bindComputePipeline(mCurrentComputePipeline->get());
    mCurrentComputePipeline->updateSerial(getCurrentQueueSerial());

    return angle::Result::Continue;
}

ANGLE_INLINE angle::Result ContextVk::handleDirtyTexturesImpl(const gl::Context *context,
                                                              vk::CommandBuffer *commandBuffer,
                                                              vk::CommandGraphResource *recorder)
{

    ANGLE_TRY(updateActiveTextures(context, recorder));

    if (mProgram->hasTextures())
    {
        ANGLE_TRY(mProgram->updateTexturesDescriptorSet(this));
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsTextures(const gl::Context *context,
                                                     vk::CommandBuffer *commandBuffer)
{
    return handleDirtyTexturesImpl(context, commandBuffer, mDrawFramebuffer->getFramebuffer());
}

angle::Result ContextVk::handleDirtyComputeTextures(const gl::Context *context,
                                                    vk::CommandBuffer *commandBuffer)
{
    return handleDirtyTexturesImpl(context, commandBuffer, &mDispatcher);
}

angle::Result ContextVk::handleDirtyGraphicsVertexBuffers(const gl::Context *context,
                                                          vk::CommandBuffer *commandBuffer)
{
    uint32_t maxAttrib = mProgram->getState().getMaxActiveAttribLocation();
    const gl::AttribArray<VkBuffer> &bufferHandles = mVertexArray->getCurrentArrayBufferHandles();
    const gl::AttribArray<VkDeviceSize> &bufferOffsets =
        mVertexArray->getCurrentArrayBufferOffsets();

    commandBuffer->bindVertexBuffers(0, maxAttrib, bufferHandles.data(), bufferOffsets.data());

    const gl::AttribArray<vk::BufferHelper *> &arrayBufferResources =
        mVertexArray->getCurrentArrayBuffers();
    vk::FramebufferHelper *framebuffer = mDrawFramebuffer->getFramebuffer();

    for (size_t attribIndex : context->getStateCache().getActiveBufferedAttribsMask())
    {
        vk::BufferHelper *arrayBuffer = arrayBufferResources[attribIndex];
        if (arrayBuffer)
        {
            arrayBuffer->onRead(framebuffer, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
        }
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsIndexBuffer(const gl::Context *context,
                                                        vk::CommandBuffer *commandBuffer)
{
    vk::BufferHelper *elementArrayBuffer = mVertexArray->getCurrentElementArrayBuffer();
    ASSERT(elementArrayBuffer != nullptr);

    commandBuffer->bindIndexBuffer(elementArrayBuffer->getBuffer(),
                                   mVertexArray->getCurrentElementArrayBufferOffset(),
                                   gl_vk::kIndexTypeMap[mCurrentDrawElementsType]);

    vk::FramebufferHelper *framebuffer = mDrawFramebuffer->getFramebuffer();
    elementArrayBuffer->onRead(framebuffer, VK_ACCESS_INDEX_READ_BIT);

    return angle::Result::Continue;
}

ANGLE_INLINE angle::Result ContextVk::handleDirtyShaderResourcesImpl(
    const gl::Context *context,
    vk::CommandBuffer *commandBuffer,
    vk::CommandGraphResource *recorder)
{
    if (mProgram->hasUniformBuffers() || mProgram->hasStorageBuffers() ||
        mProgram->hasAtomicCounterBuffers())
    {
        ANGLE_TRY(mProgram->updateShaderResourcesDescriptorSet(this, recorder));
    }
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsShaderResources(const gl::Context *context,
                                                            vk::CommandBuffer *commandBuffer)
{
    return handleDirtyShaderResourcesImpl(context, commandBuffer,
                                          mDrawFramebuffer->getFramebuffer());
}

angle::Result ContextVk::handleDirtyComputeShaderResources(const gl::Context *context,
                                                           vk::CommandBuffer *commandBuffer)
{
    return handleDirtyShaderResourcesImpl(context, commandBuffer, &mDispatcher);
}

angle::Result ContextVk::handleDirtyGraphicsTransformFeedbackBuffers(
    const gl::Context *context,
    vk::CommandBuffer *commandBuffer)
{
    if (mProgram->hasTransformFeedbackOutput() && mState.isTransformFeedbackActive())
    {
        ANGLE_TRY(mProgram->updateTransformFeedbackDescriptorSet(
            this, mDrawFramebuffer->getFramebuffer()));
    }
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyGraphicsDescriptorSets(const gl::Context *context,
                                                           vk::CommandBuffer *commandBuffer)
{
    ANGLE_TRY(mProgram->updateDescriptorSets(this, commandBuffer));

    // Bind the graphics descriptor sets.
    commandBuffer->bindDescriptorSets(
        mProgram->getPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS,
        kDriverUniformsDescriptorSetIndex, 1, &mDriverUniformsDescriptorSet, 1,
        &mDriverUniformsDynamicOffset);
    return angle::Result::Continue;
}

angle::Result ContextVk::handleDirtyComputeDescriptorSets(const gl::Context *context,
                                                          vk::CommandBuffer *commandBuffer)
{
    return mProgram->updateDescriptorSets(this, commandBuffer);
}

angle::Result ContextVk::submitFrame(const VkSubmitInfo &submitInfo,
                                     vk::PrimaryCommandBuffer &&commandBuffer)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::submitFrame");
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = 0;

    VkDevice device = getDevice();
    vk::Scoped<CommandBatch> scopedBatch(device);
    CommandBatch &batch = scopedBatch.get();
    ANGLE_TRY(getNextSubmitFence(&batch.fence));

    ANGLE_TRY(getRenderer()->queueSubmit(this, submitInfo, batch.fence.get()));

    // TODO: this comment still valid?
    // Notify the Contexts that they should be starting new command buffers.
    // We use one command pool per serial/submit associated with this VkQueue. We can also
    // have multiple Contexts sharing one VkQueue. In ContextVk::setupDraw we don't explicitly
    // check for a new serial when starting a new command buffer. We just check that the current
    // recording command buffer is valid. Thus we need to explicitly notify every other Context
    // using this VkQueue that they their current command buffer is no longer valid.
    onRenderPassFinished();
    mComputeDirtyBits |= mNewComputeCommandBufferDirtyBits;

    // Store this command buffer in the in-flight list.
    batch.commandPool = std::move(mCommandPool);
    batch.serial      = mCurrentQueueSerial;

    mInFlightCommands.emplace_back(scopedBatch.release());

    // Make sure a new fence is created for the next submission.
    mRenderer->resetSharedFence(&mSubmitFence);

    // CPU should be throttled to avoid mInFlightCommands from growing too fast.  That is done on
    // swap() though, and there could be multiple submissions in between (through glFlush() calls),
    // so the limit is larger than the expected number of images.  The
    // InterleavedAttributeDataBenchmark perf test for example issues a large number of flushes.
    ASSERT(mInFlightCommands.size() <= kInFlightCommandsLimit);

    mLastSubmittedQueueSerial = mCurrentQueueSerial;
    mCurrentQueueSerial       = getRenderer()->nextSerial();

    ANGLE_TRY(checkCompletedCommands());

    if (mGpuEventsEnabled)
    {
        ANGLE_TRY(checkCompletedGpuEvents());
    }

    // Simply null out the command buffer here - it was allocated using the command pool.
    commandBuffer.releaseHandle();

    // Reallocate the command pool for next frame.
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex        = getRenderer()->getQueueFamilyIndex();

    if (mCommandPoolFreeList.empty())
    {
        ANGLE_VK_TRY(this, mCommandPool.init(device, poolInfo));
    }
    else
    {
        mCommandPool = std::move(mCommandPoolFreeList.back());
        mCommandPoolFreeList.pop_back();
    }

    return angle::Result::Continue;
}

void ContextVk::freeAllInFlightResources()
{
    VkDevice device = getDevice();

    for (CommandBatch &batch : mInFlightCommands)
    {
        // On device loss we need to wait for fence to be signaled before destroying it
        if (getRenderer()->isDeviceLost())
        {
            VkResult status = batch.fence.get().wait(device, kMaxFenceWaitTimeNs);
            // If wait times out, it is probably not possible to recover from lost device
            ASSERT(status == VK_SUCCESS || status == VK_ERROR_DEVICE_LOST);
        }

        batch.commandPool.reset(device, 0);
        mCommandPoolFreeList.emplace_back(std::move(batch.commandPool));

        batch.fence.reset(device);
    }
    mInFlightCommands.clear();

    for (auto &garbage : mGarbage)
    {
        garbage.destroy(device);
    }
    mGarbage.clear();

    mLastCompletedQueueSerial = mLastSubmittedQueueSerial;
}

angle::Result ContextVk::flushCommandGraph(vk::PrimaryCommandBuffer *commandBatch)
{
    if (mIsAnyHostVisibleBufferWritten)
    {
        mCommandGraph.makeHostVisibleBufferWriteAvailable();
    }
    mIsAnyHostVisibleBufferWritten = false;

    return mCommandGraph.submitCommands(this, mCurrentQueueSerial, &mRenderPassCache, &mCommandPool,
                                        commandBatch);
}

angle::Result ContextVk::synchronizeCpuGpuTime()
{
    ASSERT(mGpuEventsEnabled);

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    // To synchronize CPU and GPU times, we need to get the CPU timestamp as close as possible to
    // the GPU timestamp.  The process of getting the GPU timestamp is as follows:
    //
    //             CPU                            GPU
    //
    //     Record command buffer
    //     with timestamp query
    //
    //     Submit command buffer
    //
    //     Post-submission work             Begin execution
    //
    //            ????                    Write timestamp Tgpu
    //
    //            ????                       End execution
    //
    //            ????                    Return query results
    //
    //            ????
    //
    //       Get query results
    //
    // The areas of unknown work (????) on the CPU indicate that the CPU may or may not have
    // finished post-submission work while the GPU is executing in parallel. With no further work,
    // querying CPU timestamps before submission and after getting query results give the bounds to
    // Tgpu, which could be quite large.
    //
    // Using VkEvents, the GPU can be made to wait for the CPU and vice versa, in an effort to
    // reduce this range. This function implements the following procedure:
    //
    //             CPU                            GPU
    //
    //     Record command buffer
    //     with timestamp query
    //
    //     Submit command buffer
    //
    //     Post-submission work             Begin execution
    //
    //            ????                    Set Event GPUReady
    //
    //    Wait on Event GPUReady         Wait on Event CPUReady
    //
    //       Get CPU Time Ts             Wait on Event CPUReady
    //
    //      Set Event CPUReady           Wait on Event CPUReady
    //
    //      Get CPU Time Tcpu              Get GPU Time Tgpu
    //
    //    Wait on Event GPUDone            Set Event GPUDone
    //
    //       Get CPU Time Te                 End Execution
    //
    //            Idle                    Return query results
    //
    //      Get query results
    //
    // If Te-Ts > epsilon, a GPU or CPU interruption can be assumed and the operation can be
    // retried.  Once Te-Ts < epsilon, Tcpu can be taken to presumably match Tgpu.  Finding an
    // epsilon that's valid for all devices may be difficult, so the loop can be performed only a
    // limited number of times and the Tcpu,Tgpu pair corresponding to smallest Te-Ts used for
    // calibration.
    //
    // Note: Once VK_EXT_calibrated_timestamps is ubiquitous, this should be redone.

    // Make sure nothing is running
    ASSERT(mCommandGraph.empty());

    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::synchronizeCpuGpuTime");

    // Create a query used to receive the GPU timestamp
    vk::QueryHelper timestampQuery;
    ANGLE_TRY(mGpuEventQueryPool.allocateQuery(this, &timestampQuery));

    // Create the three events
    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType             = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
    eventCreateInfo.flags             = 0;

    VkDevice device = getDevice();
    vk::Scoped<vk::Event> cpuReady(device), gpuReady(device), gpuDone(device);
    ANGLE_VK_TRY(this, cpuReady.get().init(device, eventCreateInfo));
    ANGLE_VK_TRY(this, gpuReady.get().init(device, eventCreateInfo));
    ANGLE_VK_TRY(this, gpuDone.get().init(device, eventCreateInfo));

    constexpr uint32_t kRetries = 10;

    // Time suffixes used are S for seconds and Cycles for cycles
    double tightestRangeS = 1e6f;
    double TcpuS          = 0;
    uint64_t TgpuCycles   = 0;
    for (uint32_t i = 0; i < kRetries; ++i)
    {
        // Reset the events
        ANGLE_VK_TRY(this, cpuReady.get().reset(device));
        ANGLE_VK_TRY(this, gpuReady.get().reset(device));
        ANGLE_VK_TRY(this, gpuDone.get().reset(device));

        // Record the command buffer
        vk::Scoped<vk::PrimaryCommandBuffer> commandBatch(device);
        vk::PrimaryCommandBuffer &commandBuffer = commandBatch.get();

        VkCommandBufferAllocateInfo commandBufferInfo = {};
        commandBufferInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandPool        = mCommandPool.getHandle();
        commandBufferInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferInfo.commandBufferCount = 1;

        ANGLE_VK_TRY(this, commandBuffer.init(device, commandBufferInfo));

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags                    = 0;
        beginInfo.pInheritanceInfo         = nullptr;

        ANGLE_VK_TRY(this, commandBuffer.begin(beginInfo));

        commandBuffer.setEvent(gpuReady.get().getHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
        commandBuffer.waitEvents(1, cpuReady.get().ptr(), VK_PIPELINE_STAGE_HOST_BIT,
                                 VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, nullptr, 0, nullptr, 0,
                                 nullptr);

        commandBuffer.resetQueryPool(timestampQuery.getQueryPool()->getHandle(),
                                     timestampQuery.getQuery(), 1);
        commandBuffer.writeTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                     timestampQuery.getQueryPool()->getHandle(),
                                     timestampQuery.getQuery());

        commandBuffer.setEvent(gpuDone.get().getHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

        ANGLE_VK_TRY(this, commandBuffer.end());

        // Submit the command buffer
        VkSubmitInfo submitInfo = {};
        InitializeSubmitInfo(&submitInfo, commandBatch.get(), {}, &mWaitSemaphoreStageMasks,
                             nullptr);

        ANGLE_TRY(submitFrame(submitInfo, std::move(commandBuffer)));

        // Wait for GPU to be ready.  This is a short busy wait.
        VkResult result = VK_EVENT_RESET;
        do
        {
            result = gpuReady.get().getStatus(device);
            if (result != VK_EVENT_SET && result != VK_EVENT_RESET)
            {
                ANGLE_VK_TRY(this, result);
            }
        } while (result == VK_EVENT_RESET);

        double TsS = platform->monotonicallyIncreasingTime(platform);

        // Tell the GPU to go ahead with the timestamp query.
        ANGLE_VK_TRY(this, cpuReady.get().set(device));
        double cpuTimestampS = platform->monotonicallyIncreasingTime(platform);

        // Wait for GPU to be done.  Another short busy wait.
        do
        {
            result = gpuDone.get().getStatus(device);
            if (result != VK_EVENT_SET && result != VK_EVENT_RESET)
            {
                ANGLE_VK_TRY(this, result);
            }
        } while (result == VK_EVENT_RESET);

        double TeS = platform->monotonicallyIncreasingTime(platform);

        // Get the query results
        ANGLE_TRY(finishToSerial(getLastSubmittedQueueSerial()));

        constexpr VkQueryResultFlags queryFlags = VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT;

        uint64_t gpuTimestampCycles = 0;
        ANGLE_VK_TRY(this, timestampQuery.getQueryPool()->getResults(
                               device, timestampQuery.getQuery(), 1, sizeof(gpuTimestampCycles),
                               &gpuTimestampCycles, sizeof(gpuTimestampCycles), queryFlags));

        // Use the first timestamp queried as origin.
        if (mGpuEventTimestampOrigin == 0)
        {
            mGpuEventTimestampOrigin = gpuTimestampCycles;
        }

        // Take these CPU and GPU timestamps if there is better confidence.
        double confidenceRangeS = TeS - TsS;
        if (confidenceRangeS < tightestRangeS)
        {
            tightestRangeS = confidenceRangeS;
            TcpuS          = cpuTimestampS;
            TgpuCycles     = gpuTimestampCycles;
        }
    }

    mGpuEventQueryPool.freeQuery(this, &timestampQuery);

    // timestampPeriod gives nanoseconds/cycle.
    double TgpuS =
        (TgpuCycles - mGpuEventTimestampOrigin) *
        static_cast<double>(getRenderer()->getPhysicalDeviceProperties().limits.timestampPeriod) /
        1'000'000'000.0;

    flushGpuEvents(TgpuS, TcpuS);

    mGpuClockSync.gpuTimestampS = TgpuS;
    mGpuClockSync.cpuTimestampS = TcpuS;

    return angle::Result::Continue;
}

angle::Result ContextVk::traceGpuEventImpl(vk::PrimaryCommandBuffer *commandBuffer,
                                           char phase,
                                           const char *name)
{
    ASSERT(mGpuEventsEnabled);

    GpuEventQuery event;

    event.name   = name;
    event.phase  = phase;
    event.serial = mCurrentQueueSerial;

    ANGLE_TRY(mGpuEventQueryPool.allocateQuery(this, &event.queryPoolIndex, &event.queryIndex));

    commandBuffer->resetQueryPool(
        mGpuEventQueryPool.getQueryPool(event.queryPoolIndex)->getHandle(), event.queryIndex, 1);
    commandBuffer->writeTimestamp(
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        mGpuEventQueryPool.getQueryPool(event.queryPoolIndex)->getHandle(), event.queryIndex);

    mInFlightGpuEventQueries.push_back(std::move(event));

    return angle::Result::Continue;
}

angle::Result ContextVk::checkCompletedGpuEvents()
{
    ASSERT(mGpuEventsEnabled);

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    int finishedCount = 0;

    for (GpuEventQuery &eventQuery : mInFlightGpuEventQueries)
    {
        // Only check the timestamp query if the submission has finished.
        if (eventQuery.serial > mLastCompletedQueueSerial)
        {
            break;
        }

        // See if the results are available.
        uint64_t gpuTimestampCycles = 0;
        VkResult result             = mGpuEventQueryPool.getQueryPool(eventQuery.queryPoolIndex)
                              ->getResults(getDevice(), eventQuery.queryIndex, 1,
                                           sizeof(gpuTimestampCycles), &gpuTimestampCycles,
                                           sizeof(gpuTimestampCycles), VK_QUERY_RESULT_64_BIT);
        if (result == VK_NOT_READY)
        {
            break;
        }
        ANGLE_VK_TRY(this, result);

        mGpuEventQueryPool.freeQuery(this, eventQuery.queryPoolIndex, eventQuery.queryIndex);

        GpuEvent event;
        event.gpuTimestampCycles = gpuTimestampCycles;
        event.name               = eventQuery.name;
        event.phase              = eventQuery.phase;

        mGpuEvents.emplace_back(event);

        ++finishedCount;
    }

    mInFlightGpuEventQueries.erase(mInFlightGpuEventQueries.begin(),
                                   mInFlightGpuEventQueries.begin() + finishedCount);

    return angle::Result::Continue;
}

void ContextVk::flushGpuEvents(double nextSyncGpuTimestampS, double nextSyncCpuTimestampS)
{
    if (mGpuEvents.size() == 0)
    {
        return;
    }

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    // Find the slope of the clock drift for adjustment
    double lastGpuSyncTimeS  = mGpuClockSync.gpuTimestampS;
    double lastGpuSyncDiffS  = mGpuClockSync.cpuTimestampS - mGpuClockSync.gpuTimestampS;
    double gpuSyncDriftSlope = 0;

    double nextGpuSyncTimeS = nextSyncGpuTimestampS;
    double nextGpuSyncDiffS = nextSyncCpuTimestampS - nextSyncGpuTimestampS;

    // No gpu trace events should have been generated before the clock sync, so if there is no
    // "previous" clock sync, there should be no gpu events (i.e. the function early-outs above).
    ASSERT(mGpuClockSync.gpuTimestampS != std::numeric_limits<double>::max() &&
           mGpuClockSync.cpuTimestampS != std::numeric_limits<double>::max());

    gpuSyncDriftSlope =
        (nextGpuSyncDiffS - lastGpuSyncDiffS) / (nextGpuSyncTimeS - lastGpuSyncTimeS);

    for (const GpuEvent &event : mGpuEvents)
    {
        double gpuTimestampS =
            (event.gpuTimestampCycles - mGpuEventTimestampOrigin) *
            static_cast<double>(
                getRenderer()->getPhysicalDeviceProperties().limits.timestampPeriod) *
            1e-9;

        // Account for clock drift.
        gpuTimestampS += lastGpuSyncDiffS + gpuSyncDriftSlope * (gpuTimestampS - lastGpuSyncTimeS);

        // Generate the trace now that the GPU timestamp is available and clock drifts are accounted
        // for.
        static long long eventId = 1;
        static const unsigned char *categoryEnabled =
            TRACE_EVENT_API_GET_CATEGORY_ENABLED(platform, "gpu.angle.gpu");
        platform->addTraceEvent(platform, event.phase, categoryEnabled, event.name, eventId++,
                                gpuTimestampS, 0, nullptr, nullptr, nullptr, TRACE_EVENT_FLAG_NONE);
    }

    mGpuEvents.clear();
}

void ContextVk::handleDeviceLost()
{
    mCommandGraph.clear();
    // TODO: generate a new serial neccessary here?
    freeAllInFlightResources();

    mRenderer->notifyDeviceLost();
}

angle::Result ContextVk::drawArrays(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    GLint first,
                                    GLsizei count)
{
    vk::CommandBuffer *commandBuffer = nullptr;
    uint32_t clampedVertexCount      = gl::GetClampedVertexCount<uint32_t>(count);

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        size_t numIndices;
        ANGLE_TRY(setupLineLoopDraw(context, mode, first, count, gl::DrawElementsType::InvalidEnum,
                                    nullptr, &commandBuffer, &numIndices));
        vk::LineLoopHelper::Draw(numIndices, commandBuffer);
    }
    else
    {
        ANGLE_TRY(setupDraw(context, mode, first, count, 1, gl::DrawElementsType::InvalidEnum,
                            nullptr, mNonIndexedDirtyBitsMask, &commandBuffer));
        commandBuffer->draw(clampedVertexCount, first);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::drawArraysInstanced(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei instances)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        // TODO - http://anglebug.com/2672
        ANGLE_VK_UNREACHABLE(this);
        return angle::Result::Stop;
    }

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(setupDraw(context, mode, first, count, instances, gl::DrawElementsType::InvalidEnum,
                        nullptr, mNonIndexedDirtyBitsMask, &commandBuffer));
    commandBuffer->drawInstanced(gl::GetClampedVertexCount<uint32_t>(count), instances, first);
    return angle::Result::Continue;
}

angle::Result ContextVk::drawElements(const gl::Context *context,
                                      gl::PrimitiveMode mode,
                                      GLsizei count,
                                      gl::DrawElementsType type,
                                      const void *indices)
{
    vk::CommandBuffer *commandBuffer = nullptr;
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        size_t indexCount;
        ANGLE_TRY(
            setupLineLoopDraw(context, mode, 0, count, type, indices, &commandBuffer, &indexCount));
        vk::LineLoopHelper::Draw(indexCount, commandBuffer);
    }
    else
    {
        ANGLE_TRY(setupIndexedDraw(context, mode, count, 1, type, indices, &commandBuffer));
        commandBuffer->drawIndexed(count);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::drawElementsInstanced(const gl::Context *context,
                                               gl::PrimitiveMode mode,
                                               GLsizei count,
                                               gl::DrawElementsType type,
                                               const void *indices,
                                               GLsizei instances)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        // TODO - http://anglebug.com/2672
        ANGLE_VK_UNREACHABLE(this);
        return angle::Result::Stop;
    }

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(setupIndexedDraw(context, mode, count, instances, type, indices, &commandBuffer));
    commandBuffer->drawIndexedInstanced(count, instances);
    return angle::Result::Continue;
}

angle::Result ContextVk::drawRangeElements(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           GLuint start,
                                           GLuint end,
                                           GLsizei count,
                                           gl::DrawElementsType type,
                                           const void *indices)
{
    return drawElements(context, mode, count, type, indices);
}

VkDevice ContextVk::getDevice() const
{
    return mRenderer->getDevice();
}

angle::Result ContextVk::drawArraysIndirect(const gl::Context *context,
                                            gl::PrimitiveMode mode,
                                            const void *indirect)
{
    ANGLE_VK_UNREACHABLE(this);
    return angle::Result::Stop;
}

angle::Result ContextVk::drawElementsIndirect(const gl::Context *context,
                                              gl::PrimitiveMode mode,
                                              gl::DrawElementsType type,
                                              const void *indirect)
{
    ANGLE_VK_UNREACHABLE(this);
    return angle::Result::Stop;
}

gl::GraphicsResetStatus ContextVk::getResetStatus()
{
    if (mRenderer->isDeviceLost())
    {
        // TODO(geofflang): It may be possible to track which context caused the device lost and
        // return either GL_GUILTY_CONTEXT_RESET or GL_INNOCENT_CONTEXT_RESET.
        // http://anglebug.com/2787
        return gl::GraphicsResetStatus::UnknownContextReset;
    }

    return gl::GraphicsResetStatus::NoError;
}

std::string ContextVk::getVendorString() const
{
    UNIMPLEMENTED();
    return std::string();
}

std::string ContextVk::getRendererDescription() const
{
    return mRenderer->getRendererDescription();
}

void ContextVk::insertEventMarker(GLsizei length, const char *marker)
{
    std::string markerStr(marker, length <= 0 ? strlen(marker) : length);
    mCommandGraph.insertDebugMarker(GL_DEBUG_SOURCE_APPLICATION, std::move(marker));
}

void ContextVk::pushGroupMarker(GLsizei length, const char *marker)
{
    std::string markerStr(marker, length <= 0 ? strlen(marker) : length);
    mCommandGraph.pushDebugMarker(GL_DEBUG_SOURCE_APPLICATION, std::move(marker));
}

void ContextVk::popGroupMarker()
{
    mCommandGraph.popDebugMarker();
}

void ContextVk::pushDebugGroup(GLenum source, GLuint id, const std::string &message)
{
    mCommandGraph.insertDebugMarker(source, std::string(message));
}

void ContextVk::popDebugGroup()
{
    mCommandGraph.popDebugMarker();
}

bool ContextVk::isViewportFlipEnabledForDrawFBO() const
{
    return mFlipViewportForDrawFramebuffer && mFlipYForCurrentSurface;
}

bool ContextVk::isViewportFlipEnabledForReadFBO() const
{
    return mFlipViewportForReadFramebuffer;
}

void ContextVk::updateColorMask(const gl::BlendState &blendState)
{
    mClearColorMask =
        gl_vk::GetColorComponentFlags(blendState.colorMaskRed, blendState.colorMaskGreen,
                                      blendState.colorMaskBlue, blendState.colorMaskAlpha);

    FramebufferVk *framebufferVk = vk::GetImpl(mState.getDrawFramebuffer());
    mGraphicsPipelineDesc->updateColorWriteMask(&mGraphicsPipelineTransition, mClearColorMask,
                                                framebufferVk->getEmulatedAlphaAttachmentMask());
}

void ContextVk::updateSampleMask(const gl::State &glState)
{
    // If sample coverage is enabled, emulate it by generating and applying a mask on top of the
    // sample mask.
    uint32_t coverageSampleCount = GetCoverageSampleCount(glState, mDrawFramebuffer);

    static_assert(sizeof(uint32_t) == sizeof(GLbitfield), "Vulkan assumes 32-bit sample masks");
    for (uint32_t maskNumber = 0; maskNumber < glState.getMaxSampleMaskWords(); ++maskNumber)
    {
        uint32_t mask = glState.isSampleMaskEnabled() ? glState.getSampleMaskWord(maskNumber)
                                                      : std::numeric_limits<uint32_t>::max();

        ApplySampleCoverage(glState, coverageSampleCount, maskNumber, &mask);

        mGraphicsPipelineDesc->updateSampleMask(&mGraphicsPipelineTransition, maskNumber, mask);
    }
}

void ContextVk::updateViewport(FramebufferVk *framebufferVk,
                               const gl::Rectangle &viewport,
                               float nearPlane,
                               float farPlane,
                               bool invertViewport)
{
    VkViewport vkViewport;
    gl_vk::GetViewport(viewport, nearPlane, farPlane, invertViewport,
                       framebufferVk->getState().getDimensions().height, &vkViewport);
    mGraphicsPipelineDesc->updateViewport(&mGraphicsPipelineTransition, vkViewport);
    invalidateDriverUniforms();
}

void ContextVk::updateDepthRange(float nearPlane, float farPlane)
{
    invalidateDriverUniforms();
    mGraphicsPipelineDesc->updateDepthRange(&mGraphicsPipelineTransition, nearPlane, farPlane);
}

void ContextVk::updateScissor(const gl::State &glState)
{
    FramebufferVk *framebufferVk = vk::GetImpl(glState.getDrawFramebuffer());
    gl::Rectangle renderArea     = framebufferVk->getCompleteRenderArea();

    // Clip the render area to the viewport.
    gl::Rectangle viewportClippedRenderArea;
    gl::ClipRectangle(renderArea, glState.getViewport(), &viewportClippedRenderArea);

    gl::Rectangle scissoredArea = ClipRectToScissor(getState(), viewportClippedRenderArea, false);
    if (isViewportFlipEnabledForDrawFBO())
    {
        scissoredArea.y = renderArea.height - scissoredArea.y - scissoredArea.height;
    }

    if (getRenderer()->getFeatures().forceNonZeroScissor.enabled && scissoredArea.width == 0 &&
        scissoredArea.height == 0)
    {
        // There is no overlap between the app-set viewport and clippedRect.  This code works
        // around an Intel driver bug that causes the driver to treat a (0,0,0,0) scissor as if
        // scissoring is disabled.  In this case, set the scissor to be just outside of the
        // renderArea.  Remove this work-around when driver version 25.20.100.6519 has been
        // deployed.  http://anglebug.com/3407
        scissoredArea.x      = renderArea.x;
        scissoredArea.y      = renderArea.y;
        scissoredArea.width  = 1;
        scissoredArea.height = 1;
    }
    mGraphicsPipelineDesc->updateScissor(&mGraphicsPipelineTransition,
                                         gl_vk::GetRect(scissoredArea));

    framebufferVk->onScissorChange(this);
}

angle::Result ContextVk::syncState(const gl::Context *context,
                                   const gl::State::DirtyBits &dirtyBits,
                                   const gl::State::DirtyBits &bitMask)
{
    const gl::State &glState = context->getState();

    if ((dirtyBits & mPipelineDirtyBitsMask).any() &&
        (glState.getProgram() == nullptr || !glState.getProgram()->isCompute()))
    {
        invalidateVertexAndIndexBuffers();
    }

    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::State::DIRTY_BIT_SCISSOR_TEST_ENABLED:
            case gl::State::DIRTY_BIT_SCISSOR:
                updateScissor(glState);
                break;
            case gl::State::DIRTY_BIT_VIEWPORT:
            {
                FramebufferVk *framebufferVk = vk::GetImpl(glState.getDrawFramebuffer());
                updateViewport(framebufferVk, glState.getViewport(), glState.getNearPlane(),
                               glState.getFarPlane(), isViewportFlipEnabledForDrawFBO());
                // Update the scissor, which will be constrained to the viewport
                updateScissor(glState);
                break;
            }
            case gl::State::DIRTY_BIT_DEPTH_RANGE:
                updateDepthRange(glState.getNearPlane(), glState.getFarPlane());
                break;
            case gl::State::DIRTY_BIT_BLEND_ENABLED:
                mGraphicsPipelineDesc->updateBlendEnabled(&mGraphicsPipelineTransition,
                                                          glState.isBlendEnabled());
                break;
            case gl::State::DIRTY_BIT_BLEND_COLOR:
                mGraphicsPipelineDesc->updateBlendColor(&mGraphicsPipelineTransition,
                                                        glState.getBlendColor());
                break;
            case gl::State::DIRTY_BIT_BLEND_FUNCS:
                mGraphicsPipelineDesc->updateBlendFuncs(&mGraphicsPipelineTransition,
                                                        glState.getBlendState());
                break;
            case gl::State::DIRTY_BIT_BLEND_EQUATIONS:
                mGraphicsPipelineDesc->updateBlendEquations(&mGraphicsPipelineTransition,
                                                            glState.getBlendState());
                break;
            case gl::State::DIRTY_BIT_COLOR_MASK:
                updateColorMask(glState.getBlendState());
                break;
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED:
                mGraphicsPipelineDesc->updateAlphaToCoverageEnable(
                    &mGraphicsPipelineTransition, glState.isSampleAlphaToCoverageEnabled());
                break;
            case gl::State::DIRTY_BIT_SAMPLE_COVERAGE_ENABLED:
                updateSampleMask(glState);
                break;
            case gl::State::DIRTY_BIT_SAMPLE_COVERAGE:
                updateSampleMask(glState);
                break;
            case gl::State::DIRTY_BIT_SAMPLE_MASK_ENABLED:
                updateSampleMask(glState);
                break;
            case gl::State::DIRTY_BIT_SAMPLE_MASK:
                updateSampleMask(glState);
                break;
            case gl::State::DIRTY_BIT_DEPTH_TEST_ENABLED:
                mGraphicsPipelineDesc->updateDepthTestEnabled(&mGraphicsPipelineTransition,
                                                              glState.getDepthStencilState(),
                                                              glState.getDrawFramebuffer());
                break;
            case gl::State::DIRTY_BIT_DEPTH_FUNC:
                mGraphicsPipelineDesc->updateDepthFunc(&mGraphicsPipelineTransition,
                                                       glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_DEPTH_MASK:
                mGraphicsPipelineDesc->updateDepthWriteEnabled(&mGraphicsPipelineTransition,
                                                               glState.getDepthStencilState(),
                                                               glState.getDrawFramebuffer());
                break;
            case gl::State::DIRTY_BIT_STENCIL_TEST_ENABLED:
                mGraphicsPipelineDesc->updateStencilTestEnabled(&mGraphicsPipelineTransition,
                                                                glState.getDepthStencilState(),
                                                                glState.getDrawFramebuffer());
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_FRONT:
                mGraphicsPipelineDesc->updateStencilFrontFuncs(&mGraphicsPipelineTransition,
                                                               glState.getStencilRef(),
                                                               glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_STENCIL_FUNCS_BACK:
                mGraphicsPipelineDesc->updateStencilBackFuncs(&mGraphicsPipelineTransition,
                                                              glState.getStencilBackRef(),
                                                              glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_STENCIL_OPS_FRONT:
                mGraphicsPipelineDesc->updateStencilFrontOps(&mGraphicsPipelineTransition,
                                                             glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_STENCIL_OPS_BACK:
                mGraphicsPipelineDesc->updateStencilBackOps(&mGraphicsPipelineTransition,
                                                            glState.getDepthStencilState());
                break;
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT:
                mGraphicsPipelineDesc->updateStencilFrontWriteMask(&mGraphicsPipelineTransition,
                                                                   glState.getDepthStencilState(),
                                                                   glState.getDrawFramebuffer());
                break;
            case gl::State::DIRTY_BIT_STENCIL_WRITEMASK_BACK:
                mGraphicsPipelineDesc->updateStencilBackWriteMask(&mGraphicsPipelineTransition,
                                                                  glState.getDepthStencilState(),
                                                                  glState.getDrawFramebuffer());
                break;
            case gl::State::DIRTY_BIT_CULL_FACE_ENABLED:
            case gl::State::DIRTY_BIT_CULL_FACE:
                mGraphicsPipelineDesc->updateCullMode(&mGraphicsPipelineTransition,
                                                      glState.getRasterizerState());
                break;
            case gl::State::DIRTY_BIT_FRONT_FACE:
                mGraphicsPipelineDesc->updateFrontFace(&mGraphicsPipelineTransition,
                                                       glState.getRasterizerState(),
                                                       isViewportFlipEnabledForDrawFBO());
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED:
                mGraphicsPipelineDesc->updatePolygonOffsetFillEnabled(
                    &mGraphicsPipelineTransition, glState.isPolygonOffsetFillEnabled());
                break;
            case gl::State::DIRTY_BIT_POLYGON_OFFSET:
                mGraphicsPipelineDesc->updatePolygonOffset(&mGraphicsPipelineTransition,
                                                           glState.getRasterizerState());
                break;
            case gl::State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED:
                mGraphicsPipelineDesc->updateRasterizerDiscardEnabled(
                    &mGraphicsPipelineTransition, glState.isRasterizerDiscardEnabled());
                break;
            case gl::State::DIRTY_BIT_LINE_WIDTH:
                mGraphicsPipelineDesc->updateLineWidth(&mGraphicsPipelineTransition,
                                                       glState.getLineWidth());
                break;
            case gl::State::DIRTY_BIT_PRIMITIVE_RESTART_ENABLED:
                mGraphicsPipelineDesc->updatePrimitiveRestartEnabled(
                    &mGraphicsPipelineTransition, glState.isPrimitiveRestartEnabled());
                break;
            case gl::State::DIRTY_BIT_CLEAR_COLOR:
                mClearColorValue.color.float32[0] = glState.getColorClearValue().red;
                mClearColorValue.color.float32[1] = glState.getColorClearValue().green;
                mClearColorValue.color.float32[2] = glState.getColorClearValue().blue;
                mClearColorValue.color.float32[3] = glState.getColorClearValue().alpha;
                break;
            case gl::State::DIRTY_BIT_CLEAR_DEPTH:
                mClearDepthStencilValue.depthStencil.depth = glState.getDepthClearValue();
                break;
            case gl::State::DIRTY_BIT_CLEAR_STENCIL:
                mClearDepthStencilValue.depthStencil.stencil =
                    static_cast<uint32_t>(glState.getStencilClearValue());
                break;
            case gl::State::DIRTY_BIT_UNPACK_STATE:
                // This is a no-op, its only important to use the right unpack state when we do
                // setImage or setSubImage in TextureVk, which is plumbed through the frontend call
                break;
            case gl::State::DIRTY_BIT_UNPACK_BUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_PACK_STATE:
                // This is a no-op, its only important to use the right pack state when we do
                // call readPixels later on.
                break;
            case gl::State::DIRTY_BIT_PACK_BUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_DITHER_ENABLED:
                break;
            case gl::State::DIRTY_BIT_GENERATE_MIPMAP_HINT:
                break;
            case gl::State::DIRTY_BIT_SHADER_DERIVATIVE_HINT:
                break;
            case gl::State::DIRTY_BIT_READ_FRAMEBUFFER_BINDING:
                updateFlipViewportReadFramebuffer(context->getState());
                break;
            case gl::State::DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING:
            {
                // FramebufferVk::syncState signals that we should start a new command buffer. But
                // changing the binding can skip FramebufferVk::syncState if the Framebuffer has no
                // dirty bits. Thus we need to explicitly clear the current command buffer to
                // ensure we start a new one. Note that we need a new command buffer because a
                // command graph node can only support one RenderPass configuration at a time.
                onRenderPassFinished();

                mDrawFramebuffer = vk::GetImpl(glState.getDrawFramebuffer());
                updateFlipViewportDrawFramebuffer(glState);
                updateViewport(mDrawFramebuffer, glState.getViewport(), glState.getNearPlane(),
                               glState.getFarPlane(), isViewportFlipEnabledForDrawFBO());
                updateColorMask(glState.getBlendState());
                updateSampleMask(glState);
                mGraphicsPipelineDesc->updateRasterizationSamples(&mGraphicsPipelineTransition,
                                                                  mDrawFramebuffer->getSamples());
                mGraphicsPipelineDesc->updateFrontFace(&mGraphicsPipelineTransition,
                                                       glState.getRasterizerState(),
                                                       isViewportFlipEnabledForDrawFBO());
                updateScissor(glState);
                mGraphicsPipelineDesc->updateDepthTestEnabled(&mGraphicsPipelineTransition,
                                                              glState.getDepthStencilState(),
                                                              glState.getDrawFramebuffer());
                mGraphicsPipelineDesc->updateDepthWriteEnabled(&mGraphicsPipelineTransition,
                                                               glState.getDepthStencilState(),
                                                               glState.getDrawFramebuffer());
                mGraphicsPipelineDesc->updateStencilTestEnabled(&mGraphicsPipelineTransition,
                                                                glState.getDepthStencilState(),
                                                                glState.getDrawFramebuffer());
                mGraphicsPipelineDesc->updateStencilFrontWriteMask(&mGraphicsPipelineTransition,
                                                                   glState.getDepthStencilState(),
                                                                   glState.getDrawFramebuffer());
                mGraphicsPipelineDesc->updateStencilBackWriteMask(&mGraphicsPipelineTransition,
                                                                  glState.getDepthStencilState(),
                                                                  glState.getDrawFramebuffer());
                mGraphicsPipelineDesc->updateRenderPassDesc(&mGraphicsPipelineTransition,
                                                            mDrawFramebuffer->getRenderPassDesc());
                invalidateCurrentTransformFeedbackBuffers();
                break;
            }
            case gl::State::DIRTY_BIT_RENDERBUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_VERTEX_ARRAY_BINDING:
            {
                mVertexArray = vk::GetImpl(glState.getVertexArray());
                invalidateDefaultAttributes(context->getStateCache().getActiveDefaultAttribsMask());
                break;
            }
            case gl::State::DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_DISPATCH_INDIRECT_BUFFER_BINDING:
                break;
            case gl::State::DIRTY_BIT_PROGRAM_BINDING:
                mProgram = vk::GetImpl(glState.getProgram());
                break;
            case gl::State::DIRTY_BIT_PROGRAM_EXECUTABLE:
            {
                invalidateCurrentTextures();
                invalidateCurrentShaderResources();
                if (glState.getProgram()->isCompute())
                {
                    invalidateCurrentComputePipeline();
                }
                else
                {
                    // No additional work is needed here. We will update the pipeline desc later.
                    invalidateDefaultAttributes(
                        context->getStateCache().getActiveDefaultAttribsMask());
                    bool useVertexBuffer = (mProgram->getState().getMaxActiveAttribLocation());
                    mNonIndexedDirtyBitsMask.set(DIRTY_BIT_VERTEX_BUFFERS, useVertexBuffer);
                    mIndexedDirtyBitsMask.set(DIRTY_BIT_VERTEX_BUFFERS, useVertexBuffer);
                    mCurrentGraphicsPipeline = nullptr;
                    mGraphicsPipelineTransition.reset();
                }
                break;
            }
            case gl::State::DIRTY_BIT_TEXTURE_BINDINGS:
                invalidateCurrentTextures();
                break;
            case gl::State::DIRTY_BIT_SAMPLER_BINDINGS:
                invalidateCurrentTextures();
                break;
            case gl::State::DIRTY_BIT_TRANSFORM_FEEDBACK_BINDING:
                // Nothing to do.
                break;
            case gl::State::DIRTY_BIT_SHADER_STORAGE_BUFFER_BINDING:
                invalidateCurrentShaderResources();
                break;
            case gl::State::DIRTY_BIT_UNIFORM_BUFFER_BINDINGS:
                invalidateCurrentShaderResources();
                break;
            case gl::State::DIRTY_BIT_ATOMIC_COUNTER_BUFFER_BINDING:
                invalidateCurrentShaderResources();
                break;
            case gl::State::DIRTY_BIT_IMAGE_BINDINGS:
                break;
            case gl::State::DIRTY_BIT_MULTISAMPLING:
                // TODO(syoussefi): this should configure the pipeline to render as if
                // single-sampled, and write the results to all samples of a pixel regardless of
                // coverage. See EXT_multisample_compatibility.  http://anglebug.com/3204
                break;
            case gl::State::DIRTY_BIT_SAMPLE_ALPHA_TO_ONE:
                // TODO(syoussefi): this is part of EXT_multisample_compatibility.  The alphaToOne
                // Vulkan feature should be enabled to support this extension.
                // http://anglebug.com/3204
                mGraphicsPipelineDesc->updateAlphaToOneEnable(&mGraphicsPipelineTransition,
                                                              glState.isSampleAlphaToOneEnabled());
                break;
            case gl::State::DIRTY_BIT_COVERAGE_MODULATION:
                break;
            case gl::State::DIRTY_BIT_PATH_RENDERING:
                break;
            case gl::State::DIRTY_BIT_FRAMEBUFFER_SRGB:
                break;
            case gl::State::DIRTY_BIT_CURRENT_VALUES:
            {
                invalidateDefaultAttributes(glState.getAndResetDirtyCurrentValues());
                break;
            }
            case gl::State::DIRTY_BIT_PROVOKING_VERTEX:
                break;
            default:
                UNREACHABLE();
                break;
        }
    }

    return angle::Result::Continue;
}

GLint ContextVk::getGPUDisjoint()
{
    // No extension seems to be available to query this information.
    return 0;
}

GLint64 ContextVk::getTimestamp()
{
    uint64_t timestamp = 0;

    (void)getTimestamp(&timestamp);

    return static_cast<GLint64>(timestamp);
}

angle::Result ContextVk::onMakeCurrent(const gl::Context *context)
{
    ASSERT(mCommandGraph.empty());
    mCurrentQueueSerial = getRenderer()->nextSerial();

    // Flip viewports if FeaturesVk::flipViewportY is enabled and the user did not request that the
    // surface is flipped.
    egl::Surface *drawSurface = context->getCurrentDrawSurface();
    mFlipYForCurrentSurface =
        drawSurface != nullptr && mRenderer->getFeatures().flipViewportY.enabled &&
        !IsMaskFlagSet(drawSurface->getOrientation(), EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE);

    if (drawSurface && drawSurface->getType() == EGL_WINDOW_BIT)
    {
        mCurrentWindowSurface = GetImplAs<WindowSurfaceVk>(drawSurface);
    }
    else
    {
        mCurrentWindowSurface = nullptr;
    }

    const gl::State &glState = context->getState();
    updateFlipViewportDrawFramebuffer(glState);
    updateFlipViewportReadFramebuffer(glState);
    invalidateDriverUniforms();

    return angle::Result::Continue;
}

angle::Result ContextVk::onUnMakeCurrent(const gl::Context *context)
{
    ANGLE_TRY(flushImpl(nullptr));
    mCurrentWindowSurface = nullptr;
    return angle::Result::Continue;
}

void ContextVk::updateFlipViewportDrawFramebuffer(const gl::State &glState)
{
    gl::Framebuffer *drawFramebuffer = glState.getDrawFramebuffer();
    mFlipViewportForDrawFramebuffer =
        drawFramebuffer->isDefault() && mRenderer->getFeatures().flipViewportY.enabled;
}

void ContextVk::updateFlipViewportReadFramebuffer(const gl::State &glState)
{
    gl::Framebuffer *readFramebuffer = glState.getReadFramebuffer();
    mFlipViewportForReadFramebuffer =
        readFramebuffer->isDefault() && mRenderer->getFeatures().flipViewportY.enabled;
}

gl::Caps ContextVk::getNativeCaps() const
{
    return mRenderer->getNativeCaps();
}

const gl::TextureCapsMap &ContextVk::getNativeTextureCaps() const
{
    return mRenderer->getNativeTextureCaps();
}

const gl::Extensions &ContextVk::getNativeExtensions() const
{
    return mRenderer->getNativeExtensions();
}

const gl::Limitations &ContextVk::getNativeLimitations() const
{
    return mRenderer->getNativeLimitations();
}

CompilerImpl *ContextVk::createCompiler()
{
    return new CompilerVk();
}

ShaderImpl *ContextVk::createShader(const gl::ShaderState &state)
{
    return new ShaderVk(state);
}

ProgramImpl *ContextVk::createProgram(const gl::ProgramState &state)
{
    return new ProgramVk(state);
}

FramebufferImpl *ContextVk::createFramebuffer(const gl::FramebufferState &state)
{
    return FramebufferVk::CreateUserFBO(mRenderer, state);
}

TextureImpl *ContextVk::createTexture(const gl::TextureState &state)
{
    return new TextureVk(state, mRenderer);
}

RenderbufferImpl *ContextVk::createRenderbuffer(const gl::RenderbufferState &state)
{
    return new RenderbufferVk(state);
}

BufferImpl *ContextVk::createBuffer(const gl::BufferState &state)
{
    return new BufferVk(state);
}

VertexArrayImpl *ContextVk::createVertexArray(const gl::VertexArrayState &state)
{
    return new VertexArrayVk(this, state);
}

QueryImpl *ContextVk::createQuery(gl::QueryType type)
{
    return new QueryVk(type);
}

FenceNVImpl *ContextVk::createFenceNV()
{
    return new FenceNVVk();
}

SyncImpl *ContextVk::createSync()
{
    return new SyncVk();
}

TransformFeedbackImpl *ContextVk::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedbackVk(state);
}

SamplerImpl *ContextVk::createSampler(const gl::SamplerState &state)
{
    return new SamplerVk(state);
}

ProgramPipelineImpl *ContextVk::createProgramPipeline(const gl::ProgramPipelineState &state)
{
    return new ProgramPipelineVk(state);
}

std::vector<PathImpl *> ContextVk::createPaths(GLsizei)
{
    return std::vector<PathImpl *>();
}

MemoryObjectImpl *ContextVk::createMemoryObject()
{
    return new MemoryObjectVk();
}

SemaphoreImpl *ContextVk::createSemaphore()
{
    return new SemaphoreVk();
}

void ContextVk::invalidateCurrentTextures()
{
    ASSERT(mProgram);
    if (mProgram->hasTextures())
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_TEXTURES);
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
        mComputeDirtyBits.set(DIRTY_BIT_TEXTURES);
        mComputeDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }
}

void ContextVk::invalidateCurrentShaderResources()
{
    ASSERT(mProgram);
    if (mProgram->hasUniformBuffers() || mProgram->hasStorageBuffers() ||
        mProgram->hasAtomicCounterBuffers())
    {
        mGraphicsDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);
        mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
        mComputeDirtyBits.set(DIRTY_BIT_SHADER_RESOURCES);
        mComputeDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
    }
}

void ContextVk::invalidateDriverUniforms()
{
    mGraphicsDirtyBits.set(DIRTY_BIT_DRIVER_UNIFORMS);
    mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
}

void ContextVk::onDrawFramebufferChange(FramebufferVk *framebufferVk)
{
    const vk::RenderPassDesc &renderPassDesc = framebufferVk->getRenderPassDesc();

    // Ensure that the RenderPass description is updated.
    invalidateCurrentGraphicsPipeline();
    mGraphicsPipelineDesc->updateRenderPassDesc(&mGraphicsPipelineTransition, renderPassDesc);
}

void ContextVk::invalidateCurrentTransformFeedbackBuffers()
{
    mGraphicsDirtyBits.set(DIRTY_BIT_TRANSFORM_FEEDBACK_BUFFERS);
    mGraphicsDirtyBits.set(DIRTY_BIT_DESCRIPTOR_SETS);
}

void ContextVk::onTransformFeedbackPauseResume()
{
    invalidateDriverUniforms();
}

angle::Result ContextVk::dispatchCompute(const gl::Context *context,
                                         GLuint numGroupsX,
                                         GLuint numGroupsY,
                                         GLuint numGroupsZ)
{
    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(setupDispatch(context, &commandBuffer));

    commandBuffer->dispatch(numGroupsX, numGroupsY, numGroupsZ);

    return angle::Result::Continue;
}

angle::Result ContextVk::dispatchComputeIndirect(const gl::Context *context, GLintptr indirect)
{
    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(setupDispatch(context, &commandBuffer));

    gl::Buffer *glBuffer     = getState().getTargetBuffer(gl::BufferBinding::DispatchIndirect);
    vk::BufferHelper &buffer = vk::GetImpl(glBuffer)->getBuffer();
    buffer.onRead(&mDispatcher, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

    commandBuffer->dispatchIndirect(buffer.getBuffer(), indirect);

    return angle::Result::Continue;
}

angle::Result ContextVk::memoryBarrier(const gl::Context *context, GLbitfield barriers)
{
    // Note: most of the barriers specified here don't require us to issue a memory barrier, as the
    // relevant resources already insert the appropriate barriers.  They do however require the
    // resource writing nodes to finish so future buffer barriers are placed correctly, as well as
    // resource dependencies not creating a graph loop.  This is done by inserting a command graph
    // barrier that does nothing!

    VkAccessFlags srcAccess = 0;
    VkAccessFlags dstAccess = 0;

    if ((barriers & GL_COMMAND_BARRIER_BIT) != 0)
    {
        srcAccess |= VK_ACCESS_SHADER_WRITE_BIT;
        dstAccess |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }

    mCommandGraph.memoryBarrier(srcAccess, dstAccess, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
    return angle::Result::Continue;
}

angle::Result ContextVk::memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers)
{
    // There aren't any barrier bits here that aren't otherwise automatically handled.  We only
    // need to make sure writer resources (framebuffers and the dispatcher) start a new node.
    //
    // Note: memoryBarrierByRegion is expected to affect only the fragment pipeline.  Specifying
    // that here is currently unnecessary, but is a reminder of this fact in case we do need to
    // especially handle some future barrier bit.

    mCommandGraph.memoryBarrier(0, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    return angle::Result::Continue;
}

vk::DynamicQueryPool *ContextVk::getQueryPool(gl::QueryType queryType)
{
    ASSERT(queryType == gl::QueryType::AnySamples ||
           queryType == gl::QueryType::AnySamplesConservative ||
           queryType == gl::QueryType::Timestamp || queryType == gl::QueryType::TimeElapsed);
    ASSERT(mQueryPools[queryType].isValid());
    return &mQueryPools[queryType];
}

const VkClearValue &ContextVk::getClearColorValue() const
{
    return mClearColorValue;
}

const VkClearValue &ContextVk::getClearDepthStencilValue() const
{
    return mClearDepthStencilValue;
}

VkColorComponentFlags ContextVk::getClearColorMask() const
{
    return mClearColorMask;
}

angle::Result ContextVk::handleDirtyGraphicsDriverUniforms(const gl::Context *context,
                                                           vk::CommandBuffer *commandBuffer)
{
    // Release any previously retained buffers.
    mDriverUniformsBuffer.releaseInFlightBuffers(this);

    const gl::Rectangle &glViewport = mState.getViewport();
    float halfRenderAreaHeight =
        static_cast<float>(mDrawFramebuffer->getState().getDimensions().height) * 0.5f;

    // Allocate a new region in the dynamic buffer.
    uint8_t *ptr;
    VkBuffer buffer;
    VkDeviceSize offset;
    bool newBuffer;
    ANGLE_TRY(mDriverUniformsBuffer.allocate(this, sizeof(DriverUniforms), &ptr, &buffer, &offset,
                                             &newBuffer));
    float scaleY = isViewportFlipEnabledForDrawFBO() ? -1.0f : 1.0f;

    mDriverUniformsDynamicOffset = static_cast<uint32_t>(offset);

    uint32_t xfbActiveUnpaused = mState.isTransformFeedbackActiveUnpaused();

    float depthRangeNear = mState.getNearPlane();
    float depthRangeFar  = mState.getFarPlane();
    float depthRangeDiff = depthRangeFar - depthRangeNear;

    // Copy and flush to the device.
    DriverUniforms *driverUniforms = reinterpret_cast<DriverUniforms *>(ptr);
    *driverUniforms                = {
        {static_cast<float>(glViewport.x), static_cast<float>(glViewport.y),
         static_cast<float>(glViewport.width), static_cast<float>(glViewport.height)},
        halfRenderAreaHeight,
        scaleY,
        -scaleY,
        xfbActiveUnpaused,
        {},
        {depthRangeNear, depthRangeFar, depthRangeDiff, 0.0f}};

    if (xfbActiveUnpaused)
    {
        TransformFeedbackVk *transformFeedbackVk =
            vk::GetImpl(mState.getCurrentTransformFeedback());
        transformFeedbackVk->getBufferOffsets(this, mState.getProgram()->getState(), mXfbBaseVertex,
                                              driverUniforms->xfbBufferOffsets.data(),
                                              driverUniforms->xfbBufferOffsets.size());
    }

    ANGLE_TRY(mDriverUniformsBuffer.flush(this));

    if (newBuffer)
    {
        // Allocate a new descriptor set.
        ANGLE_TRY(mDriverUniformsDescriptorPool.allocateSets(
            this, mDriverUniformsSetLayout.get().ptr(), 1, &mDriverUniformsDescriptorPoolBinding,
            &mDriverUniformsDescriptorSet));

        // Update the driver uniform descriptor set.
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer                 = buffer;
        bufferInfo.offset                 = 0;
        bufferInfo.range                  = sizeof(DriverUniforms);

        VkWriteDescriptorSet writeInfo = {};
        writeInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeInfo.dstSet               = mDriverUniformsDescriptorSet;
        writeInfo.dstBinding           = 0;
        writeInfo.dstArrayElement      = 0;
        writeInfo.descriptorCount      = 1;
        writeInfo.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        writeInfo.pImageInfo           = nullptr;
        writeInfo.pTexelBufferView     = nullptr;
        writeInfo.pBufferInfo          = &bufferInfo;

        vkUpdateDescriptorSets(getDevice(), 1, &writeInfo, 0, nullptr);
    }

    return angle::Result::Continue;
}

void ContextVk::handleError(VkResult errorCode,
                            const char *file,
                            const char *function,
                            unsigned int line)
{
    ASSERT(errorCode != VK_SUCCESS);

    GLenum glErrorCode = DefaultGLErrorCode(errorCode);

    std::stringstream errorStream;
    errorStream << "Internal Vulkan error: " << VulkanResultString(errorCode) << ".";

    if (errorCode == VK_ERROR_DEVICE_LOST)
    {
        WARN() << errorStream.str();
        handleDeviceLost();
    }

    mErrors->handleError(glErrorCode, errorStream.str().c_str(), file, function, line);
}

angle::Result ContextVk::updateActiveTextures(const gl::Context *context,
                                              vk::CommandGraphResource *recorder)
{
    const gl::State &glState   = mState;
    const gl::Program *program = glState.getProgram();

    uint32_t prevMaxIndex = mActiveTexturesDesc.getMaxIndex();
    memset(mActiveTextures.data(), 0, sizeof(mActiveTextures[0]) * prevMaxIndex);
    mActiveTexturesDesc.reset();

    const gl::ActiveTexturePointerArray &textures  = glState.getActiveTexturesCache();
    const gl::ActiveTextureMask &activeTextures    = program->getActiveSamplersMask();
    const gl::ActiveTextureTypeArray &textureTypes = program->getActiveSamplerTypes();

    const auto &uniforms = program->getState().getUniforms();
    for (size_t textureUnit : activeTextures)
    {
        gl::Texture *texture        = textures[textureUnit];
        gl::TextureType textureType = textureTypes[textureUnit];

        // Null textures represent incomplete textures.
        if (texture == nullptr)
        {
            ANGLE_TRY(getIncompleteTexture(context, textureType, &texture));
        }

        TextureVk *textureVk = vk::GetImpl(texture);

        vk::ImageHelper &image = textureVk->getImage();

        // The image should be flushed and ready to use at this point. There may still be lingering
        // staged updates in its staging buffer for unused texture mip levels or layers. Therefore
        // we can't verify it has no staged updates right here.

        // Find out the image is used in which shader stage.
        vk::ImageLayout textureLayout = vk::ImageLayout::FragmentShaderReadOnly;
        if (program->isCompute())
        {
            textureLayout = vk::ImageLayout::ComputeShaderReadOnly;
        }
        else if (uniforms[textureUnit].isActive(gl::ShaderType::Vertex))
        {
            textureLayout = vk::ImageLayout::AllGraphicsShadersReadOnly;
        }

        // Ensure the image is in read-only layout
        if (image.isLayoutChangeNecessary(textureLayout))
        {
            vk::CommandBuffer *srcLayoutChange;
            ANGLE_TRY(image.recordCommands(this, &srcLayoutChange));

            VkImageAspectFlags aspectFlags = image.getAspectFlags();
            ASSERT(aspectFlags != 0);
            image.changeLayout(aspectFlags, textureLayout, srcLayoutChange);
        }

        image.addReadDependency(recorder);

        mActiveTextures[textureUnit] = textureVk;
        mActiveTexturesDesc.update(textureUnit, textureVk->getSerial());
    }

    return angle::Result::Continue;
}

const gl::ActiveTextureArray<TextureVk *> &ContextVk::getActiveTextures() const
{
    return mActiveTextures;
}

void ContextVk::insertWaitSemaphore(const vk::Semaphore *waitSemaphore)
{
    ASSERT(waitSemaphore);
    mWaitSemaphores.push_back(waitSemaphore->getHandle());
}

angle::Result ContextVk::flushImpl(const vk::Semaphore *signalSemaphore)
{
    if (mCommandGraph.empty() && !signalSemaphore && mWaitSemaphores.empty())
    {
        return angle::Result::Continue;
    }

    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::flush");

    vk::Scoped<vk::PrimaryCommandBuffer> commandBatch(getDevice());
    if (!mCommandGraph.empty())
    {
        ANGLE_TRY(flushCommandGraph(&commandBatch.get()));
    }

    waitForSwapchainImageIfNecessary();

    VkSubmitInfo submitInfo = {};
    InitializeSubmitInfo(&submitInfo, commandBatch.get(), mWaitSemaphores,
                         &mWaitSemaphoreStageMasks, signalSemaphore);

    ANGLE_TRY(submitFrame(submitInfo, commandBatch.release()));

    mWaitSemaphores.clear();

    return angle::Result::Continue;
}

angle::Result ContextVk::finishImpl()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "ContextVk::finish");

    ANGLE_TRY(flushImpl(nullptr));

    ANGLE_TRY(finishToSerial(mLastSubmittedQueueSerial));
    freeAllInFlightResources();

    if (mGpuEventsEnabled)
    {
        // This loop should in practice execute once since the queue is already idle.
        while (mInFlightGpuEventQueries.size() > 0)
        {
            ANGLE_TRY(checkCompletedGpuEvents());
        }
        // Recalculate the CPU/GPU time difference to account for clock drifting.  Avoid unnecessary
        // synchronization if there is no event to be adjusted (happens when finish() gets called
        // multiple times towards the end of the application).
        if (mGpuEvents.size() > 0)
        {
            ANGLE_TRY(synchronizeCpuGpuTime());
        }
    }

    return angle::Result::Continue;
}

void ContextVk::addWaitSemaphore(VkSemaphore semaphore)
{
    mWaitSemaphores.push_back(semaphore);
}

const vk::CommandPool &ContextVk::getCommandPool() const
{
    return mCommandPool;
}

bool ContextVk::isSerialInUse(Serial serial) const
{
    return serial > mLastCompletedQueueSerial;
}

angle::Result ContextVk::checkCompletedCommands()
{
    VkDevice device = getDevice();

    int finishedCount = 0;

    for (CommandBatch &batch : mInFlightCommands)
    {
        VkResult result = batch.fence.get().getStatus(device);
        if (result == VK_NOT_READY)
        {
            break;
        }
        ANGLE_VK_TRY(this, result);

        ASSERT(batch.serial > mLastCompletedQueueSerial);
        mLastCompletedQueueSerial = batch.serial;

        batch.fence.reset(device);
        ANGLE_TRACE_EVENT0("gpu.angle", "commandPool.destroy");
        batch.commandPool.destroy(device);
        ++finishedCount;
    }

    mInFlightCommands.erase(mInFlightCommands.begin(), mInFlightCommands.begin() + finishedCount);

    size_t freeIndex = 0;
    for (; freeIndex < mGarbage.size(); ++freeIndex)
    {
        if (!mGarbage[freeIndex].destroyIfComplete(device, mLastCompletedQueueSerial))
            break;
    }

    // Remove the entries from the garbage list - they should be ready to go.
    if (freeIndex > 0)
    {
        mGarbage.erase(mGarbage.begin(), mGarbage.begin() + freeIndex);
    }

    return angle::Result::Continue;
}

angle::Result ContextVk::finishToSerial(Serial serial)
{
    bool timedOut        = false;
    angle::Result result = finishToSerialOrTimeout(serial, kMaxFenceWaitTimeNs, &timedOut);

    // Don't tolerate timeout.  If such a large wait time results in timeout, something's wrong.
    if (timedOut)
    {
        result = angle::Result::Stop;
    }
    return result;
}

angle::Result ContextVk::finishToSerialOrTimeout(Serial serial, uint64_t timeout, bool *outTimedOut)
{
    *outTimedOut = false;

    if (!isSerialInUse(serial) || mInFlightCommands.empty())
    {
        return angle::Result::Continue;
    }

    // Find the first batch with serial equal to or bigger than given serial (note that
    // the batch serials are unique, otherwise upper-bound would have been necessary).
    size_t batchIndex = mInFlightCommands.size() - 1;
    for (size_t i = 0; i < mInFlightCommands.size(); ++i)
    {
        if (mInFlightCommands[i].serial >= serial)
        {
            batchIndex = i;
            break;
        }
    }
    const CommandBatch &batch = mInFlightCommands[batchIndex];

    // Wait for it finish
    VkDevice device = getDevice();
    VkResult status = batch.fence.get().wait(device, kMaxFenceWaitTimeNs);

    // If timed out, report it as such.
    if (status == VK_TIMEOUT)
    {
        *outTimedOut = true;
        return angle::Result::Continue;
    }

    ANGLE_VK_TRY(this, status);

    // Clean up finished batches.
    return checkCompletedCommands();
}

angle::Result ContextVk::getCompatibleRenderPass(const vk::RenderPassDesc &desc,
                                                 vk::RenderPass **renderPassOut)
{
    return mRenderPassCache.getCompatibleRenderPass(this, mCurrentQueueSerial, desc, renderPassOut);
}

angle::Result ContextVk::getRenderPassWithOps(const vk::RenderPassDesc &desc,
                                              const vk::AttachmentOpsArray &ops,
                                              vk::RenderPass **renderPassOut)
{
    return mRenderPassCache.getRenderPassWithOps(this, mCurrentQueueSerial, desc, ops,
                                                 renderPassOut);
}

angle::Result ContextVk::getNextSubmitFence(vk::Shared<vk::Fence> *sharedFenceOut)
{
    if (!mSubmitFence.isReferenced())
    {
        ANGLE_TRY(getRenderer()->newSharedFence(this, &mSubmitFence));
    }
    getRenderer()->resetSharedFence(sharedFenceOut);
    sharedFenceOut->copy(getDevice(), mSubmitFence);
    return angle::Result::Continue;
}

vk::Shared<vk::Fence> ContextVk::getLastSubmittedFence() const
{
    vk::Shared<vk::Fence> fence;
    if (!mInFlightCommands.empty())
    {
        fence.copy(getDevice(), mInFlightCommands.back().fence);
    }

    return fence;
}

vk::CommandGraph *ContextVk::getCommandGraph()
{
    return &mCommandGraph;
}

angle::Result ContextVk::getTimestamp(uint64_t *timestampOut)
{
    // The intent of this function is to query the timestamp without stalling the GPU.  Currently,
    // that seems impossible, so instead, we are going to make a small submission with just a
    // timestamp query.  First, the disjoint timer query extension says:
    //
    // > This will return the GL time after all previous commands have reached the GL server but
    // have not yet necessarily executed.
    //
    // The previous commands are stored in the command graph at the moment and are not yet flushed.
    // The wording allows us to make a submission to get the timestamp without performing a flush.
    //
    // Second:
    //
    // > By using a combination of this synchronous get command and the asynchronous timestamp query
    // object target, applications can measure the latency between when commands reach the GL server
    // and when they are realized in the framebuffer.
    //
    // This fits with the above strategy as well, although inevitably we are possibly introducing a
    // GPU bubble.  This function directly generates a command buffer and submits it instead of
    // using the other member functions.  This is to avoid changing any state, such as the queue
    // serial.

    // Create a query used to receive the GPU timestamp
    VkDevice device = getDevice();
    vk::Scoped<vk::DynamicQueryPool> timestampQueryPool(device);
    vk::QueryHelper timestampQuery;
    ANGLE_TRY(timestampQueryPool.get().init(this, VK_QUERY_TYPE_TIMESTAMP, 1));
    ANGLE_TRY(timestampQueryPool.get().allocateQuery(this, &timestampQuery));

    // Record the command buffer
    vk::Scoped<vk::PrimaryCommandBuffer> commandBatch(device);
    vk::PrimaryCommandBuffer &commandBuffer = commandBatch.get();

    VkCommandBufferAllocateInfo commandBufferInfo = {};
    commandBufferInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.commandPool                 = mCommandPool.getHandle();
    commandBufferInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferInfo.commandBufferCount          = 1;

    ANGLE_VK_TRY(this, commandBuffer.init(device, commandBufferInfo));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = 0;
    beginInfo.pInheritanceInfo         = nullptr;

    ANGLE_VK_TRY(this, commandBuffer.begin(beginInfo));

    commandBuffer.resetQueryPool(timestampQuery.getQueryPool()->getHandle(),
                                 timestampQuery.getQuery(), 1);
    commandBuffer.writeTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 timestampQuery.getQueryPool()->getHandle(),
                                 timestampQuery.getQuery());

    ANGLE_VK_TRY(this, commandBuffer.end());

    // Create fence for the submission
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = 0;

    vk::Scoped<vk::Fence> fence(device);
    ANGLE_VK_TRY(this, fence.get().init(device, fenceInfo));

    // Submit the command buffer
    VkSubmitInfo submitInfo         = {};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 0;
    submitInfo.pWaitSemaphores      = nullptr;
    submitInfo.pWaitDstStageMask    = nullptr;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = commandBuffer.ptr();
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores    = nullptr;

    ANGLE_TRY(getRenderer()->queueSubmit(this, submitInfo, fence.get()));

    // Wait for the submission to finish.  Given no semaphores, there is hope that it would execute
    // in parallel with what's already running on the GPU.
    ANGLE_VK_TRY(this, fence.get().wait(device, kMaxFenceWaitTimeNs));

    // Get the query results
    constexpr VkQueryResultFlags queryFlags = VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT;

    ANGLE_VK_TRY(this, timestampQuery.getQueryPool()->getResults(
                           device, timestampQuery.getQuery(), 1, sizeof(*timestampOut),
                           timestampOut, sizeof(*timestampOut), queryFlags));

    timestampQueryPool.get().freeQuery(this, &timestampQuery);

    // Convert results to nanoseconds.
    *timestampOut = static_cast<uint64_t>(
        *timestampOut *
        static_cast<double>(getRenderer()->getPhysicalDeviceProperties().limits.timestampPeriod));

    return angle::Result::Continue;
}

void ContextVk::invalidateDefaultAttribute(size_t attribIndex)
{
    mDirtyDefaultAttribsMask.set(attribIndex);
    mGraphicsDirtyBits.set(DIRTY_BIT_DEFAULT_ATTRIBS);
}

void ContextVk::invalidateDefaultAttributes(const gl::AttributesMask &dirtyMask)
{
    if (dirtyMask.any())
    {
        mDirtyDefaultAttribsMask |= dirtyMask;
        mGraphicsDirtyBits.set(DIRTY_BIT_DEFAULT_ATTRIBS);
    }
}

angle::Result ContextVk::updateDefaultAttribute(size_t attribIndex)
{
    vk::DynamicBuffer &defaultBuffer = mDefaultAttribBuffers[attribIndex];

    defaultBuffer.releaseInFlightBuffers(this);

    uint8_t *ptr;
    VkBuffer bufferHandle = VK_NULL_HANDLE;
    VkDeviceSize offset   = 0;
    ANGLE_TRY(
        defaultBuffer.allocate(this, kDefaultValueSize, &ptr, &bufferHandle, &offset, nullptr));

    const gl::State &glState = mState;
    const gl::VertexAttribCurrentValueData &defaultValue =
        glState.getVertexAttribCurrentValues()[attribIndex];
    memcpy(ptr, &defaultValue.Values, kDefaultValueSize);

    ANGLE_TRY(defaultBuffer.flush(this));

    mVertexArray->updateDefaultAttrib(this, attribIndex, bufferHandle,
                                      static_cast<uint32_t>(offset));
    return angle::Result::Continue;
}

void ContextVk::waitForSwapchainImageIfNecessary()
{
    if (mCurrentWindowSurface)
    {
        vk::Semaphore waitSemaphore = mCurrentWindowSurface->getAcquireImageSemaphore();
        if (waitSemaphore.valid())
        {
            addWaitSemaphore(waitSemaphore.getHandle());
            releaseObject(getCurrentQueueSerial(), &waitSemaphore);
        }
    }
}

vk::DescriptorSetLayoutDesc ContextVk::getDriverUniformsDescriptorSetDesc() const
{
    vk::DescriptorSetLayoutDesc desc;
    desc.update(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL_GRAPHICS);
    return desc;
}
}  // namespace rx

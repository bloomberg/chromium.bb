//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TransformFeedbackVk.cpp:
//    Implements the class methods for TransformFeedbackVk.
//

#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"

#include "libANGLE/Context.h"
#include "libANGLE/Query.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/QueryVk.h"

#include "common/debug.h"

namespace rx
{

TransformFeedbackVk::TransformFeedbackVk(const gl::TransformFeedbackState &state)
    : TransformFeedbackImpl(state)
{}

TransformFeedbackVk::~TransformFeedbackVk() {}

angle::Result TransformFeedbackVk::begin(const gl::Context *context,
                                         gl::PrimitiveMode primitiveMode)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // Make sure the transform feedback buffers are bound to the program descriptor sets.
    contextVk->invalidateCurrentTransformFeedbackBuffers();

    vk::GetImpl(context)->onTransformFeedbackPauseResume();
    onBeginEnd(context);

    return angle::Result::Continue;
}

angle::Result TransformFeedbackVk::end(const gl::Context *context)
{
    // If there's an active transform feedback query, accumulate the primitives drawn.
    const gl::State &glState = context->getState();
    gl::Query *transformFeedbackQuery =
        glState.getActiveQuery(gl::QueryType::TransformFeedbackPrimitivesWritten);
    if (transformFeedbackQuery)
    {
        vk::GetImpl(transformFeedbackQuery)->onTransformFeedbackEnd(context);
    }

    vk::GetImpl(context)->onTransformFeedbackPauseResume();
    onBeginEnd(context);

    return angle::Result::Continue;
}

angle::Result TransformFeedbackVk::pause(const gl::Context *context)
{
    vk::GetImpl(context)->onTransformFeedbackPauseResume();
    return angle::Result::Continue;
}

angle::Result TransformFeedbackVk::resume(const gl::Context *context)
{
    vk::GetImpl(context)->onTransformFeedbackPauseResume();
    return angle::Result::Continue;
}

angle::Result TransformFeedbackVk::bindIndexedBuffer(
    const gl::Context *context,
    size_t index,
    const gl::OffsetBindingPointer<gl::Buffer> &binding)
{
    RendererVk *rendererVk = vk::GetImpl(context)->getRenderer();
    const VkDeviceSize offsetAlignment =
        rendererVk->getPhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;

    // Make sure there's no possible under/overflow with binding size.
    static_assert(sizeof(VkDeviceSize) >= sizeof(binding.getSize()), "VkDeviceSize too small");

    mBoundBufferRanges[index].offset = binding.getOffset();
    mBoundBufferRanges[index].size   = gl::GetBoundBufferAvailableSize(binding);

    // Set the offset as close as possible to the requested offset while remaining aligned.
    mBoundBufferRanges[index].alignedOffset =
        (mBoundBufferRanges[index].offset / offsetAlignment) * offsetAlignment;

    return angle::Result::Continue;
}

void TransformFeedbackVk::updateDescriptorSetLayout(
    const gl::ProgramState &programState,
    vk::DescriptorSetLayoutDesc *descSetLayoutOut) const
{
    size_t xfbBufferCount = programState.getTransformFeedbackBufferCount();

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        descSetLayoutOut->update(kXfbBindingIndexStart + bufferIndex,
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT);
    }
}

void TransformFeedbackVk::addFramebufferDependency(ContextVk *contextVk,
                                                   const gl::ProgramState &programState,
                                                   vk::FramebufferHelper *framebuffer) const
{
    const std::vector<gl::OffsetBindingPointer<gl::Buffer>> &xfbBuffers =
        mState.getIndexedBuffers();
    size_t xfbBufferCount = programState.getTransformFeedbackBufferCount();

    ASSERT(xfbBufferCount > 0);
    ASSERT(programState.getTransformFeedbackBufferMode() != GL_INTERLEAVED_ATTRIBS ||
           xfbBufferCount == 1);

    // Set framebuffer dependent to the transform feedback buffers.  This is especially done
    // separately from |updateDescriptorSet|, to avoid introducing unnecessary buffer barriers
    // every time the descriptor set is updated (which, as the set is shared with default uniforms,
    // could get updated frequently).
    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding = xfbBuffers[bufferIndex];
        gl::Buffer *buffer                                        = bufferBinding.get();
        ASSERT(buffer != nullptr);

        vk::BufferHelper &bufferHelper = vk::GetImpl(buffer)->getBuffer();
        bufferHelper.onWrite(contextVk, framebuffer, 0, VK_ACCESS_SHADER_WRITE_BIT);
    }
}

void TransformFeedbackVk::updateDescriptorSet(ContextVk *contextVk,
                                              const gl::ProgramState &programState,
                                              VkDescriptorSet descSet) const
{
    const std::vector<gl::OffsetBindingPointer<gl::Buffer>> &xfbBuffers =
        mState.getIndexedBuffers();
    size_t xfbBufferCount = programState.getTransformFeedbackBufferCount();

    ASSERT(xfbBufferCount > 0);
    ASSERT(programState.getTransformFeedbackBufferMode() != GL_INTERLEAVED_ATTRIBS ||
           xfbBufferCount == 1);

    std::array<VkDescriptorBufferInfo, gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS>
        descriptorBufferInfo;

    // Write default uniforms for each shader type.
    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        VkDescriptorBufferInfo &bufferInfo  = descriptorBufferInfo[bufferIndex];
        const BoundBufferRange &bufferRange = mBoundBufferRanges[bufferIndex];

        const gl::OffsetBindingPointer<gl::Buffer> &bufferBinding = xfbBuffers[bufferIndex];
        gl::Buffer *buffer                                        = bufferBinding.get();
        ASSERT(buffer != nullptr);

        vk::BufferHelper &bufferHelper = vk::GetImpl(buffer)->getBuffer();

        bufferInfo.buffer = bufferHelper.getBuffer().getHandle();
        bufferInfo.offset = bufferRange.alignedOffset;
        bufferInfo.range  = bufferRange.size + (bufferRange.offset - bufferRange.alignedOffset);
    }

    VkDevice device = contextVk->getDevice();

    VkWriteDescriptorSet writeDescriptorInfo = {};
    writeDescriptorInfo.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorInfo.dstSet               = descSet;
    writeDescriptorInfo.dstBinding           = kXfbBindingIndexStart;
    writeDescriptorInfo.dstArrayElement      = 0;
    writeDescriptorInfo.descriptorCount      = xfbBufferCount;
    writeDescriptorInfo.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorInfo.pImageInfo           = nullptr;
    writeDescriptorInfo.pBufferInfo          = descriptorBufferInfo.data();
    writeDescriptorInfo.pTexelBufferView     = nullptr;

    vkUpdateDescriptorSets(device, 1, &writeDescriptorInfo, 0, nullptr);
}

void TransformFeedbackVk::getBufferOffsets(ContextVk *contextVk,
                                           const gl::ProgramState &programState,
                                           GLint drawCallFirstVertex,
                                           int32_t *offsetsOut,
                                           size_t offsetsSize) const
{
    GLsizeiptr verticesDrawn = mState.getVerticesDrawn();
    const std::vector<GLsizei> &bufferStrides =
        mState.getBoundProgram()->getTransformFeedbackStrides();
    size_t xfbBufferCount = programState.getTransformFeedbackBufferCount();

    ASSERT(xfbBufferCount > 0);

    // The caller should make sure the offsets array has enough space.  The maximum possible number
    // of outputs is gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS.
    ASSERT(offsetsSize >= xfbBufferCount);

    for (size_t bufferIndex = 0; bufferIndex < xfbBufferCount; ++bufferIndex)
    {
        const BoundBufferRange &bufferRange = mBoundBufferRanges[bufferIndex];

        int64_t offsetFromDescriptor =
            static_cast<int64_t>(bufferRange.offset - bufferRange.alignedOffset);
        int64_t drawCallVertexOffset = static_cast<int64_t>(verticesDrawn) - drawCallFirstVertex;

        int64_t writeOffset =
            (offsetFromDescriptor + drawCallVertexOffset * bufferStrides[bufferIndex]) /
            static_cast<int64_t>(sizeof(uint32_t));

        offsetsOut[bufferIndex] = static_cast<int32_t>(writeOffset);

        // Assert on overflow.  For now, support transform feedback up to 2GB.
        ASSERT(offsetsOut[bufferIndex] == writeOffset);
    }
}

void TransformFeedbackVk::onBeginEnd(const gl::Context *context)
{
    // Currently, we don't handle resources switching from read-only to writable and back correctly.
    // In the case of transform feedback, the attached buffers can switch between being written by
    // transform feedback and being read as a vertex array buffer.  For now, we'll end the render
    // pass every time transform feedback is started or ended to work around this issue temporarily.
    //
    // TODO(syoussefi): detect changes to buffer usage (e.g. as transform feedback output, vertex
    // or index data etc) in the front end and notify the backend.  A new node should be created
    // only on such changes.  http://anglebug.com/3205
    ContextVk *contextVk               = vk::GetImpl(context);
    FramebufferVk *framebufferVk       = vk::GetImpl(context->getState().getDrawFramebuffer());
    vk::FramebufferHelper *framebuffer = framebufferVk->getFramebuffer();

    if (framebuffer->hasStartedRenderPass())
    {
        framebuffer->finishCurrentCommands(contextVk);
    }
}

}  // namespace rx

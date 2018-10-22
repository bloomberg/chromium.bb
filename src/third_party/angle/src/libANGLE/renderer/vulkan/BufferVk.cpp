//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferVk.cpp:
//    Implements the class methods for BufferVk.
//

#include "libANGLE/renderer/vulkan/BufferVk.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

BufferVk::BufferVk(const gl::BufferState &state) : BufferImpl(state)
{
}

BufferVk::~BufferVk()
{
}

void BufferVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    release(renderer);
}

void BufferVk::release(RendererVk *renderer)
{
    renderer->releaseObject(getStoredQueueSerial(), &mBuffer);
    renderer->releaseObject(getStoredQueueSerial(), &mBufferMemory);
}

gl::Error BufferVk::setData(const gl::Context *context,
                            gl::BufferBinding target,
                            const void *data,
                            size_t size,
                            gl::BufferUsage usage)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (size > static_cast<size_t>(mState.getSize()))
    {
        // Release and re-create the memory and buffer.
        release(contextVk->getRenderer());

        const VkImageUsageFlags usageFlags =
            (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
             VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

        // TODO(jmadill): Proper usage bit implementation. Likely will involve multiple backing
        // buffers like in D3D11.
        VkBufferCreateInfo createInfo;
        createInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.pNext                 = nullptr;
        createInfo.flags                 = 0;
        createInfo.size                  = size;
        createInfo.usage                 = usageFlags;
        createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices   = nullptr;

        ANGLE_TRY(mBuffer.init(contextVk, createInfo));

        // Assume host vislble/coherent memory available.
        const VkMemoryPropertyFlags memoryPropertyFlags =
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        ANGLE_TRY(
            vk::AllocateBufferMemory(contextVk, memoryPropertyFlags, &mBuffer, &mBufferMemory));
    }

    if (data)
    {
        ANGLE_TRY(setDataImpl(contextVk, static_cast<const uint8_t *>(data), size, 0));
    }

    return gl::NoError();
}

gl::Error BufferVk::setSubData(const gl::Context *context,
                               gl::BufferBinding target,
                               const void *data,
                               size_t size,
                               size_t offset)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBufferMemory.getHandle() != VK_NULL_HANDLE);

    ContextVk *contextVk = vk::GetImpl(context);
    ANGLE_TRY(setDataImpl(contextVk, static_cast<const uint8_t *>(data), size, offset));

    return gl::NoError();
}

gl::Error BufferVk::copySubData(const gl::Context *context,
                                BufferImpl *source,
                                GLintptr sourceOffset,
                                GLintptr destOffset,
                                GLsizeiptr size)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error BufferVk::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBufferMemory.getHandle() != VK_NULL_HANDLE);

    ContextVk *contextVk = vk::GetImpl(context);
    ANGLE_TRY(mapImpl(contextVk, mapPtr));
    return gl::NoError();
}

angle::Result BufferVk::mapImpl(ContextVk *contextVk, void **mapPtr)
{
    return mBufferMemory.map(contextVk, 0, mState.getSize(), 0,
                             reinterpret_cast<uint8_t **>(mapPtr));
}

GLint64 BufferVk::getSize()
{
    return mState.getSize();
}

gl::Error BufferVk::mapRange(const gl::Context *context,
                             size_t offset,
                             size_t length,
                             GLbitfield access,
                             void **mapPtr)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBufferMemory.getHandle() != VK_NULL_HANDLE);

    ContextVk *contextVk = vk::GetImpl(context);

    ANGLE_TRY(
        mBufferMemory.map(contextVk, offset, length, 0, reinterpret_cast<uint8_t **>(mapPtr)));

    return gl::NoError();
}

gl::Error BufferVk::unmap(const gl::Context *context, GLboolean *result)
{
    return unmapImpl(vk::GetImpl(context));
}

angle::Result BufferVk::unmapImpl(ContextVk *contextVk)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBufferMemory.getHandle() != VK_NULL_HANDLE);

    mBufferMemory.unmap(contextVk->getDevice());

    return angle::Result::Continue();
}

gl::Error BufferVk::getIndexRange(const gl::Context *context,
                                  GLenum type,
                                  size_t offset,
                                  size_t count,
                                  bool primitiveRestartEnabled,
                                  gl::IndexRange *outRange)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // TODO(jmadill): Consider keeping a shadow system memory copy in some cases.
    ASSERT(mBuffer.valid());

    const gl::Type &typeInfo = gl::GetTypeInfo(type);

    uint8_t *mapPointer = nullptr;
    ANGLE_TRY(mBufferMemory.map(contextVk, offset, typeInfo.bytes * count, 0, &mapPointer));

    *outRange = gl::ComputeIndexRange(type, mapPointer, count, primitiveRestartEnabled);

    mBufferMemory.unmap(contextVk->getDevice());
    return gl::NoError();
}

angle::Result BufferVk::setDataImpl(ContextVk *contextVk,
                                    const uint8_t *data,
                                    size_t size,
                                    size_t offset)
{
    RendererVk *renderer = contextVk->getRenderer();
    VkDevice device      = contextVk->getDevice();

    // Use map when available.
    if (isResourceInUse(renderer))
    {
        vk::StagingBuffer stagingBuffer;
        ANGLE_TRY(stagingBuffer.init(contextVk, static_cast<VkDeviceSize>(size),
                                     vk::StagingUsage::Write));

        uint8_t *mapPointer = nullptr;
        ANGLE_TRY(stagingBuffer.getDeviceMemory().map(contextVk, 0, size, 0, &mapPointer));
        ASSERT(mapPointer);

        memcpy(mapPointer, data, size);
        stagingBuffer.getDeviceMemory().unmap(device);

        // Enqueue a copy command on the GPU.
        // 'beginWriteResource' will stop any subsequent rendering from using the old buffer data,
        // by marking any current read operations / command buffers as 'finished'.
        vk::CommandBuffer *commandBuffer = nullptr;
        ANGLE_TRY(beginWriteResource(contextVk, &commandBuffer));

        // Insert a barrier to ensure reads from the buffer are complete.
        // TODO(jmadill): Insert minimal barriers.
        VkBufferMemoryBarrier bufferBarrier;
        bufferBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarrier.pNext               = nullptr;
        bufferBarrier.srcAccessMask       = VK_ACCESS_MEMORY_READ_BIT;
        bufferBarrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        bufferBarrier.srcQueueFamilyIndex = 0;
        bufferBarrier.dstQueueFamilyIndex = 0;
        bufferBarrier.buffer              = mBuffer.getHandle();
        bufferBarrier.offset              = offset;
        bufferBarrier.size                = static_cast<VkDeviceSize>(size);

        commandBuffer->singleBufferBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, bufferBarrier);

        VkBufferCopy copyRegion = {0, offset, size};
        commandBuffer->copyBuffer(stagingBuffer.getBuffer(), mBuffer, 1, &copyRegion);

        // Insert a barrier to ensure copy has done.
        // TODO(jie.a.chen@intel.com): Insert minimal barriers.
        bufferBarrier.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarrier.pNext         = nullptr;
        bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bufferBarrier.dstAccessMask =
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT |
            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT |
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT |
            VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;

        bufferBarrier.srcQueueFamilyIndex = 0;
        bufferBarrier.dstQueueFamilyIndex = 0;
        bufferBarrier.buffer              = mBuffer.getHandle();
        bufferBarrier.offset              = offset;
        bufferBarrier.size                = static_cast<VkDeviceSize>(size);

        commandBuffer->singleBufferBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, bufferBarrier);

        // Immediately release staging buffer.
        // TODO(jmadill): Staging buffer re-use.
        renderer->releaseObject(getStoredQueueSerial(), &stagingBuffer);
    }
    else
    {
        uint8_t *mapPointer = nullptr;
        ANGLE_TRY(mBufferMemory.map(contextVk, offset, size, 0, &mapPointer));
        ASSERT(mapPointer);

        memcpy(mapPointer, data, size);

        mBufferMemory.unmap(device);
    }

    return angle::Result::Continue();
}

const vk::Buffer &BufferVk::getVkBuffer() const
{
    return mBuffer;
}

}  // namespace rx

//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PersistentCommandPool.cpp:
//    Implements the class methods for PersistentCommandPool
//

#include "libANGLE/renderer/vulkan/PersistentCommandPool.h"

namespace rx
{

namespace vk
{

PersistentCommandPool::PersistentCommandPool() {}

PersistentCommandPool::~PersistentCommandPool()
{
    ASSERT(!mCommandPool.valid() && mFreeBuffers.empty());
}

angle::Result PersistentCommandPool::init(vk::Context *context, uint32_t queueFamilyIndex)
{
    ASSERT(!mCommandPool.valid());

    // Initialize the command pool now that we know the queue family index.
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex        = queueFamilyIndex;

    ANGLE_VK_TRY(context, mCommandPool.init(context->getDevice(), commandPoolInfo));

    for (uint32_t i = 0; i < kInitBufferNum; i++)
    {
        ANGLE_TRY(allocateCommandBuffer(context));
    }

    return angle::Result::Continue;
}

void PersistentCommandPool::destroy(VkDevice device)
{
    if (!valid())
        return;

    ASSERT(mCommandPool.valid());

    for (vk::PrimaryCommandBuffer &cmdBuf : mFreeBuffers)
    {
        cmdBuf.destroy(device, mCommandPool);
    }
    mFreeBuffers.clear();

    mCommandPool.destroy(device);
}

angle::Result PersistentCommandPool::allocate(vk::Context *context,
                                              vk::PrimaryCommandBuffer *commandBufferOut)
{
    if (mFreeBuffers.empty())
    {
        ANGLE_TRY(allocateCommandBuffer(context));
        ASSERT(!mFreeBuffers.empty());
    }

    *commandBufferOut = std::move(mFreeBuffers.back());
    mFreeBuffers.pop_back();

    return angle::Result::Continue;
}

angle::Result PersistentCommandPool::collect(vk::Context *context,
                                             vk::PrimaryCommandBuffer &&buffer)
{
    // VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT NOT set, The CommandBuffer
    // can still hold the memory resource
    ANGLE_VK_TRY(context, vkResetCommandBuffer(buffer.getHandle(), 0));

    mFreeBuffers.emplace_back(std::move(buffer));
    return angle::Result::Continue;
}

angle::Result PersistentCommandPool::allocateCommandBuffer(vk::Context *context)
{
    vk::PrimaryCommandBuffer commandBuffer;
    {
        // Only used for primary CommandBuffer allocation
        VkCommandBufferAllocateInfo commandBufferInfo = {};
        commandBufferInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandPool        = mCommandPool.getHandle();
        commandBufferInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferInfo.commandBufferCount = 1;

        ANGLE_VK_TRY(context, commandBuffer.init(context->getDevice(), commandBufferInfo));
    }

    mFreeBuffers.emplace_back(std::move(commandBuffer));

    return angle::Result::Continue;
}

}  // namespace vk

}  // namespace rx

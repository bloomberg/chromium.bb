// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SemaphoreVk.cpp: Defines the class interface for SemaphoreVk, implementing
// SemaphoreImpl.

#include "libANGLE/renderer/vulkan/SemaphoreVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"

namespace rx
{

namespace
{
vk::ImageLayout GetVulkanImageLayout(GLenum layout)
{
    switch (layout)
    {
        case GL_NONE:
            return vk::ImageLayout::Undefined;
        case GL_LAYOUT_GENERAL_EXT:
            return vk::ImageLayout::ExternalShadersWrite;
        case GL_LAYOUT_COLOR_ATTACHMENT_EXT:
            return vk::ImageLayout::ColorAttachment;
        case GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT:
        case GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT:
        case GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT:
        case GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT:
            // Note: once VK_KHR_separate_depth_stencil_layouts becomes core or ubiquitous, we
            // should optimize depth/stencil image layout transitions to only be performed on the
            // aspect that needs transition.  In that case, these four layouts can be distinguished
            // and optimized.  Note that the exact equivalent of these layouts are specified in
            // VK_KHR_maintenance2, which are also usable, granted we transition the pair of
            // depth/stencil layouts accordingly elsewhere in ANGLE.
            return vk::ImageLayout::DepthStencilAttachment;
        case GL_LAYOUT_SHADER_READ_ONLY_EXT:
            return vk::ImageLayout::ExternalShadersReadOnly;
        case GL_LAYOUT_TRANSFER_SRC_EXT:
            return vk::ImageLayout::TransferSrc;
        case GL_LAYOUT_TRANSFER_DST_EXT:
            return vk::ImageLayout::TransferDst;
        default:
            UNREACHABLE();
            return vk::ImageLayout::Undefined;
    }
}
}  // anonymous namespace

SemaphoreVk::SemaphoreVk() = default;

SemaphoreVk::~SemaphoreVk() = default;

void SemaphoreVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    contextVk->addGarbage(&mSemaphore);
}

angle::Result SemaphoreVk::importFd(gl::Context *context, gl::HandleType handleType, GLint fd)
{
    switch (handleType)
    {
        case gl::HandleType::OpaqueFd:
            return importOpaqueFd(context, fd);

        default:
            ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
            return angle::Result::Stop;
    }
}

angle::Result SemaphoreVk::wait(gl::Context *context,
                                const gl::BufferBarrierVector &bufferBarriers,
                                const gl::TextureBarrierVector &textureBarriers)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (!bufferBarriers.empty() || !textureBarriers.empty())
    {
        // Create one global memory barrier to cover all barriers.
        contextVk->getCommandGraph()->syncExternalMemory();
    }

    uint32_t rendererQueueFamilyIndex = contextVk->getRenderer()->getQueueFamilyIndex();

    if (!bufferBarriers.empty())
    {
        // Perform a queue ownership transfer for each buffer.
        for (gl::Buffer *buffer : bufferBarriers)
        {
            BufferVk *bufferVk             = vk::GetImpl(buffer);
            vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

            // If there were GL commands using this buffer prior to this call, that's a
            // synchronization error on behalf of the program.
            ASSERT(!bufferHelper.isCurrentlyInGraph());

            vk::CommandBuffer *queueChange;
            ANGLE_TRY(bufferHelper.recordCommands(contextVk, &queueChange));

            // Queue ownership transfer.
            bufferHelper.changeQueue(rendererQueueFamilyIndex, queueChange);
        }
    }

    if (!textureBarriers.empty())
    {
        // Perform a queue ownership transfer for each texture.  Additionally, we are being
        // informed that the layout of the image has been externally transitioned, so we need to
        // update our internal state tracking.
        for (const gl::TextureAndLayout &textureAndLayout : textureBarriers)
        {
            TextureVk *textureVk   = vk::GetImpl(textureAndLayout.texture);
            vk::ImageHelper &image = textureVk->getImage();
            vk::ImageLayout layout = GetVulkanImageLayout(textureAndLayout.layout);

            // If there were GL commands using this image prior to this call, that's a
            // synchronization error on behalf of the program.
            ASSERT(!image.isCurrentlyInGraph());

            // Inform the image that the layout has been externally changed.
            image.onExternalLayoutChange(layout);

            vk::CommandBuffer *queueChange;
            ANGLE_TRY(image.recordCommands(contextVk, &queueChange));

            // Queue ownership transfer.
            image.changeLayoutAndQueue(image.getAspectFlags(), layout, rendererQueueFamilyIndex,
                                       queueChange);
        }
    }

    contextVk->insertWaitSemaphore(&mSemaphore);
    return angle::Result::Continue;
}

angle::Result SemaphoreVk::signal(gl::Context *context,
                                  const gl::BufferBarrierVector &bufferBarriers,
                                  const gl::TextureBarrierVector &textureBarriers)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (!bufferBarriers.empty())
    {
        // Perform a queue ownership transfer for each buffer.
        for (gl::Buffer *buffer : bufferBarriers)
        {
            BufferVk *bufferVk             = vk::GetImpl(buffer);
            vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

            vk::CommandBuffer *queueChange;
            ANGLE_TRY(bufferHelper.recordCommands(contextVk, &queueChange));

            // Queue ownership transfer.
            bufferHelper.changeQueue(VK_QUEUE_FAMILY_EXTERNAL, queueChange);
        }
    }

    if (!textureBarriers.empty())
    {
        // Perform a queue ownership transfer for each texture.  Additionally, transition the image
        // to the requested layout.
        for (const gl::TextureAndLayout &textureAndLayout : textureBarriers)
        {
            TextureVk *textureVk   = vk::GetImpl(textureAndLayout.texture);
            vk::ImageHelper &image = textureVk->getImage();
            vk::ImageLayout layout = GetVulkanImageLayout(textureAndLayout.layout);

            // Don't transition to Undefined layout.  If external wants to transition the image away
            // from Undefined after this operation, it's perfectly fine to keep the layout as is in
            // ANGLE.  Note that vk::ImageHelper doesn't expect transitions to Undefined.
            if (layout == vk::ImageLayout::Undefined)
            {
                layout = image.getCurrentImageLayout();
            }

            vk::CommandBuffer *layoutChange;
            ANGLE_TRY(image.recordCommands(contextVk, &layoutChange));

            // Queue ownership transfer and layout transition.
            image.changeLayoutAndQueue(image.getAspectFlags(), layout, VK_QUEUE_FAMILY_EXTERNAL,
                                       layoutChange);
        }
    }

    if (!bufferBarriers.empty() || !textureBarriers.empty())
    {
        // Create one global memory barrier to cover all barriers.
        contextVk->getCommandGraph()->syncExternalMemory();
    }

    return contextVk->flushImpl(&mSemaphore);
}

angle::Result SemaphoreVk::importOpaqueFd(gl::Context *context, GLint fd)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    if (!mSemaphore.valid())
    {
        mSemaphore.init(renderer->getDevice());
    }

    ASSERT(mSemaphore.valid());

    VkImportSemaphoreFdInfoKHR importSemaphoreFdInfo = {};
    importSemaphoreFdInfo.sType      = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    importSemaphoreFdInfo.semaphore  = mSemaphore.getHandle();
    importSemaphoreFdInfo.flags      = 0;
    importSemaphoreFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    importSemaphoreFdInfo.fd         = fd;

    ANGLE_VK_TRY(contextVk, vkImportSemaphoreFdKHR(renderer->getDevice(), &importSemaphoreFdInfo));

    return angle::Result::Continue;
}

}  // namespace rx

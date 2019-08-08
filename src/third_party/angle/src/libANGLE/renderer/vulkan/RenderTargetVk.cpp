//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetVk:
//   Wrapper around a Vulkan renderable resource, using an ImageView.
//

#include "libANGLE/renderer/vulkan/RenderTargetVk.h"

#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
RenderTargetVk::RenderTargetVk()
    : mImage(nullptr),
      mImageView(nullptr),
      mCubeImageFetchView(nullptr),
      mLevelIndex(0),
      mLayerIndex(0)
{}

RenderTargetVk::~RenderTargetVk() {}

RenderTargetVk::RenderTargetVk(RenderTargetVk &&other)
    : mImage(other.mImage),
      mImageView(other.mImageView),
      mCubeImageFetchView(other.mCubeImageFetchView),
      mLevelIndex(other.mLevelIndex),
      mLayerIndex(other.mLayerIndex)
{}

void RenderTargetVk::init(vk::ImageHelper *image,
                          vk::ImageView *imageView,
                          vk::ImageView *cubeImageFetchView,
                          size_t levelIndex,
                          size_t layerIndex)
{
    mImage              = image;
    mImageView          = imageView;
    mCubeImageFetchView = cubeImageFetchView;
    mLevelIndex         = levelIndex;
    mLayerIndex         = layerIndex;
}

void RenderTargetVk::reset()
{
    mImage              = nullptr;
    mImageView          = nullptr;
    mCubeImageFetchView = nullptr;
    mLevelIndex         = 0;
    mLayerIndex         = 0;
}

angle::Result RenderTargetVk::onColorDraw(ContextVk *contextVk,
                                          vk::FramebufferHelper *framebufferVk,
                                          vk::CommandBuffer *commandBuffer)
{
    ASSERT(commandBuffer->valid());
    ASSERT(!mImage->getFormat().imageFormat().hasDepthOrStencilBits());

    // TODO(jmadill): Use automatic layout transition. http://anglebug.com/2361
    mImage->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::ColorAttachment,
                         commandBuffer);

    // Set up dependencies between the RT resource and the Framebuffer.
    mImage->addWriteDependency(framebufferVk);

    return angle::Result::Continue;
}

angle::Result RenderTargetVk::onDepthStencilDraw(ContextVk *contextVk,
                                                 vk::FramebufferHelper *framebufferVk,
                                                 vk::CommandBuffer *commandBuffer)
{
    ASSERT(commandBuffer->valid());
    ASSERT(mImage->getFormat().imageFormat().hasDepthOrStencilBits());

    // TODO(jmadill): Use automatic layout transition. http://anglebug.com/2361
    const angle::Format &format    = mImage->getFormat().imageFormat();
    VkImageAspectFlags aspectFlags = vk::GetDepthStencilAspectFlags(format);

    mImage->changeLayout(aspectFlags, vk::ImageLayout::DepthStencilAttachment, commandBuffer);

    // Set up dependencies between the RT resource and the Framebuffer.
    mImage->addWriteDependency(framebufferVk);

    return angle::Result::Continue;
}

vk::ImageHelper &RenderTargetVk::getImage()
{
    ASSERT(mImage && mImage->valid());
    return *mImage;
}

const vk::ImageHelper &RenderTargetVk::getImage() const
{
    ASSERT(mImage && mImage->valid());
    return *mImage;
}

vk::ImageView *RenderTargetVk::getDrawImageView() const
{
    ASSERT(mImageView && mImageView->valid());
    return mImageView;
}

vk::ImageView *RenderTargetVk::getReadImageView() const
{
    return getDrawImageView();
}

vk::ImageView *RenderTargetVk::getFetchImageView() const
{
    return mCubeImageFetchView && mCubeImageFetchView->valid() ? mCubeImageFetchView
                                                               : getReadImageView();
}

const vk::Format &RenderTargetVk::getImageFormat() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getFormat();
}

gl::Extents RenderTargetVk::getExtents() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getLevelExtents2D(mLevelIndex);
}

void RenderTargetVk::updateSwapchainImage(vk::ImageHelper *image, vk::ImageView *imageView)
{
    ASSERT(image && image->valid() && imageView && imageView->valid());
    mImage              = image;
    mImageView          = imageView;
    mCubeImageFetchView = nullptr;
}

vk::ImageHelper *RenderTargetVk::getImageForRead(vk::CommandGraphResource *readingResource,
                                                 vk::ImageLayout layout,
                                                 vk::CommandBuffer *commandBuffer)
{
    ASSERT(mImage && mImage->valid());

    // TODO(jmadill): Better simultaneous resource access. http://anglebug.com/2679
    //
    // A better alternative would be:
    //
    // if (mImage->isLayoutChangeNecessary(layout)
    // {
    //     vk::CommandBuffer *srcLayoutChange;
    //     ANGLE_TRY(mImage->recordCommands(contextVk, &srcLayoutChange));
    //     mImage->changeLayout(mImage->getAspectFlags(), layout, srcLayoutChange);
    // }
    // mImage->addReadDependency(readingResource);
    //
    // I.e. the transition should happen on a node generated from mImage itself.
    // However, this needs context to be available here, or all call sites changed
    // to perform the layout transition and set the dependency.
    mImage->addWriteDependency(readingResource);

    mImage->changeLayout(mImage->getAspectFlags(), layout, commandBuffer);

    return mImage;
}

vk::ImageHelper *RenderTargetVk::getImageForWrite(vk::CommandGraphResource *writingResource) const
{
    ASSERT(mImage && mImage->valid());
    mImage->addWriteDependency(writingResource);
    return mImage;
}

angle::Result RenderTargetVk::flushStagedUpdates(ContextVk *contextVk)
{
    ASSERT(mImage->valid());
    if (!mImage->hasStagedUpdates())
        return angle::Result::Continue;

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(mImage->recordCommands(contextVk, &commandBuffer));
    return mImage->flushStagedUpdates(contextVk, mLevelIndex, mLevelIndex + 1, mLayerIndex,
                                      mLayerIndex + 1, commandBuffer);
}

}  // namespace rx

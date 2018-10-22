//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_utils:
//    Helper functions for the Vulkan Renderer.
//

#include "libANGLE/renderer/vulkan/vk_utils.h"

#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace
{
VkImageUsageFlags GetStagingBufferUsageFlags(rx::vk::StagingUsage usage)
{
    switch (usage)
    {
        case rx::vk::StagingUsage::Read:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case rx::vk::StagingUsage::Write:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case rx::vk::StagingUsage::Both:
            return (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        default:
            UNREACHABLE();
            return 0;
    }
}
}  // anonymous namespace

namespace angle
{
egl::Error ToEGL(Result result, rx::DisplayVk *displayVk, EGLint errorCode)
{
    if (result.isError())
    {
        return displayVk->getEGLError(errorCode);
    }
    else
    {
        return egl::NoError();
    }
}
}  // namespace angle

namespace rx
{
// Mirrors std_validation_str in loader.c
const char *g_VkStdValidationLayerName = "VK_LAYER_LUNARG_standard_validation";
const char *g_VkValidationLayerNames[] = {
    "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation",
    "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation",
    "VK_LAYER_GOOGLE_unique_objects"};
const uint32_t g_VkNumValidationLayerNames =
    sizeof(g_VkValidationLayerNames) / sizeof(g_VkValidationLayerNames[0]);

bool HasValidationLayer(const std::vector<VkLayerProperties> &layerProps, const char *layerName)
{
    for (const auto &layerProp : layerProps)
    {
        if (std::string(layerProp.layerName) == layerName)
        {
            return true;
        }
    }

    return false;
}

bool HasStandardValidationLayer(const std::vector<VkLayerProperties> &layerProps)
{
    return HasValidationLayer(layerProps, g_VkStdValidationLayerName);
}

bool HasValidationLayers(const std::vector<VkLayerProperties> &layerProps)
{
    for (const auto &layerName : g_VkValidationLayerNames)
    {
        if (!HasValidationLayer(layerProps, layerName))
        {
            return false;
        }
    }

    return true;
}

angle::Result FindAndAllocateCompatibleMemory(vk::Context *context,
                                              const vk::MemoryProperties &memoryProperties,
                                              VkMemoryPropertyFlags memoryPropertyFlags,
                                              const VkMemoryRequirements &memoryRequirements,
                                              vk::DeviceMemory *deviceMemoryOut)
{
    uint32_t memoryTypeIndex = 0;
    ANGLE_TRY(memoryProperties.findCompatibleMemoryIndex(context, memoryRequirements,
                                                         memoryPropertyFlags, &memoryTypeIndex));

    VkMemoryAllocateInfo allocInfo;
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext           = nullptr;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    allocInfo.allocationSize  = memoryRequirements.size;

    ANGLE_TRY(deviceMemoryOut->allocate(context, allocInfo));
    return angle::Result::Continue();
}

template <typename T>
angle::Result AllocateBufferOrImageMemory(vk::Context *context,
                                          VkMemoryPropertyFlags memoryPropertyFlags,
                                          T *bufferOrImage,
                                          vk::DeviceMemory *deviceMemoryOut)
{
    const vk::MemoryProperties &memoryProperties = context->getRenderer()->getMemoryProperties();

    // Call driver to determine memory requirements.
    VkMemoryRequirements memoryRequirements;
    bufferOrImage->getMemoryRequirements(context->getDevice(), &memoryRequirements);

    ANGLE_TRY(FindAndAllocateCompatibleMemory(context, memoryProperties, memoryPropertyFlags,
                                              memoryRequirements, deviceMemoryOut));
    ANGLE_TRY(bufferOrImage->bindMemory(context, *deviceMemoryOut));

    return angle::Result::Continue();
}

uint32_t GetImageLayerCount(gl::TextureType textureType)
{
    if (textureType == gl::TextureType::CubeMap)
    {
        return gl::CUBE_FACE_COUNT;
    }
    else
    {
        return 1;
    }
}

const char *g_VkLoaderLayersPathEnv = "VK_LAYER_PATH";
const char *g_VkICDPathEnv          = "VK_ICD_FILENAMES";

const char *VulkanResultString(VkResult result)
{
    switch (result)
    {
        case VK_SUCCESS:
            return "Command successfully completed.";
        case VK_NOT_READY:
            return "A fence or query has not yet completed.";
        case VK_TIMEOUT:
            return "A wait operation has not completed in the specified time.";
        case VK_EVENT_SET:
            return "An event is signaled.";
        case VK_EVENT_RESET:
            return "An event is unsignaled.";
        case VK_INCOMPLETE:
            return "A return array was too small for the result.";
        case VK_SUBOPTIMAL_KHR:
            return "A swapchain no longer matches the surface properties exactly, but can still be "
                   "used to present to the surface successfully.";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "A host memory allocation has failed.";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "A device memory allocation has failed.";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "Initialization of an object could not be completed for implementation-specific "
                   "reasons.";
        case VK_ERROR_DEVICE_LOST:
            return "The logical or physical device has been lost.";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "Mapping of a memory object has failed.";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "A requested layer is not present or could not be loaded.";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "A requested extension is not supported.";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "A requested feature is not supported.";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "The requested version of Vulkan is not supported by the driver or is otherwise "
                   "incompatible for implementation-specific reasons.";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "Too many objects of the type have already been created.";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "A requested format is not supported on this device.";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "A surface is no longer available.";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "The requested window is already connected to a VkSurfaceKHR, or to some other "
                   "non-Vulkan API.";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "A surface has changed in such a way that it is no longer compatible with the "
                   "swapchain.";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "The display used by a swapchain does not use the same presentable image "
                   "layout, or is incompatible in a way that prevents sharing an image.";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "The validation layers detected invalid API usage.";
        default:
            return "Unknown vulkan error code.";
    }
}

bool GetAvailableValidationLayers(const std::vector<VkLayerProperties> &layerProps,
                                  bool mustHaveLayers,
                                  const char *const **enabledLayerNames,
                                  uint32_t *enabledLayerCount)
{
    if (HasStandardValidationLayer(layerProps))
    {
        *enabledLayerNames = &g_VkStdValidationLayerName;
        *enabledLayerCount = 1;
    }
    else if (HasValidationLayers(layerProps))
    {
        *enabledLayerNames = g_VkValidationLayerNames;
        *enabledLayerCount = g_VkNumValidationLayerNames;
    }
    else
    {
        // Generate an error if the layers were explicitly requested, warning otherwise.
        if (mustHaveLayers)
        {
            ERR() << "Vulkan validation layers are missing.";
        }
        else
        {
            WARN() << "Vulkan validation layers are missing.";
        }

        *enabledLayerNames = nullptr;
        *enabledLayerCount = 0;
        return false;
    }

    return true;
}

namespace vk
{
VkImageAspectFlags GetDepthStencilAspectFlags(const angle::Format &format)
{
    return (format.depthBits > 0 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
           (format.stencilBits > 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
}

VkImageAspectFlags GetFormatAspectFlags(const angle::Format &format)
{
    return (format.redBits > 0 ? VK_IMAGE_ASPECT_COLOR_BIT : 0) |
           GetDepthStencilAspectFlags(format);
}

VkImageAspectFlags GetDepthStencilAspectFlagsForCopy(bool copyDepth, bool copyStencil)
{
    return copyDepth ? VK_IMAGE_ASPECT_DEPTH_BIT
                     : 0 | copyStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
}

// Context implementation.
Context::Context(RendererVk *renderer) : mRenderer(renderer)
{
}

Context::~Context()
{
}

VkDevice Context::getDevice() const
{
    return mRenderer->getDevice();
}

// BufferAndMemory implementation.
BufferAndMemory::BufferAndMemory() = default;

BufferAndMemory::BufferAndMemory(Buffer &&buffer, DeviceMemory &&deviceMemory)
    : buffer(std::move(buffer)), memory(std::move(deviceMemory))
{
}

BufferAndMemory::BufferAndMemory(BufferAndMemory &&other)
    : buffer(std::move(other.buffer)), memory(std::move(other.memory))
{
}

BufferAndMemory &BufferAndMemory::operator=(BufferAndMemory &&other)
{
    buffer = std::move(other.buffer);
    memory = std::move(other.memory);
    return *this;
}

// CommandPool implementation.
CommandPool::CommandPool()
{
}

void CommandPool::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyCommandPool(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result CommandPool::init(Context *context, const VkCommandPoolCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context,
                 vkCreateCommandPool(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

// CommandBuffer implementation.
CommandBuffer::CommandBuffer()
{
}

VkCommandBuffer CommandBuffer::releaseHandle()
{
    VkCommandBuffer handle = mHandle;
    mHandle                = nullptr;
    return handle;
}

angle::Result CommandBuffer::init(Context *context, const VkCommandBufferAllocateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context, vkAllocateCommandBuffers(context->getDevice(), &createInfo, &mHandle));
    return angle::Result::Continue();
}

void CommandBuffer::blitImage(const Image &srcImage,
                              VkImageLayout srcImageLayout,
                              const Image &dstImage,
                              VkImageLayout dstImageLayout,
                              uint32_t regionCount,
                              VkImageBlit *pRegions,
                              VkFilter filter)
{
    ASSERT(valid());
    vkCmdBlitImage(mHandle, srcImage.getHandle(), srcImageLayout, dstImage.getHandle(),
                   dstImageLayout, regionCount, pRegions, filter);
}

angle::Result CommandBuffer::begin(Context *context, const VkCommandBufferBeginInfo &info)
{
    ASSERT(valid());
    ANGLE_VK_TRY(context, vkBeginCommandBuffer(mHandle, &info));
    return angle::Result::Continue();
}

angle::Result CommandBuffer::end(Context *context)
{
    ASSERT(valid());
    ANGLE_VK_TRY(context, vkEndCommandBuffer(mHandle));
    return angle::Result::Continue();
}

angle::Result CommandBuffer::reset(Context *context)
{
    ASSERT(valid());
    ANGLE_VK_TRY(context, vkResetCommandBuffer(mHandle, 0));
    return angle::Result::Continue();
}

void CommandBuffer::singleImageBarrier(VkPipelineStageFlags srcStageMask,
                                       VkPipelineStageFlags dstStageMask,
                                       VkDependencyFlags dependencyFlags,
                                       const VkImageMemoryBarrier &imageMemoryBarrier)
{
    ASSERT(valid());
    vkCmdPipelineBarrier(mHandle, srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 0,
                         nullptr, 1, &imageMemoryBarrier);
}

void CommandBuffer::singleBufferBarrier(VkPipelineStageFlags srcStageMask,
                                        VkPipelineStageFlags dstStageMask,
                                        VkDependencyFlags dependencyFlags,
                                        const VkBufferMemoryBarrier &bufferBarrier)
{
    ASSERT(valid());
    vkCmdPipelineBarrier(mHandle, srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 1,
                         &bufferBarrier, 0, nullptr);
}

void CommandBuffer::destroy(VkDevice device)
{
    releaseHandle();
}

void CommandBuffer::destroy(VkDevice device, const vk::CommandPool &commandPool)
{
    if (valid())
    {
        ASSERT(commandPool.valid());
        vkFreeCommandBuffers(device, commandPool.getHandle(), 1, &mHandle);
        mHandle = VK_NULL_HANDLE;
    }
}

void CommandBuffer::copyBuffer(const vk::Buffer &srcBuffer,
                               const vk::Buffer &destBuffer,
                               uint32_t regionCount,
                               const VkBufferCopy *regions)
{
    ASSERT(valid());
    ASSERT(srcBuffer.valid() && destBuffer.valid());
    vkCmdCopyBuffer(mHandle, srcBuffer.getHandle(), destBuffer.getHandle(), regionCount, regions);
}

void CommandBuffer::copyBuffer(const VkBuffer &srcBuffer,
                               const VkBuffer &destBuffer,
                               uint32_t regionCount,
                               const VkBufferCopy *regions)
{
    ASSERT(valid());
    vkCmdCopyBuffer(mHandle, srcBuffer, destBuffer, regionCount, regions);
}

void CommandBuffer::copyBufferToImage(VkBuffer srcBuffer,
                                      const Image &dstImage,
                                      VkImageLayout dstImageLayout,
                                      uint32_t regionCount,
                                      const VkBufferImageCopy *regions)
{
    ASSERT(valid());
    ASSERT(srcBuffer != VK_NULL_HANDLE);
    ASSERT(dstImage.valid());
    vkCmdCopyBufferToImage(mHandle, srcBuffer, dstImage.getHandle(), dstImageLayout, regionCount,
                           regions);
}

void CommandBuffer::copyImageToBuffer(const Image &srcImage,
                                      VkImageLayout srcImageLayout,
                                      VkBuffer dstBuffer,
                                      uint32_t regionCount,
                                      const VkBufferImageCopy *regions)
{
    ASSERT(valid());
    ASSERT(dstBuffer != VK_NULL_HANDLE);
    ASSERT(srcImage.valid());
    vkCmdCopyImageToBuffer(mHandle, srcImage.getHandle(), srcImageLayout, dstBuffer, regionCount,
                           regions);
}

void CommandBuffer::clearColorImage(const vk::Image &image,
                                    VkImageLayout imageLayout,
                                    const VkClearColorValue &color,
                                    uint32_t rangeCount,
                                    const VkImageSubresourceRange *ranges)
{
    ASSERT(valid());
    vkCmdClearColorImage(mHandle, image.getHandle(), imageLayout, &color, rangeCount, ranges);
}

void CommandBuffer::clearDepthStencilImage(const vk::Image &image,
                                           VkImageLayout imageLayout,
                                           const VkClearDepthStencilValue &depthStencil,
                                           uint32_t rangeCount,
                                           const VkImageSubresourceRange *ranges)
{
    ASSERT(valid());
    vkCmdClearDepthStencilImage(mHandle, image.getHandle(), imageLayout, &depthStencil, rangeCount,
                                ranges);
}

void CommandBuffer::clearAttachments(uint32_t attachmentCount,
                                     const VkClearAttachment *attachments,
                                     uint32_t rectCount,
                                     const VkClearRect *rects)
{
    ASSERT(valid());

    vkCmdClearAttachments(mHandle, attachmentCount, attachments, rectCount, rects);
}

void CommandBuffer::copyImage(const vk::Image &srcImage,
                              VkImageLayout srcImageLayout,
                              const vk::Image &dstImage,
                              VkImageLayout dstImageLayout,
                              uint32_t regionCount,
                              const VkImageCopy *regions)
{
    ASSERT(valid() && srcImage.valid() && dstImage.valid());
    vkCmdCopyImage(mHandle, srcImage.getHandle(), srcImageLayout, dstImage.getHandle(),
                   dstImageLayout, 1, regions);
}

void CommandBuffer::beginRenderPass(const VkRenderPassBeginInfo &beginInfo,
                                    VkSubpassContents subpassContents)
{
    ASSERT(valid());
    vkCmdBeginRenderPass(mHandle, &beginInfo, subpassContents);
}

void CommandBuffer::endRenderPass()
{
    ASSERT(mHandle != VK_NULL_HANDLE);
    vkCmdEndRenderPass(mHandle);
}

void CommandBuffer::draw(uint32_t vertexCount,
                         uint32_t instanceCount,
                         uint32_t firstVertex,
                         uint32_t firstInstance)
{
    ASSERT(valid());
    vkCmdDraw(mHandle, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::drawIndexed(uint32_t indexCount,
                                uint32_t instanceCount,
                                uint32_t firstIndex,
                                int32_t vertexOffset,
                                uint32_t firstInstance)
{
    ASSERT(valid());
    vkCmdDrawIndexed(mHandle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer::bindPipeline(VkPipelineBindPoint pipelineBindPoint,
                                 const vk::Pipeline &pipeline)
{
    ASSERT(valid() && pipeline.valid());
    vkCmdBindPipeline(mHandle, pipelineBindPoint, pipeline.getHandle());
}

void CommandBuffer::bindVertexBuffers(uint32_t firstBinding,
                                      uint32_t bindingCount,
                                      const VkBuffer *buffers,
                                      const VkDeviceSize *offsets)
{
    ASSERT(valid());
    vkCmdBindVertexBuffers(mHandle, firstBinding, bindingCount, buffers, offsets);
}

void CommandBuffer::bindIndexBuffer(const VkBuffer &buffer,
                                    VkDeviceSize offset,
                                    VkIndexType indexType)
{
    ASSERT(valid());
    vkCmdBindIndexBuffer(mHandle, buffer, offset, indexType);
}

void CommandBuffer::bindDescriptorSets(VkPipelineBindPoint bindPoint,
                                       const vk::PipelineLayout &layout,
                                       uint32_t firstSet,
                                       uint32_t descriptorSetCount,
                                       const VkDescriptorSet *descriptorSets,
                                       uint32_t dynamicOffsetCount,
                                       const uint32_t *dynamicOffsets)
{
    ASSERT(valid());
    vkCmdBindDescriptorSets(mHandle, bindPoint, layout.getHandle(), firstSet, descriptorSetCount,
                            descriptorSets, dynamicOffsetCount, dynamicOffsets);
}

void CommandBuffer::executeCommands(uint32_t commandBufferCount,
                                    const vk::CommandBuffer *commandBuffers)
{
    ASSERT(valid());
    vkCmdExecuteCommands(mHandle, commandBufferCount, commandBuffers[0].ptr());
}

void CommandBuffer::updateBuffer(const vk::Buffer &buffer,
                                 VkDeviceSize dstOffset,
                                 VkDeviceSize dataSize,
                                 const void *data)
{
    ASSERT(valid() && buffer.valid());
    vkCmdUpdateBuffer(mHandle, buffer.getHandle(), dstOffset, dataSize, data);
}

void CommandBuffer::pushConstants(const PipelineLayout &layout,
                                  VkShaderStageFlags flag,
                                  uint32_t offset,
                                  uint32_t size,
                                  const void *data)
{
    ASSERT(valid() && layout.valid());
    vkCmdPushConstants(mHandle, layout.getHandle(), flag, offset, size, data);
}

// Image implementation.
Image::Image()
{
}

void Image::setHandle(VkImage handle)
{
    mHandle = handle;
}

void Image::reset()
{
    mHandle = VK_NULL_HANDLE;
}

void Image::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyImage(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result Image::init(Context *context, const VkImageCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context, vkCreateImage(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

void Image::getMemoryRequirements(VkDevice device, VkMemoryRequirements *requirementsOut) const
{
    ASSERT(valid());
    vkGetImageMemoryRequirements(device, mHandle, requirementsOut);
}

angle::Result Image::bindMemory(Context *context, const vk::DeviceMemory &deviceMemory)
{
    ASSERT(valid() && deviceMemory.valid());
    ANGLE_VK_TRY(context,
                 vkBindImageMemory(context->getDevice(), mHandle, deviceMemory.getHandle(), 0));
    return angle::Result::Continue();
}

void Image::getSubresourceLayout(VkDevice device,
                                 VkImageAspectFlagBits aspectMask,
                                 uint32_t mipLevel,
                                 uint32_t arrayLayer,
                                 VkSubresourceLayout *outSubresourceLayout) const
{
    VkImageSubresource subresource;
    subresource.aspectMask = aspectMask;
    subresource.mipLevel   = mipLevel;
    subresource.arrayLayer = arrayLayer;

    vkGetImageSubresourceLayout(device, getHandle(), &subresource, outSubresourceLayout);
}

// ImageView implementation.
ImageView::ImageView()
{
}

void ImageView::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyImageView(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result ImageView::init(Context *context, const VkImageViewCreateInfo &createInfo)
{
    ANGLE_VK_TRY(context, vkCreateImageView(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

// Semaphore implementation.
Semaphore::Semaphore()
{
}

void Semaphore::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroySemaphore(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result Semaphore::init(Context *context)
{
    ASSERT(!valid());

    VkSemaphoreCreateInfo semaphoreInfo;
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    ANGLE_VK_TRY(context,
                 vkCreateSemaphore(context->getDevice(), &semaphoreInfo, nullptr, &mHandle));

    return angle::Result::Continue();
}

// Framebuffer implementation.
Framebuffer::Framebuffer()
{
}

void Framebuffer::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyFramebuffer(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result Framebuffer::init(Context *context, const VkFramebufferCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context,
                 vkCreateFramebuffer(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

void Framebuffer::setHandle(VkFramebuffer handle)
{
    mHandle = handle;
}

// DeviceMemory implementation.
DeviceMemory::DeviceMemory()
{
}

void DeviceMemory::destroy(VkDevice device)
{
    if (valid())
    {
        vkFreeMemory(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result DeviceMemory::allocate(Context *context, const VkMemoryAllocateInfo &allocInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context, vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

angle::Result DeviceMemory::map(Context *context,
                                VkDeviceSize offset,
                                VkDeviceSize size,
                                VkMemoryMapFlags flags,
                                uint8_t **mapPointer) const
{
    ASSERT(valid());
    ANGLE_VK_TRY(context, vkMapMemory(context->getDevice(), mHandle, offset, size, flags,
                                      reinterpret_cast<void **>(mapPointer)));
    return angle::Result::Continue();
}

void DeviceMemory::unmap(VkDevice device) const
{
    ASSERT(valid());
    vkUnmapMemory(device, mHandle);
}

// RenderPass implementation.
RenderPass::RenderPass()
{
}

void RenderPass::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyRenderPass(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result RenderPass::init(Context *context, const VkRenderPassCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context, vkCreateRenderPass(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

// Buffer implementation.
Buffer::Buffer()
{
}

void Buffer::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyBuffer(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result Buffer::init(Context *context, const VkBufferCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context, vkCreateBuffer(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

angle::Result Buffer::bindMemory(Context *context, const DeviceMemory &deviceMemory)
{
    ASSERT(valid() && deviceMemory.valid());
    ANGLE_VK_TRY(context,
                 vkBindBufferMemory(context->getDevice(), mHandle, deviceMemory.getHandle(), 0));
    return angle::Result::Continue();
}

void Buffer::getMemoryRequirements(VkDevice device, VkMemoryRequirements *memoryRequirementsOut)
{
    ASSERT(valid());
    vkGetBufferMemoryRequirements(device, mHandle, memoryRequirementsOut);
}

// ShaderModule implementation.
ShaderModule::ShaderModule()
{
}

void ShaderModule::destroy(VkDevice device)
{
    if (mHandle != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result ShaderModule::init(Context *context, const VkShaderModuleCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context,
                 vkCreateShaderModule(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

// Pipeline implementation.
Pipeline::Pipeline()
{
}

void Pipeline::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyPipeline(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result Pipeline::initGraphics(Context *context,
                                     const VkGraphicsPipelineCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context, vkCreateGraphicsPipelines(context->getDevice(), VK_NULL_HANDLE, 1,
                                                    &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

// PipelineLayout implementation.
PipelineLayout::PipelineLayout()
{
}

void PipelineLayout::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyPipelineLayout(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result PipelineLayout::init(Context *context, const VkPipelineLayoutCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context,
                 vkCreatePipelineLayout(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

// DescriptorSetLayout implementation.
DescriptorSetLayout::DescriptorSetLayout()
{
}

void DescriptorSetLayout::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyDescriptorSetLayout(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result DescriptorSetLayout::init(Context *context,
                                        const VkDescriptorSetLayoutCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context,
                 vkCreateDescriptorSetLayout(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

// DescriptorPool implementation.
DescriptorPool::DescriptorPool()
{
}

void DescriptorPool::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyDescriptorPool(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result DescriptorPool::init(Context *context, const VkDescriptorPoolCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context,
                 vkCreateDescriptorPool(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

angle::Result DescriptorPool::allocateDescriptorSets(Context *context,
                                                     const VkDescriptorSetAllocateInfo &allocInfo,
                                                     VkDescriptorSet *descriptorSetsOut)
{
    ASSERT(valid());
    ANGLE_VK_TRY(context,
                 vkAllocateDescriptorSets(context->getDevice(), &allocInfo, descriptorSetsOut));
    return angle::Result::Continue();
}

angle::Result DescriptorPool::freeDescriptorSets(Context *context,
                                                 uint32_t descriptorSetCount,
                                                 const VkDescriptorSet *descriptorSets)
{
    ASSERT(valid());
    ASSERT(descriptorSetCount > 0);
    ANGLE_VK_TRY(context, vkFreeDescriptorSets(context->getDevice(), mHandle, descriptorSetCount,
                                               descriptorSets));
    return angle::Result::Continue();
}

// Sampler implementation.
Sampler::Sampler()
{
}

void Sampler::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroySampler(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result Sampler::init(Context *context, const VkSamplerCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context, vkCreateSampler(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

// Fence implementation.
Fence::Fence()
{
}

void Fence::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyFence(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

angle::Result Fence::init(Context *context, const VkFenceCreateInfo &createInfo)
{
    ASSERT(!valid());
    ANGLE_VK_TRY(context, vkCreateFence(context->getDevice(), &createInfo, nullptr, &mHandle));
    return angle::Result::Continue();
}

VkResult Fence::getStatus(VkDevice device) const
{
    return vkGetFenceStatus(device, mHandle);
}

// MemoryProperties implementation.
MemoryProperties::MemoryProperties() : mMemoryProperties{0}
{
}

void MemoryProperties::init(VkPhysicalDevice physicalDevice)
{
    ASSERT(mMemoryProperties.memoryTypeCount == 0);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mMemoryProperties);
    ASSERT(mMemoryProperties.memoryTypeCount > 0);
}

void MemoryProperties::destroy()
{
    mMemoryProperties = {0};
}

angle::Result MemoryProperties::findCompatibleMemoryIndex(
    Context *context,
    const VkMemoryRequirements &memoryRequirements,
    VkMemoryPropertyFlags memoryPropertyFlags,
    uint32_t *typeIndexOut) const
{
    ASSERT(mMemoryProperties.memoryTypeCount > 0 && mMemoryProperties.memoryTypeCount <= 32);

    // Find a compatible memory pool index. If the index doesn't change, we could cache it.
    // Not finding a valid memory pool means an out-of-spec driver, or internal error.
    // TODO(jmadill): Determine if it is possible to cache indexes.
    // TODO(jmadill): More efficient memory allocation.
    for (size_t memoryIndex : angle::BitSet32<32>(memoryRequirements.memoryTypeBits))
    {
        ASSERT(memoryIndex < mMemoryProperties.memoryTypeCount);

        if ((mMemoryProperties.memoryTypes[memoryIndex].propertyFlags & memoryPropertyFlags) ==
            memoryPropertyFlags)
        {
            *typeIndexOut = static_cast<uint32_t>(memoryIndex);
            return angle::Result::Continue();
        }
    }

    // TODO(jmadill): Add error message to error.
    context->handleError(VK_ERROR_INCOMPATIBLE_DRIVER, __FILE__, __LINE__);
    return angle::Result::Stop();
}

// StagingBuffer implementation.
StagingBuffer::StagingBuffer() : mSize(0)
{
}

void StagingBuffer::destroy(VkDevice device)
{
    mBuffer.destroy(device);
    mDeviceMemory.destroy(device);
    mSize = 0;
}

angle::Result StagingBuffer::init(Context *context, VkDeviceSize size, StagingUsage usage)
{
    VkBufferCreateInfo createInfo;
    createInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.pNext                 = nullptr;
    createInfo.flags                 = 0;
    createInfo.size                  = size;
    createInfo.usage                 = GetStagingBufferUsageFlags(usage);
    createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices   = nullptr;

    VkMemoryPropertyFlags flags =
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    ANGLE_TRY(mBuffer.init(context, createInfo));
    ANGLE_TRY(AllocateBufferMemory(context, flags, &mBuffer, &mDeviceMemory));
    mSize = static_cast<size_t>(size);
    return angle::Result::Continue();
}

void StagingBuffer::dumpResources(Serial serial, std::vector<vk::GarbageObject> *garbageQueue)
{
    mBuffer.dumpResources(serial, garbageQueue);
    mDeviceMemory.dumpResources(serial, garbageQueue);
}

angle::Result AllocateBufferMemory(vk::Context *context,
                                   VkMemoryPropertyFlags memoryPropertyFlags,
                                   Buffer *buffer,
                                   DeviceMemory *deviceMemoryOut)
{
    return AllocateBufferOrImageMemory(context, memoryPropertyFlags, buffer, deviceMemoryOut);
}

angle::Result AllocateImageMemory(vk::Context *context,
                                  VkMemoryPropertyFlags memoryPropertyFlags,
                                  Image *image,
                                  DeviceMemory *deviceMemoryOut)
{
    return AllocateBufferOrImageMemory(context, memoryPropertyFlags, image, deviceMemoryOut);
}

angle::Result InitShaderAndSerial(Context *context,
                                  ShaderAndSerial *shaderAndSerial,
                                  const uint32_t *shaderCode,
                                  size_t shaderCodeSize)
{
    VkShaderModuleCreateInfo createInfo;
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext    = nullptr;
    createInfo.flags    = 0;
    createInfo.codeSize = shaderCodeSize;
    createInfo.pCode    = shaderCode;

    ANGLE_TRY(shaderAndSerial->get().init(context, createInfo));
    shaderAndSerial->updateSerial(context->getRenderer()->issueShaderSerial());
    return angle::Result::Continue();
}

// GarbageObject implementation.
GarbageObject::GarbageObject()
    : mSerial(), mHandleType(HandleType::Invalid), mHandle(VK_NULL_HANDLE)
{
}

GarbageObject::GarbageObject(const GarbageObject &other) = default;

GarbageObject &GarbageObject::operator=(const GarbageObject &other) = default;

bool GarbageObject::destroyIfComplete(VkDevice device, Serial completedSerial)
{
    if (completedSerial >= mSerial)
    {
        destroy(device);
        return true;
    }

    return false;
}

void GarbageObject::destroy(VkDevice device)
{
    switch (mHandleType)
    {
        case HandleType::Semaphore:
            vkDestroySemaphore(device, reinterpret_cast<VkSemaphore>(mHandle), nullptr);
            break;
        case HandleType::CommandBuffer:
            // Command buffers are pool allocated.
            UNREACHABLE();
            break;
        case HandleType::Fence:
            vkDestroyFence(device, reinterpret_cast<VkFence>(mHandle), nullptr);
            break;
        case HandleType::DeviceMemory:
            vkFreeMemory(device, reinterpret_cast<VkDeviceMemory>(mHandle), nullptr);
            break;
        case HandleType::Buffer:
            vkDestroyBuffer(device, reinterpret_cast<VkBuffer>(mHandle), nullptr);
            break;
        case HandleType::Image:
            vkDestroyImage(device, reinterpret_cast<VkImage>(mHandle), nullptr);
            break;
        case HandleType::ImageView:
            vkDestroyImageView(device, reinterpret_cast<VkImageView>(mHandle), nullptr);
            break;
        case HandleType::ShaderModule:
            vkDestroyShaderModule(device, reinterpret_cast<VkShaderModule>(mHandle), nullptr);
            break;
        case HandleType::PipelineLayout:
            vkDestroyPipelineLayout(device, reinterpret_cast<VkPipelineLayout>(mHandle), nullptr);
            break;
        case HandleType::RenderPass:
            vkDestroyRenderPass(device, reinterpret_cast<VkRenderPass>(mHandle), nullptr);
            break;
        case HandleType::Pipeline:
            vkDestroyPipeline(device, reinterpret_cast<VkPipeline>(mHandle), nullptr);
            break;
        case HandleType::DescriptorSetLayout:
            vkDestroyDescriptorSetLayout(device, reinterpret_cast<VkDescriptorSetLayout>(mHandle),
                                         nullptr);
            break;
        case HandleType::Sampler:
            vkDestroySampler(device, reinterpret_cast<VkSampler>(mHandle), nullptr);
            break;
        case HandleType::DescriptorPool:
            vkDestroyDescriptorPool(device, reinterpret_cast<VkDescriptorPool>(mHandle), nullptr);
            break;
        case HandleType::Framebuffer:
            vkDestroyFramebuffer(device, reinterpret_cast<VkFramebuffer>(mHandle), nullptr);
            break;
        case HandleType::CommandPool:
            vkDestroyCommandPool(device, reinterpret_cast<VkCommandPool>(mHandle), nullptr);
            break;
        default:
            UNREACHABLE();
            break;
    }
}
}  // namespace vk

namespace gl_vk
{

VkFilter GetFilter(const GLenum filter)
{
    switch (filter)
    {
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR:
            return VK_FILTER_LINEAR;
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST:
            return VK_FILTER_NEAREST;
        default:
            UNIMPLEMENTED();
            return VK_FILTER_MAX_ENUM;
    }
}

VkSamplerMipmapMode GetSamplerMipmapMode(const GLenum filter)
{
    switch (filter)
    {
        case GL_LINEAR:
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_NEAREST_MIPMAP_LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case GL_NEAREST:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_NEAREST:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        default:
            UNIMPLEMENTED();
            return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
    }
}

VkSamplerAddressMode GetSamplerAddressMode(const GLenum wrap)
{
    switch (wrap)
    {
        case GL_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case GL_MIRRORED_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case GL_CLAMP_TO_BORDER:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case GL_CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        default:
            UNIMPLEMENTED();
            return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    }
}

VkRect2D GetRect(const gl::Rectangle &source)
{
    return {{source.x, source.y},
            {static_cast<uint32_t>(source.width), static_cast<uint32_t>(source.height)}};
}

VkPrimitiveTopology GetPrimitiveTopology(gl::PrimitiveMode mode)
{
    switch (mode)
    {
        case gl::PrimitiveMode::Triangles:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case gl::PrimitiveMode::Points:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case gl::PrimitiveMode::Lines:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case gl::PrimitiveMode::LineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case gl::PrimitiveMode::TriangleFan:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        case gl::PrimitiveMode::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case gl::PrimitiveMode::LineLoop:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        default:
            UNREACHABLE();
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
}

VkCullModeFlags GetCullMode(const gl::RasterizerState &rasterState)
{
    if (!rasterState.cullFace)
    {
        return VK_CULL_MODE_NONE;
    }

    switch (rasterState.cullMode)
    {
        case gl::CullFaceMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case gl::CullFaceMode::Back:
            return VK_CULL_MODE_BACK_BIT;
        case gl::CullFaceMode::FrontAndBack:
            return VK_CULL_MODE_FRONT_AND_BACK;
        default:
            UNREACHABLE();
            return VK_CULL_MODE_NONE;
    }
}

VkFrontFace GetFrontFace(GLenum frontFace, bool invertCullFace)
{
    // Invert CW and CCW to have the same behavior as OpenGL.
    switch (frontFace)
    {
        case GL_CW:
            return invertCullFace ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
        case GL_CCW:
            return invertCullFace ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        default:
            UNREACHABLE();
            return VK_FRONT_FACE_CLOCKWISE;
    }
}

VkSampleCountFlagBits GetSamples(GLint sampleCount)
{
    switch (sampleCount)
    {
        case 0:
        case 1:
            return VK_SAMPLE_COUNT_1_BIT;
        case 2:
            return VK_SAMPLE_COUNT_2_BIT;
        case 4:
            return VK_SAMPLE_COUNT_4_BIT;
        case 8:
            return VK_SAMPLE_COUNT_8_BIT;
        case 16:
            return VK_SAMPLE_COUNT_16_BIT;
        case 32:
            return VK_SAMPLE_COUNT_32_BIT;
        default:
            UNREACHABLE();
            return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    }
}

VkComponentSwizzle GetSwizzle(const GLenum swizzle)
{
    switch (swizzle)
    {
        case GL_ALPHA:
            return VK_COMPONENT_SWIZZLE_A;
        case GL_RED:
            return VK_COMPONENT_SWIZZLE_R;
        case GL_GREEN:
            return VK_COMPONENT_SWIZZLE_G;
        case GL_BLUE:
            return VK_COMPONENT_SWIZZLE_B;
        case GL_ZERO:
            return VK_COMPONENT_SWIZZLE_ZERO;
        case GL_ONE:
            return VK_COMPONENT_SWIZZLE_ONE;
        default:
            UNREACHABLE();
            return VK_COMPONENT_SWIZZLE_IDENTITY;
    }
}

VkIndexType GetIndexType(GLenum elementType)
{
    switch (elementType)
    {
        case GL_UNSIGNED_BYTE:
        case GL_UNSIGNED_SHORT:
            return VK_INDEX_TYPE_UINT16;
        case GL_UNSIGNED_INT:
            return VK_INDEX_TYPE_UINT32;
        default:
            UNREACHABLE();
            return VK_INDEX_TYPE_MAX_ENUM;
    }
}

void GetOffset(const gl::Offset &glOffset, VkOffset3D *vkOffset)
{
    vkOffset->x = glOffset.x;
    vkOffset->y = glOffset.y;
    vkOffset->z = glOffset.z;
}

void GetExtent(const gl::Extents &glExtent, VkExtent3D *vkExtent)
{
    vkExtent->width  = glExtent.width;
    vkExtent->height = glExtent.height;
    vkExtent->depth  = glExtent.depth;
}

VkImageType GetImageType(gl::TextureType textureType)
{
    switch (textureType)
    {
        case gl::TextureType::_2D:
            return VK_IMAGE_TYPE_2D;
        case gl::TextureType::CubeMap:
            return VK_IMAGE_TYPE_2D;
        default:
            // We will need to implement all the texture types for ES3+.
            UNIMPLEMENTED();
            return VK_IMAGE_TYPE_MAX_ENUM;
    }
}

VkImageViewType GetImageViewType(gl::TextureType textureType)
{
    switch (textureType)
    {
        case gl::TextureType::_2D:
            return VK_IMAGE_VIEW_TYPE_2D;
        case gl::TextureType::CubeMap:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        default:
            // We will need to implement all the texture types for ES3+.
            UNIMPLEMENTED();
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    }
}

VkColorComponentFlags GetColorComponentFlags(bool red, bool green, bool blue, bool alpha)
{
    return (red ? VK_COLOR_COMPONENT_R_BIT : 0) | (green ? VK_COLOR_COMPONENT_G_BIT : 0) |
           (blue ? VK_COLOR_COMPONENT_B_BIT : 0) | (alpha ? VK_COLOR_COMPONENT_A_BIT : 0);
}
}  // namespace gl_vk
}  // namespace rx

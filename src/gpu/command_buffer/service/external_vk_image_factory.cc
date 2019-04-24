// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_factory.h"

#include <unistd.h>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

namespace gpu {

ExternalVkImageFactory::ExternalVkImageFactory(
    SharedContextState* context_state)
    : context_state_(context_state) {}

ExternalVkImageFactory::~ExternalVkImageFactory() {
  if (command_pool_) {
    command_pool_->Destroy();
    command_pool_.reset();
  }
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  VkDevice device = context_state_->vk_context_provider()
                        ->GetDeviceQueue()
                        ->GetVulkanDevice();

  VkFormat vk_format = ToVkFormat(format);
  VkResult result;
  VkImage image;
  result = CreateExternalVkImage(vk_format, size, &image);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Failed to create external VkImage: " << result;
    return nullptr;
  }

  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(device, image, &requirements);

  if (!requirements.memoryTypeBits) {
    LOG(ERROR) << "Unable to find appropriate memory type for external VkImage";
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  constexpr uint32_t kInvalidTypeIndex = 32;
  uint32_t type_index = kInvalidTypeIndex;
  for (int i = 0; i < 32; i++) {
    if ((1u << i) & requirements.memoryTypeBits) {
      type_index = i;
      break;
    }
  }
  DCHECK_NE(kInvalidTypeIndex, type_index);

  VkExportMemoryAllocateInfoKHR external_info;
  external_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
  external_info.pNext = nullptr;
  external_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

  VkMemoryAllocateInfo mem_alloc_info;
  mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc_info.pNext = &external_info;
  mem_alloc_info.allocationSize = requirements.size;
  mem_alloc_info.memoryTypeIndex = type_index;

  VkDeviceMemory memory;
  // TODO(crbug.com/932286): Allocating a separate piece of memory for every
  // VkImage might have too much overhead. It is recommended that one large
  // VkDeviceMemory be sub-allocated to multiple VkImages instead.
  result = vkAllocateMemory(device, &mem_alloc_info, nullptr, &memory);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Failed to allocate memory for external VkImage: " << result;
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  result = vkBindImageMemory(device, image, memory, 0);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Failed to bind memory to external VkImage: " << result;
    vkFreeMemory(device, memory, nullptr);
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  TransitionToColorAttachment(image);

  return std::make_unique<ExternalVkImageBacking>(
      mailbox, format, size, color_space, usage, context_state_, image, memory,
      requirements.size, vk_format);
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  // GpuMemoryBuffers are not supported on Linux.
  NOTREACHED();
  return nullptr;
}

VkResult ExternalVkImageFactory::CreateExternalVkImage(VkFormat format,
                                                       const gfx::Size& size,
                                                       VkImage* image) {
  VkDevice device = context_state_->vk_context_provider()
                        ->GetDeviceQueue()
                        ->GetVulkanDevice();

  VkExternalMemoryImageCreateInfoKHR external_info;
  external_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
  external_info.pNext = nullptr;
  external_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

  VkImageCreateInfo create_info;
  create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  create_info.pNext = &external_info;
  create_info.flags = 0;
  create_info.imageType = VK_IMAGE_TYPE_2D;
  create_info.format = format;
  create_info.extent = {size.width(), size.height(), 1};
  create_info.mipLevels = 1;
  create_info.arrayLayers = 1;
  create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  create_info.usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.queueFamilyIndexCount = context_state_->vk_context_provider()
                                          ->GetDeviceQueue()
                                          ->GetVulkanQueueIndex();
  create_info.pQueueFamilyIndices = nullptr;
  create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  return vkCreateImage(device, &create_info, nullptr, image);
}

void ExternalVkImageFactory::TransitionToColorAttachment(VkImage image) {
  if (!command_pool_) {
    command_pool_ = context_state_->vk_context_provider()
                        ->GetDeviceQueue()
                        ->CreateCommandPool();
  }
  std::unique_ptr<VulkanCommandBuffer> command_buffer =
      command_pool_->CreatePrimaryCommandBuffer();
  CHECK(command_buffer->Initialize());
  {
    ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);
    VkImageMemoryBarrier image_memory_barrier;
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = nullptr;
    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barrier.subresourceRange.baseMipLevel = 0;
    image_memory_barrier.subresourceRange.levelCount = 1;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    image_memory_barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(recorder.handle(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &image_memory_barrier);
  }
  command_buffer->Submit(0, nullptr, 0, nullptr);
  // TODO(crbug.com/932260): Remove blocking call to VkQueueWaitIdle once we
  // have a better approach for determining when |command_buffer| is safe to
  // destroy.
  vkQueueWaitIdle(context_state_->vk_context_provider()
                      ->GetDeviceQueue()
                      ->GetVulkanQueue());
  command_buffer->Destroy();
  command_buffer.reset();
}

}  // namespace gpu

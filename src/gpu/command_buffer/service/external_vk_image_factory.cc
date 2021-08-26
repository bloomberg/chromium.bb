// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_factory.h"

#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gfx/buffer_format_util.h"

namespace gpu {

namespace {

VkImageUsageFlags GetMaximalImageUsageFlags(
    VkFormatFeatureFlags feature_flags) {
  VkImageUsageFlags usage_flags = 0;
  // The TRANSFER_SRC/DST format features were added in Vulkan 1.1 and their
  // support is required when SAMPLED_IMAGE is supported. In Vulkan 1.0 all
  // formats support these features implicitly. See discussion in
  // https://github.com/KhronosGroup/Vulkan-Docs/issues/1223
  if (feature_flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
    usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT |
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  // VUID-VkImageViewCreateInfo-usage-02652: support for INPUT_ATTACHMENT is
  // implied by both of COLOR_ATTACHNENT and DEPTH_STENCIL_ATTACHMENT
  if (feature_flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
    usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  if (feature_flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

  if (feature_flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
    usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  if (feature_flags & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)
    usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if (feature_flags & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
    usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  return usage_flags;
}

VulkanImageUsageCache CreateImageUsageCache(
    VkPhysicalDevice vk_physical_device) {
  VulkanImageUsageCache image_usage_cache = {};

  for (int i = 0; i <= static_cast<int>(viz::RESOURCE_FORMAT_MAX); ++i) {
    viz::ResourceFormat format = static_cast<viz::ResourceFormat>(i);
    if (!viz::HasVkFormat(format))
      continue;
    VkFormat vk_format = viz::ToVkFormat(format);
    DCHECK_NE(vk_format, VK_FORMAT_UNDEFINED);
    VkFormatProperties format_props = {};
    vkGetPhysicalDeviceFormatProperties(vk_physical_device, vk_format,
                                        &format_props);
    image_usage_cache.optimal_tiling_usage[format] =
        GetMaximalImageUsageFlags(format_props.optimalTilingFeatures);
  }

  return image_usage_cache;
}

}  // namespace

ExternalVkImageFactory::ExternalVkImageFactory(
    scoped_refptr<SharedContextState> context_state)
    : context_state_(std::move(context_state)),
      command_pool_(context_state_->vk_context_provider()
                        ->GetDeviceQueue()
                        ->CreateCommandPool()),
      image_usage_cache_(
          CreateImageUsageCache(context_state_->vk_context_provider()
                                    ->GetDeviceQueue()
                                    ->GetVulkanPhysicalDevice())) {}

ExternalVkImageFactory::~ExternalVkImageFactory() {
  if (command_pool_) {
    context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetFenceHelper()
        ->EnqueueVulkanObjectCleanupForSubmittedWork(std::move(command_pool_));
  }
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return ExternalVkImageBacking::Create(
      context_state_, command_pool_.get(), mailbox, format, size, color_space,
      surface_origin, alpha_type, usage, &image_usage_cache_,
      base::span<const uint8_t>());
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  return ExternalVkImageBacking::Create(
      context_state_, command_pool_.get(), mailbox, format, size, color_space,
      surface_origin, alpha_type, usage, &image_usage_cache_, pixel_data);
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  DCHECK(CanImportGpuMemoryBuffer(handle.type));
  if (plane != gfx::BufferPlane::DEFAULT) {
    LOG(ERROR) << "Invalid plane";
    return nullptr;
  }
  return ExternalVkImageBacking::CreateFromGMB(
      context_state_, command_pool_.get(), mailbox, std::move(handle),
      buffer_format, size, color_space, surface_origin, alpha_type, usage,
      &image_usage_cache_);
}

bool ExternalVkImageFactory::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return context_state_->vk_context_provider()
             ->GetVulkanImplementation()
             ->CanImportGpuMemoryBuffer(memory_buffer_type) ||
         memory_buffer_type == gfx::SHARED_MEMORY_BUFFER;
}

bool ExternalVkImageFactory::IsSupported(uint32_t usage,
                                         viz::ResourceFormat format,
                                         bool thread_safe,
                                         gfx::GpuMemoryBufferType gmb_type,
                                         GrContextType gr_context_type,
                                         bool* allow_legacy_mailbox,
                                         bool is_pixel_used) {
  if (gmb_type != gfx::EMPTY_BUFFER && !CanImportGpuMemoryBuffer(gmb_type)) {
    return false;
  }
  // TODO(crbug.com/969114): Not all shared image factory implementations
  // support concurrent read/write usage.
  if (usage & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) {
    return false;
  }
  if (thread_safe) {
    LOG(ERROR) << "ExternalVkImageFactory currently do not support "
                  "cross-thread usage.";
    return false;
  }

#if defined(OS_ANDROID)
  // Scanout on Android requires explicit fence synchronization which is only
  // supported by the interop factory.
  if (usage & SHARED_IMAGE_USAGE_SCANOUT) {
    return false;
  }
#endif

  *allow_legacy_mailbox = false;
  return true;
}

}  // namespace gpu

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_surface.h"

#include <vulkan/vulkan.h>

#include "base/macros.h"
#include "base/stl_util.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_swap_chain.h"

namespace gpu {

namespace {
const VkFormat kPreferredVkFormats32[] = {
    VK_FORMAT_B8G8R8A8_UNORM,  // FORMAT_BGRA8888,
    VK_FORMAT_R8G8B8A8_UNORM,  // FORMAT_RGBA8888,
};

const VkFormat kPreferredVkFormats16[] = {
    VK_FORMAT_R5G6B5_UNORM_PACK16,  // FORMAT_RGB565,
};

}  // namespace

VulkanSurface::~VulkanSurface() {
  DCHECK_EQ(static_cast<VkSurfaceKHR>(VK_NULL_HANDLE), surface_);
}

VulkanSurface::VulkanSurface(VkInstance vk_instance,
                             VkSurfaceKHR surface,
                             base::OnceClosure destruction_callback)
    : vk_instance_(vk_instance),
      surface_(surface),
      destruction_callback_(std::move(destruction_callback)) {
  DCHECK_NE(static_cast<VkSurfaceKHR>(VK_NULL_HANDLE), surface_);
}

bool VulkanSurface::Initialize(VulkanDeviceQueue* device_queue,
                               VulkanSurface::Format format) {
  DCHECK(format >= 0 && format < NUM_SURFACE_FORMATS);
  DCHECK(device_queue);

  device_queue_ = device_queue;

  VkResult result = VK_SUCCESS;

  VkBool32 present_support;
  if (vkGetPhysicalDeviceSurfaceSupportKHR(
          device_queue_->GetVulkanPhysicalDevice(),
          device_queue_->GetVulkanQueueIndex(), surface_,
          &present_support) != VK_SUCCESS) {
    DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceSupportKHR() failed: " << result;
    return false;
  }
  if (!present_support) {
    DLOG(ERROR) << "Surface not supported by present queue.";
    return false;
  }

  // Get list of supported formats.
  uint32_t format_count = 0;
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      device_queue_->GetVulkanPhysicalDevice(), surface_, &format_count,
      nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceFormatsKHR() failed: " << result;
    return false;
  }

  std::vector<VkSurfaceFormatKHR> formats(format_count);
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      device_queue_->GetVulkanPhysicalDevice(), surface_, &format_count,
      formats.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceFormatsKHR() failed: " << result;
    return false;
  }

  const VkFormat* preferred_formats = (format == FORMAT_RGBA_32)
                                          ? kPreferredVkFormats32
                                          : kPreferredVkFormats16;
  unsigned int size = (format == FORMAT_RGBA_32)
                          ? base::size(kPreferredVkFormats32)
                          : base::size(kPreferredVkFormats16);

  if (formats.size() == 1 && VK_FORMAT_UNDEFINED == formats[0].format) {
    surface_format_.format = preferred_formats[0];
    surface_format_.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  } else {
    bool format_set = false;
    for (VkSurfaceFormatKHR supported_format : formats) {
      unsigned int counter = 0;
      while (counter < size && format_set == false) {
        if (supported_format.format == preferred_formats[counter]) {
          surface_format_ = supported_format;
          surface_format_.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
          format_set = true;
        }
        counter++;
      }
      if (format_set)
        break;
    }
    if (!format_set) {
      DLOG(ERROR) << "Format not supported.";
      return false;
    }
  }
  // Delay creating SwapChain to when the surface size is specified by Resize().
  return true;
}

void VulkanSurface::Destroy() {
  swap_chain_->Destroy();
  vkDestroySurfaceKHR(vk_instance_, surface_, nullptr);
  surface_ = VK_NULL_HANDLE;
  std::move(destruction_callback_).Run();
}

gfx::SwapResult VulkanSurface::SwapBuffers() {
  return swap_chain_->SwapBuffers();
}

VulkanSwapChain* VulkanSurface::GetSwapChain() {
  return swap_chain_.get();
}

void VulkanSurface::Finish() {
  vkQueueWaitIdle(device_queue_->GetVulkanQueue());
}

bool VulkanSurface::SetSize(const gfx::Size& size) {
  // Get Surface Information.
  VkSurfaceCapabilitiesKHR surface_caps;
  VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      device_queue_->GetVulkanPhysicalDevice(), surface_, &surface_caps);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() failed: "
                << result;
    return false;
  }

  // If width and height of the surface are 0xFFFFFFFF, it means the surface
  // size will be determined by the extent of a swapchain targeting the surface.
  // In that case, we will use the |size| which is the window size for the
  // swapchain. Otherwise, we just use the current surface size for the
  // swapchian.
  if (surface_caps.currentExtent.width ==
          std::numeric_limits<uint32_t>::max() ||
      surface_caps.currentExtent.height ==
          std::numeric_limits<uint32_t>::max()) {
    DCHECK_EQ(surface_caps.currentExtent.width,
              std::numeric_limits<uint32_t>::max());
    DCHECK_EQ(surface_caps.currentExtent.height,
              std::numeric_limits<uint32_t>::max());
    surface_caps.currentExtent.width = size.width();
    surface_caps.currentExtent.height = size.height();
  }

  DCHECK_GE(surface_caps.currentExtent.width,
            surface_caps.minImageExtent.width);
  DCHECK_GE(surface_caps.currentExtent.height,
            surface_caps.minImageExtent.height);
  DCHECK_LE(surface_caps.currentExtent.width,
            surface_caps.maxImageExtent.width);
  DCHECK_LE(surface_caps.currentExtent.height,
            surface_caps.maxImageExtent.height);
  DCHECK_GT(surface_caps.currentExtent.width, 0u);
  DCHECK_GT(surface_caps.currentExtent.height, 0u);

  gfx::Size new_size(surface_caps.currentExtent.width,
                     surface_caps.currentExtent.height);
  if (size_ == new_size)
    return true;

  size_ = new_size;
  auto swap_chain = std::make_unique<VulkanSwapChain>();
  // Create Swapchain.
  if (!swap_chain->Initialize(device_queue_, surface_, surface_caps,
                              surface_format_, std::move(swap_chain_))) {
    return false;
  }

  swap_chain_ = std::move(swap_chain);
  return true;
}

}  // namespace gpu

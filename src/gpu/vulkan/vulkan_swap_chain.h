// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_SWAP_CHAIN_H_
#define GPU_VULKAN_VULKAN_SWAP_CHAIN_H_

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "base/logging.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/swap_result.h"

namespace gpu {

class VulkanCommandBuffer;
class VulkanCommandPool;
class VulkanDeviceQueue;

class VULKAN_EXPORT VulkanSwapChain {
 public:
  VulkanSwapChain();
  ~VulkanSwapChain();

  bool Initialize(VulkanDeviceQueue* device_queue,
                  VkSurfaceKHR surface,
                  const VkSurfaceCapabilitiesKHR& surface_caps,
                  const VkSurfaceFormatKHR& surface_format,
                  std::unique_ptr<VulkanSwapChain> old_swap_chain);
  void Destroy();

  gfx::SwapResult SwapBuffers();

  uint32_t num_images() const { return static_cast<uint32_t>(images_.size()); }
  uint32_t current_image() const { return current_image_; }
  const gfx::Size& size() const { return size_; }

  VulkanCommandBuffer* GetCurrentCommandBuffer() const {
    DCHECK_LT(current_image_, images_.size());
    return images_[current_image_]->pre_raster_command_buffer.get();
  }

  VkImage GetImage(uint32_t index) const {
    DCHECK_LT(index, images_.size());
    return images_[index]->image;
  }

  VkImage GetCurrentImage() const {
    DCHECK_LT(current_image_, images_.size());
    return images_[current_image_]->image;
  }

  VkImageLayout GetCurrentImageLayout() const {
    DCHECK_LT(current_image_, images_.size());
    return images_[current_image_]->layout;
  }

  void SetCurrentImageLayout(VkImageLayout layout) {
    DCHECK_LT(current_image_, images_.size());
    images_[current_image_]->layout = layout;
  }

 private:
  bool InitializeSwapChain(VkSurfaceKHR surface,
                           const VkSurfaceCapabilitiesKHR& surface_caps,
                           const VkSurfaceFormatKHR& surface_format,
                           std::unique_ptr<VulkanSwapChain> old_swap_chain);
  void DestroySwapChain();

  bool InitializeSwapImages(const VkSurfaceCapabilitiesKHR& surface_caps,
                            const VkSurfaceFormatKHR& surface_format);
  void DestroySwapImages();

  VulkanDeviceQueue* device_queue_;
  VkSwapchainKHR swap_chain_ = VK_NULL_HANDLE;

  std::unique_ptr<VulkanCommandPool> command_pool_;

  gfx::Size size_;

  struct ImageData {
    ImageData();
    ~ImageData();

    VkImage image = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::unique_ptr<VulkanCommandBuffer> pre_raster_command_buffer;
    std::unique_ptr<VulkanCommandBuffer> post_raster_command_buffer;

    VkSemaphore render_semaphore = VK_NULL_HANDLE;
    VkSemaphore present_semaphore = VK_NULL_HANDLE;
  };
  std::vector<std::unique_ptr<ImageData>> images_;
  uint32_t current_image_ = 0;

  VkSemaphore next_present_semaphore_ = VK_NULL_HANDLE;
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_SWAP_CHAIN_H_

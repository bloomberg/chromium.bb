// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_SURFACE_H_
#define GPU_VULKAN_VULKAN_SURFACE_H_

#include <vulkan/vulkan_core.h>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_swap_chain.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/swap_result.h"

namespace gpu {

class VulkanDeviceQueue;
class VulkanSwapChain;

class COMPONENT_EXPORT(VULKAN) VulkanSurface {
 public:
  // Minimum bit depth of surface.
  enum Format {
    FORMAT_RGBA_32,
    FORMAT_RGB_16,

    NUM_SURFACE_FORMATS,
    DEFAULT_SURFACE_FORMAT = FORMAT_RGBA_32
  };

  VulkanSurface(VkInstance vk_instance,
                gfx::AcceleratedWidget accelerated_widget,
                VkSurfaceKHR surface,
                uint64_t acquire_next_image_timeout_ns = UINT64_MAX);

  VulkanSurface(const VulkanSurface&) = delete;
  VulkanSurface& operator=(const VulkanSurface&) = delete;

  virtual ~VulkanSurface();

  bool Initialize(VulkanDeviceQueue* device_queue,
                  VulkanSurface::Format format);
  // Destroy() should be called when all related GPU tasks have been finished.
  virtual void Destroy();

  gfx::SwapResult SwapBuffers();
  gfx::SwapResult PostSubBuffer(const gfx::Rect& rect);
  void PostSubBufferAsync(
      const gfx::Rect& rect,
      VulkanSwapChain::PostSubBufferCompletionCallback callback);

  void Finish();

  // Reshape the the surface and recreate swap chian if it is needed. The size
  // is the current surface (window) size. The transform is the pre transform
  // relative to the hardware natural orientation, applied to frame content.
  // See VkSwapchainCreateInfoKHR::preTransform for detail.
  virtual bool Reshape(const gfx::Size& size, gfx::OverlayTransform transform);

  gfx::AcceleratedWidget accelerated_widget() const {
    return accelerated_widget_;
  }
  VulkanSwapChain* swap_chain() const { return swap_chain_.get(); }
  uint32_t swap_chain_generation() const { return swap_chain_generation_; }
  const gfx::Size& image_size() const { return image_size_; }
  VkImageUsageFlags image_usage_flags() const { return image_usage_flags_; }
  gfx::OverlayTransform transform() const { return transform_; }
  VkSurfaceFormatKHR surface_format() const { return surface_format_; }

 private:
  bool CreateSwapChain(const gfx::Size& size, gfx::OverlayTransform transform);

  const VkInstance vk_instance_;

  const gfx::AcceleratedWidget accelerated_widget_;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkSurfaceFormatKHR surface_format_ = {};
  raw_ptr<VulkanDeviceQueue> device_queue_ = nullptr;

  const uint64_t acquire_next_image_timeout_ns_;

  // The generation of |swap_chain_|, it will be increased if a new
  // |swap_chain_| is created due to resizing, etc.
  uint32_t swap_chain_generation_ = 0u;

  // Swap chain image size.
  gfx::Size image_size_;

  VkImageUsageFlags image_usage_flags_ = 0;

  // Swap chain pre-transform.
  gfx::OverlayTransform transform_ = gfx::OVERLAY_TRANSFORM_INVALID;

  std::unique_ptr<VulkanSwapChain> swap_chain_;
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_SURFACE_H_

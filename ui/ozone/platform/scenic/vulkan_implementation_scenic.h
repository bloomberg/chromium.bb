// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_VULKAN_IMPLEMENTATION_SCENIC_H_
#define UI_OZONE_PLATFORM_SCENIC_VULKAN_IMPLEMENTATION_SCENIC_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <memory>

#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"

namespace ui {

class ScenicWindowManager;

class VulkanImplementationScenic : public gpu::VulkanImplementation {
 public:
  VulkanImplementationScenic(ScenicWindowManager* scenic_window_manager,
                             fuchsia::ui::scenic::Scenic* scenic);
  ~VulkanImplementationScenic() override;

  // VulkanImplementation:
  bool InitializeVulkanInstance() override;
  VkInstance GetVulkanInstance() override;
  std::unique_ptr<gpu::VulkanSurface> CreateViewSurface(
      gfx::AcceleratedWidget window) override;
  bool GetPhysicalDevicePresentationSupport(
      VkPhysicalDevice device,
      const std::vector<VkQueueFamilyProperties>& queue_family_properties,
      uint32_t queue_family_index) override;
  std::vector<const char*> GetRequiredDeviceExtensions() override;
  VkFence CreateVkFenceForGpuFence(VkDevice vk_device) override;
  std::unique_ptr<gfx::GpuFence> ExportVkFenceToGpuFence(
      VkDevice vk_device,
      VkFence vk_fence) override;

 private:
  ScenicWindowManager* const scenic_window_manager_;
  fuchsia::ui::scenic::Scenic* const scenic_;
  gpu::VulkanInstance vulkan_instance_;

  PFN_vkVoidFunction vkCreateMagmaSurfaceKHR_ = nullptr;
  PFN_vkVoidFunction vkGetPhysicalDeviceMagmaPresentationSupportKHR_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VulkanImplementationScenic);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_VULKAN_IMPLEMENTATION_SCENIC_H_

// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/vulkan_implementation_gbm.h"

#include "base/files/file_path.h"
#include "base/native_library.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"

namespace ui {

VulkanImplementationGbm::VulkanImplementationGbm() {}

VulkanImplementationGbm::~VulkanImplementationGbm() {}

bool VulkanImplementationGbm::InitializeVulkanInstance() {
  gpu::VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  base::NativeLibraryLoadError native_library_load_error;
  vulkan_function_pointers->vulkan_loader_library_ = base::LoadNativeLibrary(
      base::FilePath("libvulkan.so.1"), &native_library_load_error);
  if (!vulkan_function_pointers->vulkan_loader_library_)
    return false;

  std::vector<const char*> required_extensions;
  if (!vulkan_instance_.Initialize(required_extensions)) {
    vulkan_instance_.Destroy();
    return false;
  }

  return true;
}

VkInstance VulkanImplementationGbm::GetVulkanInstance() {
  return vulkan_instance_.vk_instance();
}

std::unique_ptr<gpu::VulkanSurface> VulkanImplementationGbm::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  return nullptr;
}

bool VulkanImplementationGbm::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  return true;
}

std::vector<const char*>
VulkanImplementationGbm::GetRequiredDeviceExtensions() {
  return std::vector<const char*>();
}

}  // namespace ui

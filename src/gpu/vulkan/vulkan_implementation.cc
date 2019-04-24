// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_implementation.h"

#include "base/bind.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"

namespace gpu {

VulkanImplementation::VulkanImplementation() {}

VulkanImplementation::~VulkanImplementation() {}

std::unique_ptr<VulkanDeviceQueue> CreateVulkanDeviceQueue(
    VulkanImplementation* vulkan_implementation,
    uint32_t option) {
  auto device_queue = std::make_unique<VulkanDeviceQueue>(
      vulkan_implementation->GetVulkanInstance()->vk_instance());
  auto callback = base::BindRepeating(
      &VulkanImplementation::GetPhysicalDevicePresentationSupport,
      base::Unretained(vulkan_implementation));
  std::vector<const char*> required_extensions =
      vulkan_implementation->GetRequiredDeviceExtensions();
  if (!device_queue->Initialize(option, std::move(required_extensions),
                                callback)) {
    device_queue->Destroy();
    return nullptr;
  }

  return device_queue;
}

bool VulkanImplementation::SubmitSignalSemaphore(VkQueue vk_queue,
                                                 VkSemaphore vk_semaphore,
                                                 VkFence vk_fence) {
  // Structure specifying a queue submit operation.
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &vk_semaphore;
  const unsigned int submit_count = 1;
  if (vkQueueSubmit(vk_queue, submit_count, &submit_info, vk_fence) !=
      VK_SUCCESS) {
    return false;
  }
  return true;
}

bool VulkanImplementation::SubmitWaitSemaphore(VkQueue vk_queue,
                                               VkSemaphore vk_semaphore,
                                               VkFence vk_fence) {
  // Structure specifying a queue submit operation.
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &vk_semaphore;
  const unsigned int submit_count = 1;
  if (vkQueueSubmit(vk_queue, submit_count, &submit_info, vk_fence) !=
      VK_SUCCESS) {
    return false;
  }
  return true;
}

}  // namespace gpu

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/eventfd.h>

#include "base/files/scoped_file.h"
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/vulkan/android/vulkan_implementation_android.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class VulkanImplementationAndroidTest : public testing::Test {
 public:
  void SetUp() override {
    // Create a vulkan implementation.
    vk_implementation_ = std::make_unique<VulkanImplementationAndroid>();
    ASSERT_TRUE(vk_implementation_);
    ASSERT_TRUE(vk_implementation_->InitializeVulkanInstance());

    // Create vulkan context provider.
    vk_context_provider_ =
        viz::VulkanInProcessContextProvider::Create(vk_implementation_.get());
    ASSERT_TRUE(vk_context_provider_);

    // Get the VkDevice.
    vk_device_ = vk_context_provider_->GetDeviceQueue()->GetVulkanDevice();
    ASSERT_TRUE(vk_device_);
  }

  void TearDown() override {
    vk_context_provider_->Destroy();
    vk_device_ = VK_NULL_HANDLE;
  }

 protected:
  std::unique_ptr<VulkanImplementationAndroid> vk_implementation_;
  scoped_refptr<viz::VulkanInProcessContextProvider> vk_context_provider_;
  VkDevice vk_device_;
};

TEST_F(VulkanImplementationAndroidTest, ExportImportSyncFd) {
  // Create a vk semaphore which can be exported.
  // To create a semaphore whose payload can be exported to external handles,
  // add the VkExportSemaphoreCreateInfo structure to the pNext chain of the
  // VkSemaphoreCreateInfo structure.
  VkExportSemaphoreCreateInfo export_info;
  export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
  export_info.pNext = nullptr;
  export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

  VkSemaphore semaphore1;
  VkSemaphoreCreateInfo sem_info;
  sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  sem_info.pNext = &export_info;
  sem_info.flags = 0;
  bool result = vkCreateSemaphore(vk_device_, &sem_info, nullptr, &semaphore1);
  EXPECT_EQ(result, VK_SUCCESS);

  // SYNC_FD semaphores must be signalled or have an associated semaphore
  // signal operation pending execution before the export.
  // Semaphores can be signaled by including them in a batch as part of a queue
  // submission command, defining a queue operation to signal that semaphore.
  unsigned int submit_count = 1;
  VkFence fence = VK_NULL_HANDLE;
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &semaphore1;
  result =
      vkQueueSubmit(vk_context_provider_->GetDeviceQueue()->GetVulkanQueue(),
                    submit_count, &submit_info, fence);
  EXPECT_EQ(result, VK_SUCCESS);

  // Export a sync fd from the semaphore.
  base::ScopedFD sync_fd;
  EXPECT_TRUE(
      vk_implementation_->GetSemaphoreFdKHR(vk_device_, semaphore1, &sync_fd));
  EXPECT_GT(sync_fd.get(), -1);

  // Import the above sync fd into a new semaphore.
  VkSemaphore semaphore2;
  EXPECT_TRUE(vk_implementation_->ImportSemaphoreFdKHR(
      vk_device_, std::move(sync_fd), &semaphore2));

  // Wait for the device to be idle.
  result = vkDeviceWaitIdle(vk_device_);
  EXPECT_EQ(result, VK_SUCCESS);

  // Destroy the semaphores.
  vkDestroySemaphore(vk_device_, semaphore1, nullptr);
  vkDestroySemaphore(vk_device_, semaphore2, nullptr);
}

}  // namespace gpu

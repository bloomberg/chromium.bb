// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/semaphore_handle.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

class VulkanCommandPool;

class ExternalVkImageBacking : public SharedImageBacking {
 public:
  static std::unique_ptr<ExternalVkImageBacking> Create(
      SharedContextState* context_state,
      VulkanCommandPool* command_pool,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      base::span<const uint8_t> pixel_data,
      bool using_gmb = false);

  static std::unique_ptr<ExternalVkImageBacking> CreateFromGMB(
      SharedContextState* context_state,
      VulkanCommandPool* command_pool,
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat buffer_format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage);

  ~ExternalVkImageBacking() override;

  VkImage image() const { return image_; }
  VkDeviceMemory memory() const { return memory_; }
  size_t memory_size() const { return memory_size_; }
  VkFormat vk_format() const { return vk_format_; }
  SharedContextState* context_state() const { return context_state_; }
  VulkanImplementation* vulkan_implementation() const {
    return context_state()->vk_context_provider()->GetVulkanImplementation();
  }
  VkDevice device() const {
    return context_state()
        ->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanDevice();
  }
  bool need_sychronization() const {
    return usage() & SHARED_IMAGE_USAGE_GLES2;
  }

  // Notifies the backing that an access will start. Return false if there is
  // currently any other conflict access in progress. Otherwise, returns true
  // and semaphore handles which will be waited on before accessing.
  bool BeginAccess(bool readonly,
                   std::vector<SemaphoreHandle>* semaphore_handles);

  // Notifies the backing that an access has ended. The representation must
  // provide a semaphore handle that has been signaled at the end of the write
  // access.
  void EndAccess(bool readonly, SemaphoreHandle semaphore_handle);

  // SharedImageBacking implementation.
  bool IsCleared() const override;
  void SetCleared() override;
  void Update() override;
  void Destroy() override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;

 protected:
  bool BeginAccessInternal(bool readonly,
                           std::vector<SemaphoreHandle>* semaphore_handles);
  void EndAccessInternal(bool readonly, SemaphoreHandle semaphore_handle);

  // SharedImageBacking implementation.
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  ExternalVkImageBacking(const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         uint32_t usage,
                         SharedContextState* context_state,
                         VkImage image,
                         VkDeviceMemory memory,
                         size_t memory_size,
                         VkFormat vk_format,
                         VulkanCommandPool* command_pool);

  // Install a shared memory GMB to the backing.
  void InstallSharedMemory(
      base::WritableSharedMemoryMapping shared_memory_mapping,
      size_t stride,
      size_t memory_offset);

  bool WritePixels(const base::span<const uint8_t>& pixel_data, size_t stride);

  SharedContextState* const context_state_;
  VkImage image_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  SemaphoreHandle write_semaphore_handle_;
  std::vector<SemaphoreHandle> read_semaphore_handles_;
  const size_t memory_size_;
  bool is_cleared_ = false;
  const VkFormat vk_format_;
  VulkanCommandPool* const command_pool_;

  bool is_write_in_progress_ = false;
  uint32_t reads_in_progress_ = 0;
  gles2::Texture* texture_ = nullptr;

  // GMB related stuff.
  bool shared_memory_is_updated_ = false;
  base::WritableSharedMemoryMapping shared_memory_mapping_;
  size_t stride_ = 0;
  size_t memory_offset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ExternalVkImageBacking);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_

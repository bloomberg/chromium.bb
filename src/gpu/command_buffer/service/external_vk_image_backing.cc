// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_backing.h"

#include "gpu/command_buffer/service/external_vk_image_gl_representation.h"
#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gl/gl_context.h"

#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586

namespace gpu {

ExternalVkImageBacking::ExternalVkImageBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    SharedContextState* context_state,
    VkImage image,
    VkDeviceMemory memory,
    size_t memory_size,
    VkFormat vk_format)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         usage,
                         memory_size),
      context_state_(context_state),
      image_(image),
      memory_(memory),
      memory_size_(memory_size),
      vk_format_(vk_format) {}

ExternalVkImageBacking::~ExternalVkImageBacking() {
  // Destroy() will do any necessary cleanup.
}

VkSemaphore ExternalVkImageBacking::CreateExternalVkSemaphore() {
  VkExportSemaphoreCreateInfo export_info;
  export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
  export_info.pNext = nullptr;
  export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

  VkSemaphoreCreateInfo sem_info;
  sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  sem_info.pNext = &export_info;
  sem_info.flags = 0;

  VkSemaphore semaphore;
  VkResult result = vkCreateSemaphore(device(), &sem_info, nullptr, &semaphore);

  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Failed to create VkSemaphore: " << result;
    return VK_NULL_HANDLE;
  }

  return semaphore;
}

bool ExternalVkImageBacking::BeginVulkanReadAccess(
    VkSemaphore* gl_write_finished_semaphore) {
  if (is_write_in_progress_) {
    LOG(ERROR) << "Unable to begin read access for ExternalVkImageBacking "
               << "because a write access is in progress";
    return false;
  }
  ++reads_in_progress_;
  *gl_write_finished_semaphore = gl_write_finished_semaphore_;
  gl_write_finished_semaphore_ = VK_NULL_HANDLE;
  return true;
}

void ExternalVkImageBacking::EndVulkanReadAccess(
    VkSemaphore vulkan_read_finished_semaphore) {
  DCHECK_NE(0u, reads_in_progress_);
  --reads_in_progress_;
  // GL only needs to block on the latest semaphore. Destroy any existing
  // semaphore if it's not used yet.
  if (vulkan_read_finished_semaphore_ != VK_NULL_HANDLE) {
    // TODO(crbug.com/932260): This call is safe because we previously called
    // vkQueueWaitIdle in ExternalVkImageSkiaRepresentation::EndReadAccess.
    // However, vkQueueWaitIdle is a blocking call and should eventually be
    // replaced with better alternatives.
    vkDestroySemaphore(device(), vulkan_read_finished_semaphore_, nullptr);
  }
  vulkan_read_finished_semaphore_ = vulkan_read_finished_semaphore;
}

bool ExternalVkImageBacking::BeginGlWriteAccess(
    VkSemaphore* vulkan_read_finished_semaphore) {
  if (is_write_in_progress_ || reads_in_progress_) {
    LOG(ERROR) << "Unable to begin write access for ExternalVkImageBacking "
               << "because another read or write access is in progress";
    return false;
  }
  is_write_in_progress_ = true;
  *vulkan_read_finished_semaphore = vulkan_read_finished_semaphore_;
  vulkan_read_finished_semaphore_ = VK_NULL_HANDLE;
  return true;
}

void ExternalVkImageBacking::EndGlWriteAccess(
    VkSemaphore gl_write_finished_semaphore) {
  DCHECK(is_write_in_progress_);
  is_write_in_progress_ = false;
  // Vulkan only needs to block on the latest semaphore. Destroy any existing
  // semaphore if it's not used yet.
  if (gl_write_finished_semaphore_ != VK_NULL_HANDLE) {
    // This call is safe because this semaphore has only been used in GL and
    // therefore it's not associated with any unfinished task in a VkQueue.
    vkDestroySemaphore(device(), gl_write_finished_semaphore_, nullptr);
  }
  gl_write_finished_semaphore_ = gl_write_finished_semaphore;
}

bool ExternalVkImageBacking::BeginGlReadAccess() {
  if (is_write_in_progress_)
    return false;
  ++reads_in_progress_;
  return true;
}

void ExternalVkImageBacking::EndGlReadAccess() {
  DCHECK_NE(0u, reads_in_progress_);
  --reads_in_progress_;
}

bool ExternalVkImageBacking::IsCleared() const {
  return is_cleared_;
}

void ExternalVkImageBacking::SetCleared() {
  is_cleared_ = true;
}

void ExternalVkImageBacking::Update() {}

void ExternalVkImageBacking::Destroy() {
  // TODO(crbug.com/932260): We call vkQueueWaitIdle to ensure all these objects
  // are no longer associated with any queue command that has not completed
  // execution yet. Remove this call once we have better alternatives.
  vkQueueWaitIdle(context_state()
                      ->vk_context_provider()
                      ->GetDeviceQueue()
                      ->GetVulkanQueue());
  vkDestroyImage(device(), image_, nullptr);
  vkFreeMemory(device(), memory_, nullptr);
  if (vulkan_read_finished_semaphore_ != VK_NULL_HANDLE)
    vkDestroySemaphore(device(), vulkan_read_finished_semaphore_, nullptr);
  if (gl_write_finished_semaphore_ != VK_NULL_HANDLE)
    vkDestroySemaphore(device(), gl_write_finished_semaphore_, nullptr);
}

bool ExternalVkImageBacking::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  // It is not safe to produce a legacy mailbox because it would bypass the
  // synchronization between Vulkan and GL that is implemented in the
  // representation classes.
  return false;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
ExternalVkImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  VkMemoryGetFdInfoKHR get_fd_info;
  get_fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
  get_fd_info.pNext = nullptr;
  get_fd_info.memory = memory_;
  get_fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

  int memory_fd = -1;
  vkGetMemoryFdKHR(device(), &get_fd_info, &memory_fd);
  if (memory_fd < 0) {
    LOG(ERROR) << "Unable to extract file descriptor out of external VkImage";
    return nullptr;
  }

  gl::GLApi* api = gl::g_current_gl_context;

  constexpr GLenum target = GL_TEXTURE_2D;
  constexpr GLenum get_target = GL_TEXTURE_BINDING_2D;
  GLuint internal_format = viz::TextureStorageFormat(format());

  GLuint memory_object;
  api->glCreateMemoryObjectsEXTFn(1, &memory_object);
  api->glImportMemoryFdEXTFn(memory_object, memory_size_,
                             GL_HANDLE_TYPE_OPAQUE_FD_EXT, memory_fd);
  GLuint texture_service_id;
  api->glGenTexturesFn(1, &texture_service_id);
  GLint old_texture_binding = 0;
  api->glGetIntegervFn(get_target, &old_texture_binding);
  api->glBindTextureFn(target, texture_service_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  api->glTexStorageMem2DEXTFn(GL_TEXTURE_2D, 1, internal_format, size().width(),
                              size().height(), memory_object, 0);

  gles2::Texture* texture = new gles2::Texture(texture_service_id);
  texture->SetLightweightRef();
  texture->SetTarget(target, 1);
  texture->sampler_state_.min_filter = GL_LINEAR;
  texture->sampler_state_.mag_filter = GL_LINEAR;
  texture->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
  texture->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
  // If the backing is already cleared, no need to clear it again.
  gfx::Rect cleared_rect;
  if (is_cleared_)
    cleared_rect = gfx::Rect(size());

  GLenum gl_format = viz::GLDataFormat(format());
  GLenum gl_type = viz::GLDataType(format());
  texture->SetLevelInfo(target, 0, internal_format, size().width(),
                        size().height(), 1, 0, gl_format, gl_type,
                        cleared_rect);
  texture->SetImmutable(true);

  api->glBindTextureFn(target, old_texture_binding);

  return std::make_unique<ExternalVkImageGlRepresentation>(
      manager, this, tracker, texture, texture_service_id);
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
ExternalVkImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // Passthrough command decoder is not currently used on Linux.
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationSkia>
ExternalVkImageBacking::ProduceSkia(SharedImageManager* manager,
                                    MemoryTypeTracker* tracker) {
  // This backing type is only used when vulkan is enabled, so SkiaRenderer
  // should also be using Vulkan.
  DCHECK(context_state_->use_vulkan_gr_context());
  return std::make_unique<ExternalVkImageSkiaRepresentation>(manager, this,
                                                             tracker);
}

}  // namespace gpu

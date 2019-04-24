// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_gl_representation.h"

#include "gpu/vulkan/vulkan_function_pointers.h"

#define GL_LAYOUT_COLOR_ATTACHMENT_EXT 0x958E
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586

namespace gpu {

ExternalVkImageGlRepresentation::ExternalVkImageGlRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    gles2::Texture* texture,
    GLuint texture_service_id)
    : SharedImageRepresentationGLTexture(manager, backing, tracker),
      texture_(texture),
      texture_service_id_(texture_service_id) {}

ExternalVkImageGlRepresentation::~ExternalVkImageGlRepresentation() {
  texture_->RemoveLightweightRef(backing_impl()->have_context());
}

gles2::Texture* ExternalVkImageGlRepresentation::GetTexture() {
  return texture_;
}

bool ExternalVkImageGlRepresentation::BeginAccess(GLenum mode) {
  // There should not be multiple accesses in progress on the same
  // representation.
  if (current_access_mode_) {
    LOG(ERROR) << "BeginAccess called on ExternalVkImageGlRepresentation before"
               << " the previous access ended.";
    return false;
  }

  if (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM) {
    // If there is a write in progress, we can't start a read.
    if (!backing_impl()->BeginGlReadAccess())
      return false;
    current_access_mode_ = mode;
    // In reading mode, there is no need to wait on a semaphore because Vulkan
    // never writes into the backing.
    // TODO(crbug.com/932214): Implement synchronization when Vulkan can also
    // write into the backing.
    return true;
  }

  DCHECK_EQ(static_cast<GLenum>(GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM),
            mode);

  // See if it is possible to begin writing, i.e. there is no other read or
  // write in progress. If so, take the latest semaphore that Vulkan signalled
  // after reading.
  VkSemaphore vulkan_read_finished_semaphore;
  if (!backing_impl()->BeginGlWriteAccess(&vulkan_read_finished_semaphore))
    return false;

  if (vulkan_read_finished_semaphore != VK_NULL_HANDLE) {
    GLuint gl_semaphore =
        ImportVkSemaphoreIntoGL(vulkan_read_finished_semaphore);
    if (!gl_semaphore) {
      // TODO(crbug.com/932260): This call is safe because we previously called
      // vkQueueWaitIdle in ExternalVkImageSkiaRepresentation::EndReadAccess.
      // However, vkQueueWaitIdle is a blocking call and should eventually be
      // replaced with better alternatives.
      vkDestroySemaphore(backing_impl()->device(),
                         vulkan_read_finished_semaphore, nullptr);
      return false;
    }
    GLenum src_layout = GL_LAYOUT_COLOR_ATTACHMENT_EXT;
    api()->glWaitSemaphoreEXTFn(gl_semaphore, 0, nullptr, 1,
                                &texture_service_id_, &src_layout);
    api()->glDeleteSemaphoresEXTFn(1, &gl_semaphore);
    // TODO(crbug.com/932260): This call is safe because we previously called
    // vkQueueWaitIdle in ExternalVkImageSkiaRepresentation::EndReadAccess.
    vkDestroySemaphore(backing_impl()->device(), vulkan_read_finished_semaphore,
                       nullptr);
  }
  current_access_mode_ = mode;
  return true;
}

void ExternalVkImageGlRepresentation::EndAccess() {
  if (!current_access_mode_) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(FATAL) << "EndAccess called on ExternalVkImageGlRepresentation before "
               << "BeginAccess";
    return;
  }

  if (current_access_mode_ == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM) {
    // Since Vulkan never writes into the backing, there is no need to signal
    // that GL is done reading.
    // TODO(crbug.com/932214): Implement synchronization when Vulkan can also
    // write into the backing.
    backing_impl()->EndGlReadAccess();
    current_access_mode_ = 0;
    return;
  }

  DCHECK_EQ(static_cast<GLenum>(GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM),
            current_access_mode_);
  current_access_mode_ = 0;

  VkSemaphore gl_write_finished_semaphore =
      backing_impl()->CreateExternalVkSemaphore();
  if (!gl_write_finished_semaphore) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(FATAL) << "Unable to create a VkSemaphore in "
               << "ExternalVkImageGlRepresentation for synchronization with "
               << "Vulkan";
    return;
  }
  GLuint gl_semaphore = ImportVkSemaphoreIntoGL(gl_write_finished_semaphore);
  if (!gl_semaphore) {
    // It is safe to destroy the VkSemaphore here because it has not been sent
    // to a VkQueue before.
    vkDestroySemaphore(backing_impl()->device(), gl_write_finished_semaphore,
                       nullptr);
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(FATAL) << "Unable to export VkSemaphore into GL in "
               << "ExternalVkImageGlRepresentation for synchronization with "
               << "Vulkan";
    return;
  }
  GLenum dst_layout = GL_LAYOUT_COLOR_ATTACHMENT_EXT;
  api()->glSignalSemaphoreEXTFn(gl_semaphore, 0, nullptr, 1,
                                &texture_service_id_, &dst_layout);
  api()->glDeleteSemaphoresEXTFn(1, &gl_semaphore);
  backing_impl()->EndGlWriteAccess(gl_write_finished_semaphore);
}

GLuint ExternalVkImageGlRepresentation::ImportVkSemaphoreIntoGL(
    VkSemaphore semaphore) {
  VkSemaphoreGetFdInfoKHR info;
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
  info.pNext = nullptr;
  info.semaphore = semaphore;
  info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

  int fd = -1;
  bool result = vkGetSemaphoreFdKHR(backing_impl()->device(), &info, &fd);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkGetSemaphoreFdKHR failed : " << result;
    return 0;
  }

  gl::GLApi* api = gl::g_current_gl_context;
  GLuint gl_semaphore;
  api->glGenSemaphoresEXTFn(1, &gl_semaphore);
  api->glImportSemaphoreFdEXTFn(gl_semaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);

  return gl_semaphore;
}

}  // namespace gpu

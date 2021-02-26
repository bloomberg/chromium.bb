// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_gl_representation.h"

#include <vector>

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"

#define GL_LAYOUT_GENERAL_EXT 0x958D
#define GL_LAYOUT_COLOR_ATTACHMENT_EXT 0x958E
#define GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT 0x958F
#define GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT 0x9590
#define GL_LAYOUT_SHADER_READ_ONLY_EXT 0x9591
#define GL_LAYOUT_TRANSFER_SRC_EXT 0x9592
#define GL_LAYOUT_TRANSFER_DST_EXT 0x9593
#define GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT 0x9530
#define GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT 0x9531

namespace gpu {

namespace {

GLenum ToGLImageLayout(VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return GL_NONE;
    case VK_IMAGE_LAYOUT_GENERAL:
      return GL_LAYOUT_GENERAL_EXT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return GL_LAYOUT_COLOR_ATTACHMENT_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      return GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return GL_LAYOUT_SHADER_READ_ONLY_EXT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return GL_LAYOUT_TRANSFER_SRC_EXT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return GL_LAYOUT_TRANSFER_DST_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
      return GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
      return GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT;
    default:
      NOTREACHED() << "Invalid image layout " << layout;
      return GL_NONE;
  }
}

}  // namespace

// static
void ExternalVkImageGLRepresentationShared::AcquireTexture(
    ExternalSemaphore* semaphore,
    GLuint texture_id,
    VkImageLayout src_layout) {
  GLuint gl_semaphore = semaphore->GetGLSemaphore();
  if (gl_semaphore) {
    GLenum gl_layout = ToGLImageLayout(src_layout);
    auto* api = gl::g_current_gl_context;
    api->glWaitSemaphoreEXTFn(gl_semaphore, 0, nullptr, 1, &texture_id,
                              &gl_layout);
  }
}

// static
ExternalSemaphore ExternalVkImageGLRepresentationShared::ReleaseTexture(
    ExternalSemaphorePool* pool,
    GLuint texture_id,
    VkImageLayout dst_layout) {
  ExternalSemaphore semaphore = pool->GetOrCreateSemaphore();
  if (!semaphore) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(FATAL) << "Unable to create an ExternalSemaphore in "
               << "ExternalVkImageGLRepresentation for synchronization with "
               << "Vulkan";
    return {};
  }

  GLuint gl_semaphore = semaphore.GetGLSemaphore();
  if (!gl_semaphore) {
    // TODO(crbug.com/933452): We should be able to semaphore_handle this
    // failure more gracefully rather than shutting down the whole process.
    LOG(FATAL) << "Unable to export VkSemaphore into GL in "
               << "ExternalVkImageGLRepresentation for synchronization with "
               << "Vulkan";
    return {};
  }

  GLenum gl_layout = ToGLImageLayout(dst_layout);
  auto* api = gl::g_current_gl_context;
  api->glSignalSemaphoreEXTFn(gl_semaphore, 0, nullptr, 1, &texture_id,
                              &gl_layout);
  // Base on the spec, the glSignalSemaphoreEXT() call just inserts signal
  // semaphore command in the gl context. It may or may not flush the context
  // which depends on the implementation. So to make it safe, we always call
  // glFlush() here. If the implementation does flush in the
  // glSignalSemaphoreEXT() call, the glFlush() call should be a noop.
  api->glFlushFn();

  return semaphore;
}

ExternalVkImageGLRepresentationShared::ExternalVkImageGLRepresentationShared(
    SharedImageBacking* backing,
    GLuint texture_service_id)
    : backing_(static_cast<ExternalVkImageBacking*>(backing)),
      texture_service_id_(texture_service_id) {}

ExternalVkImageGLRepresentationShared::
    ~ExternalVkImageGLRepresentationShared() = default;

bool ExternalVkImageGLRepresentationShared::BeginAccess(GLenum mode) {
  // There should not be multiple accesses in progress on the same
  // representation.
  if (current_access_mode_) {
    LOG(ERROR) << "BeginAccess called on ExternalVkImageGLRepresentation before"
               << " the previous access ended.";
    return false;
  }

  DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
         mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM ||
         mode == GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM);
  const bool readonly =
      (mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

  DCHECK(begin_access_semaphores_.empty());
  if (!backing_impl()->BeginAccess(readonly, &begin_access_semaphores_,
                                   true /* is_gl */))
    return false;

  for (auto& external_semaphore : begin_access_semaphores_) {
    GrVkImageInfo info;
    auto result = backing_impl()->backend_texture().getVkImageInfo(&info);
    DCHECK(result);
    DCHECK_EQ(info.fCurrentQueueFamily, VK_QUEUE_FAMILY_EXTERNAL);
    DCHECK_NE(info.fImageLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    DCHECK_NE(info.fImageLayout, VK_IMAGE_LAYOUT_PREINITIALIZED);
    AcquireTexture(&external_semaphore, texture_service_id_, info.fImageLayout);
  }
  current_access_mode_ = mode;
  return true;
}

void ExternalVkImageGLRepresentationShared::EndAccess() {
  if (!current_access_mode_) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(ERROR) << "EndAccess called on ExternalVkImageGLRepresentation before "
               << "BeginAccess";
    return;
  }

  DCHECK(current_access_mode_ == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
         current_access_mode_ ==
             GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM ||
         current_access_mode_ == GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM);
  const bool readonly =
      (current_access_mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  current_access_mode_ = 0;

  ExternalSemaphore external_semaphore;
  if (backing_impl()->need_synchronization() &&
      backing_impl()->gl_reads_in_progress() <= 1) {
    DCHECK(readonly == !!backing_impl()->gl_reads_in_progress());
    GrVkImageInfo info;
    auto result = backing_impl()->backend_texture().getVkImageInfo(&info);
    DCHECK(result);
    DCHECK_EQ(info.fCurrentQueueFamily, VK_QUEUE_FAMILY_EXTERNAL);
    DCHECK_NE(info.fImageLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    DCHECK_NE(info.fImageLayout, VK_IMAGE_LAYOUT_PREINITIALIZED);
    external_semaphore =
        ReleaseTexture(backing_impl()->external_semaphore_pool(),
                       texture_service_id_, info.fImageLayout);
  }
  backing_impl()->EndAccess(readonly, std::move(external_semaphore),
                            true /* is_gl */);

  // We have done with |begin_access_semaphores_|. They should have been waited.
  // So add them to pending semaphores for reusing or relaeasing.
  backing_impl()->AddSemaphoresToPendingListOrRelease(
      std::move(begin_access_semaphores_));
  begin_access_semaphores_.clear();
}

ExternalVkImageGLRepresentation::ExternalVkImageGLRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    gles2::Texture* texture,
    GLuint texture_service_id)
    : SharedImageRepresentationGLTexture(manager, backing, tracker),
      texture_(texture),
      representation_shared_(backing, texture_service_id) {}

ExternalVkImageGLRepresentation::~ExternalVkImageGLRepresentation() {}

gles2::Texture* ExternalVkImageGLRepresentation::GetTexture() {
  return texture_;
}

bool ExternalVkImageGLRepresentation::BeginAccess(GLenum mode) {
  return representation_shared_.BeginAccess(mode);
}
void ExternalVkImageGLRepresentation::EndAccess() {
  representation_shared_.EndAccess();
}

ExternalVkImageGLPassthroughRepresentation::
    ExternalVkImageGLPassthroughRepresentation(SharedImageManager* manager,
                                               SharedImageBacking* backing,
                                               MemoryTypeTracker* tracker,
                                               GLuint texture_service_id)
    : SharedImageRepresentationGLTexturePassthrough(manager, backing, tracker),
      representation_shared_(backing, texture_service_id) {}

ExternalVkImageGLPassthroughRepresentation::
    ~ExternalVkImageGLPassthroughRepresentation() {}

const scoped_refptr<gles2::TexturePassthrough>&
ExternalVkImageGLPassthroughRepresentation::GetTexturePassthrough() {
  return representation_shared_.backing_impl()->GetTexturePassthrough();
}

bool ExternalVkImageGLPassthroughRepresentation::BeginAccess(GLenum mode) {
  return representation_shared_.BeginAccess(mode);
}
void ExternalVkImageGLPassthroughRepresentation::EndAccess() {
  representation_shared_.EndAccess();
}

}  // namespace gpu

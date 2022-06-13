// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/external_semaphore.h"
#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

// ExternalVkImageGLRepresentationShared implements BeginAccess and EndAccess
// methods for ExternalVkImageGLRepresentation and
// ExternalVkImageGLPassthroughRepresentation.
class ExternalVkImageGLRepresentationShared {
 public:
  static void AcquireTexture(ExternalSemaphore* semaphore,
                             GLuint texture_id,
                             VkImageLayout src_layout);
  static ExternalSemaphore ReleaseTexture(ExternalSemaphorePool* pool,
                                          GLuint texture_id,
                                          VkImageLayout dst_layout);

  ExternalVkImageGLRepresentationShared(SharedImageBacking* backing,
                                        GLuint texture_service_id);

  ExternalVkImageGLRepresentationShared(
      const ExternalVkImageGLRepresentationShared&) = delete;
  ExternalVkImageGLRepresentationShared& operator=(
      const ExternalVkImageGLRepresentationShared&) = delete;

  ~ExternalVkImageGLRepresentationShared();

  bool BeginAccess(GLenum mode);
  void EndAccess();

  ExternalVkImageBacking* backing_impl() const { return backing_; }

 private:
  viz::VulkanContextProvider* context_provider() const {
    return backing_impl()->context_provider();
  }

  const raw_ptr<ExternalVkImageBacking> backing_;
  const GLuint texture_service_id_;
  GLenum current_access_mode_ = 0;
  std::vector<ExternalSemaphore> begin_access_semaphores_;
};

class ExternalVkImageGLRepresentation
    : public SharedImageRepresentationGLTexture {
 public:
  ExternalVkImageGLRepresentation(SharedImageManager* manager,
                                  SharedImageBacking* backing,
                                  MemoryTypeTracker* tracker,
                                  gles2::Texture* texture,
                                  GLuint texture_service_id);

  ExternalVkImageGLRepresentation(const ExternalVkImageGLRepresentation&) =
      delete;
  ExternalVkImageGLRepresentation& operator=(
      const ExternalVkImageGLRepresentation&) = delete;

  ~ExternalVkImageGLRepresentation() override;

  // SharedImageRepresentationGLTexture implementation.
  gles2::Texture* GetTexture() override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  const raw_ptr<gles2::Texture> texture_;
  ExternalVkImageGLRepresentationShared representation_shared_;
};

class ExternalVkImageGLPassthroughRepresentation
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  ExternalVkImageGLPassthroughRepresentation(SharedImageManager* manager,
                                             SharedImageBacking* backing,
                                             MemoryTypeTracker* tracker,
                                             GLuint texture_service_id);

  ExternalVkImageGLPassthroughRepresentation(
      const ExternalVkImageGLPassthroughRepresentation&) = delete;
  ExternalVkImageGLPassthroughRepresentation& operator=(
      const ExternalVkImageGLPassthroughRepresentation&) = delete;

  ~ExternalVkImageGLPassthroughRepresentation() override;

  // SharedImageRepresentationGLTexturePassthrough implementation.
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  ExternalVkImageGLRepresentationShared representation_shared_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_

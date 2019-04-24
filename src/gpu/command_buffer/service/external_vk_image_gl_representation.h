// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_

#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

class ExternalVkImageGlRepresentation
    : public SharedImageRepresentationGLTexture {
 public:
  ExternalVkImageGlRepresentation(SharedImageManager* manager,
                                  SharedImageBacking* backing,
                                  MemoryTypeTracker* tracker,
                                  gles2::Texture* texture,
                                  GLuint texture_service_id);
  ~ExternalVkImageGlRepresentation() override;

  // SharedImageRepresentationGLTexture implementation.
  gles2::Texture* GetTexture() override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  ExternalVkImageBacking* backing_impl() {
    return static_cast<ExternalVkImageBacking*>(backing());
  }

  gl::GLApi* api() { return gl::g_current_gl_context; }

  GLuint ImportVkSemaphoreIntoGL(VkSemaphore semaphore);

  gles2::Texture* texture_;
  GLuint texture_service_id_;
  GLenum current_access_mode_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ExternalVkImageGlRepresentation);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_GL_REPRESENTATION_H_

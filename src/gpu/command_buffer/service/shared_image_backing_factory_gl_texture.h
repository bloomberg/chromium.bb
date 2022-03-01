// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_H_

#include <memory>

#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory_gl_common.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_common.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace gpu {
class SharedImageBacking;
class GpuDriverBugWorkarounds;
struct GpuFeatureInfo;
struct GpuPreferences;
struct Mailbox;

// Implementation of SharedImageBackingFactory that produces GL-texture backed
// SharedImages.
class GPU_GLES2_EXPORT SharedImageBackingFactoryGLTexture
    : public SharedImageBackingFactoryGLCommon {
 public:
  SharedImageBackingFactoryGLTexture(const GpuPreferences& gpu_preferences,
                                     const GpuDriverBugWorkarounds& workarounds,
                                     const GpuFeatureInfo& gpu_feature_info,
                                     gl::ProgressReporter* progress_reporter);
  ~SharedImageBackingFactoryGLTexture() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override;
  bool IsSupported(uint32_t usage,
                   viz::ResourceFormat format,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   bool* allow_legacy_mailbox,
                   bool is_pixel_used) override;

  static std::unique_ptr<SharedImageBacking> CreateSharedImageForTest(
      const Mailbox& mailbox,
      GLenum target,
      GLuint service_id,
      bool is_cleared,
      viz::ResourceFormat format,
      const gfx::Size& size,
      uint32_t usage);

 private:
  std::unique_ptr<SharedImageBacking> CreateSharedImageInternal(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_H_

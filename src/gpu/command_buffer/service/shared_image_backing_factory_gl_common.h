// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_COMMON_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_COMMON_H_

#include "base/memory/raw_ptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_common.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace gpu {
class GpuDriverBugWorkarounds;
struct GpuFeatureInfo;
struct GpuPreferences;

// Common constructor and helper functions for
// SharedImageBackingFactoryGLTexture and SharedImageBackingFactoryGLImage.
class GPU_GLES2_EXPORT SharedImageBackingFactoryGLCommon
    : public SharedImageBackingFactory {
 public:
  struct FormatInfo {
    FormatInfo();
    FormatInfo(const FormatInfo& other);
    ~FormatInfo();

    // Whether this format is supported.
    bool enabled = false;

    // Whether this format supports TexStorage2D.
    bool supports_storage = false;

    // Whether the texture is a compressed type.
    bool is_compressed = false;

    GLenum gl_format = 0;
    GLenum gl_type = 0;
    raw_ptr<const gles2::Texture::CompatibilitySwizzle> swizzle = nullptr;
    GLenum adjusted_format = 0;

    // The internalformat portion of the format/type/internalformat triplet
    // used when calling TexImage2D
    GLuint image_internal_format = 0;

    // The internalformat portion of the format/type/internalformat triplet
    // used when calling TexStorage2D
    GLuint storage_internal_format = 0;
  };

 protected:
  SharedImageBackingFactoryGLCommon(const GpuPreferences& gpu_preferences,
                                    const GpuDriverBugWorkarounds& workarounds,
                                    const GpuFeatureInfo& gpu_feature_info,
                                    gl::ProgressReporter* progress_reporter);
  ~SharedImageBackingFactoryGLCommon() override;

  bool CanCreateSharedImage(const gfx::Size& size,
                            base::span<const uint8_t> pixel_data,
                            const FormatInfo& format_info,
                            GLenum target);

  // Whether we're using the passthrough command decoder and should generate
  // passthrough textures.
  bool use_passthrough_ = false;

  FormatInfo format_info_[viz::RESOURCE_FORMAT_MAX + 1];
  int32_t max_texture_size_ = 0;
  bool texture_usage_angle_ = false;
  SharedImageBackingGLCommon::UnpackStateAttribs attribs_;
  GpuDriverBugWorkarounds workarounds_;

  // Used to notify the watchdog before a buffer allocation in case it takes
  // long.
  const raw_ptr<gl::ProgressReporter> progress_reporter_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_COMMON_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_common.h"

#include <algorithm>
#include <list>

#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingFactoryGLCommon

SharedImageBackingFactoryGLCommon::SharedImageBackingFactoryGLCommon(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    gl::ProgressReporter* progress_reporter)
    : use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder &&
                       gles2::PassthroughCommandDecoderSupported()),
      workarounds_(workarounds),
      progress_reporter_(progress_reporter) {
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_texture_size_);
  // When the passthrough command decoder is used, the max_texture_size
  // workaround is implemented by ANGLE. Trying to adjust the max size here
  // would cause discrepancy between what we think the max size is and what
  // ANGLE tells the clients.
  if (!use_passthrough_ && workarounds.max_texture_size) {
    max_texture_size_ =
        std::min(max_texture_size_, workarounds.max_texture_size);
  }
  // Ensure max_texture_size_ is less than INT_MAX so that gfx::Rect and friends
  // can be used to accurately represent all valid sub-rects, with overflow
  // cases, clamped to INT_MAX, always invalid.
  max_texture_size_ = std::min(max_texture_size_, INT_MAX - 1);

  // TODO(piman): Can we extract the logic out of FeatureInfo?
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(workarounds, gpu_feature_info);
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                           use_passthrough_, gles2::DisallowedFeatures());
  texture_usage_angle_ = feature_info->feature_flags().angle_texture_usage;
  attribs_.es3_capable = feature_info->IsES3Capable();
  attribs_.desktop_gl = !feature_info->gl_version_info().is_es;
  // Can't use the value from feature_info, as we unconditionally enable this
  // extension, and assume it can't be used if PBOs are not used (which isn't
  // true for Skia used directly against GL).
  attribs_.supports_unpack_subimage =
      gl::g_current_gl_driver->ext.b_GL_EXT_unpack_subimage;
  bool enable_texture_storage =
      feature_info->feature_flags().ext_texture_storage;
  const gles2::Validators* validators = feature_info->validators();
  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = static_cast<viz::ResourceFormat>(i);
    FormatInfo& info = format_info_[i];
    if (!viz::GLSupportsFormat(format))
      continue;
    const GLuint image_internal_format = viz::GLInternalFormat(format);
    const GLenum gl_format = viz::GLDataFormat(format);
    const GLenum gl_type = viz::GLDataType(format);
    const bool uncompressed_format_valid =
        validators->texture_internal_format.IsValid(image_internal_format) &&
        validators->texture_format.IsValid(gl_format);
    const bool compressed_format_valid =
        validators->compressed_texture_format.IsValid(image_internal_format);
    if ((uncompressed_format_valid || compressed_format_valid) &&
        validators->pixel_type.IsValid(gl_type)) {
      info.enabled = true;
      info.is_compressed = compressed_format_valid;
      info.gl_format = gl_format;
      info.gl_type = gl_type;
      info.swizzle = gles2::TextureManager::GetCompatibilitySwizzle(
          feature_info.get(), gl_format);
      info.image_internal_format =
          gles2::TextureManager::AdjustTexInternalFormat(
              feature_info.get(), image_internal_format, gl_type);
      info.adjusted_format =
          gles2::TextureManager::AdjustTexFormat(feature_info.get(), gl_format);
    }
    if (!info.enabled)
      continue;
    if (enable_texture_storage && !info.is_compressed) {
      GLuint storage_internal_format = viz::TextureStorageFormat(format);
      if (validators->texture_internal_format_storage.IsValid(
              storage_internal_format)) {
        info.supports_storage = true;
        info.storage_internal_format =
            gles2::TextureManager::AdjustTexStorageFormat(
                feature_info.get(), storage_internal_format);
      }
    }
  }
}

SharedImageBackingFactoryGLCommon::~SharedImageBackingFactoryGLCommon() =
    default;

bool SharedImageBackingFactoryGLCommon::CanCreateSharedImage(
    const gfx::Size& size,
    base::span<const uint8_t> pixel_data,
    const FormatInfo& format_info,
    GLenum target) {
  if (!format_info.enabled) {
    LOG(ERROR) << "CreateSharedImage: invalid format";
    return false;
  }

  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_texture_size_ || size.height() > max_texture_size_) {
    LOG(ERROR) << "CreateSharedImage: invalid size: " << size.ToString()
               << ", max_texture_size_=" << max_texture_size_;
    return false;
  }

  // If we have initial data to upload, ensure it is sized appropriately.
  if (!pixel_data.empty()) {
    if (format_info.is_compressed) {
      const char* error_message = "unspecified";
      if (!gles2::ValidateCompressedTexDimensions(
              target, 0 /* level */, size.width(), size.height(), 1 /* depth */,
              format_info.image_internal_format, &error_message)) {
        LOG(ERROR) << "CreateSharedImage: "
                      "ValidateCompressedTexDimensionsFailed with error: "
                   << error_message;
        return false;
      }

      GLsizei bytes_required = 0;
      if (!gles2::GetCompressedTexSizeInBytes(
              nullptr /* function_name */, size.width(), size.height(),
              1 /* depth */, format_info.image_internal_format, &bytes_required,
              nullptr /* error_state */)) {
        LOG(ERROR) << "CreateSharedImage: Unable to compute required size for "
                      "initial texture upload.";
        return false;
      }

      if (bytes_required < 0 ||
          pixel_data.size() != static_cast<size_t>(bytes_required)) {
        LOG(ERROR) << "CreateSharedImage: Initial data does not have expected "
                      "size.";
        return false;
      }
    } else {
      uint32_t bytes_required;
      uint32_t unpadded_row_size = 0u;
      uint32_t padded_row_size = 0u;
      if (!gles2::GLES2Util::ComputeImageDataSizes(
              size.width(), size.height(), 1 /* depth */, format_info.gl_format,
              format_info.gl_type, 4 /* alignment */, &bytes_required,
              &unpadded_row_size, &padded_row_size)) {
        LOG(ERROR) << "CreateSharedImage: Unable to compute required size for "
                      "initial texture upload.";
        return false;
      }

      // The GL spec, used in the computation for required bytes in the function
      // above, assumes no padding is required for the last row in the image.
      // But the client data does include this padding, so we add it for the
      // data validation check here.
      uint32_t padding = padded_row_size - unpadded_row_size;
      bytes_required += padding;
      if (pixel_data.size() != bytes_required) {
        LOG(ERROR) << "CreateSharedImage: Initial data does not have expected "
                      "size.";
        return false;
      }
    }
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageBackingFactoryGLCommon::FormatInfo

SharedImageBackingFactoryGLCommon::FormatInfo::FormatInfo() = default;
SharedImageBackingFactoryGLCommon::FormatInfo::FormatInfo(
    const FormatInfo& other) = default;
SharedImageBackingFactoryGLCommon::FormatInfo::~FormatInfo() = default;

}  // namespace gpu

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_image_test_support.h"

#include <vector>

#include "third_party/skia/include/core/SkTypes.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if defined(USE_OZONE)
#include "base/message_loop/message_loop.h"
#endif

namespace gfx {

// static
void GLImageTestSupport::InitializeGL() {
#if defined(USE_OZONE)
  // On Ozone, the backend initializes the event system using a UI thread.
  base::MessageLoopForUI main_loop;
#endif

  std::vector<GLImplementation> allowed_impls;
  GetAllowedGLImplementations(&allowed_impls);
  DCHECK(!allowed_impls.empty());

  GLImplementation impl = allowed_impls[0];
  GLSurfaceTestSupport::InitializeOneOffImplementation(impl, true);
}

// static
void GLImageTestSupport::CleanupGL() {
  ClearGLBindings();
}

// static
GLenum GLImageTestSupport::GetPreferredInternalFormat() {
  bool has_texture_format_bgra8888 =
      GLContext::GetCurrent()->HasExtension(
          "GL_APPLE_texture_format_BGRA8888") ||
      GLContext::GetCurrent()->HasExtension("GL_EXT_texture_format_BGRA8888") ||
      !GLContext::GetCurrent()->GetVersionInfo()->is_es;
  return (!SK_B32_SHIFT && has_texture_format_bgra8888) ? GL_BGRA_EXT : GL_RGBA;
}

// static
BufferFormat GLImageTestSupport::GetPreferredBufferFormat() {
  return GetPreferredInternalFormat() == GL_BGRA_EXT ? BufferFormat::BGRA_8888
                                                     : BufferFormat::RGBA_8888;
}

// static
void GLImageTestSupport::SetBufferDataToColor(int width,
                                              int height,
                                              int stride,
                                              BufferFormat format,
                                              const uint8_t color[4],
                                              uint8_t* data) {
  switch (format) {
    case BufferFormat::RGBA_8888:
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 4 + 0] = color[0];
          data[y * stride + x * 4 + 1] = color[1];
          data[y * stride + x * 4 + 2] = color[2];
          data[y * stride + x * 4 + 3] = color[3];
        }
      }
      return;
    case BufferFormat::BGRA_8888:
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 4 + 0] = color[2];
          data[y * stride + x * 4 + 1] = color[1];
          data[y * stride + x * 4 + 2] = color[0];
          data[y * stride + x * 4 + 3] = color[3];
        }
      }
      return;
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
    case BufferFormat::R_8:
    case BufferFormat::RGBA_4444:
    case BufferFormat::BGRX_8888:
    case BufferFormat::UYVY_422:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::YUV_420:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

}  // namespace gfx

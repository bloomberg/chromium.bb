// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_image_test_support.h"

#include <vector>

#include "ui/gl/gl_implementation.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if defined(USE_OZONE)
#include "base/message_loop/message_loop.h"
#endif

namespace gl {

// static
void GLImageTestSupport::InitializeGL() {
#if defined(USE_OZONE)
  // On Ozone, the backend initializes the event system using a UI thread.
  base::MessageLoopForUI main_loop;
#endif

  std::vector<gfx::GLImplementation> allowed_impls;
  GetAllowedGLImplementations(&allowed_impls);
  DCHECK(!allowed_impls.empty());

  gfx::GLImplementation impl = allowed_impls[0];
  gfx::GLSurfaceTestSupport::InitializeOneOffImplementation(impl, true);
}

// static
void GLImageTestSupport::CleanupGL() {
  gfx::ClearGLBindings();
}

// static
void GLImageTestSupport::SetBufferDataToColor(int width,
                                              int height,
                                              int stride,
                                              gfx::BufferFormat format,
                                              const uint8_t color[4],
                                              uint8_t* data) {
  switch (format) {
    case gfx::BufferFormat::RGBX_8888:
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 4 + 0] = color[0];
          data[y * stride + x * 4 + 1] = color[1];
          data[y * stride + x * 4 + 2] = color[2];
          data[y * stride + x * 4 + 3] = 0xaa;  // unused
        }
      }
      return;
    case gfx::BufferFormat::RGBA_8888:
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 4 + 0] = color[0];
          data[y * stride + x * 4 + 1] = color[1];
          data[y * stride + x * 4 + 2] = color[2];
          data[y * stride + x * 4 + 3] = color[3];
        }
      }
      return;
    case gfx::BufferFormat::BGRX_8888:
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 4 + 0] = color[2];
          data[y * stride + x * 4 + 1] = color[1];
          data[y * stride + x * 4 + 2] = color[0];
          data[y * stride + x * 4 + 3] = 0xaa;  // unused
        }
      }
      return;
    case gfx::BufferFormat::BGRA_8888:
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          data[y * stride + x * 4 + 0] = color[2];
          data[y * stride + x * 4 + 1] = color[1];
          data[y * stride + x * 4 + 2] = color[0];
          data[y * stride + x * 4 + 3] = color[3];
        }
      }
      return;
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::UYVY_422:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::YUV_420:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

}  // namespace gl

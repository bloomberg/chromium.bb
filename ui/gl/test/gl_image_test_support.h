// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TEST_GL_IMAGE_TEST_SUPPORT_H_
#define UI_GL_TEST_GL_IMAGE_TEST_SUPPORT_H_

#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

class GLImageTestSupport {
 public:
  // Initialize GL for image testing.
  static void InitializeGL();

  // Cleanup GL after being initialized for image testing.
  static void CleanupGL();

  // Initialize buffer of a specific |format| to |color|.
  static void SetBufferDataToColor(int width,
                                   int height,
                                   int stride,
                                   gfx::BufferFormat format,
                                   const uint8_t color[4],
                                   uint8_t* data);
};

}  // namespace gl

#endif  // UI_GL_TEST_GL_IMAGE_TEST_SUPPORT_H_

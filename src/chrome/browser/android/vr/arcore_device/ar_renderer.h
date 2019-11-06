// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_AR_RENDERER_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_AR_RENDERER_H_

#include "base/macros.h"
#include "ui/gl/gl_bindings.h"

namespace device {

// Issues GL for rendering a texture for AR.
// TODO(crbug.com/838013): Share code with WebVrRenderer.
class ArRenderer {
 public:
  ArRenderer();
  ~ArRenderer();

  void Draw(int texture_handle,
            const float (&uv_transform)[16],
            float xborder,
            float yborder);

 private:
  GLuint program_handle_ = 0;
  GLuint position_handle_ = 0;
  GLuint clip_rect_handle_ = 0;
  GLuint texture_handle_ = 0;
  GLuint uv_transform_ = 0;
  GLuint x_border_handle_ = 0;
  GLuint y_border_handle_ = 0;
  GLuint vertex_buffer_ = 0;
  GLuint index_buffer_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ArRenderer);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_AR_RENDERER_H_

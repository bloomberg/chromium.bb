// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_YUV420_RGB_CONVERTER_H_
#define UI_GL_YUV420_RGB_CONVERTER_H_

#include "ui/gfx/geometry/size.h"

namespace gl {

class YUVToRGBConverter {
 public:
  YUVToRGBConverter();
  ~YUVToRGBConverter();
  void CopyYUV420ToRGB(unsigned target,
                       unsigned y_texture,
                       unsigned uv_texture,
                       const gfx::Size& size,
                       unsigned rgb_texture);

 private:
  unsigned framebuffer_ = 0;
  unsigned vertex_shader_ = 0;
  unsigned fragment_shader_ = 0;
  unsigned program_ = 0;
  int size_location_ = -1;
  unsigned vertex_buffer_ = 0;
};

}  // namespace gl

#endif  // UI_GL_YUV420_RGB_CONVERTER_H_

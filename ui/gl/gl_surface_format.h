// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_FORMAT_H_
#define UI_GL_GL_SURFACE_FORMAT_H_

#include "ui/gl/gl_export.h"

namespace gl {

class GL_EXPORT GLSurfaceFormat {
 public:

  // For use with constructor.
  enum SurfacePixelLayout {
    PIXEL_LAYOUT_DONT_CARE = -1,
    PIXEL_LAYOUT_BGRA,
    PIXEL_LAYOUT_RGBA,
  };

  GLSurfaceFormat();

  ~GLSurfaceFormat();

  GLSurfaceFormat(SurfacePixelLayout layout);

  GLSurfaceFormat(const GLSurfaceFormat& other);

  bool IsDefault();

  void SetIsSurfaceless();
  bool IsSurfaceless();

  bool IsCompatible(GLSurfaceFormat other_format);

  // Default pixel format is RGBA8888. Use this method to select
  // a preference of RGB565. TODO(klausw): use individual setter
  // methods if there's a use case for them.
  void SetRGB565();

  // Other properties for future use.
  void SetDepthBits(int depth_bits);
  int GetDepthBits();

  void SetStencilBits(int stencil_bits);
  int GetStencilBits();

  void SetSamples(int samples);
  int GetSamples();

  void SetDefaultPixelLayout(SurfacePixelLayout layout);

  SurfacePixelLayout GetPixelLayout();

  int GetBufferSize();

 private:
  bool is_default_ = true;
  SurfacePixelLayout pixel_layout_ = PIXEL_LAYOUT_DONT_CARE;
  bool is_surfaceless_ = false;
  int red_bits_ = -1;
  int green_bits_ = -1;
  int blue_bits_ = -1;
  int alpha_bits_ = -1;
  int depth_bits_ = -1;
  int samples_ = -1;
  int stencil_bits_ = -1;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_FORMAT_H_

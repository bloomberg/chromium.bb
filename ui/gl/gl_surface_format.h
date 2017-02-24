// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_FORMAT_H_
#define UI_GL_GL_SURFACE_FORMAT_H_

#include "ui/gl/gl_export.h"

namespace gl {

// Expresses surface format properties that may vary depending
// on the underlying gl_surface implementation or specific usage
// scenarios. Intended usage is to support copying formats and
// checking compatibility.
class GL_EXPORT GLSurfaceFormat {
 public:

  // For use with constructor.
  enum SurfacePixelLayout {
    PIXEL_LAYOUT_DONT_CARE = -1,
    PIXEL_LAYOUT_BGRA,
    PIXEL_LAYOUT_RGBA,
  };

  // Default surface format for the underlying gl_surface subtype.
  // Use the setters below to change attributes if needed.
  GLSurfaceFormat();

  // Use a specified pixel layout, cf. gl_surface_osmesa.
  GLSurfaceFormat(SurfacePixelLayout layout);

  // Copy constructor from pre-existing format.
  GLSurfaceFormat(const GLSurfaceFormat& other);

  ~GLSurfaceFormat();

  // Helper method to determine if the format is unchanged from the
  // default at creation time. TODO(klausw): can this be removed?
  bool IsDefault();

  // Surfaceless appears as a format property for backwards
  // compatibility with the previous enum-based implementation.
  // TODO(klausw): consider removing it and/or merging it into the
  // pre-existing IsSurfaceless() predicate for the various Surface
  // subclasses?
  void SetIsSurfaceless();
  bool IsSurfaceless();

  // A given pair of surfaces is considered compatible if glSetSurface
  // can be used to switch between them without generating BAD_MATCH
  // errors, visual errors, or gross inefficiency, and incompatible
  // otherwise. For example, a pixel layout mismatch would be
  // considered incompatible. This comparison only makes sense within
  // the context of a single gl_surface subtype.
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

  // Compute number of bits needed for storing one pixel, not
  // including any padding. At this point mainly used to distinguish
  // RGB565 (16) from RGBA8888 (32).
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

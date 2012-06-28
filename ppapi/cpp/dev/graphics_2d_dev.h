// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_GRAPHICS_2D_DEV_H_
#define PPAPI_CPP_DEV_GRAPHICS_2D_DEV_H_

#include "ppapi/cpp/graphics_2d.h"

namespace pp {

// Graphics2DDev is a version of Graphics2D that exposes under-development  APIs
// for HiDPI
class Graphics2DDev : public Graphics2D {
 public:
  /// Default constructor for creating an is_null()
  /// <code>Graphics2DDev</code> object.
  Graphics2DDev() : Graphics2D() {}

  // Constructor for creating a <code>Graphics2DDev</code> object from an
  // existing <code>Graphics2D</code> object.
  Graphics2DDev(const Graphics2D& other) : Graphics2D(other) {}

  virtual ~Graphics2DDev() {}

  /// SetScale() sets the scale factor that will be applied when painting the
  /// graphics context onto the output device. Typically, if rendering at device
  /// resolution is desired, the context would be created with the width and
  /// height scaled up by the view's GetDeviceScale and SetScale called with a
  /// scale of 1.0 / GetDeviceScale(). For example, if the view resource passed
  /// to DidChangeView has a rectangle of (w=200, h=100) and a device scale of
  /// 2.0, one would call Create with a size of (w=400, h=200) and then call
  /// SetScale with 0.5. One would then treat each pixel in the context as a
  /// single device pixel.
  ///
  /// @param[in] scale The scale to apply when painting.
  ///
  /// @return Returns <code>true</code> on success or <code>false</code>
  /// if the resource is invalid or the scale factor is 0 or less.
  bool SetScale(float scale);

  /// GetScale() gets the scale factor that will be applied when painting the
  /// graphics context onto the output device.
  ///
  /// @return Returns the scale factor for the graphics context. If the resource
  /// is invalid, 0.0 will be returned.
  float GetScale();
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_GRAPHICS_2D_DEV_H_

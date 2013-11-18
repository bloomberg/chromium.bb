// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_GRAPHICS_2D_DEV_H_
#define PPAPI_CPP_DEV_GRAPHICS_2D_DEV_H_

#include "ppapi/cpp/graphics_2d.h"

#include "ppapi/c/dev/ppb_graphics_2d_dev.h"

namespace pp {

class Point;

// Graphics2DDev is a version of Graphics2D that exposes under-development  APIs
// for HiDPI
class Graphics2D_Dev : public Graphics2D {
 public:
  /// Default constructor for creating an is_null()
  /// <code>Graphics2D_Dev</code> object.
  Graphics2D_Dev() : Graphics2D() {}

  // Constructor for creating a <code>Graphics2DDev</code> object from an
  // existing <code>Graphics2D</code> object.
  Graphics2D_Dev(const Graphics2D& other) : Graphics2D(other) {}

  virtual ~Graphics2D_Dev() {}

  /// Returns true if SetScale and GetScale are supported. False if not.
  static bool SupportsScale();

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

  /// Set the offset into the plugin element at which the graphics context is
  /// painted. This allows a portion of the plugin element to be painted to.
  /// The new offset will only be applied after Flush() has been called.
  ///
  /// @param[in] resource A <code>Graphics2D</code> context resource.
  /// @param[in] offset The offset at which the context should be painted.
  void SetOffset(const pp::Point& offset);

  /// Sets the resize mode for the graphics context. When a plugin element is
  /// resized in the DOM, it takes time for the plugin to update the graphics
  /// context in the renderer. These options affect how the existing context is
  /// displayed until the backing store is updated by the plugin.
  ///
  ///@param[in] resource A <code>Graphics2D</code> context resource.
  ///@param[in] resize_mode The resize mode to change this context to.
  void SetResizeMode(PP_Graphics2D_Dev_ResizeMode resize_mode);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_GRAPHICS_2D_DEV_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DISPLAY_H_
#define UI_GFX_DISPLAY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/rect.h"

namespace gfx {

// Note: The screen and display currently uses pixel coordinate
// system. For platforms that support DIP (density independent pixel),
// |bounds()| and |work_area| will return values in DIP coordinate
// system, not in backing pixels.
class UI_EXPORT Display {
 public:
  // Creates a display with kInvalidDisplayID as default.
  Display();
  explicit Display(int64 id);
  Display(int64 id, const Rect& bounds);
  ~Display();

  // Returns the forced device scale factor, which is given by
  // "--force-device-scale-factor".
  static float GetForcedDeviceScaleFactor();

  // Returns 64-bit persistent ID for the specified manufacturer's ID and
  // serial#.
  static int64 GetID(uint16 manufacturer_id, uint32 serial_number);

  // Sets/Gets unique identifier associated with the display.
  // -1 means invalid display and it doesn't not exit.
  int64 id() const { return id_; }
  void set_id(int64 id) { id_ = id; }

  // Gets/Sets the display's bounds in gfx::Screen's coordinates.
  const Rect& bounds() const { return bounds_; }
  void set_bounds(const Rect& bounds) { bounds_ = bounds; }

  // Gets/Sets the display's work area in gfx::Screen's coordinates.
  const Rect& work_area() const { return work_area_; }
  void set_work_area(const Rect& work_area) { work_area_ = work_area; }

  // Output device's pixel scale factor. This specifies how much the
  // UI should be scaled when the actual output has more pixels than
  // standard displays (which is around 100~120dpi.) The potential return
  // values depend on each platforms.
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  // Utility functions that just return the size of display and
  // work area.
  const Size& size() const { return bounds_.size(); }
  const Size& work_area_size() const { return work_area_.size(); }

  // Returns the work area insets.
  Insets GetWorkAreaInsets() const;

  // Sets the device scale factor and display bounds in pixel. This
  // updates the work are using the same insets between old bounds and
  // work area.
  void SetScaleAndBounds(float device_scale_factor,
                         const gfx::Rect& bounds_in_pixel);

  // Sets the display's size. This updates the work area using the same insets
  // between old bounds and work area.
  void SetSize(const gfx::Size& size_in_pixel);

  // Computes and updates the display's work are using
  // |work_area_insets| and the bounds.
  void UpdateWorkAreaFromInsets(const gfx::Insets& work_area_insets);

  // Returns the display's size in pixel coordinates.
  gfx::Size GetSizeInPixel() const;

#if defined(USE_AURA)
  // TODO(oshima|skuhne): Eliminate the use of bounds_in_pixel in events_x.cc
  // and remove bounds_in_pixel from gfx::Display.
  // Returns the display's bounds in pixel coordinates.
  const Rect& bounds_in_pixel() const { return bounds_in_pixel_; }
#endif

  // Returns a string representation of the display;
  std::string ToString() const;

  // True if the display contains valid display id.
  bool is_valid() const { return id_ != kInvalidDisplayID; }

  static const int64 kInvalidDisplayID;

 private:
  int64 id_;
  Rect bounds_;
  Rect work_area_;
#if defined(USE_AURA)
  Rect bounds_in_pixel_;
#endif
  float device_scale_factor_;
};

}  // namespace gfx

#endif  // UI_GFX_DISPLAY_H_

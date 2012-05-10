// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MONITOR_H_
#define UI_GFX_MONITOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/rect.h"

namespace gfx {

// Note: The screen and monitor currently uses pixel coordinate
// system. For platforms that support DIP (density independent pixel),
// |bounds()| and |work_area| will return values in DIP coordinate
// system, not in backing pixels.
class UI_EXPORT Monitor {
 public:
  // Returns the default value for the device scale factor, which is
  // given by "--default-device-scale-factor".
  static float GetDefaultDeviceScaleFactor();

  // Creates a monitor with invalid id(-1) as default.
  Monitor();
  explicit Monitor(int id);
  Monitor(int id, const Rect& bounds);
  ~Monitor();

  // Sets/Gets unique identifier associated with the monitor.
  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  // Gets/Sets the monitor's bounds in gfx::Screen's coordinates.
  // -1 means invalid monitor and it doesn't not exit.
  const Rect& bounds() const { return bounds_; }
  void set_bounds(const Rect& bounds) { bounds_ = bounds; }

  // Gets/Sets the monitor's work area in gfx::Screen's coordinates.
  const Rect& work_area() const { return work_area_; }
  void set_work_area(const Rect& work_area) { work_area_ = work_area; }

  // Output device's pixel scale factor. This specifies how much the
  // UI should be scaled when the actual output has more pixels than
  // standard monitors (which is around 100~120dpi.) The potential return
  // values depend on each platforms.
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  // Utility functions that just return the size of monitor and
  // work area.
  const Size& size() const { return bounds_.size(); }
  const Size& work_area_size() const { return work_area_.size(); }

  // Returns the work area insets.
  Insets GetWorkAreaInsets() const;

  // Sets the device scale factor and monitor bounds in pixel. This
  // updates the work are using the same insets between old bounds and
  // work area.
  void SetScaleAndBounds(float device_scale_factor,
                         const gfx::Rect& bounds_in_pixel);

  // Sets the monitor's size. This updates the work area using the same insets
  // between old bounds and work area.
  void SetSize(const gfx::Size& size_in_pixel);

  // Computes and updates the monitor's work are using
  // |work_area_insets| and the bounds.
  void UpdateWorkAreaFromInsets(const gfx::Insets& work_area_insets);

  // Returns the monitor's size in pixel coordinates.
  gfx::Size GetSizeInPixel() const;

#if defined(USE_AURA)
  // TODO(oshima): |bounds()| on ash is not screen's coordinate and
  // this is an workaround for this. This will be removed when ash
  // has true multi monitor support. crbug.com/119268.
  // Returns the monitor's bounds in pixel coordinates.
  const Rect& bounds_in_pixel() const { return bounds_in_pixel_; }
#endif

  // Returns a string representation of the monitor;
  std::string ToString() const;

 private:
  int id_;
  Rect bounds_;
  Rect work_area_;
#if defined(USE_AURA)
  Rect bounds_in_pixel_;
#endif
  float device_scale_factor_;
};

}  // namespace gfx

#endif  // UI_GFX_MONITOR_H_

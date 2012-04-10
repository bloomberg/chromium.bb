// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MONITOR_H_
#define UI_AURA_MONITOR_H_
#pragma once

#include "base/basictypes.h"
#include "ui/aura/aura_export.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"

namespace aura {

class AURA_EXPORT Monitor {
 public:
  Monitor();
  ~Monitor();

  // Sets/gets monitor's bounds in |gfx::screen|'s coordinates,
  // which is relative to the primary screen's origin.
  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds;}
  const gfx::Rect& bounds() const { return bounds_; };

  // Sets/gets monitor's size.
  void set_size(const gfx::Size& size) { bounds_.set_size(size); }
  const gfx::Size& size() const { return bounds_.size(); }

  // Sets/gets monitor's workarea insets.
  void set_work_area_insets(const gfx::Insets& insets) {
    work_area_insets_ = insets;
  }
  const gfx::Insets& work_area_insets() const { return work_area_insets_; }

  // Output device's pixel scale factor. This specifies how much the
  // UI should be scaled when the actual output has more pixels than
  // standard monitors (which is around 100~120dpi.) For aura, this
  // value is either 1.0 or 2.0, but may return different values on
  // other platforms.
  float GetDeviceScaleFactor() const {
    return device_scale_factor_;
  }

  // Returns the monitor's work area.
  gfx::Rect GetWorkAreaBounds() const;

 private:
  // Insets for the work area.
  gfx::Insets work_area_insets_;

  gfx::Rect bounds_;

  float device_scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(Monitor);
};

}  // namespace aura

#endif  // UI_AURA_MONITOR_H_

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
// system.  ENABLE_DIP macro (which is enabled with enable_dip=1 gyp
// flag) will make this inconsistent with views' coordinate system
// because views will use DIP coordinate system, which uses
// (1.0/device_scale_factor) scale of the pixel coordinate system.
// TODO(oshima): Change aura/screen to DIP coordinate system and
// update this comment.
class UI_EXPORT Monitor {
 public:
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

  // Sets the monitor bounds and updates the work are using the same insets
  // between old bounds and work area.
  void SetBoundsAndUpdateWorkArea(const gfx::Rect& bounds);

  // Sets the monitor size and updates the work are using the same insets
  // between old bounds and work area.
  void SetSizeAndUpdateWorkArea(const gfx::Size& size);

  // Computes and updates the monitor's work are using insets and the bounds.
  void UpdateWorkAreaWithInsets(const gfx::Insets& work_area_insets);

 private:
  int id_;
  Rect bounds_;
  Rect work_area_;
  float device_scale_factor_;
};

}  // namespace gfx

#endif  // UI_GFX_MONITOR_H_

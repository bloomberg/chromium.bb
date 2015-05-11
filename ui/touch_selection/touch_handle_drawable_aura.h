// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_HANDLE_DRAWABLE_AURA_H_
#define UI_TOUCH_SELECTION_TOUCH_HANDLE_DRAWABLE_AURA_H_

#include "ui/touch_selection/touch_handle.h"
#include "ui/touch_selection/touch_handle_orientation.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace aura {
class Window;
}

namespace aura_extra {
class ImageWindowDelegate;
}

namespace ui {

class UI_TOUCH_SELECTION_EXPORT TouchHandleDrawableAura
    : public TouchHandleDrawable {
 public:
  explicit TouchHandleDrawableAura(aura::Window* parent);
  ~TouchHandleDrawableAura() override;

 private:
  void UpdateBounds();

  bool IsVisible() const;

  // TouchHandleDrawable:
  void SetEnabled(bool enabled) override;
  void SetOrientation(TouchHandleOrientation orientation) override;
  void SetAlpha(float alpha) override;
  void SetFocus(const gfx::PointF& position) override;
  gfx::RectF GetVisibleBounds() const override;

  aura_extra::ImageWindowDelegate* window_delegate_;
  scoped_ptr<aura::Window> window_;
  bool enabled_;
  float alpha_;
  ui::TouchHandleOrientation orientation_;

  // Focal position of the handle set via SetFocus (normally located on the
  // intersection of the cursor line and the text base line), in coordinate
  // space of selection controller client (i.e. handle's parent).
  gfx::PointF focal_position_;

  // Window bounds relative to the focal position.
  gfx::RectF relative_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TouchHandleDrawableAura);
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_HANDLE_DRAWABLE_AURA_H_

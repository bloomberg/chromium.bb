// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_HANDLE_H_
#define UI_TOUCH_SELECTION_TOUCH_HANDLE_H_

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/touch_selection/touch_handle_orientation.h"
#include "ui/touch_selection/touch_selection_draggable.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace ui {

class TouchHandle;

// Interface through which |TouchHandle| delegates rendering-specific duties.
class UI_TOUCH_SELECTION_EXPORT TouchHandleDrawable {
 public:
  virtual ~TouchHandleDrawable() {}
  virtual void SetEnabled(bool enabled) = 0;
  virtual void SetOrientation(TouchHandleOrientation orientation) = 0;
  virtual void SetAlpha(float alpha) = 0;
  virtual void SetFocus(const gfx::PointF& position) = 0;
  virtual gfx::RectF GetVisibleBounds() const = 0;
};

// Interface through which |TouchHandle| communicates handle manipulation and
// requests concrete drawable instances.
class UI_TOUCH_SELECTION_EXPORT TouchHandleClient
    : public TouchSelectionDraggableClient {
 public:
  ~TouchHandleClient() override {}
  virtual void OnHandleTapped(const TouchHandle& handle) = 0;
  virtual void SetNeedsAnimate() = 0;
  virtual scoped_ptr<TouchHandleDrawable> CreateDrawable() = 0;
  virtual base::TimeDelta GetMaxTapDuration() const = 0;
};

// Responsible for displaying a selection or insertion handle for text
// interaction.
class UI_TOUCH_SELECTION_EXPORT TouchHandle : public TouchSelectionDraggable {
 public:
  // The drawable will be enabled but invisible until otherwise specified.
  TouchHandle(TouchHandleClient* client, TouchHandleOrientation orientation);
  ~TouchHandle() override;

  // TouchSelectionDraggable implementation.
  bool WillHandleTouchEvent(const MotionEvent& event) override;
  bool IsActive() const override;

  // Sets whether the handle is active, allowing resource cleanup if necessary.
  // If false, active animations or touch drag sequences will be cancelled.
  // While disabled, manipulation is *explicitly not supported*, and may lead to
  // undesirable and/or unstable side-effects. The handle can be safely
  // re-enabled to allow continued operation.
  void SetEnabled(bool enabled);

  enum AnimationStyle { ANIMATION_NONE, ANIMATION_SMOOTH };
  // Update the handle visibility, fading in/out according to |animation_style|.
  // If an animation is in-progress, it will be overriden appropriately.
  void SetVisible(bool visible, AnimationStyle animation_style);

  // Update the handle placement to |position|.
  // Note: If a fade out animation is active or the handle is invisible, the
  // handle position will not be updated until the handle regains visibility.
  void SetPosition(const gfx::PointF& position);

  // Update the handle visuals to |orientation|.
  // Note: If the handle is being dragged, the orientation change will be
  // deferred until the drag has ceased.
  void SetOrientation(TouchHandleOrientation orientation);

  // Ticks an active animation, as requested to the client by |SetNeedsAnimate|.
  // Returns true if an animation is active and requires further ticking.
  bool Animate(base::TimeTicks frame_time);

  // Get the visible bounds of the handle, based on the current position and
  // the drawable's size/orientation. If the handle is invisible or disabled,
  // the bounds will be empty.
  gfx::RectF GetVisibleBounds() const;

  const gfx::PointF& position() const { return position_; }
  TouchHandleOrientation orientation() const { return orientation_; }

 private:
  void BeginDrag();
  void EndDrag();
  void BeginFade();
  void EndFade();
  void SetAlpha(float alpha);

  scoped_ptr<TouchHandleDrawable> drawable_;

  TouchHandleClient* const client_;

  gfx::PointF position_;
  TouchHandleOrientation orientation_;
  TouchHandleOrientation deferred_orientation_;

  gfx::PointF touch_down_position_;
  gfx::Vector2dF touch_drag_offset_;
  base::TimeTicks touch_down_time_;

  // Note that when a fade animation is active, |is_visible_| and |position_|
  // may not reflect the actual visibility and position of the drawable. This
  // discrepancy is resolved either upon fade completion or cancellation.
  base::TimeTicks fade_end_time_;
  gfx::PointF fade_start_position_;
  float alpha_;
  bool animate_deferred_fade_;

  bool enabled_;
  bool is_visible_;
  bool is_dragging_;
  bool is_drag_within_tap_region_;

  DISALLOW_COPY_AND_ASSIGN(TouchHandle);
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_HANDLE_H_

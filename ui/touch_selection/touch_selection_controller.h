// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_H_

#include "ui/base/touch/selection_bound.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/touch_selection/selection_event_type.h"
#include "ui/touch_selection/touch_handle.h"
#include "ui/touch_selection/touch_handle_orientation.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace ui {
class MotionEvent;

// Interface through which |TouchSelectionController| issues selection-related
// commands, notifications and requests.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionControllerClient {
 public:
  virtual ~TouchSelectionControllerClient() {}

  virtual bool SupportsAnimation() const = 0;
  virtual void SetNeedsAnimate() = 0;
  virtual void MoveCaret(const gfx::PointF& position) = 0;
  virtual void MoveRangeSelectionExtent(const gfx::PointF& extent) = 0;
  virtual void SelectBetweenCoordinates(const gfx::PointF& base,
                                        const gfx::PointF& extent) = 0;
  virtual void OnSelectionEvent(SelectionEventType event,
                                const gfx::PointF& position) = 0;
  virtual scoped_ptr<TouchHandleDrawable> CreateDrawable() = 0;
};

// Controller for manipulating text selection via touch input.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionController
    : public TouchHandleClient {
 public:
  TouchSelectionController(TouchSelectionControllerClient* client,
                           base::TimeDelta tap_timeout,
                           float tap_slop,
                           bool show_on_tap_for_empty_editable);
  ~TouchSelectionController() override;

  // To be called when the selection bounds have changed.
  // Note that such updates will trigger handle updates only if preceded
  // by an appropriate call to allow automatic showing.
  void OnSelectionBoundsChanged(const SelectionBound& start,
                                const SelectionBound& end);

  // Allows touch-dragging of the handle.
  // Returns true iff the event was consumed, in which case the caller should
  // cease further handling of the event.
  bool WillHandleTouchEvent(const MotionEvent& event);

  // To be called before forwarding a tap event. This allows automatically
  // showing the insertion handle from subsequent bounds changes.
  void OnTapEvent();

  // To be called before forwarding a longpress event. This allows automatically
  // showing the selection or insertion handles from subsequent bounds changes.
  void OnLongPressEvent();

  // Allow showing the selection handles from the most recent selection bounds
  // update (if valid), or a future valid bounds update.
  void AllowShowingFromCurrentSelection();

  // Hide the handles and suppress bounds updates until the next explicit
  // showing allowance.
  void HideAndDisallowShowingAutomatically();

  // Override the handle visibility according to |hidden|.
  void SetTemporarilyHidden(bool hidden);

  // To be called when the editability of the focused region changes.
  void OnSelectionEditable(bool editable);

  // To be called when the contents of the focused region changes.
  void OnSelectionEmpty(bool empty);

  // Ticks an active animation, as requested to the client by |SetNeedsAnimate|.
  // Returns true if an animation is active and requires further ticking.
  bool Animate(base::TimeTicks animate_time);

 private:
  enum InputEventType { TAP, LONG_PRESS, INPUT_EVENT_TYPE_NONE };

  // TouchHandleClient implementation.
  void OnHandleDragBegin(const TouchHandle& handle) override;
  void OnHandleDragUpdate(const TouchHandle& handle,
                          const gfx::PointF& new_position) override;
  void OnHandleDragEnd(const TouchHandle& handle) override;
  void OnHandleTapped(const TouchHandle& handle) override;
  void SetNeedsAnimate() override;
  scoped_ptr<TouchHandleDrawable> CreateDrawable() override;
  base::TimeDelta GetTapTimeout() const override;
  float GetTapSlop() const override;

  void ShowInsertionHandleAutomatically();
  void ShowSelectionHandlesAutomatically();

  void OnInsertionChanged();
  void OnSelectionChanged();

  void ActivateInsertion();
  void DeactivateInsertion();
  void ActivateSelection();
  void DeactivateSelection();
  void ResetCachedValuesIfInactive();

  const gfx::PointF& GetStartPosition() const;
  const gfx::PointF& GetEndPosition() const;
  gfx::Vector2dF GetStartLineOffset() const;
  gfx::Vector2dF GetEndLineOffset() const;
  bool GetStartVisible() const;
  bool GetEndVisible() const;
  TouchHandle::AnimationStyle GetAnimationStyle(bool was_active) const;

  void LogSelectionEnd();

  TouchSelectionControllerClient* const client_;
  const base::TimeDelta tap_timeout_;
  const float tap_slop_;

  // Controls whether an insertion handle is shown on a tap for an empty
  // editable text.
  bool show_on_tap_for_empty_editable_;

  InputEventType response_pending_input_event_;

  SelectionBound start_;
  SelectionBound end_;
  TouchHandleOrientation start_orientation_;
  TouchHandleOrientation end_orientation_;

  scoped_ptr<TouchHandle> insertion_handle_;
  bool is_insertion_active_;
  bool activate_insertion_automatically_;

  scoped_ptr<TouchHandle> start_selection_handle_;
  scoped_ptr<TouchHandle> end_selection_handle_;
  bool is_selection_active_;
  bool activate_selection_automatically_;

  bool selection_empty_;
  bool selection_editable_;

  bool temporarily_hidden_;

  base::TimeTicks selection_start_time_;
  // Whether a selection handle was dragged during the current 'selection
  // session' - i.e. since the current selection has been activated.
  bool selection_handle_dragged_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionController);
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_H_

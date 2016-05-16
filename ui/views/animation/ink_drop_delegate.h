// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_DELEGATE_H_
#define UI_VIEWS_ANIMATION_INK_DROP_DELEGATE_H_

#include "base/macros.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace ui {
class GestureEvent;
class Event;
}  // namespace ui

namespace views {

// Ink ripple animation delegate that starts and stops animations based on
// View states and events.
class VIEWS_EXPORT InkDropDelegate {
 public:
  InkDropDelegate() {}
  virtual ~InkDropDelegate() {}

  // Called when ink ripple state changes.
  // TODO(bruthig): Replace the InkDropState parameter with an InkDropAction
  // enum.  The InkDropAction enum should be a subset of the InkDropState values
  // as well as a NONE value.
  virtual void OnAction(InkDropState state) = 0;

  // Immediately snaps the InkDropState to ACTIVATED.
  virtual void SnapToActivated() = 0;

  // Enables or disables the hover state.
  virtual void SetHovered(bool is_hovered) = 0;

  // Returns the current InkDropState
  virtual InkDropState GetTargetInkDropState() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_DELEGATE_H_

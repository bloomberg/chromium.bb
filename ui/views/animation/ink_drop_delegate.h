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

  // Sets sizes for the animation layers that are squares with |large_size| and
  // |small_size| being the length of each side. When painting rounded squares
  // |large_corner_radius| and |small_corner_radius| are specifying the
  // corner radius.
  virtual void SetInkDropSize(int large_size,
                              int large_corner_radius,
                              int small_size,
                              int small_corner_radius) = 0;

  // Called when the bounds or layout of the View changes necessitating change
  // in positioning of ink ripple layers.
  virtual void OnLayout() = 0;

  // Called when ink ripple state changes.
  // TODO(bruthig): Replace the InkDropState parameter with an InkDropAction
  // enum.  The InkDropAction enum should be a subset of the InkDropState values
  // as well as a NONE value.
  virtual void OnAction(InkDropState state) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_DELEGATE_H_

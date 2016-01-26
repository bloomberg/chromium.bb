// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}

namespace views {

// Pure virtual base class that manages the lifetime and state of an ink drop
// animation as well as visual hover state feedback.
class VIEWS_EXPORT InkDropAnimationController {
 public:
  virtual ~InkDropAnimationController() {}

  // Gets the current state of the ink drop.
  virtual InkDropState GetInkDropState() const = 0;

  // Animates from the current InkDropState to |ink_drop_state|.
  virtual void AnimateToState(InkDropState ink_drop_state) = 0;

  // Enables or disables the hover state.
  virtual void SetHovered(bool is_hovered) = 0;

  // Returns true if the hover state is enabled.
  virtual bool IsHovered() const = 0;

  virtual gfx::Size GetInkDropLargeSize() const = 0;

  // Sets the different sizes of the ink drop.
  virtual void SetInkDropSize(const gfx::Size& large_size,
                              int large_corner_radius,
                              const gfx::Size& small_size,
                              int small_corner_radius) = 0;

  // Sets the |center_point| of the ink drop relative to its parent Layer.
  virtual void SetInkDropCenter(const gfx::Point& center_point) = 0;

 protected:
  InkDropAnimationController() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationController);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_H_

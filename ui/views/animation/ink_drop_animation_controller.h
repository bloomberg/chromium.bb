// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_H_

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
class AppearAnimationObserver;
class InkDropDelegate;
class InkDropHost;
class View;

// Base class for an ink drop animation which is hosted by an InkDropHost.
class VIEWS_EXPORT InkDropAnimationController {
 public:
  InkDropAnimationController() {}
  virtual ~InkDropAnimationController() {}

  // Animates from the current InkDropState to |state|.
  virtual void AnimateToState(InkDropState state) = 0;

  // Sets the size of the ink drop.
  virtual void SetInkDropSize(const gfx::Size& size) = 0;

  // Returns the ink drop bounds.
  virtual gfx::Rect GetInkDropBounds() const = 0;

  // Sets the bounds for the ink drop. |bounds| are in the coordinate space of
  // the parent ui::Layer that the ink drop layer is added to.
  virtual void SetInkDropBounds(const gfx::Rect& bounds) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationController);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_IMPL_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_animation_controller.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}

namespace views {
class AppearAnimationObserver;
class InkDropDelegate;
class InkDropHost;
class View;

// Controls an ink drop animation which is hosted by an InkDropHost.
class VIEWS_EXPORT InkDropAnimationControllerImpl
    : public InkDropAnimationController {
 public:
  // Constructs an ink drop controller that will attach the ink drop to the
  // given |ink_drop_host|.
  explicit InkDropAnimationControllerImpl(InkDropHost* ink_drop_host);
  ~InkDropAnimationControllerImpl() override;

  // InkDropAnimationController:
  void AnimateToState(InkDropState state) override;
  void SetInkDropSize(const gfx::Size& size) override;
  gfx::Rect GetInkDropBounds() const override;
  void SetInkDropBounds(const gfx::Rect& bounds) override;

 private:
  // Starts the animation of a touch event.
  void AnimateTapDown();

  // Schedules the hide animation of |ink_drop_layer_| for once its current
  // animation has completed. If |ink_drop_layer_| is not animating, the hide
  // animation begins immediately.
  void AnimateHide();

  // Starts the animation of a long press, and cancels hiding |ink_drop_layer_|
  // until the long press has completed.
  void AnimateLongPress();

  // Starts the showing animation on |layer|, with a |duration| in milliseconds.
  void AnimateShow(ui::Layer* layer,
                   AppearAnimationObserver* observer,
                   base::TimeDelta duration);

  // Sets the bounds for |layer|.
  void SetLayerBounds(ui::Layer* layer);

  // Initializes |layer|'s properties.
  void SetupAnimationLayer(ui::Layer* layer, InkDropDelegate* delegate);

  InkDropHost* ink_drop_host_;

  // The root layer that parents the animating layers.
  scoped_ptr<ui::Layer> root_layer_;

  // The layer used for animating a user touch.
  scoped_ptr<ui::Layer> ink_drop_layer_;

  // ui::LayerDelegate responsible for painting to |ink_drop_layer_|.
  scoped_ptr<InkDropDelegate> ink_drop_delegate_;

  // ui::ImplicitAnimationObserver which observes |ink_drop_layer_| and can be
  // used to automatically trigger a hide animation upon completion.
  scoped_ptr<AppearAnimationObserver> appear_animation_observer_;

  // The layer used for animating a long press.
  scoped_ptr<ui::Layer> long_press_layer_;

  // ui::LayerDelegate responsible for painting to |long_press_layer_|.
  scoped_ptr<InkDropDelegate> long_press_delegate_;

  // ui::ImplicitAnimationObserver which observers |long_press_layer_| and can
  // be used to automatically trigger a hide animation upon completion.
  scoped_ptr<AppearAnimationObserver> long_press_animation_observer_;

  // The bounds of the ink drop layers. Defined in the coordinate space of the
  // parent ui::Layer that the ink drop layers were added to.
  gfx::Rect ink_drop_bounds_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationControllerImpl);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_IMPL_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/events/event_handler.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}

namespace views {
class AppearAnimationObserver;
class InkDropDelegate;
class View;

// Controls an animation which can be associated to a ui::Layer backed View.
// It sets itself as the pre-target handler for the supplied view, providing
// animations for user interactions. The events will not be consumed.
//
// The supplied views::View will be forced to have a layer, and this controller
// will add child layers to that.
class VIEWS_EXPORT InkDropAnimationController : public ui::EventHandler {
 public:
  explicit InkDropAnimationController(View* view);
  ~InkDropAnimationController() override;

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
                   bool circle,
                   base::TimeDelta duration);

  // Sets the bounds for |layer|.
  void SetLayerBounds(ui::Layer* layer, bool circle, int width, int height);

  // Initializes |layer| and attaches it to |parent|.
  void SetupAnimationLayer(ui::Layer* parent,
                           ui::Layer* layer,
                           InkDropDelegate* delegate);

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // The layer for animating a user touch. Added as a child to |view_|'s layer.
  // It will be stacked underneath.
  scoped_ptr<ui::Layer> ink_drop_layer_;

  // ui::LayerDelegate responsible for painting to |ink_drop_layer_|.
  scoped_ptr<InkDropDelegate> ink_drop_delegate_;

  // ui::ImplicitAnimationObserver which observes |ink_drop_layer_| and can be
  // used to automatically trigger a hide animation upon completion.
  scoped_ptr<AppearAnimationObserver> appear_animation_observer_;

  // The layer for animating a long press. Added as a child to |view_|'s layer.
  // It will be stacked underneath.
  scoped_ptr<ui::Layer> long_press_layer_;

  // ui::LayerDelegate responsible for painting to |long_press_layer_|.
  scoped_ptr<InkDropDelegate> long_press_delegate_;

  // ui::ImplicitAnimationObserver which observers |long_press_layer_| and can
  // be used to automatically trigger a hide animation upon completion.
  scoped_ptr<AppearAnimationObserver> long_press_animation_observer_;

  // The View for which InkDropAnimationController is a PreTargetHandler.
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationController);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_CONTROLLER_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_SQUARE_INK_DROP_ANIMATION_H_
#define UI_VIEWS_ANIMATION_SQUARE_INK_DROP_ANIMATION_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"
#include "ui/views/animation/ink_drop_animation.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace ui {
class CallbackLayerAnimationObserver;
class Layer;
class LayerAnimationObserver;
}  // namespace ui

namespace views {
class CircleLayerDelegate;
class RectangleLayerDelegate;

namespace test {
class SquareInkDropAnimationTestApi;
}  // namespace test

// An ink drop animation that smoothly animates between a circle and a rounded
// rectangle of different sizes for each of the different InkDropStates. The
// final frame for each InkDropState will be bounded by either a |large_size_|
// rectangle or a |small_size_| rectangle.
//
// The valid InkDropState transitions are defined below:
//
//   {All InkDropStates}      => HIDDEN
//   HIDDEN                   => ACTION_PENDING
//   HIDDEN, ACTION_PENDING   => QUICK_ACTION
//   ACTION_PENDING           => SLOW_ACTION_PENDING
//   SLOW_ACTION_PENDING      => SLOW_ACTION
//   {All InkDropStates}      => ACTIVATED
//   {All InkDropStates}      => DEACTIVATED
//
class VIEWS_EXPORT SquareInkDropAnimation : public InkDropAnimation {
 public:
  SquareInkDropAnimation(const gfx::Size& large_size,
                         int large_corner_radius,
                         const gfx::Size& small_size,
                         int small_corner_radius);
  ~SquareInkDropAnimation() override;

  // InkDropAnimation:
  ui::Layer* GetRootLayer() override;
  InkDropState GetTargetInkDropState() const override;
  bool IsVisible() const override;
  void AnimateToState(InkDropState ink_drop_state) override;
  void SetCenterPoint(const gfx::Point& center_point) override;
  void HideImmediately() override;

 private:
  friend class test::SquareInkDropAnimationTestApi;

  // Enumeration of the different shapes that compose the ink drop.
  enum PaintedShape {
    TOP_LEFT_CIRCLE = 0,
    TOP_RIGHT_CIRCLE,
    BOTTOM_RIGHT_CIRCLE,
    BOTTOM_LEFT_CIRCLE,
    HORIZONTAL_RECT,
    VERTICAL_RECT,
    // The total number of shapes, not an actual shape.
    PAINTED_SHAPE_COUNT
  };

  // Returns a human readable string for the |painted_shape| value.
  static std::string ToLayerName(PaintedShape painted_shape);

  // Type that contains a gfx::Tansform for each of the layers required by the
  // ink drop.
  typedef gfx::Transform InkDropTransforms[PAINTED_SHAPE_COUNT];

  float GetCurrentOpacity() const;

  // Animates the ripple from the |old_ink_drop_state| to the
  // |new_ink_drop_state|. |observer| is added to all LayerAnimationSequence's
  // used if not null.
  void AnimateStateChange(InkDropState old_ink_drop_state,
                          InkDropState new_ink_drop_state,
                          ui::LayerAnimationObserver* observer);

  // Animates all of the painted shape layers to the specified |transforms|. The
  // animation will be configured with the given |duration|, |tween|, and
  // |preemption_strategy| values. The |observer| will be added to all
  // LayerAnimationSequences if not null.
  void AnimateToTransforms(
      const InkDropTransforms transforms,
      base::TimeDelta duration,
      ui::LayerAnimator::PreemptionStrategy preemption_strategy,
      gfx::Tween::Type tween,
      ui::LayerAnimationObserver* observer);

  // Updates the transforms, opacity, and visibility to a HIDDEN state.
  void SetStateToHidden();

  // Sets the |transforms| on all of the shape layers. Note that this does not
  // perform any animation.
  void SetTransforms(const InkDropTransforms transforms);

  // Sets the opacity of the ink drop. Note that this does not perform any
  // animation.
  void SetOpacity(float opacity);

  // Animates all of the painted shape layers to the specified |opacity|. The
  // animation will be configured with the given |duration|, |tween|, and
  // |preemption_strategy| values. The |observer| will be added to all
  // LayerAnimationSequences if not null.
  void AnimateToOpacity(
      float opacity,
      base::TimeDelta duration,
      ui::LayerAnimator::PreemptionStrategy preemption_strategy,
      gfx::Tween::Type tween,
      ui::LayerAnimationObserver* observer);

  // Updates all of the Transforms in |transforms_out| for a circle of the given
  // |size|.
  void CalculateCircleTransforms(const gfx::Size& size,
                                 InkDropTransforms* transforms_out) const;

  // Updates all of the Transforms in |transforms_out| for a rounded rectangle
  // of the given |size| and |corner_radius|.
  void CalculateRectTransforms(const gfx::Size& size,
                               float corner_radius,
                               InkDropTransforms* transforms_out) const;

  // Updates all of the Transforms in |transforms_out| to the current Transforms
  // of the painted shape Layers.
  void GetCurrentTransforms(InkDropTransforms* transforms_out) const;

  // Adds and configures a new |painted_shape| layer to |painted_layers_|.
  void AddPaintLayer(PaintedShape painted_shape);

  void AbortAllAnimations();

  // The Callback invoked when all of the animation sequences for the specific
  // |ink_drop_state| animation have started. |observer| is the
  // ui::CallbackLayerAnimationObserver which is notifying the callback.
  void AnimationStartedCallback(
      InkDropState ink_drop_state,
      const ui::CallbackLayerAnimationObserver& observer);

  // The Callback invoked when all of the animation sequences for the specific
  // |ink_drop_state| animation have finished. |observer| is the
  // ui::CallbackLayerAnimationObserver which is notifying the callback.
  bool AnimationEndedCallback(
      InkDropState ink_drop_state,
      const ui::CallbackLayerAnimationObserver& observer);

  // Maximum size that an ink drop will be drawn to for any InkDropState whose
  // final frame should be large.
  gfx::Size large_size_;

  // Corner radius used to draw the rounded rectangles corner for any
  // InkDropState whose final frame should be large.
  int large_corner_radius_;

  // Maximum size that an ink drop will be drawn to for any InkDropState whose
  // final frame should be small.
  gfx::Size small_size_;

  // Corner radius used to draw the rounded rectangles corner for any
  // InkDropState whose final frame should be small.
  int small_corner_radius_;

  // ui::LayerDelegate to paint circles for all the circle layers.
  scoped_ptr<CircleLayerDelegate> circle_layer_delegate_;

  // ui::LayerDelegate to paint rectangles for all the rectangle layers.
  scoped_ptr<RectangleLayerDelegate> rect_layer_delegate_;

  // The root layer that parents the animating layers. The root layer is used to
  // manipulate opacity and location, and its children are used to manipulate
  // the different painted shapes that compose the ink drop.
  ui::Layer root_layer_;

  // ui::Layers for all of the painted shape layers that compose the ink drop.
  scoped_ptr<ui::Layer> painted_layers_[PAINTED_SHAPE_COUNT];

  // The current ink drop state.
  InkDropState ink_drop_state_;

  DISALLOW_COPY_AND_ASSIGN(SquareInkDropAnimation);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_SQUARE_INK_DROP_ANIMATION_H_

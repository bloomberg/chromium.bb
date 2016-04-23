// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HOVER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HOVER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
class CallbackLayerAnimationObserver;
}  // namespace ui

namespace views {
class RoundedRectangleLayerDelegate;

// Manages fade in/out animations for a painted Layer that is used to provide
// visual feedback on ui::Views for mouse hover states.
class VIEWS_EXPORT InkDropHover {
 public:
  InkDropHover(const gfx::Size& size,
               int corner_radius,
               const gfx::Point& center_point,
               SkColor color);
  ~InkDropHover();

  void set_explode_size(const gfx::Size& size) { explode_size_ = size; }

  // Returns true if the hover animation is either in the process of fading
  // in or is fully visible.
  bool IsFadingInOrVisible() const;

  // Fades in the hover visual over the given |duration|.
  void FadeIn(const base::TimeDelta& duration);

  // Fades out the hover visual over the given |duration|. If |explode| is true
  // then the hover will animate a size increase in addition to the fade out.
  void FadeOut(const base::TimeDelta& duration, bool explode);

  // The root Layer that can be added in to a Layer tree.
  ui::Layer* layer() { return layer_.get(); }

 private:
  enum HoverAnimationType { FADE_IN, FADE_OUT };

  // Animates a fade in/out as specified by |animation_type| combined with a
  // transformation from the |initial_size| to the |target_size| over the given
  // |duration|.
  void AnimateFade(HoverAnimationType animation_type,
                   const base::TimeDelta& duration,
                   const gfx::Size& initial_size,
                   const gfx::Size& target_size);

  // Calculates the Transform to apply to |layer_| for the given |size|.
  gfx::Transform CalculateTransform(const gfx::Size& size) const;

  // The callback that will be invoked when a fade in/out animation is complete.
  bool AnimationEndedCallback(
      HoverAnimationType animation_type,
      const ui::CallbackLayerAnimationObserver& observer);

  // The size of the hover shape when fully faded in.
  gfx::Size size_;

  // The target size of the hover shape when it expands during a fade out
  // animation.
  gfx::Size explode_size_;

  // The center point of the hover shape in the parent Layer's coordinate space.
  gfx::PointF center_point_;

  // True if the last animation to be initiated was a FADE_IN, and false
  // otherwise.
  bool last_animation_initiated_was_fade_in_;

  // The LayerDelegate that paints the hover |layer_|.
  std::unique_ptr<RoundedRectangleLayerDelegate> layer_delegate_;

  // The visual hover layer that is painted by |layer_delegate_|.
  std::unique_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHover);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HOVER_H_

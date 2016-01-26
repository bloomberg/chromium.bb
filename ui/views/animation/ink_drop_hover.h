// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HOVER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HOVER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
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
  InkDropHover(const gfx::Size& size, int corner_radius);
  ~InkDropHover();

  // Returns true if the hover layer is visible.
  bool IsVisible() const;

  // Fades in the hover visual over the given |duration|.
  void FadeIn(const base::TimeDelta& duration);

  // Fades out the hover visual over the given |duration|.
  void FadeOut(const base::TimeDelta& duration);

  // The root Layer that can be added in to a Layer tree.
  ui::Layer* layer() { return layer_.get(); }

  // Sets the |center_point| of the hover layer relative to its parent Layer.
  void SetCenterPoint(const gfx::Point& center_point);

 private:
  enum HoverAnimationType { FADE_IN, FADE_OUT };

  // Animates a fade in/out as specified by |animation_type| over the given
  // |duration|.
  void AnimateFade(HoverAnimationType animation_type,
                   const base::TimeDelta& duration);

  // The callback that will be invoked when a fade in/out animation is complete.
  bool AnimationEndedCallback(
      HoverAnimationType animation_type,
      const ui::CallbackLayerAnimationObserver& observer);

  // The LayerDelegate that paints the hover |layer_|.
  scoped_ptr<RoundedRectangleLayerDelegate> layer_delegate_;

  // The visual hover layer that is painted by |layer_delegate_|.
  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHover);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HOVER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_SHADOW_H_
#define UI_WM_CORE_SHADOW_H_

#include <memory>

#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/wm_export.h"

namespace ui {
class Layer;
}  // namespace ui

namespace wm {
enum class ShadowElevation;

// Simple class that draws a drop shadow around content at given bounds.
class WM_EXPORT Shadow : public ui::ImplicitAnimationObserver {
 public:
  Shadow();
  ~Shadow() override;

  void Init(ShadowElevation elevation);

  // Returns |layer_.get()|. This is exposed so it can be added to the same
  // layer as the content and stacked below it.  SetContentBounds() should be
  // used to adjust the shadow's size and position (rather than applying
  // transformations to this layer).
  ui::Layer* layer() const { return layer_.get(); }

  // Exposed to allow setting animation parameters for bounds and opacity
  // animations.
  ui::Layer* shadow_layer() const { return shadow_layer_.get(); }

  const gfx::Rect& content_bounds() const { return content_bounds_; }
  ShadowElevation desired_elevation() const { return desired_elevation_; }

  // Moves and resizes the shadow layer to frame |content_bounds|.
  void SetContentBounds(const gfx::Rect& content_bounds);

  // Sets the shadow's appearance, animating opacity as necessary.
  void SetElevation(ShadowElevation elevation);

  // Sets the radius for the rounded corners to take into account when
  // adjusting the shadow layer to frame |content_bounds|. 0 or greater.
  void SetRoundedCornerRadius(int rounded_corner_radius);

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

 private:
  // Updates the shadow layer and its image to reflect |desired_elevation_|.
  void RecreateShadowLayer();

  // Updates the shadow layer bounds based on the inteior inset and the current
  // |content_bounds_|.
  void UpdateLayerBounds();

  // The goal elevation, set when the transition animation starts. The elevation
  // dictates the shadow's display characteristics and is proportional to the
  // size of the blur and its offset. This may not match reality if the window
  // isn't big enough to support it.
  ShadowElevation desired_elevation_;

  // The elevation of the shadow image that's currently set on |shadow_layer_|.
  int effective_elevation_ = 0;

  // Rounded corners are drawn on top of the window's content layer,
  // we need to exclude them from the occlusion area.
  int rounded_corner_radius_;

  // The parent layer of the shadow layer. It serves as a container accessible
  // from the outside to control the visibility of the shadow.
  std::unique_ptr<ui::Layer> layer_;

  // The actual shadow layer corresponding to a cc::NinePatchLayer.
  std::unique_ptr<ui::Layer> shadow_layer_;

  // When the elevation changes, the old shadow cross-fades with the new one.
  // When non-null, this is an old |shadow_layer_| that's being animated out.
  std::unique_ptr<ui::Layer> fading_layer_;

  // Bounds of the content that the shadow encloses.
  gfx::Rect content_bounds_;

  DISALLOW_COPY_AND_ASSIGN(Shadow);
};

}  // namespace wm

#endif  // UI_WM_CORE_SHADOW_H_

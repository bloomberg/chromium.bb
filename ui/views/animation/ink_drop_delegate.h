// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class Layer;
}  // namespace ui

namespace views {

// Renders the visual feedback for an ink drop animation. Will render a circle
// if |circle_| is true, otherwise renders a rounded rectangle.
class InkDropDelegate : public ui::LayerDelegate {
 public:
  InkDropDelegate(ui::Layer* layer,
                  SkColor color,
                  int circle_radius,
                  int rounded_rect_corner_radius);
  ~InkDropDelegate() override;

  // Sets the visual style of the feedback.
  void set_should_render_circle(bool should_render_circle) {
    should_render_circle_ = should_render_circle;
  }

  bool should_render_circle() const { return should_render_circle_; }

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  base::Closure PrepareForLayerBoundsChange() override;

 private:
  // The ui::Layer being rendered to.
  ui::Layer* layer_;

  // The color to paint.
  SkColor color_;

  // When true renders a circle, otherwise renders a rounded rectangle.
  bool should_render_circle_;

  // The radius of the circle.
  const int circle_radius_;

  // The radius of the rounded rectangles corners.
  const int rounded_rect_corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(InkDropDelegate);
};

}  // namespace views

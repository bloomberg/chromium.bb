// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_PAINTED_LAYER_DELEGATES_H_
#define UI_VIEWS_ANIMATION_INK_DROP_PAINTED_LAYER_DELEGATES_H_

#include <vector>

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/views_export.h"

namespace views {

// Base ui::LayerDelegate stub that can be extended to paint shapes of a
// specific color.
class VIEWS_EXPORT BasePaintedLayerDelegate : public ui::LayerDelegate {
 public:
  ~BasePaintedLayerDelegate() override;

  // Defines the bounds of the layer that the delegate will paint into.
  virtual gfx::RectF GetPaintedBounds() const = 0;

  // Defines how to place the layer by providing an offset from the origin of
  // the parent to the visual center of the layer.
  virtual gfx::Vector2dF GetCenteringOffset() const;

  // ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 protected:
  explicit BasePaintedLayerDelegate(SkColor color);

  SkColor color() const { return color_; }

 private:
  // The color to paint.
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(BasePaintedLayerDelegate);
};

// A BasePaintedLayerDelegate that paints a circle of a specified color and
// radius.
class VIEWS_EXPORT CircleLayerDelegate : public BasePaintedLayerDelegate {
 public:
  CircleLayerDelegate(SkColor color, int radius);
  ~CircleLayerDelegate() override;

  int radius() const { return radius_; }

  // BasePaintedLayerDelegate:
  gfx::RectF GetPaintedBounds() const override;
  void OnPaintLayer(const ui::PaintContext& context) override;

 private:
  // The radius of the circle.
  int radius_;

  DISALLOW_COPY_AND_ASSIGN(CircleLayerDelegate);
};

// A BasePaintedLayerDelegate that paints a rectangle of a specified color and
// size.
class VIEWS_EXPORT RectangleLayerDelegate : public BasePaintedLayerDelegate {
 public:
  RectangleLayerDelegate(SkColor color, gfx::SizeF size);
  ~RectangleLayerDelegate() override;

  const gfx::SizeF& size() const { return size_; }

  // BasePaintedLayerDelegate:
  gfx::RectF GetPaintedBounds() const override;
  void OnPaintLayer(const ui::PaintContext& context) override;

 private:
  // The size of the rectangle.
  gfx::SizeF size_;

  DISALLOW_COPY_AND_ASSIGN(RectangleLayerDelegate);
};

// A BasePaintedLayerDelegate that paints a rounded rectangle of a specified
// color, size and corner radius.
class VIEWS_EXPORT RoundedRectangleLayerDelegate
    : public BasePaintedLayerDelegate {
 public:
  RoundedRectangleLayerDelegate(SkColor color,
                                const gfx::SizeF& size,
                                int corner_radius);
  ~RoundedRectangleLayerDelegate() override;

  const gfx::SizeF& size() const { return size_; }

  // BasePaintedLayerDelegate:
  gfx::RectF GetPaintedBounds() const override;
  void OnPaintLayer(const ui::PaintContext& context) override;

 private:
  // The size of the rectangle.
  gfx::SizeF size_;

  // The radius of the corners.
  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundedRectangleLayerDelegate);
};

// A BasePaintedLayerDelegate that paints a shadow around the outside of a
// specified roundrect, and also fills the round rect.
class VIEWS_EXPORT BorderShadowLayerDelegate : public BasePaintedLayerDelegate {
 public:
  BorderShadowLayerDelegate(const std::vector<gfx::ShadowValue>& shadows,
                            const gfx::Rect& shadowed_area_bounds,
                            SkColor fill_color,
                            int corner_radius);
  ~BorderShadowLayerDelegate() override;

  // BasePaintedLayerDelegate:
  gfx::RectF GetPaintedBounds() const override;
  gfx::Vector2dF GetCenteringOffset() const override;
  void OnPaintLayer(const ui::PaintContext& context) override;

 private:
  gfx::Rect GetTotalRect() const;

  const std::vector<gfx::ShadowValue> shadows_;

  // The bounds of the shadowed area.
  const gfx::Rect bounds_;

  const SkColor fill_color_;

  const int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(BorderShadowLayerDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_PAINTED_LAYER_DELEGATES_H_

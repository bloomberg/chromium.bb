// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_PAINTED_LAYER_DELEGATES_H_
#define UI_VIEWS_ANIMATION_INK_DROP_PAINTED_LAYER_DELEGATES_H_

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace views {

// Base ui::LayerDelegate stub that can be extended to paint shapes of a
// specific color.
class BasePaintedLayerDelegate : public ui::LayerDelegate {
 public:
  ~BasePaintedLayerDelegate() override;

  SkColor color() const { return color_; }

  // Returns the center point of the painted shape.
  virtual gfx::PointF GetCenterPoint() const = 0;

  // ui::LayerDelegate:
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  base::Closure PrepareForLayerBoundsChange() override;

 protected:
  explicit BasePaintedLayerDelegate(SkColor color);

 private:
  // The color to paint.
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(BasePaintedLayerDelegate);
};

// A BasePaintedLayerDelegate that paints a circle of a specified color and
// radius.
class CircleLayerDelegate : public BasePaintedLayerDelegate {
 public:
  CircleLayerDelegate(SkColor color, int radius);
  ~CircleLayerDelegate() override;

  int radius() const { return radius_; }

  // BasePaintedLayerDelegate:
  gfx::PointF GetCenterPoint() const override;
  void OnPaintLayer(const ui::PaintContext& context) override;

 private:
  // The radius of the circle.
  int radius_;

  DISALLOW_COPY_AND_ASSIGN(CircleLayerDelegate);
};

// A BasePaintedLayerDelegate that paints a rectangle of a specified color and
// size.
class RectangleLayerDelegate : public BasePaintedLayerDelegate {
 public:
  RectangleLayerDelegate(SkColor color, gfx::Size size);
  ~RectangleLayerDelegate() override;

  const gfx::Size& size() const { return size_; }

  // BasePaintedLayerDelegate:
  gfx::PointF GetCenterPoint() const override;
  void OnPaintLayer(const ui::PaintContext& context) override;

 private:
  // The size of the rectangle.
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(RectangleLayerDelegate);
};

// A BasePaintedLayerDelegate that paints a rounded rectangle of a specified
// color, size and corner radius.
class RoundedRectangleLayerDelegate : public BasePaintedLayerDelegate {
 public:
  RoundedRectangleLayerDelegate(SkColor color,
                                gfx::Size size,
                                int corner_radius);
  ~RoundedRectangleLayerDelegate() override;

  const gfx::Size& size() const { return size_; }

  // BasePaintedLayerDelegate:
  gfx::PointF GetCenterPoint() const override;
  void OnPaintLayer(const ui::PaintContext& context) override;

 private:
  // The size of the rectangle.
  gfx::Size size_;

  // The radius of the corners.
  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundedRectangleLayerDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_PAINTED_LAYER_DELEGATES_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_MASK_H_
#define UI_VIEWS_ANIMATION_INK_DROP_MASK_H_

#include "base/macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/views_export.h"

namespace views {

// Base class for different ink drop masks. It is responsible for creating the
// ui::Layer that can be set as the mask layer for ink drop layer.
class VIEWS_EXPORT InkDropMask : public ui::LayerDelegate {
 public:
  ~InkDropMask() override;

  ui::Layer* layer() { return &layer_; }

 protected:
  explicit InkDropMask(const gfx::Rect& layer_bounds);

 private:
  // Overriden from ui::LayerDelegate:
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override;

  void OnDeviceScaleFactorChanged(float device_scale_factor) override;

  ui::Layer layer_;

  DISALLOW_COPY_AND_ASSIGN(InkDropMask);
};

// A rectangular ink drop mask with rounded corners.
class VIEWS_EXPORT RoundRectInkDropMask : public InkDropMask {
 public:
  RoundRectInkDropMask(const gfx::Rect& layer_bounds,
                       const gfx::Rect& mask_bounds,
                       int corner_radius);

 private:
  // Overriden from InkDropMask:
  void OnPaintLayer(const ui::PaintContext& context) override;

  gfx::Rect mask_bounds_;
  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundRectInkDropMask);
};

// A circular ink drop mask.
class VIEWS_EXPORT CircleInkDropMask : public InkDropMask {
 public:
  CircleInkDropMask(const gfx::Rect& layer_bounds,
                    const gfx::Point& mask_center,
                    int mask_radius);

 private:
  // Overriden from InkDropMask:
  void OnPaintLayer(const ui::PaintContext& context) override;

  gfx::Point mask_center_;
  int mask_radius_;

  DISALLOW_COPY_AND_ASSIGN(CircleInkDropMask);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_MASK_H_

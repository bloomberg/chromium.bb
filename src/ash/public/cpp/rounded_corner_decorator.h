// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ROUNDED_CORNER_DECORATOR_H_
#define ASH_PUBLIC_CPP_ROUNDED_CORNER_DECORATOR_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_observer.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

constexpr int kPipRoundedCornerRadius = 8;

// Applies rounded corners to the given layer, and modifies the shadow of
// the given window to be rounded.
class ASH_PUBLIC_EXPORT RoundedCornerDecorator : public ui::LayerDelegate,
                                                 public ui::LayerObserver,
                                                 public aura::WindowObserver {
 public:
  RoundedCornerDecorator(aura::Window* shadow_window,
                         aura::Window* layer_window,
                         ui::Layer* layer,
                         int radius);
  ~RoundedCornerDecorator() override;

  // Returns true if the rounded corner decorator is still applied to a valid
  // layer.
  bool IsValid();

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  void Update(const gfx::Size& size);
  void Shutdown();

  aura::Window* layer_window_;
  ui::Layer* layer_;
  std::unique_ptr<ui::Layer> mask_layer_;
  int radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundedCornerDecorator);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ROUNDED_CORNER_DECORATOR_H_

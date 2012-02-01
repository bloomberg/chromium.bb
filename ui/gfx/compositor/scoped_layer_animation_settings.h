// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_SCOPED_LAYER_ANIMATION_SETTINGS_H_
#define UI_GFX_COMPOSITOR_SCOPED_LAYER_ANIMATION_SETTINGS_H_
#pragma once

#include <set>

#include "base/time.h"

#include "ui/gfx/compositor/compositor_export.h"

namespace ui {

class ImplicitAnimationObserver;
class LayerAnimator;
class LayerAnimationObserver;

// Scoped settings allow you to temporarily change the animator's settings and
// these changes are reverted when the object is destroyed. NOTE: when the
// settings object is created, it applies the default transition duration
// (200ms).
class COMPOSITOR_EXPORT ScopedLayerAnimationSettings {
 public:
  explicit ScopedLayerAnimationSettings(LayerAnimator* animator);
  virtual ~ScopedLayerAnimationSettings();

  void AddObserver(LayerAnimationObserver* observer);
  void AddImplicitObserver(ImplicitAnimationObserver* observer);
  void SetTransitionDuration(base::TimeDelta duration);

 private:
  LayerAnimator* animator_;
  base::TimeDelta old_transition_duration_;
  std::set<LayerAnimationObserver*> observers_;
  std::set<ImplicitAnimationObserver*> implicit_observers_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLayerAnimationSettings);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_SCOPED_LAYER_ANIMATION_SETTINGS_H_

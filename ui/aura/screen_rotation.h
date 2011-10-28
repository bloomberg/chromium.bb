// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCREEN_ROTATION_H_
#define UI_AURA_SCREEN_ROTATION_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/compositor/layer_animation_element.h"
#include "ui/gfx/point.h"

namespace ui {
class InterpolatedTransform;
class LayerAnimationDelegate;
}

// A screen rotation represents a single transition from one screen orientation
// to another. The  intended usage is that a new instance of the class is
// created for every transition. It is possible to update the target orientation
// in the middle of a transition.
class ScreenRotation : public ui::LayerAnimationElement {
 public:
  // The screen rotation does not own the view or the listener, and these
  // objects are required to outlive the Screen rotation object.
  ScreenRotation(int degrees);
  virtual ~ScreenRotation();

 private:
  // Implementation of ui::LayerAnimationDelegate
  virtual void OnStart(ui::LayerAnimationDelegate* delegate) OVERRIDE;
  virtual void OnProgress(double t,
                          ui::LayerAnimationDelegate* delegate) OVERRIDE;
  virtual void OnGetTarget(TargetValue* target) const OVERRIDE;
  virtual void OnAbort() OVERRIDE;

  static const ui::LayerAnimationElement::AnimatableProperties& GetProperties();

  // Generates the intermediate transformation matrices used during the
  // animation.
  scoped_ptr<ui::InterpolatedTransform> interpolated_transform_;

  // The number of degrees to rotate.
  int degrees_;

  // The target origin for |view_|
  gfx::Point new_origin_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotation);
};

#endif  // UI_AURA_SCREEN_ROTATION_H_

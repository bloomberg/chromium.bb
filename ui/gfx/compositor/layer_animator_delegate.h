// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_ANIMATOR_DELEGATE_H_
#define UI_GFX_COMPOSITOR_LAYER_ANIMATOR_DELEGATE_H_
#pragma once

#include "ui/gfx/compositor/compositor_export.h"

namespace gfx {
class Rect;
}

namespace ui {

class Transform;

// LayerAnimator modifies the Layer using this interface.
class COMPOSITOR_EXPORT LayerAnimatorDelegate {
 public:
  virtual void SetBoundsFromAnimator(const gfx::Rect& bounds) = 0;
  virtual void SetTransformFromAnimator(const Transform& transform) = 0;
  virtual void SetOpacityFromAnimator(float opacity) = 0;

 protected:
  virtual ~LayerAnimatorDelegate() {}
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_LAYER_H_

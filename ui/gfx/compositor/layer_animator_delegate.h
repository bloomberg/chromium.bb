// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_ANIMATOR_DELEGATE_H_
#define UI_GFX_COMPOSITOR_LAYER_ANIMATOR_DELEGATE_H_
#pragma once

#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/compositor/compositor_export.h"
#include "ui/gfx/compositor/layer_animation_delegate.h"

namespace ui {

class LayerAnimationSequence;

// Layer animators interact with the layers using this interface.
class COMPOSITOR_EXPORT LayerAnimatorDelegate : public LayerAnimationDelegate {
 public:
  // Called when the |sequence| ends. Not called if |sequence| is aborted.
  virtual void OnLayerAnimationEnded(LayerAnimationSequence* sequence) = 0;

  // if this becomes necessary, this would be the appropriate place to add
  // notifications about elements starting or ending, or sequences starting.

 protected:
  virtual ~LayerAnimatorDelegate() {}
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_LAYER_ANIMATOR_DELEGATE_H_

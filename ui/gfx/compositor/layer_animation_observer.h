// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_ANIMATION_OBSERVER_H_
#define UI_GFX_COMPOSITOR_LAYER_ANIMATION_OBSERVER_H_
#pragma once

#include "ui/gfx/compositor/compositor_export.h"

namespace ui {

class LayerAnimationSequence;

// LayerAnimationObservers are notified when animations complete.
class COMPOSITOR_EXPORT LayerAnimationObserver  {
 public:
  // Called when the |sequence| ends. Not called if |sequence| is aborted.
  virtual void OnLayerAnimationEnded(
      const LayerAnimationSequence* sequence) = 0;

  // Called if |sequence| is aborted for any reason. Should never do anything
  // that may cause another animation to be started.
  virtual void OnLayerAnimationAborted(
      const LayerAnimationSequence* sequence) = 0;

  // Called when the animation is scheduled.
  virtual void OnLayerAnimationScheduled(
      const LayerAnimationSequence* sequence) = 0;

 protected:
  virtual ~LayerAnimationObserver() {}
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_LAYER_ANIMATION_OBSERVER_H_

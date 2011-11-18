// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_OBSERVER_
#define UI_GFX_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_OBSERVER_
#pragma once

#include "base/compiler_specific.h"
#include "ui/gfx/compositor/layer_animation_observer.h"

namespace ui {

class LayerAnimationSequence;

// Listens to animation ended notifications. Remembers the last sequence that
// it was notified about.
class TestLayerAnimationObserver : public LayerAnimationObserver {
 public:
  TestLayerAnimationObserver();
  virtual ~TestLayerAnimationObserver();

  virtual void OnLayerAnimationEnded(
      const LayerAnimationSequence* sequence) OVERRIDE;

  virtual void OnLayerAnimationAborted(
      const LayerAnimationSequence* sequence) OVERRIDE;

  virtual void OnLayerAnimationScheduled(
      const LayerAnimationSequence* sequence) OVERRIDE;


  const LayerAnimationSequence* last_ended_sequence() const {
    return last_ended_sequence_;
  }

  const LayerAnimationSequence* last_scheduled_sequence() const {
    return last_scheduled_sequence_;
  }

  const LayerAnimationSequence* last_aborted_sequence() const {
    return last_aborted_sequence_;
  }

 private:
  const LayerAnimationSequence* last_ended_sequence_;
  const LayerAnimationSequence* last_scheduled_sequence_;
  const LayerAnimationSequence* last_aborted_sequence_;

  // Copy and assign are allowed.
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_OBSERVER_

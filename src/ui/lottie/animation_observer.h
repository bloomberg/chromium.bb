// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LOTTIE_ANIMATION_OBSERVER_H_
#define UI_LOTTIE_ANIMATION_OBSERVER_H_

#include "base/component_export.h"

namespace lottie {
class Animation;

class COMPONENT_EXPORT(UI_LOTTIE) AnimationObserver {
 public:
  // Called when the animation started playing.
  virtual void AnimationWillStartPlaying(const Animation* animation) {}

  // Called when one animation cycle has completed. This happens when a linear
  // animation has reached its end, or a loop/throbbing animation has finished
  // a cycle.
  virtual void AnimationCycleEnded(const Animation* animation) {}

  // Called when the animation has successfully resumed.
  virtual void AnimationResuming(const Animation* animation) {}

 protected:
  virtual ~AnimationObserver() = default;
};

}  // namespace lottie

#endif  // UI_LOTTIE_ANIMATION_OBSERVER_H_

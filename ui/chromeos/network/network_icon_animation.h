// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_H_
#define UI_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_H_

#include <set>
#include <string>

#include "base/observer_list.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"

namespace ui {
namespace network_icon {

class AnimationObserver;

// Single instance class to handle icon animations and keep them in sync.
class UI_CHROMEOS_EXPORT NetworkIconAnimation : public gfx::AnimationDelegate {
 public:
  NetworkIconAnimation();
  virtual ~NetworkIconAnimation();

  // Returns the current animation value, [0-1].
  double GetAnimation();

  // The animation stops when all observers have been removed.
  // Be sure to remove observers when no associated icons are animating.
  void AddObserver(AnimationObserver* observer);
  void RemoveObserver(AnimationObserver* observer);

  // gfx::AnimationDelegate implementation.
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

  static NetworkIconAnimation* GetInstance();

 private:
  gfx::ThrobAnimation animation_;
  ObserverList<AnimationObserver> observers_;
};

}  // namespace network_icon
}  // namespace ui

#endif  // UI_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_H_

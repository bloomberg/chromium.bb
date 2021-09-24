// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_KEY_H_
#define UI_VIEWS_ANIMATION_ANIMATION_KEY_H_

#include <tuple>

#include "ui/compositor/layer_animation_element.h"

namespace ui {
class Layer;
}

namespace views {

struct AnimationKey {
  ui::Layer* target;
  ui::LayerAnimationElement::AnimatableProperty property;

  bool operator<(const AnimationKey& key) const {
    return std::tie(target, property) < std::tie(key.target, key.property);
  }
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_KEY_H_

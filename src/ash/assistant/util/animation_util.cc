// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/animation_util.h"

#include "base/time/time.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"

namespace ash {
namespace assistant {
namespace util {

::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    const LayerAnimationSequenceParams& params) {
  return CreateLayerAnimationSequence(std::move(a), nullptr, nullptr, nullptr,
                                      params);
}

::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    std::unique_ptr<::ui::LayerAnimationElement> b,
    const LayerAnimationSequenceParams& params) {
  return CreateLayerAnimationSequence(std::move(a), std::move(b), nullptr,
                                      nullptr, params);
}

::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    std::unique_ptr<::ui::LayerAnimationElement> b,
    std::unique_ptr<::ui::LayerAnimationElement> c,
    const LayerAnimationSequenceParams& params) {
  return CreateLayerAnimationSequence(std::move(a), std::move(b), std::move(c),
                                      nullptr, params);
}

::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    std::unique_ptr<::ui::LayerAnimationElement> b,
    std::unique_ptr<::ui::LayerAnimationElement> c,
    std::unique_ptr<::ui::LayerAnimationElement> d,
    const LayerAnimationSequenceParams& params) {
  ui::LayerAnimationSequence* layer_animation_sequence =
      new ui::LayerAnimationSequence();

  layer_animation_sequence->AddElement(std::move(a));

  if (b)
    layer_animation_sequence->AddElement(std::move(b));

  if (c)
    layer_animation_sequence->AddElement(std::move(c));

  if (d)
    layer_animation_sequence->AddElement(std::move(d));

  layer_animation_sequence->set_is_cyclic(params.is_cyclic);

  return layer_animation_sequence;
}

std::unique_ptr<::ui::LayerAnimationElement> CreateOpacityElement(
    float opacity,
    const base::TimeDelta& duration,
    const gfx::Tween::Type& tween) {
  std::unique_ptr<::ui::LayerAnimationElement> layer_animation_element =
      ::ui::LayerAnimationElement::CreateOpacityElement(opacity, duration);
  layer_animation_element->set_tween_type(tween);
  return layer_animation_element;
}

std::unique_ptr<::ui::LayerAnimationElement> CreateTransformElement(
    const gfx::Transform& transform,
    const base::TimeDelta& duration,
    const gfx::Tween::Type& tween) {
  std::unique_ptr<::ui::LayerAnimationElement> layer_animation_element =
      ::ui::LayerAnimationElement::CreateTransformElement(transform, duration);
  layer_animation_element->set_tween_type(tween);
  return layer_animation_element;
}

void StartLayerAnimationSequence(
    ::ui::LayerAnimator* layer_animator,
    ::ui::LayerAnimationSequence* layer_animation_sequence,
    ::ui::LayerAnimationObserver* observer) {
  if (observer)
    layer_animation_sequence->AddObserver(observer);
  layer_animator->StartAnimation(layer_animation_sequence);
}

void StartLayerAnimationSequencesTogether(
    ::ui::LayerAnimator* layer_animator,
    const std::vector<ui::LayerAnimationSequence*>& layer_animation_sequences,
    ::ui::LayerAnimationObserver* observer) {
  if (observer) {
    for (::ui::LayerAnimationSequence* layer_animation_sequence :
         layer_animation_sequences) {
      layer_animation_sequence->AddObserver(observer);
    }
  }
  layer_animator->StartTogether(layer_animation_sequences);
}

}  // namespace util
}  // namespace assistant
}  // namespace ash

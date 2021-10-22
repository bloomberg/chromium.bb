// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_sequence_block.h"

#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_key.h"

namespace views {

using PassKey = base::PassKey<AnimationSequenceBlock>;

AnimationSequenceBlock::AnimationSequenceBlock(
    base::PassKey<AnimationBuilder> builder_key,
    AnimationBuilder* owner,
    base::TimeDelta start)
    : builder_key_(builder_key), owner_(owner), start_(start) {}

AnimationSequenceBlock::AnimationSequenceBlock(AnimationSequenceBlock&& other)
    : builder_key_(std::move(other.builder_key_)),
      owner_(other.owner_),
      start_(std::move(other.start_)),
      duration_(std::move(other.duration_)),
      elements_(std::move(other.elements_)) {
  DCHECK(!other.finalized_)
      << "Do not access old blocks after creating new ones.";
  other.finalized_ = true;
}

AnimationSequenceBlock& AnimationSequenceBlock::operator=(
    AnimationSequenceBlock&& other) {
  DCHECK(!other.finalized_)
      << "Do not access old blocks after creating new ones.";
  builder_key_ = std::move(other.builder_key_);
  owner_ = other.owner_;
  start_ = std::move(other.start_);
  duration_ = std::move(other.duration_);
  elements_ = std::move(other.elements_);
  finalized_ = false;
  other.finalized_ = true;
  return *this;
}

AnimationSequenceBlock::~AnimationSequenceBlock() {
  if (!finalized_) {
    TerminateBlock();
    owner_->TerminateSequence(PassKey());
  }
}

AnimationSequenceBlock& AnimationSequenceBlock::SetDuration(
    base::TimeDelta duration) {
  DCHECK(!finalized_) << "Do not access old blocks after creating new ones.";
  DCHECK(!duration_.has_value()) << "Duration may be set at most once.";
  duration_ = duration;
  return *this;
}

AnimationSequenceBlock& AnimationSequenceBlock::SetBounds(
    ui::Layer* target,
    const gfx::Rect& bounds,
    gfx::Tween::Type tween_type) {
  return AddAnimation({target, ui::LayerAnimationElement::BOUNDS},
                      Element(bounds, tween_type));
}

AnimationSequenceBlock& AnimationSequenceBlock::SetBounds(
    ui::LayerOwner* target,
    const gfx::Rect& bounds,
    gfx::Tween::Type tween_type) {
  return SetBounds(target->layer(), bounds, tween_type);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetBrightness(
    ui::Layer* target,
    float brightness,
    gfx::Tween::Type tween_type) {
  return AddAnimation({target, ui::LayerAnimationElement::BRIGHTNESS},
                      Element(brightness, tween_type));
}

AnimationSequenceBlock& AnimationSequenceBlock::SetBrightness(
    ui::LayerOwner* target,
    float brightness,
    gfx::Tween::Type tween_type) {
  return SetBrightness(target->layer(), brightness, tween_type);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetClipRect(
    ui::Layer* target,
    const gfx::Rect& clip_rect,
    gfx::Tween::Type tween_type) {
  return AddAnimation({target, ui::LayerAnimationElement::CLIP},
                      Element(clip_rect, tween_type));
}

AnimationSequenceBlock& AnimationSequenceBlock::SetClipRect(
    ui::LayerOwner* target,
    const gfx::Rect& clip_rect,
    gfx::Tween::Type tween_type) {
  return SetClipRect(target->layer(), clip_rect, tween_type);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetColor(
    ui::Layer* target,
    SkColor color,
    gfx::Tween::Type tween_type) {
  return AddAnimation({target, ui::LayerAnimationElement::COLOR},
                      Element(color, tween_type));
}

AnimationSequenceBlock& AnimationSequenceBlock::SetColor(
    ui::LayerOwner* target,
    SkColor color,
    gfx::Tween::Type tween_type) {
  return SetColor(target->layer(), color, tween_type);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetGrayscale(
    ui::Layer* target,
    float grayscale,
    gfx::Tween::Type tween_type) {
  return AddAnimation({target, ui::LayerAnimationElement::GRAYSCALE},
                      Element(grayscale, tween_type));
}

AnimationSequenceBlock& AnimationSequenceBlock::SetGrayscale(
    ui::LayerOwner* target,
    float grayscale,
    gfx::Tween::Type tween_type) {
  return SetGrayscale(target->layer(), grayscale, tween_type);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetOpacity(
    ui::Layer* target,
    float opacity,
    gfx::Tween::Type tween_type) {
  return AddAnimation({target, ui::LayerAnimationElement::OPACITY},
                      Element(opacity, tween_type));
}

AnimationSequenceBlock& AnimationSequenceBlock::SetOpacity(
    ui::LayerOwner* target,
    float opacity,
    gfx::Tween::Type tween_type) {
  return SetOpacity(target->layer(), opacity, tween_type);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetTransform(
    ui::Layer* target,
    gfx::Transform transform,
    gfx::Tween::Type tween_type) {
  return AddAnimation({target, ui::LayerAnimationElement::TRANSFORM},
                      Element(std::move(transform), tween_type));
}

AnimationSequenceBlock& AnimationSequenceBlock::SetTransform(
    ui::LayerOwner* target,
    gfx::Transform transform,
    gfx::Tween::Type tween_type) {
  return SetTransform(target->layer(), std::move(transform), tween_type);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetRoundedCorners(
    ui::Layer* target,
    const gfx::RoundedCornersF& rounded_corners,
    gfx::Tween::Type tween_type) {
  return AddAnimation({target, ui::LayerAnimationElement::ROUNDED_CORNERS},
                      Element(rounded_corners, tween_type));
}

AnimationSequenceBlock& AnimationSequenceBlock::SetRoundedCorners(
    ui::LayerOwner* target,
    const gfx::RoundedCornersF& rounded_corners,
    gfx::Tween::Type tween_type) {
  return SetRoundedCorners(target->layer(), rounded_corners, tween_type);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetVisibility(
    ui::Layer* target,
    bool visible,
    gfx::Tween::Type tween_type) {
  return AddAnimation({target, ui::LayerAnimationElement::VISIBILITY},
                      Element(visible, tween_type));
}

AnimationSequenceBlock& AnimationSequenceBlock::SetVisibility(
    ui::LayerOwner* target,
    bool visible,
    gfx::Tween::Type tween_type) {
  return SetVisibility(target->layer(), visible, tween_type);
}

AnimationSequenceBlock AnimationSequenceBlock::At(
    base::TimeDelta since_sequence_start) {
  DCHECK(!finalized_) << "Do not access old blocks after creating new ones.";
  TerminateBlock();
  finalized_ = true;
  return AnimationSequenceBlock(builder_key_, owner_, since_sequence_start);
}

AnimationSequenceBlock AnimationSequenceBlock::Offset(
    base::TimeDelta since_last_block_start) {
  return At(start_ + since_last_block_start);
}

AnimationSequenceBlock AnimationSequenceBlock::Then() {
  return Offset(duration_.value_or(base::TimeDelta()));
}

AnimationSequenceBlock::Element::Element(AnimationValue animation_value,
                                         gfx::Tween::Type tween_type)
    : animation_value_(std::move(animation_value)), tween_type_(tween_type) {}
AnimationSequenceBlock::Element::~Element() = default;
AnimationSequenceBlock::Element::Element(Element&&) = default;
AnimationSequenceBlock::Element& AnimationSequenceBlock::Element::operator=(
    Element&&) = default;

AnimationSequenceBlock& AnimationSequenceBlock::AddAnimation(AnimationKey key,
                                                             Element element) {
  DCHECK(!finalized_) << "Do not access old blocks after creating new ones.";
  DCHECK(key.target) << "Animation targets must paint to a layer.";
  const auto result =
      elements_.insert(std::make_pair(std::move(key), std::move(element)));
  DCHECK(result.second) << "Animate (target, property) at most once per block.";
  return *this;
}

void AnimationSequenceBlock::TerminateBlock() {
  const auto duration = duration_.value_or(base::TimeDelta());
  for (auto& pair : elements_) {
    std::unique_ptr<ui::LayerAnimationElement> element;
    switch (pair.first.property) {
      case ui::LayerAnimationElement::TRANSFORM:
        element = ui::LayerAnimationElement::CreateTransformElement(
            absl::get<gfx::Transform>(std::move(pair.second.animation_value_)),
            duration);
        break;
      case ui::LayerAnimationElement::BOUNDS:
        element = ui::LayerAnimationElement::CreateBoundsElement(
            absl::get<gfx::Rect>(pair.second.animation_value_), duration);
        break;
      case ui::LayerAnimationElement::OPACITY:
        element = ui::LayerAnimationElement::CreateOpacityElement(
            absl::get<float>(pair.second.animation_value_), duration);
        break;
      case ui::LayerAnimationElement::VISIBILITY:
        element = ui::LayerAnimationElement::CreateVisibilityElement(
            absl::get<bool>(pair.second.animation_value_), duration);
        break;
      case ui::LayerAnimationElement::BRIGHTNESS:
        element = ui::LayerAnimationElement::CreateBrightnessElement(
            absl::get<float>(pair.second.animation_value_), duration);
        break;
      case ui::LayerAnimationElement::GRAYSCALE:
        element = ui::LayerAnimationElement::CreateGrayscaleElement(
            absl::get<float>(pair.second.animation_value_), duration);
        break;
      case ui::LayerAnimationElement::COLOR:
        element = ui::LayerAnimationElement::CreateColorElement(
            absl::get<SkColor>(pair.second.animation_value_), duration);
        break;
      case ui::LayerAnimationElement::CLIP:
        element = ui::LayerAnimationElement::CreateClipRectElement(
            absl::get<gfx::Rect>(pair.second.animation_value_), duration);
        break;
      case ui::LayerAnimationElement::ROUNDED_CORNERS:
        element = ui::LayerAnimationElement::CreateRoundedCornersElement(
            absl::get<gfx::RoundedCornersF>(pair.second.animation_value_),
            duration);
        break;
      default:
        NOTREACHED();
    }
    element->set_tween_type(pair.second.tween_type_);
    owner_->AddLayerAnimationElement(PassKey(), pair.first, start_, duration,
                                     std::move(element));
  }

  owner_->BlockEndedAt(PassKey(), start_ + duration);
  elements_.clear();
}

}  // namespace views

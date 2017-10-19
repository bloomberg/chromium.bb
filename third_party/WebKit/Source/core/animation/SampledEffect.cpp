// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SampledEffect.h"

namespace blink {

SampledEffect::SampledEffect(KeyframeEffectReadOnly* effect)
    : effect_(effect),
      sequence_number_(effect->GetAnimation()->SequenceNumber()),
      priority_(effect->GetPriority()) {}

void SampledEffect::Clear() {
  effect_ = nullptr;
  interpolations_.clear();
}

bool SampledEffect::WillNeverChange() const {
  return !effect_ || !effect_->GetAnimation();
}

void SampledEffect::RemoveReplacedInterpolations(
    const HashSet<PropertyHandle>& replaced_properties) {
  size_t new_size = 0;
  for (auto& interpolation : interpolations_) {
    if (!replaced_properties.Contains(interpolation->GetProperty()))
      interpolations_[new_size++].swap(interpolation);
  }
  interpolations_.Shrink(new_size);
}

void SampledEffect::UpdateReplacedProperties(
    HashSet<PropertyHandle>& replaced_properties) {
  for (const auto& interpolation : interpolations_) {
    if (!interpolation->DependsOnUnderlyingValue())
      replaced_properties.insert(interpolation->GetProperty());
  }
}

void SampledEffect::Trace(blink::Visitor* visitor) {
  visitor->Trace(effect_);
}

}  // namespace blink

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/KeyframeEffectModel.h"

#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSPropertyEquality.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "platform/animation/AnimationUtilities.h"
#include "platform/geometry/FloatBox.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

PropertyHandleSet KeyframeEffectModelBase::Properties() const {
  PropertyHandleSet result;
  for (const auto& keyframe : keyframes_) {
    for (const auto& property : keyframe->Properties())
      result.insert(property);
  }
  return result;
}

void KeyframeEffectModelBase::SetFrames(KeyframeVector& keyframes) {
  // TODO(samli): Should also notify/invalidate the animation
  keyframes_ = keyframes;
  keyframe_groups_ = nullptr;
  interpolation_effect_.Clear();
  last_fraction_ = std::numeric_limits<double>::quiet_NaN();
}

bool KeyframeEffectModelBase::Sample(
    int iteration,
    double fraction,
    double iteration_duration,
    Vector<RefPtr<Interpolation>>& result) const {
  DCHECK_GE(iteration, 0);
  DCHECK(!IsNull(fraction));
  EnsureKeyframeGroups();
  EnsureInterpolationEffectPopulated();

  bool changed = iteration != last_iteration_ || fraction != last_fraction_ ||
                 iteration_duration != last_iteration_duration_;
  last_iteration_ = iteration;
  last_fraction_ = fraction;
  last_iteration_duration_ = iteration_duration;
  interpolation_effect_.GetActiveInterpolations(fraction, iteration_duration,
                                                result);
  return changed;
}

bool KeyframeEffectModelBase::SnapshotNeutralCompositorKeyframes(
    Element& element,
    const ComputedStyle& old_style,
    const ComputedStyle& new_style,
    const ComputedStyle* parent_style) const {
  bool updated = false;
  EnsureKeyframeGroups();
  for (CSSPropertyID property : CompositorAnimations::kCompositableProperties) {
    if (CSSPropertyEquality::PropertiesEqual(PropertyHandle(property),
                                             old_style, new_style))
      continue;
    PropertySpecificKeyframeGroup* keyframe_group =
        keyframe_groups_->at(PropertyHandle(property));
    if (!keyframe_group)
      continue;
    for (auto& keyframe : keyframe_group->keyframes_) {
      if (keyframe->IsNeutral())
        updated |= keyframe->PopulateAnimatableValue(property, element,
                                                     new_style, parent_style);
    }
  }
  return updated;
}

bool KeyframeEffectModelBase::SnapshotAllCompositorKeyframes(
    Element& element,
    const ComputedStyle& base_style,
    const ComputedStyle* parent_style) const {
  needs_compositor_keyframes_snapshot_ = false;
  bool updated = false;
  bool has_neutral_compositable_keyframe = false;
  EnsureKeyframeGroups();
  for (CSSPropertyID property : CompositorAnimations::kCompositableProperties) {
    PropertySpecificKeyframeGroup* keyframe_group =
        keyframe_groups_->at(PropertyHandle(property));
    if (!keyframe_group)
      continue;
    for (auto& keyframe : keyframe_group->keyframes_) {
      updated |= keyframe->PopulateAnimatableValue(property, element,
                                                   base_style, parent_style);
      has_neutral_compositable_keyframe |= keyframe->IsNeutral();
    }
  }
  if (updated && has_neutral_compositable_keyframe) {
    UseCounter::Count(element.GetDocument(),
                      WebFeature::kSyntheticKeyframesInCompositedCSSAnimation);
  }
  return updated;
}

KeyframeEffectModelBase::KeyframeVector
KeyframeEffectModelBase::NormalizedKeyframes(const KeyframeVector& keyframes) {
  double last_offset = 0;
  KeyframeVector result;
  result.ReserveCapacity(keyframes.size());

  for (const auto& keyframe : keyframes) {
    double offset = keyframe->Offset();
    if (!IsNull(offset)) {
      DCHECK_GE(offset, 0);
      DCHECK_LE(offset, 1);
      DCHECK_GE(offset, last_offset);
      last_offset = offset;
    }
    result.push_back(keyframe->Clone());
  }

  if (result.IsEmpty())
    return result;

  if (IsNull(result.back()->Offset()))
    result.back()->SetOffset(1);

  if (result.size() > 1 && IsNull(result[0]->Offset()))
    result.front()->SetOffset(0);

  size_t last_index = 0;
  last_offset = result.front()->Offset();
  for (size_t i = 1; i < result.size(); ++i) {
    double offset = result[i]->Offset();
    if (!IsNull(offset)) {
      for (size_t j = 1; j < i - last_index; ++j)
        result[last_index + j]->SetOffset(
            last_offset + (offset - last_offset) * j / (i - last_index));
      last_index = i;
      last_offset = offset;
    }
  }

  return result;
}

bool KeyframeEffectModelBase::IsTransformRelatedEffect() const {
  return Affects(PropertyHandle(CSSPropertyTransform)) ||
         Affects(PropertyHandle(CSSPropertyRotate)) ||
         Affects(PropertyHandle(CSSPropertyScale)) ||
         Affects(PropertyHandle(CSSPropertyTranslate));
}

void KeyframeEffectModelBase::EnsureKeyframeGroups() const {
  if (keyframe_groups_)
    return;

  keyframe_groups_ = WTF::WrapUnique(new KeyframeGroupMap);
  RefPtr<TimingFunction> zero_offset_easing = default_keyframe_easing_;
  for (const auto& keyframe : NormalizedKeyframes(GetFrames())) {
    if (keyframe->Offset() == 0)
      zero_offset_easing = &keyframe->Easing();

    for (const PropertyHandle& property : keyframe->Properties()) {
      KeyframeGroupMap::iterator group_iter = keyframe_groups_->find(property);
      PropertySpecificKeyframeGroup* group;
      if (group_iter == keyframe_groups_->end()) {
        group = keyframe_groups_
                    ->insert(property,
                             WTF::WrapUnique(new PropertySpecificKeyframeGroup))
                    .stored_value->value.get();
      } else {
        group = group_iter->value.get();
      }

      group->AppendKeyframe(keyframe->CreatePropertySpecificKeyframe(property));
    }
  }

  // Add synthetic keyframes.
  has_synthetic_keyframes_ = false;
  for (const auto& entry : *keyframe_groups_) {
    if (entry.value->AddSyntheticKeyframeIfRequired(zero_offset_easing))
      has_synthetic_keyframes_ = true;

    entry.value->RemoveRedundantKeyframes();
  }
}

void KeyframeEffectModelBase::EnsureInterpolationEffectPopulated() const {
  if (interpolation_effect_.IsPopulated())
    return;

  for (const auto& entry : *keyframe_groups_) {
    const PropertySpecificKeyframeVector& keyframes = entry.value->Keyframes();
    for (size_t i = 0; i < keyframes.size() - 1; i++) {
      size_t start_index = i;
      size_t end_index = i + 1;
      double start_offset = keyframes[start_index]->Offset();
      double end_offset = keyframes[end_index]->Offset();
      double apply_from = start_offset;
      double apply_to = end_offset;

      if (i == 0) {
        apply_from = -std::numeric_limits<double>::infinity();
        DCHECK_EQ(start_offset, 0.0);
        if (end_offset == 0.0) {
          DCHECK_NE(keyframes[end_index + 1]->Offset(), 0.0);
          end_index = start_index;
        }
      }
      if (i == keyframes.size() - 2) {
        apply_to = std::numeric_limits<double>::infinity();
        DCHECK_EQ(end_offset, 1.0);
        if (start_offset == 1.0) {
          DCHECK_NE(keyframes[start_index - 1]->Offset(), 1.0);
          start_index = end_index;
        }
      }

      if (apply_from != apply_to) {
        interpolation_effect_.AddInterpolationsFromKeyframes(
            entry.key, *keyframes[start_index], *keyframes[end_index],
            apply_from, apply_to);
      }
      // else the interpolation will never be used in sampling
    }
  }

  interpolation_effect_.SetPopulated();
}

bool KeyframeEffectModelBase::IsReplaceOnly() {
  EnsureKeyframeGroups();
  for (const auto& entry : *keyframe_groups_) {
    for (const auto& keyframe : entry.value->Keyframes()) {
      if (keyframe->Composite() != EffectModel::kCompositeReplace)
        return false;
    }
  }
  return true;
}

Keyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(
    double offset,
    RefPtr<TimingFunction> easing,
    EffectModel::CompositeOperation composite)
    : offset_(offset), easing_(std::move(easing)), composite_(composite) {
  DCHECK(!IsNull(offset));
}

void KeyframeEffectModelBase::PropertySpecificKeyframeGroup::AppendKeyframe(
    RefPtr<Keyframe::PropertySpecificKeyframe> keyframe) {
  DCHECK(keyframes_.IsEmpty() ||
         keyframes_.back()->Offset() <= keyframe->Offset());
  keyframes_.push_back(std::move(keyframe));
}

void KeyframeEffectModelBase::PropertySpecificKeyframeGroup::
    RemoveRedundantKeyframes() {
  // As an optimization, removes interior keyframes that have the same offset
  // as both their neighbours, as they will never be used by sample().
  // Note that synthetic keyframes must be added before this method is
  // called.
  DCHECK_GE(keyframes_.size(), 2U);
  for (int i = keyframes_.size() - 2; i > 0; --i) {
    double offset = keyframes_[i]->Offset();
    bool has_same_offset_as_previous_neighbor =
        keyframes_[i - 1]->Offset() == offset;
    bool has_same_offset_as_next_neighbor =
        keyframes_[i + 1]->Offset() == offset;
    if (has_same_offset_as_previous_neighbor &&
        has_same_offset_as_next_neighbor)
      keyframes_.erase(i);
  }
  DCHECK_GE(keyframes_.size(), 2U);
}

bool KeyframeEffectModelBase::PropertySpecificKeyframeGroup::
    AddSyntheticKeyframeIfRequired(RefPtr<TimingFunction> zero_offset_easing) {
  DCHECK(!keyframes_.IsEmpty());

  bool added_synthetic_keyframe = false;

  if (keyframes_.front()->Offset() != 0.0) {
    keyframes_.insert(0, keyframes_.front()->NeutralKeyframe(
                             0, std::move(zero_offset_easing)));
    added_synthetic_keyframe = true;
  }
  if (keyframes_.back()->Offset() != 1.0) {
    AppendKeyframe(keyframes_.back()->NeutralKeyframe(1, nullptr));
    added_synthetic_keyframe = true;
  }

  return added_synthetic_keyframe;
}

}  // namespace blink

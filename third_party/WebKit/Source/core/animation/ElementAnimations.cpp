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

#include "core/animation/ElementAnimations.h"

#include "core/style/ComputedStyle.h"

namespace blink {

ElementAnimations::ElementAnimations() : animation_style_change_(false) {}

ElementAnimations::~ElementAnimations() {}

void ElementAnimations::UpdateAnimationFlags(ComputedStyle& style) {
  for (const auto& entry : animations_) {
    const Animation& animation = *entry.key;
    DCHECK(animation.effect());
    // FIXME: Needs to consider AnimationGroup once added.
    DCHECK(animation.effect()->IsKeyframeEffectReadOnly());
    const KeyframeEffectReadOnly& effect =
        *ToKeyframeEffectReadOnly(animation.effect());
    if (effect.IsCurrent()) {
      if (effect.Affects(PropertyHandle(CSSPropertyOpacity)))
        style.SetHasCurrentOpacityAnimation(true);
      if (effect.Affects(PropertyHandle(CSSPropertyTransform)) ||
          effect.Affects(PropertyHandle(CSSPropertyRotate)) ||
          effect.Affects(PropertyHandle(CSSPropertyScale)) ||
          effect.Affects(PropertyHandle(CSSPropertyTranslate)))
        style.SetHasCurrentTransformAnimation(true);
      if (effect.Affects(PropertyHandle(CSSPropertyFilter)))
        style.SetHasCurrentFilterAnimation(true);
      if (effect.Affects(PropertyHandle(CSSPropertyBackdropFilter)))
        style.SetHasCurrentBackdropFilterAnimation(true);
    }
  }

  if (style.HasCurrentOpacityAnimation()) {
    style.SetIsRunningOpacityAnimationOnCompositor(
        effect_stack_.HasActiveAnimationsOnCompositor(
            PropertyHandle(CSSPropertyOpacity)));
  }
  if (style.HasCurrentTransformAnimation()) {
    style.SetIsRunningTransformAnimationOnCompositor(
        effect_stack_.HasActiveAnimationsOnCompositor(
            PropertyHandle(CSSPropertyTransform)));
  }
  if (style.HasCurrentFilterAnimation()) {
    style.SetIsRunningFilterAnimationOnCompositor(
        effect_stack_.HasActiveAnimationsOnCompositor(
            PropertyHandle(CSSPropertyFilter)));
  }
  if (style.HasCurrentBackdropFilterAnimation()) {
    style.SetIsRunningBackdropFilterAnimationOnCompositor(
        effect_stack_.HasActiveAnimationsOnCompositor(
            PropertyHandle(CSSPropertyBackdropFilter)));
  }
}

void ElementAnimations::RestartAnimationOnCompositor() {
  for (const auto& entry : animations_)
    entry.key->RestartAnimationOnCompositor();
}

DEFINE_TRACE(ElementAnimations) {
  visitor->Trace(css_animations_);
  visitor->Trace(custom_compositor_animations_);
  visitor->Trace(effect_stack_);
  visitor->Trace(animations_);
}

const ComputedStyle* ElementAnimations::BaseComputedStyle() const {
#if !DCHECK_IS_ON()
  if (IsAnimationStyleChange())
    return base_computed_style_.Get();
#endif
  return nullptr;
}

void ElementAnimations::UpdateBaseComputedStyle(
    const ComputedStyle* computed_style) {
  if (!IsAnimationStyleChange()) {
    base_computed_style_ = nullptr;
    return;
  }
#if DCHECK_IS_ON()
  if (base_computed_style_ && computed_style)
    DCHECK(*base_computed_style_ == *computed_style);
#endif
  base_computed_style_ = ComputedStyle::Clone(*computed_style);
}

void ElementAnimations::ClearBaseComputedStyle() {
  base_computed_style_ = nullptr;
}

bool ElementAnimations::IsAnimationStyleChange() const {
  // TODO(rune@opera.com): The FontFaceCache version number may be increased
  // without forcing a style recalc (see crbug.com/471079). ComputedStyle
  // objects created with different cache versions will not be considered equal
  // as Font::operator== will compare versions, hence ComputedStyle::operator==
  // will return false. We avoid using baseComputedStyle (the check for
  // isFallbackValid()) in that case to avoid triggering the ComputedStyle
  // comparison ASSERT in updateBaseComputedStyle.
  return animation_style_change_ &&
         (!base_computed_style_ ||
          base_computed_style_->GetFont().IsFallbackValid());
}

}  // namespace blink

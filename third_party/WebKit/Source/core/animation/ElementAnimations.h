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

#ifndef ElementAnimations_h
#define ElementAnimations_h

#include "core/animation/EffectStack.h"
#include "core/animation/css/CSSAnimations.h"
#include "platform/wtf/HashCountedSet.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class CSSAnimations;

using AnimationCountedSet = HeapHashCountedSet<WeakMember<Animation>>;

class ElementAnimations : public GarbageCollectedFinalized<ElementAnimations> {
  WTF_MAKE_NONCOPYABLE(ElementAnimations);

 public:
  ElementAnimations();
  ~ElementAnimations();

  // Animations that are currently active for this element, their effects will
  // be applied during a style recalc. CSS Transitions are included in this
  // stack.
  EffectStack& GetEffectStack() { return effect_stack_; }
  const EffectStack& GetEffectStack() const { return effect_stack_; }
  // Tracks the state of active CSS Animations and Transitions. The individual
  // animations will also be part of the animation stack, but the mapping
  // between animation name and animation is kept here.
  CSSAnimations& CssAnimations() { return css_animations_; }
  const CSSAnimations& CssAnimations() const { return css_animations_; }

  // Animations which have effects targeting this element.
  AnimationCountedSet& Animations() { return animations_; }

  bool IsEmpty() const {
    return effect_stack_.IsEmpty() && css_animations_.IsEmpty() &&
           animations_.IsEmpty();
  }

  void RestartAnimationOnCompositor();

  void UpdateAnimationFlags(ComputedStyle&);
  void SetAnimationStyleChange(bool animation_style_change) {
    animation_style_change_ = animation_style_change;
  }

  const ComputedStyle* BaseComputedStyle() const;
  void UpdateBaseComputedStyle(const ComputedStyle*);
  void ClearBaseComputedStyle();

  DECLARE_TRACE();

 private:
  bool IsAnimationStyleChange() const;

  EffectStack effect_stack_;
  CSSAnimations css_animations_;
  AnimationCountedSet animations_;
  bool animation_style_change_;
  RefPtr<ComputedStyle> base_computed_style_;

  // CSSAnimations checks if a style change is due to animation.
  friend class CSSAnimations;
};

}  // namespace blink

#endif  // ElementAnimations_h

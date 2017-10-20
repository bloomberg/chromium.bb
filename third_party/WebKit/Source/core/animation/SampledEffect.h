// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SampledEffect_h
#define SampledEffect_h

#include "core/animation/Animation.h"
#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffectReadOnly.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

// Associates the results of sampling an EffectModel with metadata used for
// effect ordering and managing composited animations.
class SampledEffect : public GarbageCollectedFinalized<SampledEffect> {
  WTF_MAKE_NONCOPYABLE(SampledEffect);

 public:
  static SampledEffect* Create(KeyframeEffectReadOnly* animation) {
    return new SampledEffect(animation);
  }

  void Clear();

  const Vector<scoped_refptr<Interpolation>>& Interpolations() const {
    return interpolations_;
  }
  Vector<scoped_refptr<Interpolation>>& MutableInterpolations() {
    return interpolations_;
  }

  KeyframeEffectReadOnly* Effect() const { return effect_; }
  unsigned SequenceNumber() const { return sequence_number_; }
  KeyframeEffectReadOnly::Priority GetPriority() const { return priority_; }
  bool WillNeverChange() const;
  void RemoveReplacedInterpolations(const HashSet<PropertyHandle>&);
  void UpdateReplacedProperties(HashSet<PropertyHandle>&);

  void Trace(blink::Visitor*);

 private:
  SampledEffect(KeyframeEffectReadOnly*);

  WeakMember<KeyframeEffectReadOnly> effect_;
  Vector<scoped_refptr<Interpolation>> interpolations_;
  const unsigned sequence_number_;
  KeyframeEffectReadOnly::Priority priority_;
};

}  // namespace blink

#endif  // SampledEffect_h

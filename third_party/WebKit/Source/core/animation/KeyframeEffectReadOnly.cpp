// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/KeyframeEffectReadOnly.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/animation/EffectInput.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectOptions.h"
#include "core/animation/SampledEffect.h"
#include "core/animation/TimingInput.h"
#include "core/dom/Element.h"
#include "core/frame/UseCounter.h"

namespace blink {

// Web Animations API Bindings constructors. We never actually want to create a
// KeyframeEffectReadOnly object, we just want to provide a read-only interface
// to the KeyframeEffect object, so pass straight through to the subclass
// factory methods.
KeyframeEffectReadOnly* KeyframeEffectReadOnly::create(
    ExecutionContext* executionContext,
    Element* element,
    const DictionarySequenceOrDictionary& effectInput,
    double duration,
    ExceptionState& exceptionState) {
  return KeyframeEffect::create(executionContext, element, effectInput,
                                duration, exceptionState);
}

KeyframeEffectReadOnly* KeyframeEffectReadOnly::create(
    ExecutionContext* executionContext,
    Element* element,
    const DictionarySequenceOrDictionary& effectInput,
    const KeyframeEffectOptions& timingInput,
    ExceptionState& exceptionState) {
  return KeyframeEffect::create(executionContext, element, effectInput,
                                timingInput, exceptionState);
}

KeyframeEffectReadOnly* KeyframeEffectReadOnly::create(
    ExecutionContext* executionContext,
    Element* element,
    const DictionarySequenceOrDictionary& effectInput,
    ExceptionState& exceptionState) {
  return KeyframeEffect::create(executionContext, element, effectInput,
                                exceptionState);
}

KeyframeEffectReadOnly::KeyframeEffectReadOnly(Element* target,
                                               EffectModel* model,
                                               const Timing& timing,
                                               Priority priority,
                                               EventDelegate* eventDelegate)
    : AnimationEffectReadOnly(timing, eventDelegate),
      m_target(target),
      m_model(model),
      m_sampledEffect(nullptr),
      m_priority(priority) {}

DEFINE_TRACE(KeyframeEffectReadOnly) {
  visitor->trace(m_target);
  visitor->trace(m_model);
  visitor->trace(m_sampledEffect);
  AnimationEffectReadOnly::trace(visitor);
}

}  // namespace blink

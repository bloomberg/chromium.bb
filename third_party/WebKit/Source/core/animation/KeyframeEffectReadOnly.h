// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KeyframeEffectReadOnly_h
#define KeyframeEffectReadOnly_h

#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/EffectModel.h"

namespace blink {

class KeyframeEffectOptions;
class DictionarySequenceOrDictionary;
class Element;
class ExceptionState;
class ExecutionContext;
class SampledEffect;

// Represents the effect of an Animation on an Element's properties.
// http://w3c.github.io/web-animations/#the-keyframeeffect-interfaces
class CORE_EXPORT KeyframeEffectReadOnly : public AnimationEffectReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Priority { DefaultPriority, TransitionPriority };

  static KeyframeEffectReadOnly* create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effectInput,
      double duration,
      ExceptionState&);
  static KeyframeEffectReadOnly* create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effectInput,
      const KeyframeEffectOptions& timingInput,
      ExceptionState&);
  static KeyframeEffectReadOnly* create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effectInput,
      ExceptionState&);

  ~KeyframeEffectReadOnly() override {}

  bool isKeyframeEffectReadOnly() const override { return true; }

  Priority getPriority() const { return m_priority; }
  void downgradeToNormal() { m_priority = DefaultPriority; }

  DECLARE_VIRTUAL_TRACE();

 protected:
  KeyframeEffectReadOnly(Element*,
                         EffectModel*,
                         const Timing&,
                         Priority,
                         EventDelegate*);

  Member<Element> m_target;
  Member<EffectModel> m_model;
  Member<SampledEffect> m_sampledEffect;

  Priority m_priority;
};

// TODO(suzyh): Replace calls to toKeyframeEffect with toKeyframeEffectReadOnly
// where possible
DEFINE_TYPE_CASTS(KeyframeEffectReadOnly,
                  AnimationEffectReadOnly,
                  animationNode,
                  animationNode->isKeyframeEffectReadOnly(),
                  animationNode.isKeyframeEffectReadOnly());

}  // namespace blink

#endif  // KeyframeEffectReadOnly_h

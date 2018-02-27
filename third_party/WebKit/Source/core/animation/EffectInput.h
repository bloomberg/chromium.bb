// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EffectInput_h
#define EffectInput_h

#include "core/CoreExport.h"
#include "core/animation/EffectModel.h"
#include "core/animation/KeyframeEffectModel.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Element;
class ExceptionState;
class ScriptState;
class ScriptValue;

class CORE_EXPORT EffectInput {
  STATIC_ONLY(EffectInput);

 public:
  static KeyframeEffectModelBase* Convert(Element*,
                                          const ScriptValue& keyframes,
                                          EffectModel::CompositeOperation,
                                          ScriptState*,
                                          ExceptionState&);

  // Implements "Processing a keyframes argument" from the web-animations spec.
  // https://drafts.csswg.org/web-animations/#processing-a-keyframes-argument
  static StringKeyframeVector ParseKeyframesArgument(
      Element*,
      const ScriptValue& keyframes,
      ScriptState*,
      ExceptionState&);

  // Ensures that a CompositeOperation is of an allowed value for a set of
  // StringKeyframes and the current runtime flags.
  //
  // Under certain runtime flags, additive composite operations are not allowed
  // for CSS properties.
  static EffectModel::CompositeOperation ResolveCompositeOperation(
      EffectModel::CompositeOperation,
      const StringKeyframeVector&);
};

}  // namespace blink

#endif

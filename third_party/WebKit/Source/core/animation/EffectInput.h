// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EffectInput_h
#define EffectInput_h

#include "core/CoreExport.h"
#include "core/animation/EffectModel.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

class KeyframeEffectModelBase;
class DictionarySequenceOrDictionary;
class Dictionary;
class Element;
class ExceptionState;
class ExecutionContext;

class CORE_EXPORT EffectInput {
  STATIC_ONLY(EffectInput);

 public:
  // TODO(alancutter): Replace Element* parameter with Document&.
  static KeyframeEffectModelBase* Convert(Element*,
                                          const DictionarySequenceOrDictionary&,
                                          EffectModel::CompositeOperation,
                                          ExecutionContext*,
                                          ExceptionState&);

 private:
  static KeyframeEffectModelBase* ConvertArrayForm(
      Element&,
      const Vector<Dictionary>& keyframes,
      EffectModel::CompositeOperation,
      ExecutionContext*,
      ExceptionState&);
  static KeyframeEffectModelBase* ConvertObjectForm(
      Element&,
      const Dictionary& keyframe,
      EffectModel::CompositeOperation,
      ExecutionContext*,
      ExceptionState&);
};

}  // namespace blink

#endif

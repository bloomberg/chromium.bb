// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Maplike_h
#define Maplike_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8ForCore.h"

namespace blink {

template <typename KeyType, typename ValueType>
class Maplike : public PairIterable<KeyType, ValueType> {
 public:
  bool hasForBinding(ScriptState* script_state,
                     const KeyType& key,
                     ExceptionState& exception_state) {
    ValueType value;
    return GetMapEntry(script_state, key, value, exception_state);
  }

  ScriptValue getForBinding(ScriptState* script_state,
                            const KeyType& key,
                            ExceptionState& exception_state) {
    ValueType value;
    if (GetMapEntry(script_state, key, value, exception_state))
      return ScriptValue(script_state,
                         ToV8(value, script_state->GetContext()->Global(),
                              script_state->GetIsolate()));
    return ScriptValue(script_state, v8::Undefined(script_state->GetIsolate()));
  }

 private:
  virtual bool GetMapEntry(ScriptState*,
                           const KeyType&,
                           ValueType&,
                           ExceptionState&) = 0;
};

}  // namespace blink

#endif  // Maplike_h

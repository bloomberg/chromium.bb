// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ToV8ForCore_h
#define ToV8ForCore_h

// ToV8() provides C++ -> V8 conversion. Note that ToV8() can return an empty
// handle. Call sites must check IsEmpty() before using return value.

#include "bindings/core/v8/IDLDictionaryBase.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8NodeFilterCondition.h"
#include "core/dom/Node.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "platform/bindings/ToV8.h"
#include "v8/include/v8.h"

namespace blink {

class Dictionary;
class DOMWindow;
class EventTarget;

CORE_EXPORT v8::Local<v8::Value> ToV8(DOMWindow*,
                                      v8::Local<v8::Object> creation_context,
                                      v8::Isolate*);
CORE_EXPORT v8::Local<v8::Value> ToV8(EventTarget*,
                                      v8::Local<v8::Object> creation_context,
                                      v8::Isolate*);
inline v8::Local<v8::Value> ToV8(Node* node,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8(static_cast<ScriptWrappable*>(node), creation_context, isolate);
}

inline v8::Local<v8::Value> ToV8(const Dictionary& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  NOTREACHED();
  return v8::Undefined(isolate);
}

template <typename T>
inline v8::Local<v8::Value> ToV8(NotShared<T> value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return ToV8(value.View(), creation_context, isolate);
}

inline v8::Local<v8::Value> ToV8(const V8NodeFilterCondition* value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return value ? value->Callback(isolate) : v8::Null(isolate).As<v8::Value>();
}

// Dictionary

inline v8::Local<v8::Value> ToV8(const IDLDictionaryBase& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  return value.ToV8Impl(creation_context, isolate);
}

// ScriptValue

inline v8::Local<v8::Value> ToV8(const ScriptValue& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  if (value.IsEmpty())
    return v8::Undefined(isolate);
  return value.V8Value();
}

// Cannot define in ScriptValue because of the circular dependency between toV8
// and ScriptValue
template <typename T>
inline ScriptValue ScriptValue::From(ScriptState* script_state, T&& value) {
  return ScriptValue(script_state, ToV8(std::forward<T>(value), script_state));
}

}  // namespace blink

#endif  // ToV8ForCore_h

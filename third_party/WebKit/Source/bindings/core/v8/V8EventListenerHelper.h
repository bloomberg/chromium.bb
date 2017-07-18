/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef V8EventListenerHelper_h
#define V8EventListenerHelper_h

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8EventListener.h"
#include "core/CoreExport.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/Allocator.h"
#include "v8/include/v8.h"

namespace blink {

enum ListenerLookupType {
  kListenerFindOnly,
  kListenerFindOrCreate,
};

// This is a container for V8EventListener objects that uses hidden properties
// of v8::Object to speed up lookups.
class V8EventListenerHelper {
  STATIC_ONLY(V8EventListenerHelper);

 public:
  static V8EventListener* ExistingEventListener(v8::Local<v8::Value> value,
                                                ScriptState* script_state) {
    DCHECK(script_state->GetIsolate()->InContext());
    if (!value->IsObject())
      return nullptr;

    return FindEventListener(v8::Local<v8::Object>::Cast(value), false,
                             script_state);
  }

  template <typename ListenerType>
  static V8EventListener* EnsureEventListener(v8::Local<v8::Value>,
                                              bool is_attribute,
                                              ScriptState*);

  CORE_EXPORT static EventListener* GetEventListener(ScriptState*,
                                                     v8::Local<v8::Value>,
                                                     bool is_attribute,
                                                     ListenerLookupType);

 private:
  static V8EventListener* FindEventListener(v8::Local<v8::Object> object,
                                            bool is_attribute,
                                            ScriptState* script_state) {
    v8::Isolate* isolate = script_state->GetIsolate();
    v8::HandleScope scope(isolate);
    DCHECK(isolate->InContext());

    v8::Local<v8::Value> listener =
        ListenerProperty(isolate, is_attribute).GetOrEmpty(object);
    if (listener.IsEmpty())
      return nullptr;
    return static_cast<V8EventListener*>(
        v8::External::Cast(*listener)->Value());
  }

  static V8PrivateProperty::Symbol ListenerProperty(v8::Isolate* isolate,
                                                    bool is_attribute) {
    return is_attribute
               ? V8PrivateProperty::GetV8EventListenerAttributeListener(isolate)
               : V8PrivateProperty::GetV8EventListenerListener(isolate);
  }
};

template <typename ListenerType>
V8EventListener* V8EventListenerHelper::EnsureEventListener(
    v8::Local<v8::Value> value,
    bool is_attribute,
    ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  DCHECK(isolate->InContext());
  if (!value->IsObject())
    return nullptr;

  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);

  V8EventListener* listener =
      FindEventListener(object, is_attribute, script_state);
  if (listener)
    return listener;

  listener = ListenerType::Create(object, is_attribute, script_state);
  if (listener) {
    ListenerProperty(isolate, is_attribute)
        .Set(object, v8::External::New(isolate, listener));
  }

  return listener;
}

}  // namespace blink

#endif  // V8EventListenerHelper_h

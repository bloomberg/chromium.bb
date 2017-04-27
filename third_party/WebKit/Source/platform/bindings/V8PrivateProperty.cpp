// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/V8PrivateProperty.h"

#include "platform/bindings/ScriptState.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/V8Binding.h"
#include "platform/bindings/V8BindingMacros.h"

namespace blink {

v8::Local<v8::Value> V8PrivateProperty::Symbol::GetFromMainWorld(
    ScriptWrappable* script_wrappable) {
  v8::Local<v8::Object> wrapper = script_wrappable->MainWorldWrapper(isolate_);
  return wrapper.IsEmpty() ? v8::Local<v8::Value>() : GetOrEmpty(wrapper);
}

v8::Local<v8::Private> V8PrivateProperty::CreateV8Private(v8::Isolate* isolate,
                                                          const char* symbol) {
  return v8::Private::New(isolate, V8String(isolate, symbol));
}

v8::Local<v8::Private> V8PrivateProperty::CreateCachedV8Private(
    v8::Isolate* isolate,
    const char* symbol) {
  // Use ForApi() to get the same Private symbol which is not cached in Chrome.
  return v8::Private::ForApi(isolate, V8String(isolate, symbol));
}

}  // namespace blink

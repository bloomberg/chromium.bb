// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8PrivateProperty.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"

namespace blink {

v8::Local<v8::Value> V8PrivateProperty::Symbol::getFromMainWorld(
    ScriptWrappable* scriptWrappable) {
  v8::Local<v8::Object> wrapper = scriptWrappable->mainWorldWrapper(m_isolate);
  return wrapper.IsEmpty() ? v8::Local<v8::Value>() : getOrEmpty(wrapper);
}

v8::Local<v8::Private> V8PrivateProperty::createV8Private(v8::Isolate* isolate,
                                                          const char* symbol) {
  return v8::Private::New(isolate, v8String(isolate, symbol));
}

v8::Local<v8::Private> V8PrivateProperty::createCachedV8Private(
    v8::Isolate* isolate,
    const char* symbol) {
  // Use ForApi() to get the same Private symbol which is not cached in Chrome.
  return v8::Private::ForApi(isolate, v8String(isolate, symbol));
}

}  // namespace blink

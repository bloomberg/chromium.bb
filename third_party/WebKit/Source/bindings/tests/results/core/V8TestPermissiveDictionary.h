// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/dictionary_v8.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef V8TestPermissiveDictionary_h
#define V8TestPermissiveDictionary_h

#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/tests/idls/core/TestPermissiveDictionary.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;

class V8TestPermissiveDictionary {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, TestPermissiveDictionary&, ExceptionState&);
};

CORE_EXPORT bool toV8TestPermissiveDictionary(const TestPermissiveDictionary&, v8::Local<v8::Object> dictionary, v8::Local<v8::Object> creationContext, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestPermissiveDictionary& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestPermissiveDictionary& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<TestPermissiveDictionary> : public NativeValueTraitsBase<TestPermissiveDictionary> {
  CORE_EXPORT static TestPermissiveDictionary NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<TestPermissiveDictionary> {
  typedef V8TestPermissiveDictionary Type;
};

}  // namespace blink

#endif  // V8TestPermissiveDictionary_h

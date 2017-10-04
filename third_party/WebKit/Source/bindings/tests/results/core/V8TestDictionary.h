// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/dictionary_v8.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef V8TestDictionary_h
#define V8TestDictionary_h

#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/tests/idls/core/TestDictionary.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;

class V8TestDictionary {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, TestDictionary&, ExceptionState&);
};

CORE_EXPORT bool toV8TestDictionary(const TestDictionary&, v8::Local<v8::Object> dictionary, v8::Local<v8::Object> creationContext, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestDictionary& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestDictionary& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<TestDictionary> : public NativeValueTraitsBase<TestDictionary> {
  CORE_EXPORT static TestDictionary NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<TestDictionary> {
  typedef V8TestDictionary Type;
};

}  // namespace blink

#endif  // V8TestDictionary_h

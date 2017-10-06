// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/callback_function.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#ifndef V8VoidCallbackFunctionDictionaryArg_h
#define V8VoidCallbackFunctionDictionaryArg_h

#include "bindings/core/v8/NativeValueTraits.h"
#include "core/CoreExport.h"
#include "platform/bindings/CallbackFunctionBase.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class TestDictionary;

class CORE_EXPORT V8VoidCallbackFunctionDictionaryArg final : public CallbackFunctionBase {
 public:
  static V8VoidCallbackFunctionDictionaryArg* Create(ScriptState*, v8::Local<v8::Value> callback);

  ~V8VoidCallbackFunctionDictionaryArg() = default;

  bool call(ScriptWrappable* scriptWrappable, const TestDictionary& arg);

 private:
  V8VoidCallbackFunctionDictionaryArg(ScriptState*, v8::Local<v8::Function>);
};

template <>
struct NativeValueTraits<V8VoidCallbackFunctionDictionaryArg> : public NativeValueTraitsBase<V8VoidCallbackFunctionDictionaryArg> {
  CORE_EXPORT static V8VoidCallbackFunctionDictionaryArg* NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

}  // namespace blink

#endif  // V8VoidCallbackFunctionDictionaryArg_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/callback_function.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#ifndef V8VoidCallbackFunctionModules_h
#define V8VoidCallbackFunctionModules_h

#include "bindings/core/v8/NativeValueTraits.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/CallbackFunctionBase.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class MODULES_EXPORT V8VoidCallbackFunctionModules final : public CallbackFunctionBase {
 public:
  static V8VoidCallbackFunctionModules* Create(ScriptState*, v8::Local<v8::Value> callback);

  ~V8VoidCallbackFunctionModules() = default;

  bool call(ScriptWrappable* scriptWrappable);

 private:
  V8VoidCallbackFunctionModules(ScriptState*, v8::Local<v8::Function>);
};

template <>
struct NativeValueTraits<V8VoidCallbackFunctionModules> : public NativeValueTraitsBase<V8VoidCallbackFunctionModules> {
  MODULES_EXPORT static V8VoidCallbackFunctionModules* NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

}  // namespace blink

#endif  // V8VoidCallbackFunctionModules_h

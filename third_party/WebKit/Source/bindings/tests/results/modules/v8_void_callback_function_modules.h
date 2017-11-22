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

#include "modules/ModulesExport.h"
#include "platform/bindings/CallbackFunctionBase.h"

namespace blink {

class ScriptWrappable;

class MODULES_EXPORT V8VoidCallbackFunctionModules final : public CallbackFunctionBase {
 public:
  static V8VoidCallbackFunctionModules* Create(v8::Local<v8::Function> callback_function) {
    return new V8VoidCallbackFunctionModules(callback_function);
  }

  ~V8VoidCallbackFunctionModules() override = default;

  // Performs "invoke".
  // https://heycam.github.io/webidl/#es-invoking-callback-functions
  v8::Maybe<void> Invoke(ScriptWrappable* callback_this_value) WARN_UNUSED_RESULT;

  // Performs "invoke", and then reports an exception, if any, to the global
  // error handler such as DevTools' console.
  void InvokeAndReportException(ScriptWrappable* callback_this_value);

 private:
  explicit V8VoidCallbackFunctionModules(v8::Local<v8::Function> callback_function)
      : CallbackFunctionBase(callback_function) {}
};

}  // namespace blink

#endif  // V8VoidCallbackFunctionModules_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/callback_function.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#ifndef V8StringSequenceCallbackFunctionLongSequenceArg_h
#define V8StringSequenceCallbackFunctionLongSequenceArg_h

#include "core/CoreExport.h"
#include "platform/bindings/CallbackFunctionBase.h"

namespace blink {

class ScriptWrappable;

class CORE_EXPORT V8StringSequenceCallbackFunctionLongSequenceArg final : public CallbackFunctionBase {
 public:
  static V8StringSequenceCallbackFunctionLongSequenceArg* Create(v8::Local<v8::Function> callback_function) {
    return new V8StringSequenceCallbackFunctionLongSequenceArg(callback_function);
  }

  ~V8StringSequenceCallbackFunctionLongSequenceArg() override = default;

  // Performs "invoke".
  // https://heycam.github.io/webidl/#es-invoking-callback-functions
  v8::Maybe<Vector<String>> Invoke(ScriptWrappable* callback_this_value, const Vector<int32_t>& arg) WARN_UNUSED_RESULT;

 private:
  explicit V8StringSequenceCallbackFunctionLongSequenceArg(v8::Local<v8::Function> callback_function)
      : CallbackFunctionBase(callback_function) {}
};

}  // namespace blink

#endif  // V8StringSequenceCallbackFunctionLongSequenceArg_h

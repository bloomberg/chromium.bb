// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/callback_function.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#ifndef V8VoidCallbackFunctionTestInterfaceSequenceArg_h
#define V8VoidCallbackFunctionTestInterfaceSequenceArg_h

#include "bindings/core/v8/NativeValueTraits.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ScriptState;
class TestInterfaceImplementation;

class CORE_EXPORT V8VoidCallbackFunctionTestInterfaceSequenceArg final : public GarbageCollectedFinalized<V8VoidCallbackFunctionTestInterfaceSequenceArg>, public TraceWrapperBase {
 public:
  static V8VoidCallbackFunctionTestInterfaceSequenceArg* Create(ScriptState*, v8::Local<v8::Value> callback);

  ~V8VoidCallbackFunctionTestInterfaceSequenceArg() = default;

  DEFINE_INLINE_TRACE() {}
  DECLARE_TRACE_WRAPPERS();

  bool call(ScriptWrappable* scriptWrappable, const HeapVector<Member<TestInterfaceImplementation>>& arg);

  v8::Local<v8::Function> v8Value(v8::Isolate* isolate) {
    return callback_.NewLocal(isolate);
  }

 private:
  V8VoidCallbackFunctionTestInterfaceSequenceArg(ScriptState*, v8::Local<v8::Function>);

  RefPtr<ScriptState> script_state_;
  TraceWrapperV8Reference<v8::Function> callback_;
};

template <>
struct NativeValueTraits<V8VoidCallbackFunctionTestInterfaceSequenceArg> : public NativeValueTraitsBase<V8VoidCallbackFunctionTestInterfaceSequenceArg> {
  CORE_EXPORT static V8VoidCallbackFunctionTestInterfaceSequenceArg* NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

}  // namespace blink

#endif  // V8VoidCallbackFunctionTestInterfaceSequenceArg_h

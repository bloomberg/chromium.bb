// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/callback_interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#ifndef V8TestCallbackInterface_h
#define V8TestCallbackInterface_h

#include "core/CoreExport.h"
#include "platform/bindings/CallbackInterfaceBase.h"

namespace blink {

class TestInterfaceEmpty;

class CORE_EXPORT V8TestCallbackInterface final : public CallbackInterfaceBase {
 public:
  static V8TestCallbackInterface* Create(v8::Local<v8::Object> callback_object) {
    return new V8TestCallbackInterface(callback_object);
  }

  ~V8TestCallbackInterface() override = default;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethod(ScriptWrappable* callback_this_value) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<bool> booleanMethod(ScriptWrappable* callback_this_value) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethodBooleanArg(ScriptWrappable* callback_this_value, bool boolArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethodSequenceArg(ScriptWrappable* callback_this_value, const HeapVector<Member<TestInterfaceEmpty>>& sequenceArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethodFloatArg(ScriptWrappable* callback_this_value, float floatArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethodTestInterfaceEmptyArg(ScriptWrappable* callback_this_value, TestInterfaceEmpty* testInterfaceEmptyArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethodTestInterfaceEmptyStringArg(ScriptWrappable* callback_this_value, TestInterfaceEmpty* testInterfaceEmptyArg, const String& stringArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> callbackWithThisValueVoidMethodStringArg(ScriptWrappable* callback_this_value, const String& stringArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> customVoidMethodTestInterfaceEmptyArg(ScriptWrappable* callback_this_value, TestInterfaceEmpty* testInterfaceEmptyArg) WARN_UNUSED_RESULT;

 private:
  explicit V8TestCallbackInterface(v8::Local<v8::Object> callback_object)
      : CallbackInterfaceBase(callback_object, kNotSingleOperation) {}
};

}  // namespace blink

#endif  // V8TestCallbackInterface_h

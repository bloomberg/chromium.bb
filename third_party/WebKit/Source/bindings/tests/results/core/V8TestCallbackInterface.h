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

#include "bindings/tests/idls/core/TestCallbackInterface.h"
#include "core/CoreExport.h"
#include "platform/bindings/CallbackInterfaceBase.h"

namespace blink {

class CORE_EXPORT V8TestCallbackInterface final
    : public TestCallbackInterface,
      public CallbackInterfaceBase {
 public:
  static V8TestCallbackInterface* Create(v8::Local<v8::Object> callback_object) {
    return new V8TestCallbackInterface(callback_object);
  }

  ~V8TestCallbackInterface() override = default;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  void voidMethod() override;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  bool booleanMethod() override;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  void voidMethodBooleanArg(bool boolArg) override;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  void voidMethodSequenceArg(const HeapVector<Member<TestInterfaceEmpty>>& sequenceArg) override;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  void voidMethodFloatArg(float floatArg) override;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  void voidMethodTestInterfaceEmptyArg(TestInterfaceEmpty* testInterfaceEmptyArg) override;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  void voidMethodTestInterfaceEmptyStringArg(TestInterfaceEmpty* testInterfaceEmptyArg, const String& stringArg) override;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  void callbackWithThisValueVoidMethodStringArg(ScriptValue thisValue, const String& stringArg) override;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  void customVoidMethodTestInterfaceEmptyArg(TestInterfaceEmpty* testInterfaceEmptyArg) override;

 private:
  explicit V8TestCallbackInterface(v8::Local<v8::Object> callback_object)
      : CallbackInterfaceBase(callback_object, kNotSingleOperation) {}
};

}  // namespace blink

#endif  // V8TestCallbackInterface_h

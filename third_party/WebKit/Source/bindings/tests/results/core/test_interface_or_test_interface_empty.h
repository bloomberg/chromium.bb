// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef TestInterfaceOrTestInterfaceEmpty_h
#define TestInterfaceOrTestInterfaceEmpty_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class TestInterfaceEmpty;
class TestInterfaceImplementation;

class CORE_EXPORT TestInterfaceOrTestInterfaceEmpty final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  TestInterfaceOrTestInterfaceEmpty();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsTestInterface() const { return type_ == SpecificType::kTestInterface; }
  TestInterfaceImplementation* GetAsTestInterface() const;
  void SetTestInterface(TestInterfaceImplementation*);
  static TestInterfaceOrTestInterfaceEmpty FromTestInterface(TestInterfaceImplementation*);

  bool IsTestInterfaceEmpty() const { return type_ == SpecificType::kTestInterfaceEmpty; }
  TestInterfaceEmpty* GetAsTestInterfaceEmpty() const;
  void SetTestInterfaceEmpty(TestInterfaceEmpty*);
  static TestInterfaceOrTestInterfaceEmpty FromTestInterfaceEmpty(TestInterfaceEmpty*);

  TestInterfaceOrTestInterfaceEmpty(const TestInterfaceOrTestInterfaceEmpty&);
  ~TestInterfaceOrTestInterfaceEmpty();
  TestInterfaceOrTestInterfaceEmpty& operator=(const TestInterfaceOrTestInterfaceEmpty&);
  DECLARE_TRACE();

 private:
  enum class SpecificType {
    kNone,
    kTestInterface,
    kTestInterfaceEmpty,
  };
  SpecificType type_;

  Member<TestInterfaceImplementation> test_interface_;
  Member<TestInterfaceEmpty> test_interface_empty_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const TestInterfaceOrTestInterfaceEmpty&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8TestInterfaceOrTestInterfaceEmpty final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, TestInterfaceOrTestInterfaceEmpty&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const TestInterfaceOrTestInterfaceEmpty&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestInterfaceOrTestInterfaceEmpty& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestInterfaceOrTestInterfaceEmpty& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<TestInterfaceOrTestInterfaceEmpty> : public NativeValueTraitsBase<TestInterfaceOrTestInterfaceEmpty> {
  CORE_EXPORT static TestInterfaceOrTestInterfaceEmpty NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<TestInterfaceOrTestInterfaceEmpty> {
  typedef V8TestInterfaceOrTestInterfaceEmpty Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::TestInterfaceOrTestInterfaceEmpty);

#endif  // TestInterfaceOrTestInterfaceEmpty_h

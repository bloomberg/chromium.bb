// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef TestInterfaceOrLong_h
#define TestInterfaceOrLong_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class TestInterfaceImplementation;

class CORE_EXPORT TestInterfaceOrLong final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  TestInterfaceOrLong();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsLong() const { return type_ == SpecificType::kLong; }
  int32_t GetAsLong() const;
  void SetLong(int32_t);
  static TestInterfaceOrLong FromLong(int32_t);

  bool IsTestInterface() const { return type_ == SpecificType::kTestInterface; }
  TestInterfaceImplementation* GetAsTestInterface() const;
  void SetTestInterface(TestInterfaceImplementation*);
  static TestInterfaceOrLong FromTestInterface(TestInterfaceImplementation*);

  TestInterfaceOrLong(const TestInterfaceOrLong&);
  ~TestInterfaceOrLong();
  TestInterfaceOrLong& operator=(const TestInterfaceOrLong&);
  DECLARE_TRACE();

 private:
  enum class SpecificType {
    kNone,
    kLong,
    kTestInterface,
  };
  SpecificType type_;

  int32_t long_;
  Member<TestInterfaceImplementation> test_interface_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const TestInterfaceOrLong&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8TestInterfaceOrLong final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, TestInterfaceOrLong&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const TestInterfaceOrLong&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestInterfaceOrLong& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestInterfaceOrLong& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<TestInterfaceOrLong> : public NativeValueTraitsBase<TestInterfaceOrLong> {
  CORE_EXPORT static TestInterfaceOrLong NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<TestInterfaceOrLong> {
  typedef V8TestInterfaceOrLong Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::TestInterfaceOrLong);

#endif  // TestInterfaceOrLong_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef TestEnumOrTestEnumSequence_h
#define TestEnumOrTestEnumSequence_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Optional.h"

namespace blink {

class CORE_EXPORT TestEnumOrTestEnumSequence final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  TestEnumOrTestEnumSequence();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsTestEnum() const { return type_ == SpecificType::kTestEnum; }
  const String& GetAsTestEnum() const;
  void SetTestEnum(const String&);
  static TestEnumOrTestEnumSequence FromTestEnum(const String&);

  bool IsTestEnumSequence() const { return type_ == SpecificType::kTestEnumSequence; }
  const Vector<String>& GetAsTestEnumSequence() const;
  void SetTestEnumSequence(const Vector<String>&);
  static TestEnumOrTestEnumSequence FromTestEnumSequence(const Vector<String>&);

  TestEnumOrTestEnumSequence(const TestEnumOrTestEnumSequence&);
  ~TestEnumOrTestEnumSequence();
  TestEnumOrTestEnumSequence& operator=(const TestEnumOrTestEnumSequence&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kTestEnum,
    kTestEnumSequence,
  };
  SpecificType type_;

  String test_enum_;
  Vector<String> test_enum_sequence_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const TestEnumOrTestEnumSequence&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8TestEnumOrTestEnumSequence final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, TestEnumOrTestEnumSequence&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const TestEnumOrTestEnumSequence&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestEnumOrTestEnumSequence& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestEnumOrTestEnumSequence& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<TestEnumOrTestEnumSequence> : public NativeValueTraitsBase<TestEnumOrTestEnumSequence> {
  CORE_EXPORT static TestEnumOrTestEnumSequence NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static TestEnumOrTestEnumSequence NullValue() { return TestEnumOrTestEnumSequence(); }
};

template <>
struct V8TypeOf<TestEnumOrTestEnumSequence> {
  typedef V8TestEnumOrTestEnumSequence Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::TestEnumOrTestEnumSequence);

#endif  // TestEnumOrTestEnumSequence_h

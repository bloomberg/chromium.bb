// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef LongOrTestDictionary_h
#define LongOrTestDictionary_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8TestDictionary.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT LongOrTestDictionary final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  LongOrTestDictionary();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsLong() const { return type_ == SpecificType::kLong; }
  int32_t GetAsLong() const;
  void SetLong(int32_t);
  static LongOrTestDictionary FromLong(int32_t);

  bool IsTestDictionary() const { return type_ == SpecificType::kTestDictionary; }
  const TestDictionary& GetAsTestDictionary() const;
  void SetTestDictionary(const TestDictionary&);
  static LongOrTestDictionary FromTestDictionary(const TestDictionary&);

  LongOrTestDictionary(const LongOrTestDictionary&);
  ~LongOrTestDictionary();
  LongOrTestDictionary& operator=(const LongOrTestDictionary&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kLong,
    kTestDictionary,
  };
  SpecificType type_;

  int32_t long_;
  TestDictionary test_dictionary_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const LongOrTestDictionary&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8LongOrTestDictionary final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, LongOrTestDictionary&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const LongOrTestDictionary&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, LongOrTestDictionary& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, LongOrTestDictionary& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<LongOrTestDictionary> : public NativeValueTraitsBase<LongOrTestDictionary> {
  CORE_EXPORT static LongOrTestDictionary NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<LongOrTestDictionary> {
  typedef V8LongOrTestDictionary Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::LongOrTestDictionary);

#endif  // LongOrTestDictionary_h

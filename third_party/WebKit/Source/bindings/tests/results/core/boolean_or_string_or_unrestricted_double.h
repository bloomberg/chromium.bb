// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef BooleanOrStringOrUnrestrictedDouble_h
#define BooleanOrStringOrUnrestrictedDouble_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT BooleanOrStringOrUnrestrictedDouble final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  BooleanOrStringOrUnrestrictedDouble();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsBoolean() const { return type_ == SpecificType::kBoolean; }
  bool GetAsBoolean() const;
  void SetBoolean(bool);
  static BooleanOrStringOrUnrestrictedDouble FromBoolean(bool);

  bool IsString() const { return type_ == SpecificType::kString; }
  const String& GetAsString() const;
  void SetString(const String&);
  static BooleanOrStringOrUnrestrictedDouble FromString(const String&);

  bool IsUnrestrictedDouble() const { return type_ == SpecificType::kUnrestrictedDouble; }
  double GetAsUnrestrictedDouble() const;
  void SetUnrestrictedDouble(double);
  static BooleanOrStringOrUnrestrictedDouble FromUnrestrictedDouble(double);

  BooleanOrStringOrUnrestrictedDouble(const BooleanOrStringOrUnrestrictedDouble&);
  ~BooleanOrStringOrUnrestrictedDouble();
  BooleanOrStringOrUnrestrictedDouble& operator=(const BooleanOrStringOrUnrestrictedDouble&);
  DECLARE_TRACE();

 private:
  enum class SpecificType {
    kNone,
    kBoolean,
    kString,
    kUnrestrictedDouble,
  };
  SpecificType type_;

  bool boolean_;
  String string_;
  double unrestricted_double_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const BooleanOrStringOrUnrestrictedDouble&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8BooleanOrStringOrUnrestrictedDouble final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, BooleanOrStringOrUnrestrictedDouble&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const BooleanOrStringOrUnrestrictedDouble&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, BooleanOrStringOrUnrestrictedDouble& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, BooleanOrStringOrUnrestrictedDouble& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<BooleanOrStringOrUnrestrictedDouble> : public NativeValueTraitsBase<BooleanOrStringOrUnrestrictedDouble> {
  CORE_EXPORT static BooleanOrStringOrUnrestrictedDouble NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<BooleanOrStringOrUnrestrictedDouble> {
  typedef V8BooleanOrStringOrUnrestrictedDouble Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::BooleanOrStringOrUnrestrictedDouble);

#endif  // BooleanOrStringOrUnrestrictedDouble_h

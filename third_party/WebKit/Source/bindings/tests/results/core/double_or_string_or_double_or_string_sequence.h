// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef DoubleOrStringOrDoubleOrStringSequence_h
#define DoubleOrStringOrDoubleOrStringSequence_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class DoubleOrString;

class CORE_EXPORT DoubleOrStringOrDoubleOrStringSequence final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  DoubleOrStringOrDoubleOrStringSequence();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsDouble() const { return type_ == SpecificType::kDouble; }
  double GetAsDouble() const;
  void SetDouble(double);
  static DoubleOrStringOrDoubleOrStringSequence FromDouble(double);

  bool IsDoubleOrStringSequence() const { return type_ == SpecificType::kDoubleOrStringSequence; }
  const HeapVector<DoubleOrString>& GetAsDoubleOrStringSequence() const;
  void SetDoubleOrStringSequence(const HeapVector<DoubleOrString>&);
  static DoubleOrStringOrDoubleOrStringSequence FromDoubleOrStringSequence(const HeapVector<DoubleOrString>&);

  bool IsString() const { return type_ == SpecificType::kString; }
  const String& GetAsString() const;
  void SetString(const String&);
  static DoubleOrStringOrDoubleOrStringSequence FromString(const String&);

  DoubleOrStringOrDoubleOrStringSequence(const DoubleOrStringOrDoubleOrStringSequence&);
  ~DoubleOrStringOrDoubleOrStringSequence();
  DoubleOrStringOrDoubleOrStringSequence& operator=(const DoubleOrStringOrDoubleOrStringSequence&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kDouble,
    kDoubleOrStringSequence,
    kString,
  };
  SpecificType type_;

  double double_;
  HeapVector<DoubleOrString> double_or_string_sequence_;
  String string_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const DoubleOrStringOrDoubleOrStringSequence&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8DoubleOrStringOrDoubleOrStringSequence final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, DoubleOrStringOrDoubleOrStringSequence&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const DoubleOrStringOrDoubleOrStringSequence&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, DoubleOrStringOrDoubleOrStringSequence& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, DoubleOrStringOrDoubleOrStringSequence& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<DoubleOrStringOrDoubleOrStringSequence> : public NativeValueTraitsBase<DoubleOrStringOrDoubleOrStringSequence> {
  CORE_EXPORT static DoubleOrStringOrDoubleOrStringSequence NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<DoubleOrStringOrDoubleOrStringSequence> {
  typedef V8DoubleOrStringOrDoubleOrStringSequence Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::DoubleOrStringOrDoubleOrStringSequence);

#endif  // DoubleOrStringOrDoubleOrStringSequence_h

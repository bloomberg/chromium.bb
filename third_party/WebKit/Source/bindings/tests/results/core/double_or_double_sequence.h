// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef DoubleOrDoubleSequence_h
#define DoubleOrDoubleSequence_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT DoubleOrDoubleSequence final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  DoubleOrDoubleSequence();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsDouble() const { return type_ == SpecificType::kDouble; }
  double GetAsDouble() const;
  void SetDouble(double);
  static DoubleOrDoubleSequence FromDouble(double);

  bool IsDoubleSequence() const { return type_ == SpecificType::kDoubleSequence; }
  const Vector<double>& GetAsDoubleSequence() const;
  void SetDoubleSequence(const Vector<double>&);
  static DoubleOrDoubleSequence FromDoubleSequence(const Vector<double>&);

  DoubleOrDoubleSequence(const DoubleOrDoubleSequence&);
  ~DoubleOrDoubleSequence();
  DoubleOrDoubleSequence& operator=(const DoubleOrDoubleSequence&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kDouble,
    kDoubleSequence,
  };
  SpecificType type_;

  double double_;
  Vector<double> double_sequence_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const DoubleOrDoubleSequence&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8DoubleOrDoubleSequence final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, DoubleOrDoubleSequence&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const DoubleOrDoubleSequence&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, DoubleOrDoubleSequence& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, DoubleOrDoubleSequence& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<DoubleOrDoubleSequence> : public NativeValueTraitsBase<DoubleOrDoubleSequence> {
  CORE_EXPORT static DoubleOrDoubleSequence NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static DoubleOrDoubleSequence NullValue() { return DoubleOrDoubleSequence(); }
};

template <>
struct V8TypeOf<DoubleOrDoubleSequence> {
  typedef V8DoubleOrDoubleSequence Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::DoubleOrDoubleSequence);

#endif  // DoubleOrDoubleSequence_h

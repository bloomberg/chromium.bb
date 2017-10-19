// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef StringOrStringSequence_h
#define StringOrStringSequence_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT StringOrStringSequence final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  StringOrStringSequence();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsString() const { return type_ == SpecificType::kString; }
  const String& GetAsString() const;
  void SetString(const String&);
  static StringOrStringSequence FromString(const String&);

  bool IsStringSequence() const { return type_ == SpecificType::kStringSequence; }
  const Vector<String>& GetAsStringSequence() const;
  void SetStringSequence(const Vector<String>&);
  static StringOrStringSequence FromStringSequence(const Vector<String>&);

  StringOrStringSequence(const StringOrStringSequence&);
  ~StringOrStringSequence();
  StringOrStringSequence& operator=(const StringOrStringSequence&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kString,
    kStringSequence,
  };
  SpecificType type_;

  String string_;
  Vector<String> string_sequence_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const StringOrStringSequence&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8StringOrStringSequence final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, StringOrStringSequence&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const StringOrStringSequence&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, StringOrStringSequence& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, StringOrStringSequence& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<StringOrStringSequence> : public NativeValueTraitsBase<StringOrStringSequence> {
  CORE_EXPORT static StringOrStringSequence NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<StringOrStringSequence> {
  typedef V8StringOrStringSequence Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::StringOrStringSequence);

#endif  // StringOrStringSequence_h

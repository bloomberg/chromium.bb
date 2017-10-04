// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef LongOrBoolean_h
#define LongOrBoolean_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT LongOrBoolean final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  LongOrBoolean();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsBoolean() const { return type_ == SpecificType::kBoolean; }
  bool GetAsBoolean() const;
  void SetBoolean(bool);
  static LongOrBoolean FromBoolean(bool);

  bool IsLong() const { return type_ == SpecificType::kLong; }
  int32_t GetAsLong() const;
  void SetLong(int32_t);
  static LongOrBoolean FromLong(int32_t);

  LongOrBoolean(const LongOrBoolean&);
  ~LongOrBoolean();
  LongOrBoolean& operator=(const LongOrBoolean&);
  DECLARE_TRACE();

 private:
  enum class SpecificType {
    kNone,
    kBoolean,
    kLong,
  };
  SpecificType type_;

  bool boolean_;
  int32_t long_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const LongOrBoolean&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8LongOrBoolean final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, LongOrBoolean&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const LongOrBoolean&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, LongOrBoolean& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, LongOrBoolean& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<LongOrBoolean> : public NativeValueTraitsBase<LongOrBoolean> {
  CORE_EXPORT static LongOrBoolean NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<LongOrBoolean> {
  typedef V8LongOrBoolean Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::LongOrBoolean);

#endif  // LongOrBoolean_h

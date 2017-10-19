// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef FloatOrBoolean_h
#define FloatOrBoolean_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT FloatOrBoolean final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  FloatOrBoolean();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsBoolean() const { return type_ == SpecificType::kBoolean; }
  bool GetAsBoolean() const;
  void SetBoolean(bool);
  static FloatOrBoolean FromBoolean(bool);

  bool IsFloat() const { return type_ == SpecificType::kFloat; }
  float GetAsFloat() const;
  void SetFloat(float);
  static FloatOrBoolean FromFloat(float);

  FloatOrBoolean(const FloatOrBoolean&);
  ~FloatOrBoolean();
  FloatOrBoolean& operator=(const FloatOrBoolean&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kBoolean,
    kFloat,
  };
  SpecificType type_;

  bool boolean_;
  float float_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const FloatOrBoolean&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8FloatOrBoolean final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, FloatOrBoolean&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const FloatOrBoolean&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, FloatOrBoolean& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, FloatOrBoolean& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<FloatOrBoolean> : public NativeValueTraitsBase<FloatOrBoolean> {
  CORE_EXPORT static FloatOrBoolean NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<FloatOrBoolean> {
  typedef V8FloatOrBoolean Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::FloatOrBoolean);

#endif  // FloatOrBoolean_h

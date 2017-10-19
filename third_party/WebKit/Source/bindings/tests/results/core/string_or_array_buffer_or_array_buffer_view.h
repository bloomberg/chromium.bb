// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef StringOrArrayBufferOrArrayBufferView_h
#define StringOrArrayBufferOrArrayBufferView_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/FlexibleArrayBufferView.h"
#include "platform/heap/Handle.h"

namespace blink {

class TestArrayBuffer;

class CORE_EXPORT StringOrArrayBufferOrArrayBufferView final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  StringOrArrayBufferOrArrayBufferView();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsArrayBuffer() const { return type_ == SpecificType::kArrayBuffer; }
  TestArrayBuffer* GetAsArrayBuffer() const;
  void SetArrayBuffer(TestArrayBuffer*);
  static StringOrArrayBufferOrArrayBufferView FromArrayBuffer(TestArrayBuffer*);

  bool IsArrayBufferView() const { return type_ == SpecificType::kArrayBufferView; }
  NotShared<TestArrayBufferView> GetAsArrayBufferView() const;
  void SetArrayBufferView(NotShared<TestArrayBufferView>);
  static StringOrArrayBufferOrArrayBufferView FromArrayBufferView(NotShared<TestArrayBufferView>);

  bool IsString() const { return type_ == SpecificType::kString; }
  const String& GetAsString() const;
  void SetString(const String&);
  static StringOrArrayBufferOrArrayBufferView FromString(const String&);

  StringOrArrayBufferOrArrayBufferView(const StringOrArrayBufferOrArrayBufferView&);
  ~StringOrArrayBufferOrArrayBufferView();
  StringOrArrayBufferOrArrayBufferView& operator=(const StringOrArrayBufferOrArrayBufferView&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kArrayBuffer,
    kArrayBufferView,
    kString,
  };
  SpecificType type_;

  Member<TestArrayBuffer> array_buffer_;
  Member<TestArrayBufferView> array_buffer_view_;
  String string_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const StringOrArrayBufferOrArrayBufferView&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8StringOrArrayBufferOrArrayBufferView final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, StringOrArrayBufferOrArrayBufferView&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const StringOrArrayBufferOrArrayBufferView&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, StringOrArrayBufferOrArrayBufferView& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, StringOrArrayBufferOrArrayBufferView& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<StringOrArrayBufferOrArrayBufferView> : public NativeValueTraitsBase<StringOrArrayBufferOrArrayBufferView> {
  CORE_EXPORT static StringOrArrayBufferOrArrayBufferView NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<StringOrArrayBufferOrArrayBufferView> {
  typedef V8StringOrArrayBufferOrArrayBufferView Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::StringOrArrayBufferOrArrayBufferView);

#endif  // StringOrArrayBufferOrArrayBufferView_h

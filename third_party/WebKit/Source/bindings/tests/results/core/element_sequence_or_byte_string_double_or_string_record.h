// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef ElementSequenceOrByteStringDoubleOrStringRecord_h
#define ElementSequenceOrByteStringDoubleOrStringRecord_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class DoubleOrString;
class Element;

class CORE_EXPORT ElementSequenceOrByteStringDoubleOrStringRecord final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  ElementSequenceOrByteStringDoubleOrStringRecord();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsByteStringDoubleOrStringRecord() const { return type_ == SpecificType::kByteStringDoubleOrStringRecord; }
  const HeapVector<std::pair<String, DoubleOrString>>& GetAsByteStringDoubleOrStringRecord() const;
  void SetByteStringDoubleOrStringRecord(const HeapVector<std::pair<String, DoubleOrString>>&);
  static ElementSequenceOrByteStringDoubleOrStringRecord FromByteStringDoubleOrStringRecord(const HeapVector<std::pair<String, DoubleOrString>>&);

  bool IsElementSequence() const { return type_ == SpecificType::kElementSequence; }
  const HeapVector<Member<Element>>& GetAsElementSequence() const;
  void SetElementSequence(const HeapVector<Member<Element>>&);
  static ElementSequenceOrByteStringDoubleOrStringRecord FromElementSequence(const HeapVector<Member<Element>>&);

  ElementSequenceOrByteStringDoubleOrStringRecord(const ElementSequenceOrByteStringDoubleOrStringRecord&);
  ~ElementSequenceOrByteStringDoubleOrStringRecord();
  ElementSequenceOrByteStringDoubleOrStringRecord& operator=(const ElementSequenceOrByteStringDoubleOrStringRecord&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kByteStringDoubleOrStringRecord,
    kElementSequence,
  };
  SpecificType type_;

  HeapVector<std::pair<String, DoubleOrString>> byte_string_double_or_string_record_;
  HeapVector<Member<Element>> element_sequence_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const ElementSequenceOrByteStringDoubleOrStringRecord&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8ElementSequenceOrByteStringDoubleOrStringRecord final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, ElementSequenceOrByteStringDoubleOrStringRecord&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const ElementSequenceOrByteStringDoubleOrStringRecord&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, ElementSequenceOrByteStringDoubleOrStringRecord& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, ElementSequenceOrByteStringDoubleOrStringRecord& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<ElementSequenceOrByteStringDoubleOrStringRecord> : public NativeValueTraitsBase<ElementSequenceOrByteStringDoubleOrStringRecord> {
  CORE_EXPORT static ElementSequenceOrByteStringDoubleOrStringRecord NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<ElementSequenceOrByteStringDoubleOrStringRecord> {
  typedef V8ElementSequenceOrByteStringDoubleOrStringRecord Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::ElementSequenceOrByteStringDoubleOrStringRecord);

#endif  // ElementSequenceOrByteStringDoubleOrStringRecord_h

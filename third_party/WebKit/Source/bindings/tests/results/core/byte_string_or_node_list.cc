// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "byte_string_or_node_list.h"

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8NodeList.h"
#include "core/dom/NameNodeList.h"
#include "core/dom/NodeList.h"
#include "core/dom/StaticNodeList.h"
#include "core/html/forms/LabelsNodeList.h"

namespace blink {

ByteStringOrNodeList::ByteStringOrNodeList() : type_(SpecificType::kNone) {}

const String& ByteStringOrNodeList::GetAsByteString() const {
  DCHECK(IsByteString());
  return byte_string_;
}

void ByteStringOrNodeList::SetByteString(const String& value) {
  DCHECK(IsNull());
  byte_string_ = value;
  type_ = SpecificType::kByteString;
}

ByteStringOrNodeList ByteStringOrNodeList::FromByteString(const String& value) {
  ByteStringOrNodeList container;
  container.SetByteString(value);
  return container;
}

NodeList* ByteStringOrNodeList::GetAsNodeList() const {
  DCHECK(IsNodeList());
  return node_list_;
}

void ByteStringOrNodeList::SetNodeList(NodeList* value) {
  DCHECK(IsNull());
  node_list_ = value;
  type_ = SpecificType::kNodeList;
}

ByteStringOrNodeList ByteStringOrNodeList::FromNodeList(NodeList* value) {
  ByteStringOrNodeList container;
  container.SetNodeList(value);
  return container;
}

ByteStringOrNodeList::ByteStringOrNodeList(const ByteStringOrNodeList&) = default;
ByteStringOrNodeList::~ByteStringOrNodeList() = default;
ByteStringOrNodeList& ByteStringOrNodeList::operator=(const ByteStringOrNodeList&) = default;

void ByteStringOrNodeList::Trace(blink::Visitor* visitor) {
  visitor->Trace(node_list_);
}

void V8ByteStringOrNodeList::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, ByteStringOrNodeList& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (V8NodeList::hasInstance(v8Value, isolate)) {
    NodeList* cppValue = V8NodeList::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetNodeList(cppValue);
    return;
  }

  {
    V8StringResource<> cppValue = NativeValueTraits<IDLByteString>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetByteString(cppValue);
    return;
  }
}

v8::Local<v8::Value> ToV8(const ByteStringOrNodeList& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case ByteStringOrNodeList::SpecificType::kNone:
      return v8::Null(isolate);
    case ByteStringOrNodeList::SpecificType::kByteString:
      return V8String(isolate, impl.GetAsByteString());
    case ByteStringOrNodeList::SpecificType::kNodeList:
      return ToV8(impl.GetAsNodeList(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

ByteStringOrNodeList NativeValueTraits<ByteStringOrNodeList>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  ByteStringOrNodeList impl;
  V8ByteStringOrNodeList::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink

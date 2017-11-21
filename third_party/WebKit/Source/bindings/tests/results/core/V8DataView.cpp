// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/interface.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "V8DataView.h"

#include "base/memory/scoped_refptr.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8DOMConfiguration.h"
#include "bindings/core/v8/V8SharedArrayBuffer.h"
#include "core/dom/ExecutionContext.h"
#include "platform/bindings/RuntimeCallStats.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/wtf/GetPtr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8DataView::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    nullptr,
    V8DataView::Trace,
    V8DataView::TraceWrappers,
    nullptr,
    "DataView",
    &V8ArrayBufferView::wrapperTypeInfo,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestDataView.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestDataView::wrapper_type_info_ = V8DataView::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestDataView>::value,
    "TestDataView inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestDataView::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestDataView is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

TestDataView* V8DataView::ToImpl(v8::Local<v8::Object> object) {
  DCHECK(object->IsDataView());
  ScriptWrappable* scriptWrappable = ToScriptWrappable(object);
  if (scriptWrappable)
    return scriptWrappable->ToImpl<TestDataView>();

  v8::Local<v8::DataView> v8View = object.As<v8::DataView>();
  v8::Local<v8::Object> arrayBuffer = v8View->Buffer();
  TestDataView* typedArray = nullptr;
  if (arrayBuffer->IsArrayBuffer()) {
    typedArray = TestDataView::Create(V8ArrayBuffer::ToImpl(arrayBuffer), v8View->ByteOffset(), v8View->ByteLength());
  } else if (arrayBuffer->IsSharedArrayBuffer()) {
    typedArray = TestDataView::Create(V8SharedArrayBuffer::ToImpl(arrayBuffer), v8View->ByteOffset(), v8View->ByteLength());
  } else {
    NOTREACHED();
  }
  v8::Local<v8::Object> associatedWrapper = typedArray->AssociateWithWrapper(v8::Isolate::GetCurrent(), typedArray->GetWrapperTypeInfo(), object);
  DCHECK(associatedWrapper == object);

  return typedArray->ToImpl<TestDataView>();
}

TestDataView* V8DataView::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return value->IsDataView() ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestDataView* NativeValueTraits<TestDataView>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestDataView* nativeValue = V8DataView::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "DataView"));
  }
  return nativeValue;
}

}  // namespace blink

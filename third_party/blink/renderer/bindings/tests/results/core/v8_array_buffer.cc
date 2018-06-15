// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_array_buffer.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shared_array_buffer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8ArrayBuffer::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    nullptr,
    nullptr,
    "ArrayBuffer",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestArrayBuffer.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestArrayBuffer::wrapper_type_info_ = V8ArrayBuffer::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestArrayBuffer>::value,
    "TestArrayBuffer inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestArrayBuffer::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestArrayBuffer is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

TestArrayBuffer* V8ArrayBuffer::ToImpl(v8::Local<v8::Object> object) {
  DCHECK(object->IsArrayBuffer());
  v8::Local<v8::ArrayBuffer> v8buffer = object.As<v8::ArrayBuffer>();
  if (v8buffer->IsExternal()) {
    const WrapperTypeInfo* wrapperTypeInfo = ToWrapperTypeInfo(object);
    CHECK(wrapperTypeInfo);
    CHECK_EQ(wrapperTypeInfo->gin_embedder, gin::kEmbedderBlink);
    return ToScriptWrappable(object)->ToImpl<TestArrayBuffer>();
  }

  // Transfer the ownership of the allocated memory to an ArrayBuffer without
  // copying.
  v8::ArrayBuffer::Contents v8Contents = v8buffer->Externalize();
  WTF::ArrayBufferContents::AllocationKind kind = WTF::ArrayBufferContents::AllocationKind::kNormal;
  switch (v8Contents.AllocationMode()) {
    case v8::ArrayBuffer::Allocator::AllocationMode::kNormal:
      kind = WTF::ArrayBufferContents::AllocationKind::kNormal;
      break;
    case v8::ArrayBuffer::Allocator::AllocationMode::kReservation:
      kind = WTF::ArrayBufferContents::AllocationKind::kReservation;
      break;
    default:
      NOTREACHED();
  };
  WTF::ArrayBufferContents::DataHandle data(v8Contents.AllocationBase(),
                                            v8Contents.AllocationLength(),
                                            v8Contents.Data(),
                                            v8Contents.ByteLength(),
                                            kind,
                                            WTF::ArrayBufferContents::FreeMemory);
  WTF::ArrayBufferContents contents(std::move(data), WTF::ArrayBufferContents::kNotShared);
  TestArrayBuffer* buffer = TestArrayBuffer::Create(contents);
  v8::Local<v8::Object> associatedWrapper = buffer->AssociateWithWrapper(v8::Isolate::GetCurrent(), buffer->GetWrapperTypeInfo(), object);
  DCHECK(associatedWrapper == object);

  return buffer;
}

TestArrayBuffer* V8ArrayBuffer::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return value->IsArrayBuffer() ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestArrayBuffer* NativeValueTraits<TestArrayBuffer>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestArrayBuffer* nativeValue = V8ArrayBuffer::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "ArrayBuffer"));
  }
  return nativeValue;
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef V8Uint8ClampedArray_h
#define V8Uint8ClampedArray_h

#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "core/typed_arrays/FlexibleArrayBufferView.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/V8DOMWrapper.h"
#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/heap/Handle.h"

namespace blink {

class V8Uint8ClampedArray {
  STATIC_ONLY(V8Uint8ClampedArray);
 public:
  CORE_EXPORT static TestUint8ClampedArray* ToImpl(v8::Local<v8::Object> object);
  CORE_EXPORT static TestUint8ClampedArray* ToImplWithTypeCheck(v8::Isolate*, v8::Local<v8::Value>);
  CORE_EXPORT static const WrapperTypeInfo wrapperTypeInfo;
  static const int internalFieldCount = kV8DefaultWrapperInternalFieldCount;

  // Callback functions
};

template <>
struct NativeValueTraits<TestUint8ClampedArray> : public NativeValueTraitsBase<TestUint8ClampedArray> {
  CORE_EXPORT static TestUint8ClampedArray* NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static TestUint8ClampedArray* NullValue() { return nullptr; }
};

template <>
struct V8TypeOf<TestUint8ClampedArray> {
  typedef V8Uint8ClampedArray Type;
};

}  // namespace blink

#endif  // V8Uint8ClampedArray_h

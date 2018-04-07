// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef V8Uint8ClampedArray_h
#define V8Uint8ClampedArray_h

#include "bindings/core/v8/generated_code_helper.h"
#include "bindings/core/v8/native_value_traits.h"
#include "bindings/core/v8/to_v8_for_core.h"
#include "bindings/core/v8/v8_array_buffer_view.h"
#include "bindings/core/v8/v8_binding_for_core.h"
#include "core/core_export.h"
#include "core/typed_arrays/array_buffer_view_helpers.h"
#include "core/typed_arrays/dom_typed_array.h"
#include "core/typed_arrays/flexible_array_buffer_view.h"
#include "platform/bindings/script_wrappable.h"
#include "platform/bindings/v8_dom_wrapper.h"
#include "platform/bindings/wrapper_type_info.h"
#include "platform/heap/handle.h"

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

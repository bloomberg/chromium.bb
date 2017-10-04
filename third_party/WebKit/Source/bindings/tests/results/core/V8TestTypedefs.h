// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef V8TestTypedefs_h
#define V8TestTypedefs_h

#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/byte_string_sequence_sequence_or_byte_string_byte_string_record.h"
#include "bindings/core/v8/nested_union_type.h"
#include "bindings/core/v8/string_or_double.h"
#include "bindings/core/v8/test_interface_or_test_interface_empty.h"
#include "bindings/core/v8/unsigned_long_long_or_boolean_or_test_callback_interface.h"
#include "bindings/tests/idls/core/TestTypedefs.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/V8DOMWrapper.h"
#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/heap/Handle.h"

namespace blink {

class V8TestTypedefs {
  STATIC_ONLY(V8TestTypedefs);
 public:
  CORE_EXPORT static bool hasInstance(v8::Local<v8::Value>, v8::Isolate*);
  static v8::Local<v8::Object> findInstanceInPrototypeChain(v8::Local<v8::Value>, v8::Isolate*);
  CORE_EXPORT static v8::Local<v8::FunctionTemplate> domTemplate(v8::Isolate*, const DOMWrapperWorld&);
  static TestTypedefs* ToImpl(v8::Local<v8::Object> object) {
    return ToScriptWrappable(object)->ToImpl<TestTypedefs>();
  }
  CORE_EXPORT static TestTypedefs* ToImplWithTypeCheck(v8::Isolate*, v8::Local<v8::Value>);
  CORE_EXPORT static const WrapperTypeInfo wrapperTypeInfo;
  static void Trace(Visitor* visitor, ScriptWrappable* scriptWrappable) {
    visitor->TraceFromGeneratedCode(scriptWrappable->ToImpl<TestTypedefs>());
  }
  static void TraceWrappers(ScriptWrappableVisitor* visitor, ScriptWrappable* scriptWrappable) {
    visitor->TraceWrappersFromGeneratedCode(scriptWrappable->ToImpl<TestTypedefs>());
  }
  static const int internalFieldCount = kV8DefaultWrapperInternalFieldCount;

  // Callback functions
  CORE_EXPORT static void constructorCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  CORE_EXPORT static void uLongLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void uLongLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void longWithClampAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void longWithClampAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void tAttributeConstructorGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>&);
  CORE_EXPORT static void domStringOrDoubleOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void domStringOrDoubleOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  CORE_EXPORT static void voidMethodLongSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void voidMethodFloatArgStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void voidMethodTestCallbackInterfaceTypeArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void uLongLongMethodTestInterfaceEmptyTypeSequenceArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void testInterfaceOrTestInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void domStringOrDoubleMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void arrayOfStringsMethodArrayOfStringsArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void methodTakingRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void methodTakingOilpanValueRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void unionWithRecordMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void methodThatReturnsRecordMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void voidMethodNestedUnionTypeMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void voidMethodUnionWithTypedefMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  static void InstallRuntimeEnabledFeaturesOnTemplate(
      v8::Isolate*,
      const DOMWrapperWorld&,
      v8::Local<v8::FunctionTemplate> interface_template);
};

template <>
struct NativeValueTraits<TestTypedefs> : public NativeValueTraitsBase<TestTypedefs> {
  CORE_EXPORT static TestTypedefs* NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<TestTypedefs> {
  typedef V8TestTypedefs Type;
};

}  // namespace blink

#endif  // V8TestTypedefs_h

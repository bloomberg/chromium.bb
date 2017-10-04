// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef V8TestNotEnumerableNamedGetter_h
#define V8TestNotEnumerableNamedGetter_h

#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/tests/idls/modules/TestNotEnumerableNamedGetter.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/V8DOMWrapper.h"
#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/heap/Handle.h"

namespace blink {

class V8TestNotEnumerableNamedGetter {
  STATIC_ONLY(V8TestNotEnumerableNamedGetter);
 public:
  MODULES_EXPORT static bool hasInstance(v8::Local<v8::Value>, v8::Isolate*);
  static v8::Local<v8::Object> findInstanceInPrototypeChain(v8::Local<v8::Value>, v8::Isolate*);
  MODULES_EXPORT static v8::Local<v8::FunctionTemplate> domTemplate(v8::Isolate*, const DOMWrapperWorld&);
  static TestNotEnumerableNamedGetter* ToImpl(v8::Local<v8::Object> object) {
    return ToScriptWrappable(object)->ToImpl<TestNotEnumerableNamedGetter>();
  }
  MODULES_EXPORT static TestNotEnumerableNamedGetter* ToImplWithTypeCheck(v8::Isolate*, v8::Local<v8::Value>);
  MODULES_EXPORT static const WrapperTypeInfo wrapperTypeInfo;
  static void Trace(Visitor* visitor, ScriptWrappable* scriptWrappable) {
    visitor->TraceFromGeneratedCode(scriptWrappable->ToImpl<TestNotEnumerableNamedGetter>());
  }
  static void TraceWrappers(ScriptWrappableVisitor* visitor, ScriptWrappable* scriptWrappable) {
    visitor->TraceWrappersFromGeneratedCode(scriptWrappable->ToImpl<TestNotEnumerableNamedGetter>());
  }
  static const int internalFieldCount = kV8DefaultWrapperInternalFieldCount;

  // Callback functions

  MODULES_EXPORT static void namedPropertyGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>&);
  MODULES_EXPORT static void indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>&);
  MODULES_EXPORT static void indexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>&);

  static void InstallRuntimeEnabledFeaturesOnTemplate(
      v8::Isolate*,
      const DOMWrapperWorld&,
      v8::Local<v8::FunctionTemplate> interface_template);
};

template <>
struct NativeValueTraits<TestNotEnumerableNamedGetter> : public NativeValueTraitsBase<TestNotEnumerableNamedGetter> {
  MODULES_EXPORT static TestNotEnumerableNamedGetter* NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<TestNotEnumerableNamedGetter> {
  typedef V8TestNotEnumerableNamedGetter Type;
};

}  // namespace blink

#endif  // V8TestNotEnumerableNamedGetter_h

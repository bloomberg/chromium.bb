// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef V8TestInterfaceNode_h
#define V8TestInterfaceNode_h

#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/tests/idls/core/TestInterfaceNode.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/V8DOMWrapper.h"
#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/heap/Handle.h"

namespace blink {

class V8TestInterfaceNode {
  STATIC_ONLY(V8TestInterfaceNode);
 public:
  CORE_EXPORT static bool hasInstance(v8::Local<v8::Value>, v8::Isolate*);
  static v8::Local<v8::Object> findInstanceInPrototypeChain(v8::Local<v8::Value>, v8::Isolate*);
  CORE_EXPORT static v8::Local<v8::FunctionTemplate> domTemplate(v8::Isolate*, const DOMWrapperWorld&);
  static TestInterfaceNode* ToImpl(v8::Local<v8::Object> object) {
    return ToScriptWrappable(object)->ToImpl<TestInterfaceNode>();
  }
  CORE_EXPORT static TestInterfaceNode* ToImplWithTypeCheck(v8::Isolate*, v8::Local<v8::Value>);
  CORE_EXPORT static const WrapperTypeInfo wrapperTypeInfo;
  static void Trace(Visitor* visitor, ScriptWrappable* scriptWrappable) {
    visitor->TraceFromGeneratedCode(scriptWrappable->ToImpl<TestInterfaceNode>());
  }
  static void TraceWrappers(ScriptWrappableVisitor* visitor, ScriptWrappable* scriptWrappable) {
    visitor->TraceWrappersFromGeneratedCode(scriptWrappable->ToImpl<TestInterfaceNode>());
  }
  static const int internalFieldCount = kV8DefaultWrapperInternalFieldCount;

  // Callback functions

  CORE_EXPORT static void nodeNameAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void nodeNameAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void stringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void stringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void readonlyTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void eventHandlerAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void eventHandlerAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void perWorldBindingsReadonlyTestInterfaceEmptyAttributeAttributeGetterCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void reflectStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void reflectStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void reflectUrlStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void reflectUrlStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  CORE_EXPORT static void testInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void perWorldBindingsTestInterfaceEmptyMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void perWorldBindingsTestInterfaceEmptyMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  CORE_EXPORT static void perWorldBindingsTestInterfaceEmptyMethodOptionalBooleanArgMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>&);

  static void InstallRuntimeEnabledFeaturesOnTemplate(
      v8::Isolate*,
      const DOMWrapperWorld&,
      v8::Local<v8::FunctionTemplate> interface_template);
};

template <>
struct NativeValueTraits<TestInterfaceNode> : public NativeValueTraitsBase<TestInterfaceNode> {
  CORE_EXPORT static TestInterfaceNode* NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct V8TypeOf<TestInterfaceNode> {
  typedef V8TestInterfaceNode Type;
};

}  // namespace blink

#endif  // V8TestInterfaceNode_h

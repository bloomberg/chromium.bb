// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef V8TestInterfaceNode_h
#define V8TestInterfaceNode_h

#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_node.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

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
  CORE_EXPORT static TestInterfaceNode* NullValue() { return nullptr; }
};

template <>
struct V8TypeOf<TestInterfaceNode> {
  typedef V8TestInterfaceNode Type;
};

}  // namespace blink

#endif  // V8TestInterfaceNode_h

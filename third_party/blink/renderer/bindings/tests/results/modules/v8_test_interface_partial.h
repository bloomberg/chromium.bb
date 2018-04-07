// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/partial_interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef V8TestInterfacePartial_h
#define V8TestInterfacePartial_h

#include "bindings/core/v8/generated_code_helper.h"
#include "bindings/core/v8/native_value_traits.h"
#include "bindings/core/v8/to_v8_for_core.h"
#include "bindings/core/v8/unsigned_long_long_or_boolean_or_test_callback_interface.h"
#include "bindings/core/v8/v8_binding_for_core.h"
#include "bindings/tests/idls/core/test_interface_implementation.h"
#include "platform/bindings/script_wrappable.h"
#include "platform/bindings/v8_dom_wrapper.h"
#include "platform/bindings/wrapper_type_info.h"
#include "platform/heap/handle.h"

namespace blink {

class ScriptState;

class V8TestInterfacePartial {
  STATIC_ONLY(V8TestInterfacePartial);
 public:
  static void initialize();
  static void implementsCustomVoidMethodMethodCustom(const v8::FunctionCallbackInfo<v8::Value>&);
  static void InstallConditionalFeatures(
      v8::Local<v8::Context>,
      const DOMWrapperWorld&,
      v8::Local<v8::Object> instance,
      v8::Local<v8::Object> prototype,
      v8::Local<v8::Function> interface,
      v8::Local<v8::FunctionTemplate> interface_template);

  static void installOriginTrialPartialFeature(ScriptState*, v8::Local<v8::Object> instance);
  static void installOriginTrialPartialFeature(v8::Isolate*, const DOMWrapperWorld&, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, v8::Local<v8::Function> interface);
  static void installOriginTrialPartialFeature(ScriptState*);

  static void InstallRuntimeEnabledFeaturesOnTemplate(
      v8::Isolate*,
      const DOMWrapperWorld&,
      v8::Local<v8::FunctionTemplate> interface_template);

  // Callback functions
  static void partial4LongAttributeAttributeGetterCallback(    const v8::FunctionCallbackInfo<v8::Value>& info);
  static void partial4LongAttributeAttributeSetterCallback(    const v8::FunctionCallbackInfo<v8::Value>& info);
  static void partial4StaticLongAttributeAttributeGetterCallback(    const v8::FunctionCallbackInfo<v8::Value>& info);
  static void partial4StaticLongAttributeAttributeSetterCallback(    const v8::FunctionCallbackInfo<v8::Value>& info);

  static void partialVoidTestEnumModulesArgMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void partial2VoidTestEnumModulesRecordMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void unscopableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void unionWithTypedefMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void partial4VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void partial4StaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info);

 private:
  static void installV8TestInterfaceTemplate(v8::Isolate*, const DOMWrapperWorld&, v8::Local<v8::FunctionTemplate> interfaceTemplate);
};

}  // namespace blink

#endif  // V8TestInterfacePartial_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/interface.cpp.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "V8TestInterfaceCheckSecurity.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8CrossOriginSetterInfo.h"
#include "bindings/core/v8/V8DOMConfiguration.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/bindings/RuntimeCallStats.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/wtf/GetPtr.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceCheckSecurity::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceCheckSecurity::domTemplate,
    V8TestInterfaceCheckSecurity::Trace,
    V8TestInterfaceCheckSecurity::TraceWrappers,
    nullptr,
    "TestInterfaceCheckSecurity",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
    WrapperTypeInfo::kIndependent,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceCheckSecurity.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceCheckSecurity::wrapper_type_info_ = V8TestInterfaceCheckSecurity::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceCheckSecurity>::value,
    "TestInterfaceCheckSecurity inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceCheckSecurity::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceCheckSecurity is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace TestInterfaceCheckSecurityV8Internal {

static void readonlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->readonlyLongAttribute());
}

static void longAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->longAttribute());
}

static void longAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceCheckSecurity", "longAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState, kNormalConversion);
  if (exceptionState.HadException())
    return;

  impl->setLongAttribute(cppValue);
}

static void doNotCheckSecurityLongAttributeAttributeGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->doNotCheckSecurityLongAttribute());
}

static void doNotCheckSecurityLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceCheckSecurity", "doNotCheckSecurityLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState, kNormalConversion);
  if (exceptionState.HadException())
    return;

  impl->setDoNotCheckSecurityLongAttribute(cppValue);
}

static void doNotCheckSecurityReadonlyLongAttributeAttributeGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->doNotCheckSecurityReadonlyLongAttribute());
}

static void doNotCheckSecurityOnSetterLongAttributeAttributeGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->doNotCheckSecurityOnSetterLongAttribute());
}

static void doNotCheckSecurityOnSetterLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const V8CrossOriginSetterInfo& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceCheckSecurity", "doNotCheckSecurityOnSetterLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState, kNormalConversion);
  if (exceptionState.HadException())
    return;

  impl->setDoNotCheckSecurityOnSetterLongAttribute(cppValue);
}

static void doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->doNotCheckSecurityReplaceableReadonlyLongAttribute());
}

static void voidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  impl->voidMethod();
}

static void doNotCheckSecurityVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  impl->doNotCheckSecurityVoidMethod();
}

static void doNotCheckSecurityVoidMethodOriginSafeMethodGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::From(info.GetIsolate());
  const DOMWrapperWorld& world = DOMWrapperWorld::World(info.GetIsolate()->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interfaceTemplate = data->FindInterfaceTemplate(world, &V8TestInterfaceCheckSecurity::wrapperTypeInfo);
  v8::Local<v8::Signature> signature = v8::Signature::New(info.GetIsolate(), interfaceTemplate);

  v8::Local<v8::FunctionTemplate> methodTemplate = data->FindOrCreateOperationTemplate(world, &domTemplateKey, V8TestInterfaceCheckSecurity::doNotCheckSecurityVoidMethodMethodCallback, V8Undefined(), signature, 0);
  // Return the function by default, unless the user script has overwritten it.
  V8SetReturnValue(info, methodTemplate->GetFunction(info.GetIsolate()->GetCurrentContext()).ToLocalChecked());

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(info.GetIsolate()), impl, BindingSecurity::ErrorReportOption::kDoNotReport)) {
    return;
  }

  // {{method.name}} must be same with |methodName| (=name) in
  // {{cpp_class}}OriginSafeMethodSetter defined in interface.cpp.tmpl.
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(), "doNotCheckSecurityVoidMethod");
  v8::Local<v8::Object> holder = v8::Local<v8::Object>::Cast(info.Holder());
  if (propertySymbol.HasValue(holder)) {
    V8SetReturnValue(info, propertySymbol.GetOrUndefined(holder));
  }
}

static void doNotCheckSecurityPerWorldBindingsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  impl->doNotCheckSecurityPerWorldBindingsVoidMethod();
}

static void doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::From(info.GetIsolate());
  const DOMWrapperWorld& world = DOMWrapperWorld::World(info.GetIsolate()->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interfaceTemplate = data->FindInterfaceTemplate(world, &V8TestInterfaceCheckSecurity::wrapperTypeInfo);
  v8::Local<v8::Signature> signature = v8::Signature::New(info.GetIsolate(), interfaceTemplate);

  v8::Local<v8::FunctionTemplate> methodTemplate = data->FindOrCreateOperationTemplate(world, &domTemplateKey, V8TestInterfaceCheckSecurity::doNotCheckSecurityPerWorldBindingsVoidMethodMethodCallback, V8Undefined(), signature, 0);
  // Return the function by default, unless the user script has overwritten it.
  V8SetReturnValue(info, methodTemplate->GetFunction(info.GetIsolate()->GetCurrentContext()).ToLocalChecked());

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(info.GetIsolate()), impl, BindingSecurity::ErrorReportOption::kDoNotReport)) {
    return;
  }

  // {{method.name}} must be same with |methodName| (=name) in
  // {{cpp_class}}OriginSafeMethodSetter defined in interface.cpp.tmpl.
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(), "doNotCheckSecurityPerWorldBindingsVoidMethod");
  v8::Local<v8::Object> holder = v8::Local<v8::Object>::Cast(info.Holder());
  if (propertySymbol.HasValue(holder)) {
    V8SetReturnValue(info, propertySymbol.GetOrUndefined(holder));
  }
}

static void doNotCheckSecurityPerWorldBindingsVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  impl->doNotCheckSecurityPerWorldBindingsVoidMethod();
}

static void doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterForMainWorld(const v8::PropertyCallbackInfo<v8::Value>& info) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::From(info.GetIsolate());
  const DOMWrapperWorld& world = DOMWrapperWorld::World(info.GetIsolate()->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interfaceTemplate = data->FindInterfaceTemplate(world, &V8TestInterfaceCheckSecurity::wrapperTypeInfo);
  v8::Local<v8::Signature> signature = v8::Signature::New(info.GetIsolate(), interfaceTemplate);

  v8::Local<v8::FunctionTemplate> methodTemplate = data->FindOrCreateOperationTemplate(world, &domTemplateKey, V8TestInterfaceCheckSecurity::doNotCheckSecurityPerWorldBindingsVoidMethodMethodCallbackForMainWorld, V8Undefined(), signature, 0);
  // Return the function by default, unless the user script has overwritten it.
  V8SetReturnValue(info, methodTemplate->GetFunction(info.GetIsolate()->GetCurrentContext()).ToLocalChecked());

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(info.GetIsolate()), impl, BindingSecurity::ErrorReportOption::kDoNotReport)) {
    return;
  }

  // {{method.name}} must be same with |methodName| (=name) in
  // {{cpp_class}}OriginSafeMethodSetter defined in interface.cpp.tmpl.
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(), "doNotCheckSecurityPerWorldBindingsVoidMethod");
  v8::Local<v8::Object> holder = v8::Local<v8::Object>::Cast(info.Holder());
  if (propertySymbol.HasValue(holder)) {
    V8SetReturnValue(info, propertySymbol.GetOrUndefined(holder));
  }
}

static void doNotCheckSecurityUnforgeableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  impl->doNotCheckSecurityUnforgeableVoidMethod();
}

static void doNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::From(info.GetIsolate());
  const DOMWrapperWorld& world = DOMWrapperWorld::World(info.GetIsolate()->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interfaceTemplate = data->FindInterfaceTemplate(world, &V8TestInterfaceCheckSecurity::wrapperTypeInfo);
  v8::Local<v8::Signature> signature = v8::Signature::New(info.GetIsolate(), interfaceTemplate);

  v8::Local<v8::FunctionTemplate> methodTemplate = data->FindOrCreateOperationTemplate(world, &domTemplateKey, V8TestInterfaceCheckSecurity::doNotCheckSecurityUnforgeableVoidMethodMethodCallback, V8Undefined(), signature, 0);
  // Return the function by default, unless the user script has overwritten it.
  V8SetReturnValue(info, methodTemplate->GetFunction(info.GetIsolate()->GetCurrentContext()).ToLocalChecked());

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(info.GetIsolate()), impl, BindingSecurity::ErrorReportOption::kDoNotReport)) {
    return;
  }

  // {{method.name}} must be same with |methodName| (=name) in
  // {{cpp_class}}OriginSafeMethodSetter defined in interface.cpp.tmpl.
  V8PrivateProperty::Symbol propertySymbol =
      V8PrivateProperty::GetSymbol(info.GetIsolate(), "doNotCheckSecurityUnforgeableVoidMethod");
  v8::Local<v8::Object> holder = v8::Local<v8::Object>::Cast(info.Holder());
  if (propertySymbol.HasValue(holder)) {
    V8SetReturnValue(info, propertySymbol.GetOrUndefined(holder));
  }
}

static void TestInterfaceCheckSecurityOriginSafeMethodSetter(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info) {
  if (!name->IsString())
    return;
  v8::Local<v8::Object> holder = V8TestInterfaceCheckSecurity::findInstanceInPrototypeChain(info.Holder(), info.GetIsolate());
  if (holder.IsEmpty())
    return;
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);
  v8::String::Utf8Value methodName(name);
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kSetterContext, "TestInterfaceCheckSecurity", *methodName);
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(info.GetIsolate()), impl, exceptionState)) {
    return;
  }

  // |methodName| must be same with {{method.name}} in
  // {{method.name}}OriginSafeMethodGetter{{world_suffix}} defined in
  // methods.cpp.tmpl
  V8PrivateProperty::GetSymbol(info.GetIsolate(), *methodName)
      .Set(v8::Local<v8::Object>::Cast(info.Holder()), v8Value);
}
static const struct {
  using GetterCallback = void(*)(const v8::PropertyCallbackInfo<v8::Value>&);
  using SetterCallback = void(*)(v8::Local<v8::Value>, const V8CrossOriginSetterInfo&);

  const char* const name;
  const GetterCallback getter;
  const SetterCallback setter;
} kCrossOriginAttributeTable[] = {
  {
    "doNotCheckSecurityLongAttribute",
    &TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityLongAttributeAttributeGetter,
    nullptr,
  },
  {
    "doNotCheckSecurityReadonlyLongAttribute",
    &TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityReadonlyLongAttributeAttributeGetter,
    nullptr,
  },
  {
    "doNotCheckSecurityOnSetterLongAttribute",
    nullptr,
    &TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityOnSetterLongAttributeAttributeSetter,
  },
  {
    "doNotCheckSecurityReplaceableReadonlyLongAttribute",
    &TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetter,
    nullptr,
  },
  {"doNotCheckSecurityVoidMethod", &TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityVoidMethodOriginSafeMethodGetter, nullptr},
  {"doNotCheckSecurityPerWorldBindingsVoidMethod", &TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetter, nullptr},
  {"doNotCheckSecurityUnforgeableVoidMethod", &TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetter, nullptr},
};
} // namespace TestInterfaceCheckSecurityV8Internal

void V8TestInterfaceCheckSecurity::readonlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_readonlyLongAttribute_Getter");

  TestInterfaceCheckSecurityV8Internal::readonlyLongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::longAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_longAttribute_Getter");

  TestInterfaceCheckSecurityV8Internal::longAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::longAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_longAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceCheckSecurityV8Internal::longAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityLongAttributeAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityLongAttribute_Getter");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityLongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityLongAttributeAttributeSetterCallback(v8::Local<v8::Name>, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityLongAttribute_Setter");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityReadonlyLongAttributeAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityReadonlyLongAttribute_Getter");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityReadonlyLongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityOnSetterLongAttributeAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityOnSetterLongAttribute_Getter");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityOnSetterLongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityOnSetterLongAttributeAttributeSetterCallback(v8::Local<v8::Name>, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityOnSetterLongAttribute_Setter");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityOnSetterLongAttributeAttributeSetter(v8Value, V8CrossOriginSetterInfo(info.GetIsolate(), info.Holder()));
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityReplaceableReadonlyLongAttribute_Getter");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::voidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_voidMethod");

  TestInterfaceCheckSecurityV8Internal::voidMethodMethod(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityVoidMethod");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityVoidMethodMethod(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityVoidMethodOriginSafeMethodGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityVoidMethod_OriginSafeMethodGetter");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityVoidMethodOriginSafeMethodGetter(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityPerWorldBindingsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityPerWorldBindingsVoidMethod");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodMethod(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityPerWorldBindingsVoidMethod_OriginSafeMethodGetter");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetter(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityPerWorldBindingsVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityPerWorldBindingsVoidMethod");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodMethodForMainWorld(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterCallbackForMainWorld(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityPerWorldBindingsVoidMethod_OriginSafeMethodGetter");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterForMainWorld(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityUnforgeableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityUnforgeableVoidMethod");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityUnforgeableVoidMethodMethod(info);
}

void V8TestInterfaceCheckSecurity::doNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityUnforgeableVoidMethod_OriginSafeMethodGetter");

  TestInterfaceCheckSecurityV8Internal::doNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetter(info);
}

void V8TestInterfaceCheckSecurity::TestInterfaceCheckSecurityOriginSafeMethodSetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<void>& info) {
  TestInterfaceCheckSecurityV8Internal::TestInterfaceCheckSecurityOriginSafeMethodSetter(name, v8Value, info);
}

bool V8TestInterfaceCheckSecurity::securityCheck(v8::Local<v8::Context> accessingContext, v8::Local<v8::Object> accessedObject, v8::Local<v8::Value> data) {
  #error "Unexpected security check for interface TestInterfaceCheckSecurity"
}

void V8TestInterfaceCheckSecurity::crossOriginNamedGetter(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_CrossOriginNamedGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  for (const auto& attribute : TestInterfaceCheckSecurityV8Internal::kCrossOriginAttributeTable) {
    if (propertyName == attribute.name && attribute.getter) {
      attribute.getter(info);
      return;
    }
  }

  BindingSecurity::FailedAccessCheckFor(
      info.GetIsolate(),
      &V8TestInterfaceCheckSecurity::wrapperTypeInfo,
      info.Holder());
}

void V8TestInterfaceCheckSecurity::crossOriginNamedSetter(v8::Local<v8::Name> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_CrossOriginNamedSetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  for (const auto& attribute : TestInterfaceCheckSecurityV8Internal::kCrossOriginAttributeTable) {
    if (propertyName == attribute.name && attribute.setter) {
      attribute.setter(value, V8CrossOriginSetterInfo(info.GetIsolate(), info.Holder()));
      return;
    }
  }

  BindingSecurity::FailedAccessCheckFor(
      info.GetIsolate(),
      &V8TestInterfaceCheckSecurity::wrapperTypeInfo,
      info.Holder());
}

void V8TestInterfaceCheckSecurity::crossOriginNamedEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  Vector<String> names;
  for (const auto& attribute : TestInterfaceCheckSecurityV8Internal::kCrossOriginAttributeTable)
    names.push_back(attribute.name);

  // Use the current context as the creation context, as a cross-origin access
  // may involve an object that does not have a creation context.
  V8SetReturnValue(info,
                   ToV8(names, info.GetIsolate()->GetCurrentContext()->Global(),
                        info.GetIsolate()).As<v8::Array>());
}

// Suppress warning: global constructors, because AttributeConfiguration is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
static const V8DOMConfiguration::AttributeConfiguration V8TestInterfaceCheckSecurityAttributes[] = {
    { "doNotCheckSecurityLongAttribute", V8TestInterfaceCheckSecurity::doNotCheckSecurityLongAttributeAttributeGetterCallback, V8TestInterfaceCheckSecurity::doNotCheckSecurityLongAttributeAttributeSetterCallback, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },

    { "doNotCheckSecurityReadonlyLongAttribute", V8TestInterfaceCheckSecurity::doNotCheckSecurityReadonlyLongAttributeAttributeGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },

    { "doNotCheckSecurityOnSetterLongAttribute", V8TestInterfaceCheckSecurity::doNotCheckSecurityOnSetterLongAttributeAttributeGetterCallback, V8TestInterfaceCheckSecurity::doNotCheckSecurityOnSetterLongAttributeAttributeSetterCallback, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },

    { "doNotCheckSecurityReplaceableReadonlyLongAttribute", V8TestInterfaceCheckSecurity::doNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static const V8DOMConfiguration::AccessorConfiguration V8TestInterfaceCheckSecurityAccessors[] = {
    { "readonlyLongAttribute", V8TestInterfaceCheckSecurity::readonlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },

    { "longAttribute", V8TestInterfaceCheckSecurity::longAttributeAttributeGetterCallback, V8TestInterfaceCheckSecurity::longAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestInterfaceCheckSecurityMethods[] = {
    {"voidMethod", V8TestInterfaceCheckSecurity::voidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kCheckAccess, V8DOMConfiguration::kAllWorlds},
};

static void installV8TestInterfaceCheckSecurityTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceCheckSecurity::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceCheckSecurity::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Global object prototype chain consists of Immutable Prototype Exotic Objects
  prototypeTemplate->SetImmutableProto();

  // Global objects are Immutable Prototype Exotic Objects
  instanceTemplate->SetImmutableProto();

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallAttributes(
      isolate, world, instanceTemplate, prototypeTemplate,
      V8TestInterfaceCheckSecurityAttributes, WTF_ARRAY_LENGTH(V8TestInterfaceCheckSecurityAttributes));
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceCheckSecurityAccessors, WTF_ARRAY_LENGTH(V8TestInterfaceCheckSecurityAccessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceCheckSecurityMethods, WTF_ARRAY_LENGTH(V8TestInterfaceCheckSecurityMethods));

  // Cross-origin access check
  instanceTemplate->SetAccessCheckCallbackAndHandler(
      V8TestInterfaceCheckSecurity::securityCheck,
      v8::NamedPropertyHandlerConfiguration(
          V8TestInterfaceCheckSecurity::crossOriginNamedGetter,
          V8TestInterfaceCheckSecurity::crossOriginNamedSetter,
          nullptr,
          nullptr,
          V8TestInterfaceCheckSecurity::crossOriginNamedEnumerator),
      v8::IndexedPropertyHandlerConfiguration(nullptr),
      v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&V8TestInterfaceCheckSecurity::wrapperTypeInfo)));

  // Custom signature
  static const V8DOMConfiguration::AttributeConfiguration doNotCheckSecurityVoidMethodOriginSafeAttributeConfiguration[] = {
      {"doNotCheckSecurityVoidMethod", V8TestInterfaceCheckSecurity::doNotCheckSecurityVoidMethodOriginSafeMethodGetterCallback, V8TestInterfaceCheckSecurity::TestInterfaceCheckSecurityOriginSafeMethodSetterCallback, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds}
  };
  for (const auto& attributeConfig : doNotCheckSecurityVoidMethodOriginSafeAttributeConfiguration)
    V8DOMConfiguration::InstallAttribute(isolate, world, instanceTemplate, prototypeTemplate, attributeConfig);
  static const V8DOMConfiguration::AttributeConfiguration doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeAttributeConfiguration[] = {
      {"doNotCheckSecurityPerWorldBindingsVoidMethod", V8TestInterfaceCheckSecurity::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterCallbackForMainWorld, V8TestInterfaceCheckSecurity::TestInterfaceCheckSecurityOriginSafeMethodSetterCallbackForMainWorld, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::MainWorld},
      {"doNotCheckSecurityPerWorldBindingsVoidMethod", V8TestInterfaceCheckSecurity::doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterCallback, V8TestInterfaceCheckSecurity::TestInterfaceCheckSecurityOriginSafeMethodSetterCallback, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::NonMainWorlds}}
  };
  for (const auto& attributeConfig : doNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeAttributeConfiguration)
    V8DOMConfiguration::InstallAttribute(isolate, world, instanceTemplate, prototypeTemplate, attributeConfig);
  static const V8DOMConfiguration::AttributeConfiguration doNotCheckSecurityUnforgeableVoidMethodOriginSafeAttributeConfiguration[] = {
      {"doNotCheckSecurityUnforgeableVoidMethod", V8TestInterfaceCheckSecurity::doNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kAllWorlds}
  };
  for (const auto& attributeConfig : doNotCheckSecurityUnforgeableVoidMethodOriginSafeAttributeConfiguration)
    V8DOMConfiguration::InstallAttribute(isolate, world, instanceTemplate, prototypeTemplate, attributeConfig);

  V8TestInterfaceCheckSecurity::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfaceCheckSecurity::InstallRuntimeEnabledFeaturesOnTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceCheckSecurity::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceCheckSecurityTemplate);
}

bool V8TestInterfaceCheckSecurity::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceCheckSecurity::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceCheckSecurity* V8TestInterfaceCheckSecurity::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceCheckSecurity* NativeValueTraits<TestInterfaceCheckSecurity>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceCheckSecurity* nativeValue = V8TestInterfaceCheckSecurity::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceCheckSecurity"));
  }
  return nativeValue;
}

}  // namespace blink

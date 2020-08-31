// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
#include "third_party/blink/renderer/core/xml/dom_parser.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"

namespace blink {

void V8ConstructorAttributeGetter(
    v8::Local<v8::Name> property_name,
    const v8::PropertyCallbackInfo<v8::Value>& info,
    const WrapperTypeInfo* wrapper_type_info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(
      info.GetIsolate(), "Blink_V8ConstructorAttributeGetter");
  V8PerContextData* per_context_data =
      V8PerContextData::From(info.Holder()->CreationContext());
  if (!per_context_data)
    return;
  V8SetReturnValue(info,
                   per_context_data->ConstructorForType(wrapper_type_info));
}

namespace {

enum class IgnorePause { kDontIgnore, kIgnore };

// 'beforeunload' event listeners are runnable even when execution contexts are
// paused. Use |RespectPause::kPrioritizeOverPause| in such a case.
bool IsCallbackFunctionRunnableInternal(
    const ScriptState* callback_relevant_script_state,
    const ScriptState* incumbent_script_state,
    IgnorePause ignore_pause) {
  if (!callback_relevant_script_state->ContextIsValid())
    return false;
  const ExecutionContext* relevant_execution_context =
      ExecutionContext::From(callback_relevant_script_state);
  if (!relevant_execution_context ||
      relevant_execution_context->IsContextDestroyed()) {
    return false;
  }
  if (relevant_execution_context->IsContextPaused()) {
    if (ignore_pause == IgnorePause::kDontIgnore)
      return false;
  }

  // TODO(yukishiino): Callback function type value must make the incumbent
  // environment alive, i.e. the reference to v8::Context must be strong.
  v8::HandleScope handle_scope(incumbent_script_state->GetIsolate());
  v8::Local<v8::Context> incumbent_context =
      incumbent_script_state->GetContext();
  ExecutionContext* incumbent_execution_context =
      incumbent_context.IsEmpty() ? nullptr
                                  : ToExecutionContext(incumbent_context);
  // The incumbent realm schedules the currently-running callback although it
  // may not correspond to the currently-running function object. So we check
  // the incumbent context which originally schedules the currently-running
  // callback to see whether the script setting is disabled before invoking
  // the callback.
  // TODO(crbug.com/608641): move IsMainWorld check into
  // ExecutionContext::CanExecuteScripts()
  if (!incumbent_execution_context ||
      incumbent_execution_context->IsContextDestroyed()) {
    return false;
  }
  if (incumbent_execution_context->IsContextPaused()) {
    if (ignore_pause == IgnorePause::kDontIgnore)
      return false;
  }
  return !incumbent_script_state->World().IsMainWorld() ||
         incumbent_execution_context->CanExecuteScripts(kAboutToExecuteScript);
}

}  // namespace

bool IsCallbackFunctionRunnable(
    const ScriptState* callback_relevant_script_state,
    const ScriptState* incumbent_script_state) {
  return IsCallbackFunctionRunnableInternal(callback_relevant_script_state,
                                            incumbent_script_state,
                                            IgnorePause::kDontIgnore);
}

bool IsCallbackFunctionRunnableIgnoringPause(
    const ScriptState* callback_relevant_script_state,
    const ScriptState* incumbent_script_state) {
  return IsCallbackFunctionRunnableInternal(callback_relevant_script_state,
                                            incumbent_script_state,
                                            IgnorePause::kIgnore);
}

void V8SetReflectedBooleanAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const char* interface_name,
    const char* idl_attribute_name,
    const QualifiedName& content_attr) {
  v8::Isolate* isolate = info.GetIsolate();
  Element* impl = V8Element::ToImpl(info.Holder());

  V0CustomElementProcessingStack::CallbackDeliveryScope delivery_scope;
  ExceptionState exception_state(isolate, ExceptionState::kSetterContext,
                                 interface_name, idl_attribute_name);
  CEReactionsScope ce_reactions_scope;

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(isolate, info[0],
                                                              exception_state);
  if (exception_state.HadException())
    return;

  impl->SetBooleanAttribute(content_attr, cpp_value);
}

void V8SetReflectedDOMStringAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attr) {
  Element* impl = V8Element::ToImpl(info.Holder());

  V0CustomElementProcessingStack::CallbackDeliveryScope delivery_scope;
  CEReactionsScope ce_reactions_scope;

  // Prepare the value to be set.
  V8StringResource<> cpp_value = info[0];
  if (!cpp_value.Prepare())
    return;

  impl->setAttribute(content_attr, cpp_value);
}

void V8SetReflectedNullableDOMStringAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attr) {
  Element* impl = V8Element::ToImpl(info.Holder());

  V0CustomElementProcessingStack::CallbackDeliveryScope delivery_scope;
  CEReactionsScope ce_reactions_scope;

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cpp_value = info[0];
  if (!cpp_value.Prepare())
    return;

  impl->setAttribute(content_attr, cpp_value);
}

namespace bindings {

base::Optional<size_t> FindIndexInEnumStringTable(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    base::span<const char* const> enum_value_table,
    const char* enum_type_name,
    ExceptionState& exception_state) {
  const String& str_value = NativeValueTraits<IDLStringV2>::NativeValue(
      isolate, value, exception_state);
  if (exception_state.HadException())
    return base::nullopt;

  base::Optional<size_t> index =
      FindIndexInEnumStringTable(str_value, enum_value_table);

  if (!index.has_value()) {
    exception_state.ThrowTypeError("The provided value '" + str_value +
                                   "' is not a valid enum value of type " +
                                   enum_type_name + ".");
  }
  return index;
}

base::Optional<size_t> FindIndexInEnumStringTable(
    const String& str_value,
    base::span<const char* const> enum_value_table) {
  for (size_t i = 0; i < enum_value_table.size(); ++i) {
    if (Equal(str_value.Impl(), enum_value_table[i]))
      return i;
  }
  return base::nullopt;
}

bool IsEsIterableObject(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
  // https://heycam.github.io/webidl/#es-overloads
  // step 9. Otherwise: if Type(V) is Object and ...
  if (!value->IsObject())
    return false;

  // step 9.1. Let method be ? GetMethod(V, @@iterator).
  // https://tc39.es/ecma262/#sec-getmethod
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> iterator_key = v8::Symbol::GetIterator(isolate);
  v8::Local<v8::Value> iterator_value;
  if (!value.As<v8::Object>()
           ->Get(isolate->GetCurrentContext(), iterator_key)
           .ToLocal(&iterator_value)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return false;
  }

  if (iterator_value->IsNullOrUndefined())
    return false;

  if (!iterator_value->IsFunction()) {
    exception_state.ThrowTypeError("@@iterator must be a function");
    return false;
  }

  return true;
}

Document* ToDocumentFromExecutionContext(ExecutionContext* execution_context) {
  return DynamicTo<LocalDOMWindow>(execution_context)->document();
}

ExecutionContext* ExecutionContextFromV8Wrappable(const Range* range) {
  return range->startContainer()->GetExecutionContext();
}

ExecutionContext* ExecutionContextFromV8Wrappable(const DOMParser* parser) {
  return parser->GetDocument() ? parser->GetDocument()->GetExecutionContext()
                               : nullptr;
}

v8::MaybeLocal<v8::Function> CreateNamedConstructorFunction(
    ScriptState* script_state,
    v8::FunctionCallback callback,
    const char* func_name,
    int func_length,
    const WrapperTypeInfo* wrapper_type_info) {
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);
  const void* callback_key = reinterpret_cast<const void*>(callback);

  // Named constructors are not interface objcets (despite that they're
  // pretending so), but we reuse the cache of interface objects, which just
  // works because both are V8 function template.
  v8::Local<v8::FunctionTemplate> function_template =
      per_isolate_data->FindInterfaceTemplate(world, callback_key);
  if (function_template.IsEmpty()) {
    function_template = v8::FunctionTemplate::New(
        isolate, callback, v8::Local<v8::Value>(), v8::Local<v8::Signature>(),
        func_length, v8::ConstructorBehavior::kAllow,
        v8::SideEffectType::kHasSideEffect);
    v8::Local<v8::FunctionTemplate> interface_template =
        wrapper_type_info->DomTemplate(isolate, world);
    function_template->Inherit(interface_template);
    function_template->SetClassName(V8AtomicString(isolate, func_name));
    function_template->InstanceTemplate()->SetInternalFieldCount(
        kV8DefaultWrapperInternalFieldCount);
    per_isolate_data->SetInterfaceTemplate(world, callback_key,
                                           function_template);
  }

  v8::Local<v8::Context> context = script_state->GetContext();
  V8PerContextData* per_context_data = V8PerContextData::From(context);
  v8::Local<v8::Function> function;
  if (!function_template->GetFunction(context).ToLocal(&function)) {
    return v8::MaybeLocal<v8::Function>();
  }
  v8::Local<v8::Object> prototype_object =
      per_context_data->PrototypeForType(wrapper_type_info);
  bool did_define;
  if (!function
           ->DefineOwnProperty(
               context, V8AtomicString(isolate, "prototype"), prototype_object,
               static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum |
                                                  v8::DontDelete))
           .To(&did_define)) {
    return v8::MaybeLocal<v8::Function>();
  }
  CHECK(did_define);
  return function;
}

void InstallUnscopablePropertyNames(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> prototype_object,
    base::span<const char* const> property_name_table) {
  // 3.6.3. Interface prototype object
  // https://heycam.github.io/webidl/#interface-prototype-object
  // step 8. If interface has any member declared with the [Unscopable]
  //   extended attribute, then:
  // step 8.1. Let unscopableObject be the result of performing
  //   ! ObjectCreate(null).
  v8::Local<v8::Object> unscopable_object =
      v8::Object::New(isolate, v8::Null(isolate), nullptr, nullptr, 0);
  for (const char* const property_name : property_name_table) {
    // step 8.2.2. Perform ! CreateDataProperty(unscopableObject, id, true).
    unscopable_object
        ->CreateDataProperty(context, V8AtomicString(isolate, property_name),
                             v8::True(isolate))
        .ToChecked();
  }
  // step 8.3. Let desc be the PropertyDescriptor{[[Value]]: unscopableObject,
  //   [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true}.
  // step 8.4. Perform ! DefinePropertyOrThrow(interfaceProtoObj,
  //   @@unscopables, desc).
  prototype_object
      ->DefineOwnProperty(
          context, v8::Symbol::GetUnscopables(isolate), unscopable_object,
          static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum))
      .ToChecked();
}

v8::Local<v8::Array> EnumerateIndexedProperties(v8::Isolate* isolate,
                                                uint32_t length) {
  Vector<v8::Local<v8::Value>> elements;
  elements.ReserveCapacity(length);
  for (uint32_t i = 0; i < length; ++i)
    elements.UncheckedAppend(v8::Integer::New(isolate, i));
  return v8::Array::New(isolate, elements.data(), elements.size());
}

}  // namespace bindings

}  // namespace blink

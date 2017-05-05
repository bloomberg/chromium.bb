// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8HTMLConstructor.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptCustomElementDefinition.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8HTMLElement.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/custom/CustomElementRegistry.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8DOMWrapper.h"
#include "platform/bindings/V8PerContextData.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

// https://html.spec.whatwg.org/multipage/dom.html#html-element-constructors
void V8HTMLConstructor::HtmlConstructor(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const WrapperTypeInfo& wrapper_type_info,
    const HTMLElementType element_interface_name) {
  TRACE_EVENT0("blink", "HTMLConstructor");
  DCHECK(info.IsConstructCall());

  v8::Isolate* isolate = info.GetIsolate();
  ScriptState* script_state = ScriptState::Current(isolate);
  v8::Local<v8::Value> new_target = info.NewTarget();

  if (!script_state->ContextIsValid()) {
    V8ThrowException::ThrowError(isolate, "The context has been destroyed");
    return;
  }

  if (!RuntimeEnabledFeatures::customElementsV1Enabled() ||
      !script_state->World().IsMainWorld()) {
    V8ThrowException::ThrowTypeError(isolate, "Illegal constructor");
    return;
  }

  // 2. If NewTarget is equal to the active function object, then
  // throw a TypeError and abort these steps.
  v8::Local<v8::Function> active_function_object =
      script_state->PerContextData()->ConstructorForType(
          &V8HTMLElement::wrapperTypeInfo);
  if (new_target == active_function_object) {
    V8ThrowException::ThrowTypeError(isolate, "Illegal constructor");
    return;
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  CustomElementRegistry* registry = window->customElements();

  // 3. Let definition be the entry in registry with constructor equal to
  // NewTarget.
  // If there is no such definition, then throw a TypeError and abort these
  // steps.
  ScriptCustomElementDefinition* definition =
      ScriptCustomElementDefinition::ForConstructor(script_state, registry,
                                                    new_target);
  if (!definition) {
    V8ThrowException::ThrowTypeError(isolate, "Illegal constructor");
    return;
  }

  const AtomicString& local_name = definition->Descriptor().LocalName();
  const AtomicString& name = definition->Descriptor().GetName();

  if (local_name == name) {
    // Autonomous custom element
    // 4.1. If the active function object is not HTMLElement, then throw a
    // TypeError
    if (!V8HTMLElement::wrapperTypeInfo.Equals(&wrapper_type_info)) {
      V8ThrowException::ThrowTypeError(isolate,
                                       "Illegal constructor: autonomous custom "
                                       "elements must extend HTMLElement");
      return;
    }
  } else {
    // Customized built-in element
    // 5. If local name is not valid for interface, throw TypeError
    if (htmlElementTypeForTag(local_name) != element_interface_name) {
      V8ThrowException::ThrowTypeError(isolate,
                                       "Illegal constructor: localName does "
                                       "not match the HTML element interface");
      return;
    }
  }

  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "HTMLElement");
  v8::TryCatch try_catch(isolate);

  // 6. Let prototype be Get(NewTarget, "prototype"). Rethrow any exceptions.
  v8::Local<v8::Value> prototype;
  v8::Local<v8::String> prototype_string = V8AtomicString(isolate, "prototype");
  if (!new_target.As<v8::Object>()
           ->Get(script_state->GetContext(), prototype_string)
           .ToLocal(&prototype)) {
    return;
  }

  // 7. If Type(prototype) is not Object, then: ...
  if (!prototype->IsObject()) {
    if (V8PerContextData* per_context_data = V8PerContextData::From(
            new_target.As<v8::Object>()->CreationContext())) {
      prototype =
          per_context_data->PrototypeForType(&V8HTMLElement::wrapperTypeInfo);
    } else {
      V8ThrowException::ThrowError(isolate, "The context has been destroyed");
      return;
    }
  }

  // 8. If definition's construction stack is empty...
  Element* element;
  if (definition->GetConstructionStack().IsEmpty()) {
    // This is an element being created with 'new' from script
    element = definition->CreateElementForConstructor(*window->document());
  } else {
    element = definition->GetConstructionStack().back();
    if (element) {
      // This is an element being upgraded that has called super
      definition->GetConstructionStack().back().Clear();
    } else {
      // During upgrade an element has invoked the same constructor
      // before calling 'super' and that invocation has poached the
      // element.
      exception_state.ThrowDOMException(kInvalidStateError,
                                        "this instance is already constructed");
      return;
    }
  }
  const WrapperTypeInfo* wrapper_type = element->GetWrapperTypeInfo();
  v8::Local<v8::Object> wrapper = V8DOMWrapper::AssociateObjectWithWrapper(
      isolate, element, wrapper_type, info.Holder());
  // If the element had a wrapper, we now update and return that
  // instead.
  V8SetReturnValue(info, wrapper);

  // 11. Perform element.[[SetPrototypeOf]](prototype). Rethrow any exceptions.
  // Note: I don't think this prototype set *can* throw exceptions.
  wrapper->SetPrototype(script_state->GetContext(), prototype.As<v8::Object>())
      .ToChecked();
}
}  // namespace blink

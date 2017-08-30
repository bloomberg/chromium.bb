// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptCustomElementDefinition.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8CustomElementRegistry.h"
#include "bindings/core/v8/V8Element.h"
#include "bindings/core/v8/V8ErrorHandler.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8ThrowDOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/ErrorEvent.h"
#include "core/html/HTMLElement.h"
#include "core/html/custom/CustomElement.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/wtf/Allocator.h"
#include "v8.h"

namespace blink {

ScriptCustomElementDefinition* ScriptCustomElementDefinition::ForConstructor(
    ScriptState* script_state,
    CustomElementRegistry* registry,
    const v8::Local<v8::Value>& constructor) {
  V8PerContextData* per_context_data = script_state->PerContextData();
  // TODO(yukishiino): Remove this check when crbug.com/583429 is fixed.
  if (UNLIKELY(!per_context_data))
    return nullptr;
  auto private_id = per_context_data->GetPrivateCustomElementDefinitionId();
  v8::Local<v8::Value> id_value;
  if (!constructor.As<v8::Object>()
           ->GetPrivate(script_state->GetContext(), private_id)
           .ToLocal(&id_value))
    return nullptr;
  if (!id_value->IsUint32())
    return nullptr;
  uint32_t id = id_value.As<v8::Uint32>()->Value();

  // This downcast is safe because only ScriptCustomElementDefinitions
  // have an ID associated with them. This relies on three things:
  //
  // 1. Only ScriptCustomElementDefinition::Create sets the private
  //    property on a constructor.
  //
  // 2. CustomElementRegistry adds ScriptCustomElementDefinitions
  //    assigned an ID to the list of definitions without fail.
  //
  // 3. The relationship between the CustomElementRegistry and its
  //    private property is never mixed up; this is guaranteed by the
  //    bindings system because the registry is associated with its
  //    context.
  //
  // At a meta-level, this downcast is safe because there is
  // currently only one implementation of CustomElementDefinition in
  // product code and that is ScriptCustomElementDefinition. But
  // that may change in the future.
  CustomElementDefinition* definition = registry->DefinitionForId(id);
  CHECK(definition);
  return static_cast<ScriptCustomElementDefinition*>(definition);
}

ScriptCustomElementDefinition* ScriptCustomElementDefinition::Create(
    ScriptState* script_state,
    CustomElementRegistry* registry,
    const CustomElementDescriptor& descriptor,
    CustomElementDefinition::Id id,
    const v8::Local<v8::Object>& constructor,
    const v8::Local<v8::Function>& connected_callback,
    const v8::Local<v8::Function>& disconnected_callback,
    const v8::Local<v8::Function>& adopted_callback,
    const v8::Local<v8::Function>& attribute_changed_callback,
    HashSet<AtomicString>&& observed_attributes) {
  ScriptCustomElementDefinition* definition = new ScriptCustomElementDefinition(
      script_state, descriptor, constructor, connected_callback,
      disconnected_callback, adopted_callback, attribute_changed_callback,
      std::move(observed_attributes));

  // Tag the JavaScript constructor object with its ID.
  v8::Local<v8::Value> id_value =
      v8::Integer::NewFromUnsigned(script_state->GetIsolate(), id);
  auto private_id =
      script_state->PerContextData()->GetPrivateCustomElementDefinitionId();
  CHECK(
      constructor->SetPrivate(script_state->GetContext(), private_id, id_value)
          .ToChecked());

  return definition;
}

ScriptCustomElementDefinition::ScriptCustomElementDefinition(
    ScriptState* script_state,
    const CustomElementDescriptor& descriptor,
    const v8::Local<v8::Object>& constructor,
    const v8::Local<v8::Function>& connected_callback,
    const v8::Local<v8::Function>& disconnected_callback,
    const v8::Local<v8::Function>& adopted_callback,
    const v8::Local<v8::Function>& attribute_changed_callback,
    HashSet<AtomicString>&& observed_attributes)
    : CustomElementDefinition(descriptor, std::move(observed_attributes)),
      script_state_(script_state),
      constructor_(script_state->GetIsolate(), this, constructor),
      connected_callback_(this),
      disconnected_callback_(this),
      adopted_callback_(this),
      attribute_changed_callback_(this) {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (!connected_callback.IsEmpty())
    connected_callback_.Set(isolate, connected_callback);
  if (!disconnected_callback.IsEmpty())
    disconnected_callback_.Set(isolate, disconnected_callback);
  if (!adopted_callback.IsEmpty())
    adopted_callback_.Set(isolate, adopted_callback);
  if (!attribute_changed_callback.IsEmpty())
    attribute_changed_callback_.Set(isolate, attribute_changed_callback);
}

DEFINE_TRACE_WRAPPERS(ScriptCustomElementDefinition) {
  visitor->TraceWrappers(constructor_.Cast<v8::Value>());
  visitor->TraceWrappers(connected_callback_.Cast<v8::Value>());
  visitor->TraceWrappers(disconnected_callback_.Cast<v8::Value>());
  visitor->TraceWrappers(adopted_callback_.Cast<v8::Value>());
  visitor->TraceWrappers(attribute_changed_callback_.Cast<v8::Value>());
}

static void DispatchErrorEvent(v8::Isolate* isolate,
                               v8::Local<v8::Value> exception,
                               v8::Local<v8::Object> constructor) {
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  V8ScriptRunner::ThrowException(
      isolate, exception, constructor.As<v8::Function>()->GetScriptOrigin());
}

HTMLElement* ScriptCustomElementDefinition::HandleCreateElementSyncException(
    Document& document,
    const QualifiedName& tag_name,
    v8::Isolate* isolate,
    ExceptionState& exception_state) {
  DCHECK(exception_state.HadException());
  // 6.1."If any of these subsubsteps threw an exception".1
  // Report the exception.
  DispatchErrorEvent(isolate, exception_state.GetException(), Constructor());
  exception_state.ClearException();
  // ... .2 Return HTMLUnknownElement.
  return CustomElement::CreateFailedElement(document, tag_name);
}

HTMLElement* ScriptCustomElementDefinition::CreateElementSync(
    Document& document,
    const QualifiedName& tag_name) {
  if (!script_state_->ContextIsValid())
    return CustomElement::CreateFailedElement(document, tag_name);
  ScriptState::Scope scope(script_state_.Get());
  v8::Isolate* isolate = script_state_->GetIsolate();

  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "CustomElement");

  // Create an element with the synchronous custom elements flag set.
  // https://dom.spec.whatwg.org/#concept-create-element

  // TODO(dominicc): Implement step 5 which constructs customized
  // built-in elements.

  Element* element = nullptr;
  {
    v8::TryCatch try_catch(script_state_->GetIsolate());

    if (document.IsHTMLImport()) {
      // V8HTMLElement::constructorCustom() can only refer to
      // window.document() which is not the import document. Create
      // elements in import documents ahead of time so they end up in
      // the right document. This subtly violates recursive
      // construction semantics, but only in import documents.
      element = CreateElementForConstructor(document);
      DCHECK(!try_catch.HasCaught());

      ConstructionStackScope construction_stack_scope(this, element);
      element = CallConstructor();
    } else {
      element = CallConstructor();
    }

    if (try_catch.HasCaught()) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return HandleCreateElementSyncException(document, tag_name, isolate,
                                              exception_state);
    }
  }

  // 6.1.3. through 6.1.9.
  CheckConstructorResult(element, document, tag_name, exception_state);
  if (exception_state.HadException()) {
    return HandleCreateElementSyncException(document, tag_name, isolate,
                                            exception_state);
  }
  DCHECK_EQ(element->GetCustomElementState(), CustomElementState::kCustom);
  return ToHTMLElement(element);
}

// https://html.spec.whatwg.org/multipage/scripting.html#upgrades
bool ScriptCustomElementDefinition::RunConstructor(Element* element) {
  if (!script_state_->ContextIsValid())
    return false;
  ScriptState::Scope scope(script_state_.Get());
  v8::Isolate* isolate = script_state_->GetIsolate();

  // Step 5 says to rethrow the exception; but there is no one to
  // catch it. The side effect is to report the error.
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  Element* result = CallConstructor();

  // To report exception thrown from callConstructor()
  if (try_catch.HasCaught())
    return false;

  // To report InvalidStateError Exception, when the constructor returns some
  // different object
  if (result != element) {
    const String& message =
        "custom element constructors must call super() first and must "
        "not return a different object";
    v8::Local<v8::Value> exception = V8ThrowDOMException::CreateDOMException(
        script_state_->GetIsolate(), kInvalidStateError, message);
    DispatchErrorEvent(isolate, exception, Constructor());
    return false;
  }

  return true;
}

Element* ScriptCustomElementDefinition::CallConstructor() {
  v8::Isolate* isolate = script_state_->GetIsolate();
  DCHECK(ScriptState::Current(isolate) == script_state_);
  ExecutionContext* execution_context =
      ExecutionContext::From(script_state_.Get());
  v8::Local<v8::Value> result;
  if (!V8ScriptRunner::CallAsConstructor(isolate, Constructor(),
                                         execution_context, 0, nullptr)
           .ToLocal(&result)) {
    return nullptr;
  }
  return V8Element::toImplWithTypeCheck(isolate, result);
}

v8::Local<v8::Object> ScriptCustomElementDefinition::Constructor() const {
  DCHECK(!constructor_.IsEmpty());
  return constructor_.NewLocal(script_state_->GetIsolate());
}

// CustomElementDefinition
ScriptValue ScriptCustomElementDefinition::GetConstructorForScript() {
  return ScriptValue(script_state_.Get(), Constructor());
}

bool ScriptCustomElementDefinition::HasConnectedCallback() const {
  return !connected_callback_.IsEmpty();
}

bool ScriptCustomElementDefinition::HasDisconnectedCallback() const {
  return !disconnected_callback_.IsEmpty();
}

bool ScriptCustomElementDefinition::HasAdoptedCallback() const {
  return !adopted_callback_.IsEmpty();
}

void ScriptCustomElementDefinition::RunCallback(
    v8::Local<v8::Function> callback,
    Element* element,
    int argc,
    v8::Local<v8::Value> argv[]) {
  DCHECK(ScriptState::Current(script_state_->GetIsolate()) == script_state_);
  v8::Isolate* isolate = script_state_->GetIsolate();

  // Invoke custom element reactions
  // https://html.spec.whatwg.org/multipage/scripting.html#invoke-custom-element-reactions
  // If this throws any exception, then report the exception.
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  ExecutionContext* execution_context =
      ExecutionContext::From(script_state_.Get());
  v8::Local<v8::Value> element_handle =
      ToV8(element, script_state_->GetContext()->Global(), isolate);
  V8ScriptRunner::CallFunction(callback, execution_context, element_handle,
                               argc, argv, isolate);
}

void ScriptCustomElementDefinition::RunConnectedCallback(Element* element) {
  if (!script_state_->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state_.Get());
  v8::Isolate* isolate = script_state_->GetIsolate();
  RunCallback(connected_callback_.NewLocal(isolate), element);
}

void ScriptCustomElementDefinition::RunDisconnectedCallback(Element* element) {
  if (!script_state_->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state_.Get());
  v8::Isolate* isolate = script_state_->GetIsolate();
  RunCallback(disconnected_callback_.NewLocal(isolate), element);
}

void ScriptCustomElementDefinition::RunAdoptedCallback(Element* element,
                                                       Document* old_owner,
                                                       Document* new_owner) {
  if (!script_state_->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state_.Get());
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Value> argv[] = {
      ToV8(old_owner, script_state_->GetContext()->Global(), isolate),
      ToV8(new_owner, script_state_->GetContext()->Global(), isolate)};
  RunCallback(adopted_callback_.NewLocal(isolate), element,
              WTF_ARRAY_LENGTH(argv), argv);
}

void ScriptCustomElementDefinition::RunAttributeChangedCallback(
    Element* element,
    const QualifiedName& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  if (!script_state_->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state_.Get());
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Value> argv[] = {
      V8String(isolate, name.LocalName()), V8StringOrNull(isolate, old_value),
      V8StringOrNull(isolate, new_value),
      V8StringOrNull(isolate, name.NamespaceURI()),
  };
  RunCallback(attribute_changed_callback_.NewLocal(isolate), element,
              WTF_ARRAY_LENGTH(argv), argv);
}

}  // namespace blink

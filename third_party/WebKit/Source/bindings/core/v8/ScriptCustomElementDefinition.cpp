// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptCustomElementDefinition.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8CustomElementRegistry.h"
#include "bindings/core/v8/V8Element.h"
#include "bindings/core/v8/V8ErrorHandler.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8ThrowDOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/custom/CustomElement.h"
#include "core/events/ErrorEvent.h"
#include "core/html/HTMLElement.h"
#include "core/html/imports/HTMLImportsController.h"
#include "platform/wtf/Allocator.h"
#include "v8.h"

namespace blink {

// Retrieves the custom elements constructor -> name map, creating it
// if necessary.
static v8::Local<v8::Map> EnsureCustomElementRegistryMap(
    ScriptState* script_state,
    CustomElementRegistry* registry) {
  CHECK(script_state->World().IsMainWorld());
  v8::Isolate* isolate = script_state->GetIsolate();

  V8PrivateProperty::Symbol symbol =
      V8PrivateProperty::GetCustomElementRegistryMap(isolate);
  v8::Local<v8::Object> wrapper = ToV8(registry, script_state).As<v8::Object>();
  v8::Local<v8::Value> map = symbol.GetOrUndefined(wrapper);
  if (map->IsUndefined()) {
    map = v8::Map::New(isolate);
    symbol.Set(wrapper, map);
  }
  return map.As<v8::Map>();
}

ScriptCustomElementDefinition* ScriptCustomElementDefinition::ForConstructor(
    ScriptState* script_state,
    CustomElementRegistry* registry,
    const v8::Local<v8::Value>& constructor) {
  v8::Local<v8::Map> map =
      EnsureCustomElementRegistryMap(script_state, registry);
  v8::Local<v8::Value> name_value =
      map->Get(script_state->GetContext(), constructor).ToLocalChecked();
  if (!name_value->IsString())
    return nullptr;
  AtomicString name = ToCoreAtomicString(name_value.As<v8::String>());

  // This downcast is safe because only
  // ScriptCustomElementDefinitions have a name associated with a V8
  // constructor in the map; see
  // ScriptCustomElementDefinition::create. This relies on three
  // things:
  //
  // 1. Only ScriptCustomElementDefinition adds entries to the map.
  //    Audit the use of private properties in general and how the
  //    map is handled--it should never be leaked to script.
  //
  // 2. CustomElementRegistry does not overwrite definitions with a
  //    given name--see the CHECK in CustomElementRegistry::define
  //    --and adds ScriptCustomElementDefinitions to the map without
  //    fail.
  //
  // 3. The relationship between the CustomElementRegistry and its
  //    map is never mixed up; this is guaranteed by the bindings
  //    system which provides a stable wrapper, and the map hangs
  //    off the wrapper.
  //
  // At a meta-level, this downcast is safe because there is
  // currently only one implementation of CustomElementDefinition in
  // product code and that is ScriptCustomElementDefinition. But
  // that may change in the future.
  CustomElementDefinition* definition = registry->DefinitionForName(name);
  CHECK(definition);
  return static_cast<ScriptCustomElementDefinition*>(definition);
}

using SymbolGetter = V8PrivateProperty::Symbol (*)(v8::Isolate*);

template <typename T>
static void KeepAlive(v8::Local<v8::Object>& object,
                      SymbolGetter symbol_getter,
                      const v8::Local<T>& value,
                      ScopedPersistent<T>& persistent,
                      ScriptState* script_state) {
  if (value.IsEmpty())
    return;

  v8::Isolate* isolate = script_state->GetIsolate();
  symbol_getter(isolate).Set(object, value);
  persistent.Set(isolate, value);
  persistent.SetPhantom();
}

ScriptCustomElementDefinition* ScriptCustomElementDefinition::Create(
    ScriptState* script_state,
    CustomElementRegistry* registry,
    const CustomElementDescriptor& descriptor,
    const v8::Local<v8::Object>& constructor,
    const v8::Local<v8::Function>& connected_callback,
    const v8::Local<v8::Function>& disconnected_callback,
    const v8::Local<v8::Function>& adopted_callback,
    const v8::Local<v8::Function>& attribute_changed_callback,
    const HashSet<AtomicString>& observed_attributes) {
  ScriptCustomElementDefinition* definition = new ScriptCustomElementDefinition(
      script_state, descriptor, constructor, connected_callback,
      disconnected_callback, adopted_callback, attribute_changed_callback,
      observed_attributes);

  // Add a constructor -> name mapping to the registry.
  v8::Local<v8::Value> name_value =
      V8String(script_state->GetIsolate(), descriptor.GetName());
  v8::Local<v8::Map> map =
      EnsureCustomElementRegistryMap(script_state, registry);
  map->Set(script_state->GetContext(), constructor, name_value)
      .ToLocalChecked();
  definition->constructor_.SetPhantom();

  // We add the callbacks here to keep them alive. We use the name as
  // the key because it is unique per-registry.
  v8::Local<v8::Object> object = v8::Object::New(script_state->GetIsolate());
  KeepAlive(object, V8PrivateProperty::GetCustomElementConnectedCallback,
            connected_callback, definition->connected_callback_, script_state);
  KeepAlive(object, V8PrivateProperty::GetCustomElementDisconnectedCallback,
            disconnected_callback, definition->disconnected_callback_,
            script_state);
  KeepAlive(object, V8PrivateProperty::GetCustomElementAdoptedCallback,
            adopted_callback, definition->adopted_callback_, script_state);
  KeepAlive(object, V8PrivateProperty::GetCustomElementAttributeChangedCallback,
            attribute_changed_callback, definition->attribute_changed_callback_,
            script_state);
  map->Set(script_state->GetContext(), name_value, object).ToLocalChecked();

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
    const HashSet<AtomicString>& observed_attributes)
    : CustomElementDefinition(descriptor, observed_attributes),
      script_state_(script_state),
      constructor_(script_state->GetIsolate(), constructor) {}

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

    bool is_import_document =
        document.ImportsController() &&
        document.ImportsController()->Master() != document;
    if (is_import_document) {
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
  if (!V8Call(V8ScriptRunner::CallAsConstructor(isolate, Constructor(),
                                                execution_context, 0, nullptr),
              result)) {
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

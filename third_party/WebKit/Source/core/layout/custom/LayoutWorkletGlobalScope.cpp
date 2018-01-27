// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/LayoutWorkletGlobalScope.h"

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/CSSPropertyNames.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/layout/custom/CSSLayoutDefinition.h"
#include "core/layout/custom/DocumentLayoutDefinition.h"
#include "core/layout/custom/LayoutWorklet.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/GlobalScopeCreationParams.h"

namespace blink {

namespace {

bool ParsePropertyList(v8::Isolate* isolate,
                       v8::Local<v8::Context> context,
                       v8::Local<v8::Function> constructor,
                       const AtomicString list_value_name,
                       Vector<CSSPropertyID>* native_invalidation_properties,
                       Vector<AtomicString>* custom_invalidation_properties,
                       ExceptionState* exception_state,
                       v8::TryCatch* block) {
  v8::Local<v8::Value> list_value;
  if (!constructor->Get(context, V8AtomicString(isolate, list_value_name))
           .ToLocal(&list_value)) {
    exception_state->RethrowV8Exception(block->Exception());
    return false;
  }

  if (!list_value->IsNullOrUndefined()) {
    Vector<String> properties =
        NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
            isolate, list_value, *exception_state);

    if (exception_state->HadException())
      return false;

    for (const auto& property : properties) {
      CSSPropertyID property_id = cssPropertyID(property);
      if (property_id == CSSPropertyVariable) {
        custom_invalidation_properties->push_back(std::move(property));
      } else if (property_id != CSSPropertyInvalid) {
        native_invalidation_properties->push_back(std::move(property_id));
      }
    }
  }
  return true;
}

bool ParsePrototype(v8::Isolate* isolate,
                    v8::Local<v8::Context> context,
                    v8::Local<v8::Function> constructor,
                    v8::Local<v8::Object>* prototype,
                    ExceptionState* exception_state,
                    v8::TryCatch* block) {
  v8::Local<v8::Value> prototype_value;
  if (!constructor->Get(context, V8AtomicString(isolate, "prototype"))
           .ToLocal(&prototype_value)) {
    exception_state->RethrowV8Exception(block->Exception());
    return false;
  }

  if (prototype_value->IsNullOrUndefined()) {
    exception_state->ThrowTypeError(
        "The 'prototype' object on the class does not exist.");
    return false;
  }

  if (!prototype_value->IsObject()) {
    exception_state->ThrowTypeError(
        "The 'prototype' property on the class is not an object.");
    return false;
  }

  *prototype = v8::Local<v8::Object>::Cast(prototype_value);
  return true;
}

bool ParseGeneratorFunction(v8::Isolate* isolate,
                            v8::Local<v8::Context> context,
                            v8::Local<v8::Object> prototype,
                            const AtomicString function_name,
                            v8::Local<v8::Function>* function,
                            ExceptionState* exception_state,
                            v8::TryCatch* block) {
  v8::Local<v8::Value> function_value;
  if (!prototype->Get(context, V8AtomicString(isolate, function_name))
           .ToLocal(&function_value)) {
    exception_state->RethrowV8Exception(block->Exception());
    return false;
  }

  if (function_value->IsNullOrUndefined()) {
    exception_state->ThrowTypeError(
        "The '" + function_name +
        "' property on the prototype does not exist.");
    return false;
  }

  if (!function_value->IsGeneratorFunction()) {
    exception_state->ThrowTypeError(
        "The '" + function_name +
        "' property on the prototype is not a generator function.");
    return false;
  }

  *function = v8::Local<v8::Function>::Cast(function_value);
  return true;
}

}  // namespace

// static
LayoutWorkletGlobalScope* LayoutWorkletGlobalScope::Create(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    size_t global_scope_number) {
  auto* global_scope = new LayoutWorkletGlobalScope(
      frame, std::move(creation_params), reporting_proxy);
  String context_name("LayoutWorklet #");
  context_name.append(String::Number(global_scope_number));
  global_scope->ScriptController()->InitializeContextIfNeeded(context_name);
  MainThreadDebugger::Instance()->ContextCreated(
      global_scope->ScriptController()->GetScriptState(),
      global_scope->GetFrame(), global_scope->GetSecurityOrigin());
  return global_scope;
}

LayoutWorkletGlobalScope::LayoutWorkletGlobalScope(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy)
    : MainThreadWorkletGlobalScope(frame,
                                   std::move(creation_params),
                                   reporting_proxy) {}

LayoutWorkletGlobalScope::~LayoutWorkletGlobalScope() = default;

void LayoutWorkletGlobalScope::Dispose() {
  MainThreadDebugger::Instance()->ContextWillBeDestroyed(
      ScriptController()->GetScriptState());

  MainThreadWorkletGlobalScope::Dispose();
}

// https://drafts.css-houdini.org/css-layout-api/#dom-layoutworkletglobalscope-registerlayout
void LayoutWorkletGlobalScope::registerLayout(const String& name,
                                              const ScriptValue& ctor_value,
                                              ExceptionState& exception_state) {
  if (name.IsEmpty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  if (layout_definitions_.Contains(name)) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  v8::Isolate* isolate = ScriptController()->GetScriptState()->GetIsolate();
  v8::Local<v8::Context> context = ScriptController()->GetContext();
  DCHECK(ctor_value.V8Value()->IsFunction());
  v8::Local<v8::Function> constructor =
      v8::Local<v8::Function>::Cast(ctor_value.V8Value());

  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties;

  v8::TryCatch block(isolate);
  if (!ParsePropertyList(isolate, context, constructor, "inputProperties",
                         &native_invalidation_properties,
                         &custom_invalidation_properties, &exception_state,
                         &block))
    return;

  Vector<CSSPropertyID> child_native_invalidation_properties;
  Vector<AtomicString> child_custom_invalidation_properties;

  if (!ParsePropertyList(isolate, context, constructor, "childInputProperties",
                         &child_native_invalidation_properties,
                         &child_custom_invalidation_properties,
                         &exception_state, &block))
    return;

  v8::Local<v8::Object> prototype;
  if (!ParsePrototype(isolate, context, constructor, &prototype,
                      &exception_state, &block))
    return;

  v8::Local<v8::Function> intrinsic_sizes;
  if (!ParseGeneratorFunction(isolate, context, prototype, "intrinsicSizes",
                              &intrinsic_sizes, &exception_state, &block))
    return;

  v8::Local<v8::Function> layout;
  if (!ParseGeneratorFunction(isolate, context, prototype, "layout", &layout,
                              &exception_state, &block))
    return;

  CSSLayoutDefinition* definition = new CSSLayoutDefinition(
      ScriptController()->GetScriptState(), constructor, intrinsic_sizes,
      layout, native_invalidation_properties, custom_invalidation_properties,
      child_native_invalidation_properties,
      child_custom_invalidation_properties);
  layout_definitions_.Set(name, definition);

  LayoutWorklet* layout_worklet =
      LayoutWorklet::From(*GetFrame()->GetDocument()->domWindow());
  LayoutWorklet::DocumentDefinitionMap* document_definition_map =
      layout_worklet->GetDocumentDefinitionMap();
  if (document_definition_map->Contains(name)) {
    DocumentLayoutDefinition* existing_document_definition =
        document_definition_map->at(name);
    if (existing_document_definition == kInvalidDocumentLayoutDefinition)
      return;
    if (!existing_document_definition->RegisterAdditionalLayoutDefinition(
            *definition)) {
      document_definition_map->Set(name, kInvalidDocumentLayoutDefinition);
      exception_state.ThrowDOMException(kNotSupportedError,
                                        "A class with name:'" + name +
                                            "' was registered with a "
                                            "different definition.");
      return;
    }

    // TODO(ikilpatrick): Notify the pending layout objects awaiting for this
    // class to be registered.
  } else {
    DocumentLayoutDefinition* document_definition =
        new DocumentLayoutDefinition(definition);
    document_definition_map->Set(name, document_definition);
  }
}

CSSLayoutDefinition* LayoutWorkletGlobalScope::FindDefinition(
    const String& name) {
  return layout_definitions_.at(name);
}

void LayoutWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(layout_definitions_);
  MainThreadWorkletGlobalScope::Trace(visitor);
}

void LayoutWorkletGlobalScope::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  for (auto definition : layout_definitions_)
    visitor->TraceWrappers(definition.value);
  MainThreadWorkletGlobalScope::TraceWrappers(visitor);
}

}  // namespace blink

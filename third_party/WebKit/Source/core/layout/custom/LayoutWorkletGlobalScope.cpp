// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/LayoutWorkletGlobalScope.h"

#include "bindings/core/v8/V8ObjectParser.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/css_property_names.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/layout/custom/CSSLayoutDefinition.h"
#include "core/layout/custom/DocumentLayoutDefinition.h"
#include "core/layout/custom/LayoutWorklet.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/GlobalScopeCreationParams.h"

namespace blink {

// static
LayoutWorkletGlobalScope* LayoutWorkletGlobalScope::Create(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    PendingLayoutRegistry* pending_layout_registry,
    size_t global_scope_number) {
  auto* global_scope =
      new LayoutWorkletGlobalScope(frame, std::move(creation_params),
                                   reporting_proxy, pending_layout_registry);
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
    WorkerReportingProxy& reporting_proxy,
    PendingLayoutRegistry* pending_layout_registry)
    : MainThreadWorkletGlobalScope(frame,
                                   std::move(creation_params),
                                   reporting_proxy),
      pending_layout_registry_(pending_layout_registry) {}

LayoutWorkletGlobalScope::~LayoutWorkletGlobalScope() = default;

void LayoutWorkletGlobalScope::Dispose() {
  MainThreadDebugger::Instance()->ContextWillBeDestroyed(
      ScriptController()->GetScriptState());

  MainThreadWorkletGlobalScope::Dispose();
}

// https://drafts.css-houdini.org/css-layout-api/#dom-layoutworkletglobalscope-registerlayout
void LayoutWorkletGlobalScope::registerLayout(
    const AtomicString& name,
    const ScriptValue& constructor_value,
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

  v8::Local<v8::Context> context = ScriptController()->GetContext();

  DCHECK(constructor_value.V8Value()->IsFunction());
  v8::Local<v8::Function> constructor =
      v8::Local<v8::Function>::Cast(constructor_value.V8Value());

  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties;

  if (!V8ObjectParser::ParseCSSPropertyList(
          context, constructor, "inputProperties",
          &native_invalidation_properties, &custom_invalidation_properties,
          &exception_state))
    return;

  Vector<CSSPropertyID> child_native_invalidation_properties;
  Vector<AtomicString> child_custom_invalidation_properties;

  if (!V8ObjectParser::ParseCSSPropertyList(
          context, constructor, "childInputProperties",
          &child_native_invalidation_properties,
          &child_custom_invalidation_properties, &exception_state))
    return;

  v8::Local<v8::Object> prototype;
  if (!V8ObjectParser::ParsePrototype(context, constructor, &prototype,
                                      &exception_state))
    return;

  v8::Local<v8::Function> intrinsic_sizes;
  if (!V8ObjectParser::ParseGeneratorFunction(
          context, prototype, "intrinsicSizes", &intrinsic_sizes,
          &exception_state))
    return;

  v8::Local<v8::Function> layout;
  if (!V8ObjectParser::ParseGeneratorFunction(context, prototype, "layout",
                                              &layout, &exception_state))
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

    // Notify all of the pending layouts that all of the layout classes with
    // |name| have been registered and are ready to use.
    if (existing_document_definition->GetRegisteredDefinitionCount() ==
        LayoutWorklet::kNumGlobalScopes)
      pending_layout_registry_->NotifyLayoutReady(name);
  } else {
    DocumentLayoutDefinition* document_definition =
        new DocumentLayoutDefinition(definition);
    document_definition_map->Set(name, document_definition);
  }
}

CSSLayoutDefinition* LayoutWorkletGlobalScope::FindDefinition(
    const AtomicString& name) {
  return layout_definitions_.at(name);
}

void LayoutWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(layout_definitions_);
  visitor->Trace(pending_layout_registry_);
  MainThreadWorkletGlobalScope::Trace(visitor);
}

void LayoutWorkletGlobalScope::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  for (auto definition : layout_definitions_)
    visitor->TraceWrappers(definition.value);
  MainThreadWorkletGlobalScope::TraceWrappers(visitor);
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorkletGlobalScope.h"

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8ObjectParser.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "bindings/modules/v8/V8PaintRenderingContext2DSettings.h"
#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css_property_names.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"
#include "modules/csspaint/CSSPaintWorklet.h"
#include "modules/csspaint/PaintWorklet.h"
#include "platform/bindings/V8BindingMacros.h"

namespace blink {

namespace {

bool ParseInputArguments(v8::Local<v8::Context> context,
                         v8::Local<v8::Function> constructor,
                         Vector<CSSSyntaxDescriptor>* input_argument_types,
                         ExceptionState* exception_state) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch block(isolate);

  if (RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled()) {
    v8::Local<v8::Value> input_argument_type_values;
    if (!constructor->Get(context, V8AtomicString(isolate, "inputArguments"))
             .ToLocal(&input_argument_type_values)) {
      exception_state->RethrowV8Exception(block.Exception());
      return false;
    }

    if (!input_argument_type_values->IsNullOrUndefined()) {
      Vector<String> argument_types =
          NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
              isolate, input_argument_type_values, *exception_state);

      if (exception_state->HadException())
        return false;

      for (const auto& type : argument_types) {
        CSSSyntaxDescriptor syntax_descriptor(type);
        if (!syntax_descriptor.IsValid()) {
          exception_state->ThrowTypeError("Invalid argument types.");
          return false;
        }
        input_argument_types->push_back(std::move(syntax_descriptor));
      }
    }
  }
  return true;
}

bool ParsePaintRenderingContext2DSettings(
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> constructor,
    PaintRenderingContext2DSettings* context_settings,
    ExceptionState* exception_state) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch block(isolate);

  v8::Local<v8::Value> context_settings_value;
  if (!constructor->Get(context, V8AtomicString(isolate, "contextOptions"))
           .ToLocal(&context_settings_value)) {
    exception_state->RethrowV8Exception(block.Exception());
    return false;
  }
  V8PaintRenderingContext2DSettings::ToImpl(
      isolate, context_settings_value, *context_settings, *exception_state);
  if (exception_state->HadException())
    return false;
  return true;
}

}  // namespace

// static
PaintWorkletGlobalScope* PaintWorkletGlobalScope::Create(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    PaintWorkletPendingGeneratorRegistry* pending_generator_registry,
    size_t global_scope_number) {
  auto* global_scope =
      new PaintWorkletGlobalScope(frame, std::move(creation_params),
                                  reporting_proxy, pending_generator_registry);
  String context_name("PaintWorklet #");
  context_name.append(String::Number(global_scope_number));
  global_scope->ScriptController()->InitializeContextIfNeeded(context_name);
  MainThreadDebugger::Instance()->ContextCreated(
      global_scope->ScriptController()->GetScriptState(),
      global_scope->GetFrame(), global_scope->GetSecurityOrigin());
  return global_scope;
}

PaintWorkletGlobalScope::PaintWorkletGlobalScope(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    PaintWorkletPendingGeneratorRegistry* pending_generator_registry)
    : MainThreadWorkletGlobalScope(frame,
                                   std::move(creation_params),
                                   reporting_proxy),
      pending_generator_registry_(pending_generator_registry) {}

PaintWorkletGlobalScope::~PaintWorkletGlobalScope() = default;

void PaintWorkletGlobalScope::Dispose() {
  MainThreadDebugger::Instance()->ContextWillBeDestroyed(
      ScriptController()->GetScriptState());

  pending_generator_registry_ = nullptr;
  MainThreadWorkletGlobalScope::Dispose();
}

void PaintWorkletGlobalScope::registerPaint(
    const String& name,
    const ScriptValue& constructor_value,
    ExceptionState& exception_state) {
  if (name.IsEmpty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  if (paint_definitions_.Contains(name)) {
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

  // Get input argument types. Parse the argument type values only when
  // cssPaintAPIArguments is enabled.
  Vector<CSSSyntaxDescriptor> input_argument_types;
  if (!ParseInputArguments(context, constructor, &input_argument_types,
                           &exception_state))
    return;

  PaintRenderingContext2DSettings context_settings;
  if (!ParsePaintRenderingContext2DSettings(
          context, constructor, &context_settings, &exception_state))
    return;

  v8::Local<v8::Object> prototype;
  if (!V8ObjectParser::ParsePrototype(context, constructor, &prototype,
                                      &exception_state))
    return;

  v8::Local<v8::Function> paint;
  if (!V8ObjectParser::ParseFunction(context, prototype, "paint", &paint,
                                     &exception_state))
    return;

  CSSPaintDefinition* definition = CSSPaintDefinition::Create(
      ScriptController()->GetScriptState(), constructor, paint,
      native_invalidation_properties, custom_invalidation_properties,
      input_argument_types, context_settings);
  paint_definitions_.Set(name, definition);

  // TODO(xidachen): the following steps should be done with a postTask when
  // we move PaintWorklet off main thread.
  PaintWorklet* paint_worklet =
      PaintWorklet::From(*GetFrame()->GetDocument()->domWindow());
  PaintWorklet::DocumentDefinitionMap& document_definition_map =
      paint_worklet->GetDocumentDefinitionMap();
  if (document_definition_map.Contains(name)) {
    DocumentPaintDefinition* existing_document_definition =
        document_definition_map.at(name);
    if (existing_document_definition == kInvalidDocumentPaintDefinition)
      return;
    if (!existing_document_definition->RegisterAdditionalPaintDefinition(
            *definition)) {
      document_definition_map.Set(name, kInvalidDocumentPaintDefinition);
      exception_state.ThrowDOMException(
          kNotSupportedError,
          "A class with name:'" + name +
              "' was registered with a different definition.");
      return;
    }
    // Notify the generator ready only when register paint is called the second
    // time with the same |name| (i.e. there is already a document definition
    // associated with |name|
    if (existing_document_definition->GetRegisteredDefinitionCount() ==
        PaintWorklet::kNumGlobalScopes)
      pending_generator_registry_->NotifyGeneratorReady(name);
  } else {
    DocumentPaintDefinition* document_definition =
        new DocumentPaintDefinition(definition);
    document_definition_map.Set(name, document_definition);
  }
}

CSSPaintDefinition* PaintWorkletGlobalScope::FindDefinition(
    const String& name) {
  return paint_definitions_.at(name);
}

double PaintWorkletGlobalScope::devicePixelRatio() const {
  return GetFrame()->DevicePixelRatio();
}

void PaintWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(paint_definitions_);
  visitor->Trace(pending_generator_registry_);
  MainThreadWorkletGlobalScope::Trace(visitor);
}

void PaintWorkletGlobalScope::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  for (auto definition : paint_definitions_)
    visitor->TraceWrappers(definition.value);
  MainThreadWorkletGlobalScope::TraceWrappers(visitor);
}

}  // namespace blink

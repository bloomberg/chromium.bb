// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorkletGlobalScope.h"

#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSSyntaxDescriptor.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/MainThreadDebugger.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"
#include "modules/csspaint/PaintWorklet.h"
#include "modules/csspaint/WindowPaintWorklet.h"
#include "platform/bindings/V8BindingMacros.h"

namespace blink {

// static
PaintWorkletGlobalScope* PaintWorkletGlobalScope::Create(
    LocalFrame* frame,
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate,
    PaintWorkletPendingGeneratorRegistry* pending_generator_registry,
    size_t global_scope_number) {
  PaintWorkletGlobalScope* paint_worklet_global_scope =
      new PaintWorkletGlobalScope(frame, url, user_agent,
                                  std::move(security_origin), isolate,
                                  pending_generator_registry);
  String context_name("PaintWorklet #");
  context_name.append(String::Number(global_scope_number));
  paint_worklet_global_scope->ScriptController()->InitializeContextIfNeeded(
      context_name);
  MainThreadDebugger::Instance()->ContextCreated(
      paint_worklet_global_scope->ScriptController()->GetScriptState(),
      paint_worklet_global_scope->GetFrame(),
      paint_worklet_global_scope->GetSecurityOrigin());
  return paint_worklet_global_scope;
}

PaintWorkletGlobalScope::PaintWorkletGlobalScope(
    LocalFrame* frame,
    const KURL& url,
    const String& user_agent,
    PassRefPtr<SecurityOrigin> security_origin,
    v8::Isolate* isolate,
    PaintWorkletPendingGeneratorRegistry* pending_generator_registry)
    : MainThreadWorkletGlobalScope(frame,
                                   url,
                                   user_agent,
                                   std::move(security_origin),
                                   isolate),
      pending_generator_registry_(pending_generator_registry) {}

PaintWorkletGlobalScope::~PaintWorkletGlobalScope() {}

void PaintWorkletGlobalScope::Dispose() {
  MainThreadDebugger::Instance()->ContextWillBeDestroyed(
      ScriptController()->GetScriptState());

  pending_generator_registry_ = nullptr;
  WorkletGlobalScope::Dispose();
}

void PaintWorkletGlobalScope::registerPaint(const String& name,
                                            const ScriptValue& ctor_value,
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

  v8::Isolate* isolate = ScriptController()->GetScriptState()->GetIsolate();
  v8::Local<v8::Context> context = ScriptController()->GetContext();

  DCHECK(ctor_value.V8Value()->IsFunction());
  v8::Local<v8::Function> constructor =
      v8::Local<v8::Function>::Cast(ctor_value.V8Value());

  v8::Local<v8::Value> input_properties_value;
  if (!constructor->Get(context, V8AtomicString(isolate, "inputProperties"))
           .ToLocal(&input_properties_value))
    return;

  Vector<CSSPropertyID> native_invalidation_properties;
  Vector<AtomicString> custom_invalidation_properties;

  if (!IsUndefinedOrNull(input_properties_value)) {
    Vector<String> properties =
        NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
            isolate, input_properties_value, exception_state);

    if (exception_state.HadException())
      return;

    for (const auto& property : properties) {
      CSSPropertyID property_id = cssPropertyID(property);
      if (property_id == CSSPropertyVariable) {
        custom_invalidation_properties.push_back(property);
      } else if (property_id != CSSPropertyInvalid) {
        native_invalidation_properties.push_back(property_id);
      }
    }
  }

  // Get input argument types. Parse the argument type values only when
  // cssPaintAPIArguments is enabled.
  Vector<CSSSyntaxDescriptor> input_argument_types;
  if (RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled()) {
    v8::Local<v8::Value> input_argument_type_values;
    if (!constructor->Get(context, V8AtomicString(isolate, "inputArguments"))
             .ToLocal(&input_argument_type_values))
      return;

    if (!IsUndefinedOrNull(input_argument_type_values)) {
      Vector<String> argument_types =
          NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
              isolate, input_argument_type_values, exception_state);

      if (exception_state.HadException())
        return;

      for (const auto& type : argument_types) {
        CSSSyntaxDescriptor syntax_descriptor(type);
        if (!syntax_descriptor.IsValid()) {
          exception_state.ThrowTypeError("Invalid argument types.");
          return;
        }
        input_argument_types.push_back(syntax_descriptor);
      }
    }
  }

  // Parse 'alpha' AKA hasAlpha property.
  v8::Local<v8::Value> alpha_value;
  if (!constructor->Get(context, V8AtomicString(isolate, "alpha"))
           .ToLocal(&alpha_value))
    return;
  if (!IsUndefinedOrNull(alpha_value) && !alpha_value->IsBoolean()) {
    exception_state.ThrowTypeError(
        "The 'alpha' property on the class is not a boolean.");
    return;
  }
  bool has_alpha = alpha_value->IsBoolean()
                       ? v8::Local<v8::Boolean>::Cast(alpha_value)->Value()
                       : true;

  v8::Local<v8::Value> prototype_value;
  if (!constructor->Get(context, V8AtomicString(isolate, "prototype"))
           .ToLocal(&prototype_value))
    return;

  if (IsUndefinedOrNull(prototype_value)) {
    exception_state.ThrowTypeError(
        "The 'prototype' object on the class does not exist.");
    return;
  }

  if (!prototype_value->IsObject()) {
    exception_state.ThrowTypeError(
        "The 'prototype' property on the class is not an object.");
    return;
  }

  v8::Local<v8::Object> prototype =
      v8::Local<v8::Object>::Cast(prototype_value);

  v8::Local<v8::Value> paint_value;
  if (!prototype->Get(context, V8AtomicString(isolate, "paint"))
           .ToLocal(&paint_value))
    return;

  if (IsUndefinedOrNull(paint_value)) {
    exception_state.ThrowTypeError(
        "The 'paint' function on the prototype does not exist.");
    return;
  }

  if (!paint_value->IsFunction()) {
    exception_state.ThrowTypeError(
        "The 'paint' property on the prototype is not a function.");
    return;
  }

  v8::Local<v8::Function> paint = v8::Local<v8::Function>::Cast(paint_value);

  CSSPaintDefinition* definition = CSSPaintDefinition::Create(
      ScriptController()->GetScriptState(), constructor, paint,
      native_invalidation_properties, custom_invalidation_properties,
      input_argument_types, has_alpha);
  paint_definitions_.Set(name, definition);

  // TODO(xidachen): the following steps should be done with a postTask when
  // we move PaintWorklet off main thread.
  LocalDOMWindow* dom_window = GetFrame()->GetDocument()->domWindow();
  PaintWorklet* paint_worklet =
      WindowPaintWorklet::From(*dom_window).paintWorklet();
  PaintWorklet::DocumentDefinitionMap& document_definition_map =
      paint_worklet->GetDocumentDefinitionMap();
  if (document_definition_map.Contains(name)) {
    DocumentPaintDefinition* existing_document_definition =
        document_definition_map.at(name);
    if (existing_document_definition == kInvalidDocumentDefinition)
      return;
    if (!existing_document_definition->RegisterAdditionalPaintDefinition(
            *definition)) {
      document_definition_map.Set(name, kInvalidDocumentDefinition);
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

DEFINE_TRACE(PaintWorkletGlobalScope) {
  visitor->Trace(paint_definitions_);
  visitor->Trace(pending_generator_registry_);
  MainThreadWorkletGlobalScope::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(PaintWorkletGlobalScope) {
  for (auto definition : paint_definitions_)
    visitor->TraceWrappers(definition.value);
  MainThreadWorkletGlobalScope::TraceWrappers(visitor);
}

}  // namespace blink

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/CSSLayoutDefinition.h"

#include <memory>
#include "bindings/core/v8/DictionaryIterator.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8FragmentResultOptions.h"
#include "core/css/cssom/PrepopulatedComputedStylePropertyMap.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/custom/FragmentResultOptions.h"
#include "core/layout/custom/LayoutCustom.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8ObjectConstructor.h"

namespace blink {

CSSLayoutDefinition::CSSLayoutDefinition(
    ScriptState* script_state,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> intrinsic_sizes,
    v8::Local<v8::Function> layout,
    const Vector<CSSPropertyID>& native_invalidation_properties,
    const Vector<AtomicString>& custom_invalidation_properties,
    const Vector<CSSPropertyID>& child_native_invalidation_properties,
    const Vector<AtomicString>& child_custom_invalidation_properties)
    : script_state_(script_state),
      constructor_(script_state->GetIsolate(), constructor),
      intrinsic_sizes_(script_state->GetIsolate(), intrinsic_sizes),
      layout_(script_state->GetIsolate(), layout),
      constructor_has_failed_(false) {
  native_invalidation_properties_ = native_invalidation_properties;
  custom_invalidation_properties_ = custom_invalidation_properties;
  child_native_invalidation_properties_ = child_native_invalidation_properties;
  child_custom_invalidation_properties_ = child_custom_invalidation_properties;
}

CSSLayoutDefinition::~CSSLayoutDefinition() = default;

CSSLayoutDefinition::Instance::Instance(CSSLayoutDefinition* definition,
                                        v8::Local<v8::Object> instance)
    : definition_(definition),
      instance_(definition->script_state_->GetIsolate(), instance) {}

bool CSSLayoutDefinition::Instance::Layout(
    const LayoutCustom& layout_custom,
    FragmentResultOptions* fragment_result_options) {
  ScriptState* script_state = definition_->GetScriptState();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  if (!script_state->ContextIsValid()) {
    return false;
  }

  ScriptState::Scope scope(script_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Object> instance = instance_.NewLocal(isolate);
  v8::Local<v8::Context> context = script_state->GetContext();

  v8::Local<v8::Function> layout = definition_->layout_.NewLocal(isolate);

  // TODO(ikilpatrick): Determine if knowing the size of the array ahead of
  // time improves performance in any noticable way.
  v8::Local<v8::Array> children = v8::Array::New(isolate, 0);
  uint32_t index = 0;

  for (LayoutBox* child = layout_custom.FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    CustomLayoutChild* layout_child = child->GetCustomLayoutChild();
    DCHECK(layout_child);

    bool success;
    if (!children
             ->CreateDataProperty(
                 context, index++,
                 ToV8(layout_child, context->Global(), isolate))
             .To(&success) &&
        success)
      return false;
  }

  // TODO(ikilpatrick): Instead of creating a new style_map each time here,
  // store on LayoutCustom, and update when the style changes.
  StylePropertyMapReadOnly* style_map =
      new PrepopulatedComputedStylePropertyMap(
          layout_custom.GetDocument(), layout_custom.StyleRef(),
          layout_custom.GetNode(), definition_->native_invalidation_properties_,
          definition_->custom_invalidation_properties_);

  // TODO(ikilpatrick): Fill in layout constraints, and edges.
  Vector<v8::Local<v8::Value>> argv = {
      children,
      v8::Undefined(isolate),  // edges
      v8::Undefined(isolate),  // constraints
      ToV8(style_map, script_state->GetContext()->Global(), isolate),
  };

  v8::Local<v8::Value> generator_value;
  if (!V8ScriptRunner::CallFunction(layout, execution_context, instance,
                                    argv.size(), argv.data(), isolate)
           .ToLocal(&generator_value))
    return false;

  DCHECK(generator_value->IsGeneratorObject());
  v8::Local<v8::Object> generator =
      v8::Local<v8::Object>::Cast(generator_value);

  DictionaryIterator iterator(generator, isolate);

  // We run the generator until it's exhausted.
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "CSSLayoutAPI", "Layout");
  while (iterator.Next(execution_context, exception_state)) {
    // TODO(ikilpatrick): If we aren't done yet, we need to process the child
    // layout requests.
  }

  if (exception_state.HadException()) {
    V8ScriptRunner::ReportException(isolate, exception_state.GetException());
    exception_state.ClearException();
    execution_context->AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kInfoMessageLevel,
        "The layout function failed, falling back to block layout."));
    return false;
  }

  // The value should already be non-empty, if not it should have be caught be
  // the exception_state.HadException() above.
  v8::Local<v8::Value> value = iterator.GetValue().ToLocalChecked();

  // Attempt to convert the result.
  V8FragmentResultOptions::ToImpl(isolate, value, *fragment_result_options,
                                  exception_state);

  if (exception_state.HadException()) {
    V8ScriptRunner::ReportException(isolate, exception_state.GetException());
    exception_state.ClearException();
    execution_context->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kInfoMessageLevel,
                               "Unable to parse the layout function "
                               "result, falling back to block layout."));
    return false;
  }

  return true;
}

CSSLayoutDefinition::Instance* CSSLayoutDefinition::CreateInstance() {
  if (constructor_has_failed_)
    return nullptr;

  // Ensure that we don't create an instance on a detached context.
  if (!GetScriptState()->ContextIsValid()) {
    return nullptr;
  }

  Instance* instance = nullptr;

  ScriptState::Scope scope(GetScriptState());

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Function> constructor = constructor_.NewLocal(isolate);
  DCHECK(!IsUndefinedOrNull(constructor));

  v8::Local<v8::Object> layout_instance;
  if (V8ObjectConstructor::NewInstance(isolate, constructor)
          .ToLocal(&layout_instance)) {
    instance = new Instance(this, layout_instance);
  } else {
    constructor_has_failed_ = true;
  }

  return instance;
}

void CSSLayoutDefinition::Instance::Trace(blink::Visitor* visitor) {
  visitor->Trace(definition_);
}

void CSSLayoutDefinition::Instance::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(instance_.Cast<v8::Value>());
}

void CSSLayoutDefinition::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(constructor_.Cast<v8::Value>());
  visitor->TraceWrappers(intrinsic_sizes_.Cast<v8::Value>());
  visitor->TraceWrappers(layout_.Cast<v8::Value>());
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorkletGlobalScope.h"

#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSSyntaxDescriptor.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/MainThreadDebugger.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"

namespace blink {

// static
PaintWorkletGlobalScope* PaintWorkletGlobalScope::create(
    LocalFrame* frame,
    const KURL& url,
    const String& userAgent,
    PassRefPtr<SecurityOrigin> securityOrigin,
    v8::Isolate* isolate) {
  PaintWorkletGlobalScope* paintWorkletGlobalScope =
      new PaintWorkletGlobalScope(frame, url, userAgent,
                                  std::move(securityOrigin), isolate);
  paintWorkletGlobalScope->scriptController()->initializeContextIfNeeded();
  MainThreadDebugger::instance()->contextCreated(
      paintWorkletGlobalScope->scriptController()->getScriptState(),
      paintWorkletGlobalScope->frame(),
      paintWorkletGlobalScope->getSecurityOrigin());
  return paintWorkletGlobalScope;
}

PaintWorkletGlobalScope::PaintWorkletGlobalScope(
    LocalFrame* frame,
    const KURL& url,
    const String& userAgent,
    PassRefPtr<SecurityOrigin> securityOrigin,
    v8::Isolate* isolate)
    : MainThreadWorkletGlobalScope(frame,
                                   url,
                                   userAgent,
                                   std::move(securityOrigin),
                                   isolate) {}

PaintWorkletGlobalScope::~PaintWorkletGlobalScope() {}

void PaintWorkletGlobalScope::dispose() {
  MainThreadDebugger::instance()->contextWillBeDestroyed(
      scriptController()->getScriptState());
  // Explicitly clear the paint defininitions to break a reference cycle
  // between them and this global scope.
  m_paintDefinitions.clear();

  WorkletGlobalScope::dispose();
}

void PaintWorkletGlobalScope::registerPaint(const String& name,
                                            const ScriptValue& ctorValue,
                                            ExceptionState& exceptionState) {
  if (m_paintDefinitions.contains(name)) {
    exceptionState.throwDOMException(
        NotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  if (name.isEmpty()) {
    exceptionState.throwTypeError("The empty string is not a valid name.");
    return;
  }

  v8::Isolate* isolate = scriptController()->getScriptState()->isolate();
  v8::Local<v8::Context> context = scriptController()->context();

  ASSERT(ctorValue.v8Value()->IsFunction());
  v8::Local<v8::Function> constructor =
      v8::Local<v8::Function>::Cast(ctorValue.v8Value());

  v8::Local<v8::Value> inputPropertiesValue;
  if (!v8Call(constructor->Get(context, v8String(isolate, "inputProperties")),
              inputPropertiesValue))
    return;

  Vector<CSSPropertyID> nativeInvalidationProperties;
  Vector<AtomicString> customInvalidationProperties;

  if (!isUndefinedOrNull(inputPropertiesValue)) {
    Vector<String> properties = toImplArray<Vector<String>>(
        inputPropertiesValue, 0, isolate, exceptionState);

    if (exceptionState.hadException())
      return;

    for (const auto& property : properties) {
      CSSPropertyID propertyID = cssPropertyID(property);
      if (propertyID == CSSPropertyVariable) {
        customInvalidationProperties.push_back(property);
      } else if (propertyID != CSSPropertyInvalid) {
        nativeInvalidationProperties.push_back(propertyID);
      }
    }
  }

  // Get input argument types. Parse the argument type values only when
  // cssPaintAPIArguments is enabled.
  Vector<CSSSyntaxDescriptor> inputArgumentTypes;
  if (RuntimeEnabledFeatures::cssPaintAPIArgumentsEnabled()) {
    v8::Local<v8::Value> inputArgumentTypeValues;
    if (!v8Call(constructor->Get(context, v8String(isolate, "inputArguments")),
                inputArgumentTypeValues))
      return;

    if (!isUndefinedOrNull(inputArgumentTypeValues)) {
      Vector<String> argumentTypes = toImplArray<Vector<String>>(
          inputArgumentTypeValues, 0, isolate, exceptionState);

      if (exceptionState.hadException())
        return;

      for (const auto& type : argumentTypes) {
        CSSSyntaxDescriptor syntaxDescriptor(type);
        if (!syntaxDescriptor.isValid()) {
          exceptionState.throwTypeError("Invalid argument types.");
          return;
        }
        inputArgumentTypes.push_back(syntaxDescriptor);
      }
    }
  }

  // Parse 'alpha' AKA hasAlpha property.
  v8::Local<v8::Value> alphaValue;
  if (!v8Call(constructor->Get(context, v8String(isolate, "alpha")),
              alphaValue))
    return;
  if (!isUndefinedOrNull(alphaValue) && !alphaValue->IsBoolean()) {
    exceptionState.throwTypeError(
        "The 'alpha' property on the class is not a boolean.");
    return;
  }
  bool hasAlpha = alphaValue->IsBoolean()
                      ? v8::Local<v8::Boolean>::Cast(alphaValue)->Value()
                      : true;

  v8::Local<v8::Value> prototypeValue;
  if (!v8Call(constructor->Get(context, v8String(isolate, "prototype")),
              prototypeValue))
    return;

  if (isUndefinedOrNull(prototypeValue)) {
    exceptionState.throwTypeError(
        "The 'prototype' object on the class does not exist.");
    return;
  }

  if (!prototypeValue->IsObject()) {
    exceptionState.throwTypeError(
        "The 'prototype' property on the class is not an object.");
    return;
  }

  v8::Local<v8::Object> prototype = v8::Local<v8::Object>::Cast(prototypeValue);

  v8::Local<v8::Value> paintValue;
  if (!v8Call(prototype->Get(context, v8String(isolate, "paint")), paintValue))
    return;

  if (isUndefinedOrNull(paintValue)) {
    exceptionState.throwTypeError(
        "The 'paint' function on the prototype does not exist.");
    return;
  }

  if (!paintValue->IsFunction()) {
    exceptionState.throwTypeError(
        "The 'paint' property on the prototype is not a function.");
    return;
  }

  v8::Local<v8::Function> paint = v8::Local<v8::Function>::Cast(paintValue);

  CSSPaintDefinition* definition = CSSPaintDefinition::create(
      scriptController()->getScriptState(), constructor, paint,
      nativeInvalidationProperties, customInvalidationProperties,
      inputArgumentTypes, hasAlpha);
  m_paintDefinitions.set(name, definition);

  // Set the definition on any pending generators.
  GeneratorHashSet* set = m_pendingGenerators.at(name);
  if (set) {
    for (const auto& generator : *set) {
      if (generator) {
        generator->setDefinition(definition);
      }
    }
  }
  m_pendingGenerators.erase(name);
}

CSSPaintDefinition* PaintWorkletGlobalScope::findDefinition(
    const String& name) {
  return m_paintDefinitions.at(name);
}

void PaintWorkletGlobalScope::addPendingGenerator(
    const String& name,
    CSSPaintImageGeneratorImpl* generator) {
  Member<GeneratorHashSet>& set =
      m_pendingGenerators.insert(name, nullptr).storedValue->value;
  if (!set)
    set = new GeneratorHashSet;
  set->insert(generator);
}

DEFINE_TRACE(PaintWorkletGlobalScope) {
  visitor->trace(m_paintDefinitions);
  visitor->trace(m_pendingGenerators);
  MainThreadWorkletGlobalScope::trace(visitor);
}

}  // namespace blink

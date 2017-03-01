// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ConditionalFeaturesForCore.h"

#include "bindings/core/v8/ConditionalFeatures.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8HTMLLinkElement.h"
#include "bindings/core/v8/V8Navigator.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/origin_trials/OriginTrials.h"

namespace blink {

namespace {
InstallConditionalFeaturesFunction s_oldInstallConditionalFeaturesFunction =
    nullptr;
InstallPendingConditionalFeatureFunction
    s_oldInstallPendingConditionalFeatureFunction = nullptr;
}

void installConditionalFeaturesCore(const WrapperTypeInfo* wrapperTypeInfo,
                                    const ScriptState* scriptState,
                                    v8::Local<v8::Object> prototypeObject,
                                    v8::Local<v8::Function> interfaceObject) {
  (*s_oldInstallConditionalFeaturesFunction)(wrapperTypeInfo, scriptState,
                                             prototypeObject, interfaceObject);

  // TODO(iclelland): Generate all of this logic at compile-time, based on the
  // configuration of origin trial enabled attributes and interfaces in IDL
  // files. (crbug.com/615060)
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  if (!executionContext)
    return;
  v8::Isolate* isolate = scriptState->isolate();
  const DOMWrapperWorld& world = scriptState->world();
  if (wrapperTypeInfo == &V8HTMLLinkElement::wrapperTypeInfo) {
    if (OriginTrials::linkServiceWorkerEnabled(executionContext)) {
      V8HTMLLinkElement::installLinkServiceWorker(
          isolate, world, v8::Local<v8::Object>(), prototypeObject,
          interfaceObject);
    }
  }
}

void installPendingConditionalFeatureCore(const String& feature,
                                          const ScriptState* scriptState) {
  (*s_oldInstallPendingConditionalFeatureFunction)(feature, scriptState);

  // TODO(iclelland): Generate all of this logic at compile-time, based on the
  // configuration of origin trial enabled attributes and interfaces in IDL
  // files. (crbug.com/615060)
  v8::Local<v8::Object> prototypeObject;
  v8::Local<v8::Function> interfaceObject;
  v8::Isolate* isolate = scriptState->isolate();
  const DOMWrapperWorld& world = scriptState->world();
  V8PerContextData* contextData = scriptState->perContextData();
  if (feature == "ForeignFetch") {
    if (contextData->getExistingConstructorAndPrototypeForType(
            &V8HTMLLinkElement::wrapperTypeInfo, &prototypeObject,
            &interfaceObject)) {
      V8HTMLLinkElement::installLinkServiceWorker(
          isolate, world, v8::Local<v8::Object>(), prototypeObject,
          interfaceObject);
    }
    return;
  }
}

void installConditionalFeaturesOnWindow(const ScriptState* scriptState) {
  DCHECK(scriptState);
  DCHECK(scriptState->context() == scriptState->isolate()->GetCurrentContext());
  DCHECK(scriptState->perContextData());
  DCHECK(scriptState->world().isMainWorld());
  installConditionalFeatures(&V8Window::wrapperTypeInfo, scriptState,
                             v8::Local<v8::Object>(),
                             v8::Local<v8::Function>());
}

bool isFeatureEnabledInFrame(const FeaturePolicy::Feature& feature,
                             const LocalFrame* frame) {
  // If there is no frame, or if feature policy is disabled, use defaults.
  bool enabledByDefault =
      (feature.defaultPolicy == FeaturePolicy::FeatureDefault::EnableForAll ||
       (feature.defaultPolicy == FeaturePolicy::FeatureDefault::EnableForSelf &&
        !frame->isCrossOriginSubframe()));
  if (!RuntimeEnabledFeatures::featurePolicyEnabled() || !frame)
    return enabledByDefault;
  FeaturePolicy* featurePolicy = frame->securityContext()->getFeaturePolicy();
  // The policy should always be initialized before checking it to ensure we
  // properly inherit the parent policy.
  DCHECK(featurePolicy);

  // Otherwise, check policy.
  return featurePolicy->isFeatureEnabled(feature);
}

void registerInstallConditionalFeaturesForCore() {
  s_oldInstallConditionalFeaturesFunction =
      setInstallConditionalFeaturesFunction(&installConditionalFeaturesCore);
  s_oldInstallPendingConditionalFeatureFunction =
      setInstallPendingConditionalFeatureFunction(
          &installPendingConditionalFeatureCore);
}

}  // namespace blink

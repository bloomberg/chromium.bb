// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ConditionalFeatures.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8HTMLLinkElement.h"
#include "bindings/core/v8/V8Navigator.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/ExecutionContext.h"
#include "core/origin_trials/OriginTrialContext.h"

namespace blink {

void installConditionalFeaturesCore(const WrapperTypeInfo* wrapperTypeInfo, const ScriptState* scriptState, v8::Local<v8::Object> prototypeObject, v8::Local<v8::Function> interfaceObject)
{
    // TODO(iclelland): Generate all of this logic at compile-time, based on the
    // configuration of origin trial enabled attibutes and interfaces in IDL
    // files. (crbug.com/615060)
    ExecutionContext* executionContext = scriptState->getExecutionContext();
    if (!executionContext)
        return;
    OriginTrialContext* originTrialContext = OriginTrialContext::from(executionContext, OriginTrialContext::DontCreateIfNotExists);
    if (wrapperTypeInfo == &V8HTMLLinkElement::wrapperTypeInfo) {
        if (RuntimeEnabledFeatures::linkServiceWorkerEnabled() || (originTrialContext && originTrialContext->isFeatureEnabled("ForeignFetch"))) {
            V8HTMLLinkElement::installLinkServiceWorker(scriptState->isolate(), scriptState->world(), v8::Local<v8::Object>(), prototypeObject, interfaceObject);
        }
    }
}

namespace {

InstallConditionalFeaturesFunction s_installConditionalFeaturesFunction = &installConditionalFeaturesCore;

}

InstallConditionalFeaturesFunction setInstallConditionalFeaturesFunction(InstallConditionalFeaturesFunction newInstallConditionalFeaturesFunction)
{
    InstallConditionalFeaturesFunction originalFunction = s_installConditionalFeaturesFunction;
    s_installConditionalFeaturesFunction = newInstallConditionalFeaturesFunction;
    return originalFunction;
}

void installConditionalFeatures(const WrapperTypeInfo* type, const ScriptState* scriptState, v8::Local<v8::Object> prototypeObject, v8::Local<v8::Function> interfaceObject)
{
    (*s_installConditionalFeaturesFunction)(type, scriptState, prototypeObject, interfaceObject);
}

} // namespace blink

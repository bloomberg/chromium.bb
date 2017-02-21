// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ConditionalFeatures.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptState.h"

namespace blink {

void installConditionalFeaturesDefault(
    const WrapperTypeInfo* wrapperTypeInfo,
    const ScriptState* scriptState,
    v8::Local<v8::Object> prototypeObject,
    v8::Local<v8::Function> interfaceObject) {}

void installPendingConditionalFeatureDefault(const String& feature,
                                             const ScriptState* scriptState) {}

namespace {
InstallConditionalFeaturesFunction s_installConditionalFeaturesFunction =
    &installConditionalFeaturesDefault;

InstallPendingConditionalFeatureFunction
    s_installPendingConditionalFeatureFunction =
        &installPendingConditionalFeatureDefault;
}  // namespace

InstallConditionalFeaturesFunction setInstallConditionalFeaturesFunction(
    InstallConditionalFeaturesFunction newInstallConditionalFeaturesFunction) {
  InstallConditionalFeaturesFunction originalFunction =
      s_installConditionalFeaturesFunction;
  s_installConditionalFeaturesFunction = newInstallConditionalFeaturesFunction;
  return originalFunction;
}

InstallPendingConditionalFeatureFunction
setInstallPendingConditionalFeatureFunction(
    InstallPendingConditionalFeatureFunction
        newInstallPendingConditionalFeatureFunction) {
  InstallPendingConditionalFeatureFunction originalFunction =
      s_installPendingConditionalFeatureFunction;
  s_installPendingConditionalFeatureFunction =
      newInstallPendingConditionalFeatureFunction;
  return originalFunction;
}

void installConditionalFeatures(const WrapperTypeInfo* type,
                                const ScriptState* scriptState,
                                v8::Local<v8::Object> prototypeObject,
                                v8::Local<v8::Function> interfaceObject) {
  (*s_installConditionalFeaturesFunction)(type, scriptState, prototypeObject,
                                          interfaceObject);
}

void installPendingConditionalFeature(const String& feature,
                                      const ScriptState* scriptState) {
  DCHECK(scriptState);
  DCHECK(scriptState->context() == scriptState->isolate()->GetCurrentContext());
  DCHECK(scriptState->perContextData());
  DCHECK(scriptState->world().isMainWorld());

  (*s_installPendingConditionalFeatureFunction)(feature, scriptState);
}

}  // namespace blink

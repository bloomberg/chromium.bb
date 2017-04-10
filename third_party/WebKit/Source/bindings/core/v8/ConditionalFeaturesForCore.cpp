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
#include "core/frame/Frame.h"
#include "core/origin_trials/OriginTrials.h"

namespace blink {

namespace {
InstallConditionalFeaturesFunction g_old_install_conditional_features_function =
    nullptr;
InstallPendingConditionalFeatureFunction
    g_old_install_pending_conditional_feature_function = nullptr;
}

void InstallConditionalFeaturesCore(const WrapperTypeInfo* wrapper_type_info,
                                    const ScriptState* script_state,
                                    v8::Local<v8::Object> prototype_object,
                                    v8::Local<v8::Function> interface_object) {
  (*g_old_install_conditional_features_function)(
      wrapper_type_info, script_state, prototype_object, interface_object);

  // TODO(iclelland): Generate all of this logic at compile-time, based on the
  // configuration of origin trial enabled attributes and interfaces in IDL
  // files. (crbug.com/615060)
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context)
    return;
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  if (wrapper_type_info == &V8HTMLLinkElement::wrapperTypeInfo) {
    if (OriginTrials::linkServiceWorkerEnabled(execution_context)) {
      V8HTMLLinkElement::installLinkServiceWorker(
          isolate, world, v8::Local<v8::Object>(), prototype_object,
          interface_object);
    }
  }
}

void InstallPendingConditionalFeatureCore(const String& feature,
                                          const ScriptState* script_state) {
  (*g_old_install_pending_conditional_feature_function)(feature, script_state);

  // TODO(iclelland): Generate all of this logic at compile-time, based on the
  // configuration of origin trial enabled attributes and interfaces in IDL
  // files. (crbug.com/615060)
  v8::Local<v8::Object> prototype_object;
  v8::Local<v8::Function> interface_object;
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  V8PerContextData* context_data = script_state->PerContextData();
  if (feature == "ForeignFetch") {
    if (context_data->GetExistingConstructorAndPrototypeForType(
            &V8HTMLLinkElement::wrapperTypeInfo, &prototype_object,
            &interface_object)) {
      V8HTMLLinkElement::installLinkServiceWorker(
          isolate, world, v8::Local<v8::Object>(), prototype_object,
          interface_object);
    }
    return;
  }
}

void InstallConditionalFeaturesOnWindow(const ScriptState* script_state) {
  DCHECK(script_state);
  DCHECK(script_state->GetContext() ==
         script_state->GetIsolate()->GetCurrentContext());
  DCHECK(script_state->PerContextData());
  DCHECK(script_state->World().IsMainWorld());
  InstallConditionalFeatures(&V8Window::wrapperTypeInfo, script_state,
                             v8::Local<v8::Object>(),
                             v8::Local<v8::Function>());
}

void RegisterInstallConditionalFeaturesForCore() {
  g_old_install_conditional_features_function =
      SetInstallConditionalFeaturesFunction(&InstallConditionalFeaturesCore);
  g_old_install_pending_conditional_feature_function =
      SetInstallPendingConditionalFeatureFunction(
          &InstallPendingConditionalFeatureCore);
}

}  // namespace blink

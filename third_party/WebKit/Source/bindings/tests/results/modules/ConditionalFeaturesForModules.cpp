// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/ConditionalFeaturesForModules.cpp.tmpl
// by the script generate_conditional_features.py.
// DO NOT MODIFY!

// clang-format off

#include "bindings/modules/v8/ConditionalFeaturesForModules.h"

#include "bindings/core/v8/ConditionalFeaturesForCore.h"
#include "bindings/core/v8/V8Window.h"
#include "core/context_features/ContextFeatureSettings.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Frame.h"
#include "core/origin_trials/origin_trials.h"
#include "platform/bindings/ConditionalFeatures.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

namespace {
InstallConditionalFeaturesFunction
    g_original_install_conditional_features_function = nullptr;
InstallConditionalFeaturesOnGlobalFunction
    g_original_install_conditional_features_on_global_function = nullptr;
InstallPendingConditionalFeatureFunction
    g_original_install_pending_conditional_feature_function = nullptr;

void InstallConditionalFeaturesForModules(
    const WrapperTypeInfo* wrapper_type_info,
    const ScriptState* script_state,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Function> interface_object) {
  (*g_original_install_conditional_features_function)(
      wrapper_type_info, script_state, prototype_object, interface_object);
}

void InstallConditionalFeaturesOnGlobalForModules(
    const WrapperTypeInfo* wrapper_type_info,
    const ScriptState* script_state) {
  (*g_original_install_conditional_features_on_global_function)(
      wrapper_type_info, script_state);

  // TODO(chasej): Generate this logic at compile-time, based on interfaces with
  // [SecureContext] attribute.
  if (wrapper_type_info == &V8Window::wrapperTypeInfo) {
    V8WindowPartial::InstallConditionalFeaturesOnGlobal(
        script_state->GetContext(), script_state->World());
  }
  // TODO(chasej): Uncomment when [SecureContext] is applied to an interface
  // exposed to workers (i.e. StorageManager.idl).
  /*
  } else if (wrapper_type_info ==
             &V8DedicatedWorkerGlobalScope::wrapperTypeInfo) {
    V8DedicatedWorkerGlobalScopePartial::InstallConditionalFeaturesOnGlobal(
        script_state->GetContext(), script_state->World());
  } else if (wrapper_type_info == &V8SharedWorkerGlobalScope::wrapperTypeInfo) {
    V8SharedWorkerGlobalScopePartial::InstallConditionalFeaturesOnGlobal(
        script_state->GetContext(), script_state->World());
  } else if (wrapper_type_info ==
             &V8ServiceWorkerGlobalScope::wrapperTypeInfo) {
    V8ServiceWorkerGlobalScope::InstallConditionalFeaturesOnGlobal(
        script_state->GetContext(), script_state->World());
  }
  */
}

void InstallPendingConditionalFeatureForModules(
    const String& feature,
    const ScriptState* script_state) {
  (*g_original_install_pending_conditional_feature_function)(feature,
                                                             script_state);

  // TODO(iclelland): Extract this common code out of ConditionalFeaturesForCore
  // and ConditionalFeaturesForModules into a block.
}

}  // namespace

void RegisterInstallConditionalFeaturesForModules() {
  RegisterInstallConditionalFeaturesForCore();
  g_original_install_conditional_features_function =
      SetInstallConditionalFeaturesFunction(
          &InstallConditionalFeaturesForModules);
  g_original_install_conditional_features_on_global_function =
      SetInstallConditionalFeaturesOnGlobalFunction(
          &InstallConditionalFeaturesOnGlobalForModules);
  g_original_install_pending_conditional_feature_function =
      SetInstallPendingConditionalFeatureFunction(
          &InstallPendingConditionalFeatureForModules);
}

}  // namespace blink

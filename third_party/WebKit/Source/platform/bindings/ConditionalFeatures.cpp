// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/ConditionalFeatures.h"

#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

void InstallConditionalFeaturesDefault(
    const WrapperTypeInfo* wrapper_type_info,
    const ScriptState* script_state,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Function> interface_object) {}

void InstallConditionalFeaturesOnGlobalDefault(
    const WrapperTypeInfo* wrapper_type_info,
    const ScriptState* script_state) {}

void InstallPendingConditionalFeatureDefault(const String& feature,
                                             const ScriptState* script_state) {}

namespace {
InstallConditionalFeaturesFunction g_install_conditional_features_function =
    &InstallConditionalFeaturesDefault;

InstallConditionalFeaturesOnGlobalFunction
    g_install_conditional_features_on_global_function =
        &InstallConditionalFeaturesOnGlobalDefault;

InstallPendingConditionalFeatureFunction
    g_install_pending_conditional_feature_function =
        &InstallPendingConditionalFeatureDefault;
}  // namespace

InstallConditionalFeaturesFunction SetInstallConditionalFeaturesFunction(
    InstallConditionalFeaturesFunction
        new_install_conditional_features_function) {
  InstallConditionalFeaturesFunction original_function =
      g_install_conditional_features_function;
  g_install_conditional_features_function =
      new_install_conditional_features_function;
  return original_function;
}

InstallConditionalFeaturesOnGlobalFunction
SetInstallConditionalFeaturesOnGlobalFunction(
    InstallConditionalFeaturesOnGlobalFunction
        new_install_conditional_features_on_global_function) {
  InstallConditionalFeaturesOnGlobalFunction original_function =
      g_install_conditional_features_on_global_function;
  g_install_conditional_features_on_global_function =
      new_install_conditional_features_on_global_function;
  return original_function;
}

InstallPendingConditionalFeatureFunction
SetInstallPendingConditionalFeatureFunction(
    InstallPendingConditionalFeatureFunction
        new_install_pending_conditional_feature_function) {
  InstallPendingConditionalFeatureFunction original_function =
      g_install_pending_conditional_feature_function;
  g_install_pending_conditional_feature_function =
      new_install_pending_conditional_feature_function;
  return original_function;
}

void InstallConditionalFeatures(const WrapperTypeInfo* type,
                                const ScriptState* script_state,
                                v8::Local<v8::Object> prototype_object,
                                v8::Local<v8::Function> interface_object) {
  (*g_install_conditional_features_function)(
      type, script_state, prototype_object, interface_object);
}

void InstallConditionalFeaturesOnGlobal(const WrapperTypeInfo* type,
                                        const ScriptState* script_state) {
  DCHECK(script_state);
  DCHECK(script_state->GetContext() ==
         script_state->GetIsolate()->GetCurrentContext());
  DCHECK(script_state->PerContextData());

  (*g_install_conditional_features_on_global_function)(type, script_state);
}

void InstallPendingConditionalFeature(const String& feature,
                                      const ScriptState* script_state) {
  DCHECK(script_state);
  DCHECK(script_state->GetContext() ==
         script_state->GetIsolate()->GetCurrentContext());
  DCHECK(script_state->PerContextData());
  DCHECK(script_state->World().IsMainWorld());

  (*g_install_pending_conditional_feature_function)(feature, script_state);
}

}  // namespace blink

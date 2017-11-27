// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/OriginTrialFeaturesForModules.cpp.tmpl
// by the script generate_origin_trial_features.py.
// DO NOT MODIFY!

// clang-format off

#include "bindings/modules/v8/OriginTrialFeaturesForModules.h"

#include "bindings/core/v8/OriginTrialFeaturesForCore.h"
#include "bindings/core/v8/V8Window.h"
#include "core/context_features/ContextFeatureSettings.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Frame.h"
#include "core/origin_trials/origin_trials.h"
#include "platform/bindings/OriginTrialFeatures.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

namespace {
InstallOriginTrialFeaturesFunction
    g_original_install_origin_trial_features_function = nullptr;
InstallPendingOriginTrialFeatureFunction
    g_original_install_pending_origin_trial_feature_function = nullptr;

void InstallOriginTrialFeaturesForModules(
    const WrapperTypeInfo* wrapper_type_info,
    const ScriptState* script_state,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Function> interface_object) {
  (*g_original_install_origin_trial_features_function)(
      wrapper_type_info, script_state, prototype_object, interface_object);
}

void InstallPendingOriginTrialFeatureForModules(
    const String& feature,
    const ScriptState* script_state) {
  (*g_original_install_pending_origin_trial_feature_function)(feature,
                                                             script_state);

  // TODO(iclelland): Extract this common code out of OriginTrialFeaturesForCore
  // and OriginTrialFeaturesForModules into a block.
}

}  // namespace

void RegisterInstallOriginTrialFeaturesForModules() {
  RegisterInstallOriginTrialFeaturesForCore();
  g_original_install_origin_trial_features_function =
      SetInstallOriginTrialFeaturesFunction(
          &InstallOriginTrialFeaturesForModules);
  g_original_install_pending_origin_trial_feature_function =
      SetInstallPendingOriginTrialFeatureFunction(
          &InstallPendingOriginTrialFeatureForModules);
}

}  // namespace blink

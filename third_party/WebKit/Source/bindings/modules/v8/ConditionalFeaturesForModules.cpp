// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/ConditionalFeaturesForModules.h"

#include "bindings/core/v8/ConditionalFeaturesForCore.h"
#include "bindings/core/v8/V8DedicatedWorkerGlobalScope.h"
#include "bindings/core/v8/V8Navigator.h"
#include "bindings/core/v8/V8SharedWorkerGlobalScope.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/core/v8/V8WorkerNavigator.h"
#include "bindings/modules/v8/V8BudgetService.h"
#include "bindings/modules/v8/V8DedicatedWorkerGlobalScopePartial.h"
#include "bindings/modules/v8/V8Gamepad.h"
#include "bindings/modules/v8/V8GamepadButton.h"
#include "bindings/modules/v8/V8InstallEvent.h"
#include "bindings/modules/v8/V8NavigatorPartial.h"
#include "bindings/modules/v8/V8ServiceWorkerGlobalScope.h"
#include "bindings/modules/v8/V8SharedWorkerGlobalScopePartial.h"
#include "bindings/modules/v8/V8WindowPartial.h"
#include "bindings/modules/v8/V8WorkerNavigatorPartial.h"
#include "core/dom/ExecutionContext.h"
#include "core/origin_trials/OriginTrials.h"
#include "platform/bindings/ConditionalFeatures.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

namespace {
InstallConditionalFeaturesFunction
    g_original_install_conditional_features_function = nullptr;
InstallPendingConditionalFeatureFunction
    g_original_install_pending_conditional_feature_function = nullptr;
}

void InstallConditionalFeaturesForModules(
    const WrapperTypeInfo* wrapper_type_info,
    const ScriptState* script_state,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Function> interface_object) {
  // TODO(iclelland): Generate all of this logic at compile-time, based on the
  // configuration of origin trial enabled attibutes and interfaces in IDL
  // files. (crbug.com/615060)
  (*g_original_install_conditional_features_function)(
      wrapper_type_info, script_state, prototype_object, interface_object);

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context)
    return;
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  if (wrapper_type_info == &V8Navigator::wrapperTypeInfo) {
    if (OriginTrials::installedAppEnabled(execution_context)) {
      V8NavigatorPartial::installInstalledApp(
          isolate, world, v8::Local<v8::Object>(), prototype_object,
          interface_object);
    }
    if (OriginTrials::webVREnabled(execution_context)) {
      V8NavigatorPartial::installWebVR(isolate, world, v8::Local<v8::Object>(),
                                       prototype_object, interface_object);
    }
  } else if (wrapper_type_info == &V8Window::wrapperTypeInfo) {
    v8::Local<v8::Object> instance_object =
        script_state->GetContext()->Global();
    if (OriginTrials::webVREnabled(execution_context)) {
      V8WindowPartial::installWebVR(isolate, world, instance_object,
                                    prototype_object, interface_object);
    }
    if (OriginTrials::gamepadExtensionsEnabled(execution_context)) {
      V8WindowPartial::installGamepadExtensions(
          isolate, world, instance_object, prototype_object, interface_object);
    }
    if (OriginTrials::budgetQueryEnabled(execution_context)) {
      V8WindowPartial::installBudgetQuery(isolate, world, instance_object,
                                          prototype_object, interface_object);
    }
  } else if (wrapper_type_info ==
             &V8DedicatedWorkerGlobalScope::wrapperTypeInfo) {
    v8::Local<v8::Object> instance_object =
        script_state->GetContext()->Global();
    if (OriginTrials::budgetQueryEnabled(execution_context)) {
      V8DedicatedWorkerGlobalScopePartial::installBudgetQuery(
          isolate, world, instance_object, prototype_object, interface_object);
    }
  } else if (wrapper_type_info == &V8SharedWorkerGlobalScope::wrapperTypeInfo) {
    v8::Local<v8::Object> instance_object =
        script_state->GetContext()->Global();
    if (OriginTrials::budgetQueryEnabled(execution_context)) {
      V8SharedWorkerGlobalScopePartial::installBudgetQuery(
          isolate, world, instance_object, prototype_object, interface_object);
    }
  } else if (wrapper_type_info ==
             &V8ServiceWorkerGlobalScope::wrapperTypeInfo) {
    v8::Local<v8::Object> instance_object =
        script_state->GetContext()->Global();
    if (OriginTrials::foreignFetchEnabled(execution_context)) {
      V8ServiceWorkerGlobalScope::installForeignFetch(
          isolate, world, instance_object, prototype_object, interface_object);
    }
    if (OriginTrials::budgetQueryEnabled(execution_context)) {
      V8ServiceWorkerGlobalScope::installBudgetQuery(
          isolate, world, instance_object, prototype_object, interface_object);
    }
  } else if (wrapper_type_info == &V8InstallEvent::wrapperTypeInfo) {
    if (OriginTrials::foreignFetchEnabled(execution_context)) {
      V8InstallEvent::installForeignFetch(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototype_object, interface_object);
    }
  } else if (wrapper_type_info == &V8BudgetService::wrapperTypeInfo) {
    if (OriginTrials::budgetQueryEnabled(execution_context)) {
      V8BudgetService::installBudgetQuery(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototype_object, interface_object);
    }
  } else if (wrapper_type_info == &V8Gamepad::wrapperTypeInfo) {
    if (OriginTrials::gamepadExtensionsEnabled(execution_context)) {
      V8Gamepad::installGamepadExtensions(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototype_object, interface_object);
    }
  } else if (wrapper_type_info == &V8GamepadButton::wrapperTypeInfo) {
    if (OriginTrials::gamepadExtensionsEnabled(execution_context)) {
      V8GamepadButton::installGamepadExtensions(
          isolate, world, v8::Local<v8::Object>(), prototype_object,
          interface_object);
    }
  }
}

void InstallPendingConditionalFeatureForModules(
    const String& feature,
    const ScriptState* script_state) {
  // TODO(iclelland): Generate all of this logic at compile-time, based on the
  // configuration of origin trial enabled attributes and interfaces in IDL
  // files. (crbug.com/615060)
  (*g_original_install_pending_conditional_feature_function)(feature,
                                                             script_state);
  v8::Local<v8::Object> global_instance_object;
  v8::Local<v8::Object> prototype_object;
  v8::Local<v8::Function> interface_object;
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  V8PerContextData* context_data = script_state->PerContextData();
  if (feature == "ForeignFetch") {
    if (context_data->GetExistingConstructorAndPrototypeForType(
            &V8InstallEvent::wrapperTypeInfo, &prototype_object,
            &interface_object)) {
      V8InstallEvent::installForeignFetch(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototype_object, interface_object);
    }
    return;
  }
  if (feature == "InstalledApp") {
    if (context_data->GetExistingConstructorAndPrototypeForType(
            &V8Navigator::wrapperTypeInfo, &prototype_object,
            &interface_object)) {
      V8NavigatorPartial::installInstalledApp(
          isolate, world, v8::Local<v8::Object>(), prototype_object,
          interface_object);
    }
    return;
  }
  if (feature == "WebVR1.1") {
    global_instance_object = script_state->GetContext()->Global();
    V8WindowPartial::installGamepadExtensions(
        isolate, world, global_instance_object, v8::Local<v8::Object>(),
        v8::Local<v8::Function>());
    V8WindowPartial::installWebVR(isolate, world, global_instance_object,
                                  v8::Local<v8::Object>(),
                                  v8::Local<v8::Function>());
    if (context_data->GetExistingConstructorAndPrototypeForType(
            &V8Gamepad::wrapperTypeInfo, &prototype_object,
            &interface_object)) {
      V8Gamepad::installGamepadExtensions(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototype_object, interface_object);
    }
    if (context_data->GetExistingConstructorAndPrototypeForType(
            &V8GamepadButton::wrapperTypeInfo, &prototype_object,
            &interface_object)) {
      V8GamepadButton::installGamepadExtensions(
          isolate, world, v8::Local<v8::Object>(), prototype_object,
          interface_object);
    }
    if (context_data->GetExistingConstructorAndPrototypeForType(
            &V8Navigator::wrapperTypeInfo, &prototype_object,
            &interface_object)) {
      V8NavigatorPartial::installWebVR(isolate, world, v8::Local<v8::Object>(),
                                       prototype_object, interface_object);
    }
    return;
  }
  if (feature == "BudgetQuery") {
    global_instance_object = script_state->GetContext()->Global();

    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    if (execution_context->IsDocument()) {
      V8WindowPartial::installBudgetQuery(
          isolate, world, global_instance_object, v8::Local<v8::Object>(),
          v8::Local<v8::Function>());
    } else if (execution_context->IsDedicatedWorkerGlobalScope()) {
      V8DedicatedWorkerGlobalScopePartial::installBudgetQuery(
          isolate, world, global_instance_object, v8::Local<v8::Object>(),
          v8::Local<v8::Function>());
    } else if (execution_context->IsSharedWorkerGlobalScope()) {
      V8SharedWorkerGlobalScopePartial::installBudgetQuery(
          isolate, world, global_instance_object, v8::Local<v8::Object>(),
          v8::Local<v8::Function>());
    } else if (execution_context->IsServiceWorkerGlobalScope()) {
      V8ServiceWorkerGlobalScope::installBudgetQuery(
          isolate, world, global_instance_object, v8::Local<v8::Object>(),
          v8::Local<v8::Function>());
    }

    if (context_data->GetExistingConstructorAndPrototypeForType(
            &V8BudgetService::wrapperTypeInfo, &prototype_object,
            &interface_object)) {
      V8BudgetService::installBudgetQuery(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototype_object, interface_object);
    }
    return;
  }
}

void RegisterInstallConditionalFeaturesForModules() {
  RegisterInstallConditionalFeaturesForCore();
  g_original_install_conditional_features_function =
      SetInstallConditionalFeaturesFunction(
          &InstallConditionalFeaturesForModules);
  g_original_install_pending_conditional_feature_function =
      SetInstallPendingConditionalFeatureFunction(
          &InstallPendingConditionalFeatureForModules);
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/ConditionalFeaturesForModules.h"

#include "bindings/core/v8/ConditionalFeatures.h"
#include "bindings/core/v8/ConditionalFeaturesForCore.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8DedicatedWorkerGlobalScope.h"
#include "bindings/core/v8/V8Navigator.h"
#include "bindings/core/v8/V8SharedWorkerGlobalScope.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/core/v8/V8WorkerNavigator.h"
#include "bindings/modules/v8/V8DedicatedWorkerGlobalScopePartial.h"
#include "bindings/modules/v8/V8FetchEvent.h"
#include "bindings/modules/v8/V8Gamepad.h"
#include "bindings/modules/v8/V8GamepadButton.h"
#include "bindings/modules/v8/V8InstallEvent.h"
#include "bindings/modules/v8/V8NavigatorPartial.h"
#include "bindings/modules/v8/V8ServiceWorkerGlobalScope.h"
#include "bindings/modules/v8/V8ServiceWorkerRegistration.h"
#include "bindings/modules/v8/V8SharedWorkerGlobalScopePartial.h"
#include "bindings/modules/v8/V8WindowPartial.h"
#include "bindings/modules/v8/V8WorkerNavigatorPartial.h"
#include "core/dom/ExecutionContext.h"
#include "core/origin_trials/OriginTrials.h"

namespace blink {

namespace {
InstallConditionalFeaturesFunction
    s_originalInstallConditionalFeaturesFunction = nullptr;
InstallPendingConditionalFeatureFunction
    s_originalInstallPendingConditionalFeatureFunction = nullptr;
}

void installConditionalFeaturesForModules(
    const WrapperTypeInfo* wrapperTypeInfo,
    const ScriptState* scriptState,
    v8::Local<v8::Object> prototypeObject,
    v8::Local<v8::Function> interfaceObject) {
  // TODO(iclelland): Generate all of this logic at compile-time, based on the
  // configuration of origin trial enabled attibutes and interfaces in IDL
  // files. (crbug.com/615060)
  (*s_originalInstallConditionalFeaturesFunction)(
      wrapperTypeInfo, scriptState, prototypeObject, interfaceObject);

  ExecutionContext* executionContext = scriptState->getExecutionContext();
  if (!executionContext)
    return;
  v8::Isolate* isolate = scriptState->isolate();
  const DOMWrapperWorld& world = scriptState->world();
  if (wrapperTypeInfo == &V8Navigator::wrapperTypeInfo) {
    if (OriginTrials::webShareEnabled(executionContext)) {
      V8NavigatorPartial::installWebShare(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototypeObject, interfaceObject);
    }
    // Mimics the [SecureContext] extended attribute.
    if (OriginTrials::webUSBEnabled(executionContext) &&
        executionContext->isSecureContext()) {
      V8NavigatorPartial::installWebUSB(isolate, world, v8::Local<v8::Object>(),
                                        prototypeObject, interfaceObject);
    }
    if (OriginTrials::webVREnabled(executionContext)) {
      V8NavigatorPartial::installWebVR(isolate, world, v8::Local<v8::Object>(),
                                       prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8Window::wrapperTypeInfo) {
    v8::Local<v8::Object> instanceObject = scriptState->context()->Global();
    if (OriginTrials::imageCaptureEnabled(executionContext)) {
      V8WindowPartial::installImageCapture(isolate, world, instanceObject,
                                           prototypeObject, interfaceObject);
    }
    // Mimics the [SecureContext] extended attribute.
    if (OriginTrials::webUSBEnabled(executionContext) &&
        executionContext->isSecureContext()) {
      V8WindowPartial::installWebUSB(isolate, world, instanceObject,
                                     prototypeObject, interfaceObject);
    }
    if (OriginTrials::webVREnabled(executionContext)) {
      V8WindowPartial::installWebVR(isolate, world, instanceObject,
                                    prototypeObject, interfaceObject);
    }
    if (OriginTrials::gamepadExtensionsEnabled(executionContext)) {
      V8WindowPartial::installGamepadExtensions(
          isolate, world, instanceObject, prototypeObject, interfaceObject);
    }
    if (OriginTrials::serviceWorkerNavigationPreloadEnabled(executionContext)) {
      V8WindowPartial::installServiceWorkerNavigationPreload(
          isolate, world, instanceObject, prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo ==
             &V8DedicatedWorkerGlobalScope::wrapperTypeInfo) {
    v8::Local<v8::Object> instanceObject = scriptState->context()->Global();
    if (OriginTrials::serviceWorkerNavigationPreloadEnabled(executionContext)) {
      V8DedicatedWorkerGlobalScopePartial::
          installServiceWorkerNavigationPreload(
              isolate, world, instanceObject, prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8SharedWorkerGlobalScope::wrapperTypeInfo) {
    v8::Local<v8::Object> instanceObject = scriptState->context()->Global();
    if (OriginTrials::serviceWorkerNavigationPreloadEnabled(executionContext)) {
      V8SharedWorkerGlobalScopePartial::installServiceWorkerNavigationPreload(
          isolate, world, instanceObject, prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8ServiceWorkerGlobalScope::wrapperTypeInfo) {
    v8::Local<v8::Object> instanceObject = scriptState->context()->Global();
    if (OriginTrials::foreignFetchEnabled(executionContext)) {
      V8ServiceWorkerGlobalScope::installForeignFetch(
          isolate, world, instanceObject, prototypeObject, interfaceObject);
    }
    if (OriginTrials::serviceWorkerNavigationPreloadEnabled(executionContext)) {
      V8ServiceWorkerGlobalScope::installServiceWorkerNavigationPreload(
          isolate, world, instanceObject, prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8ServiceWorkerRegistration::wrapperTypeInfo) {
    if (OriginTrials::serviceWorkerNavigationPreloadEnabled(executionContext)) {
      V8ServiceWorkerRegistration::installServiceWorkerNavigationPreload(
          isolate, world, v8::Local<v8::Object>(), prototypeObject,
          interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8FetchEvent::wrapperTypeInfo) {
    if (OriginTrials::serviceWorkerNavigationPreloadEnabled(executionContext)) {
      V8FetchEvent::installServiceWorkerNavigationPreload(
          isolate, world, v8::Local<v8::Object>(), prototypeObject,
          interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8InstallEvent::wrapperTypeInfo) {
    if (OriginTrials::foreignFetchEnabled(executionContext)) {
      V8InstallEvent::installForeignFetch(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8Gamepad::wrapperTypeInfo) {
    if (OriginTrials::gamepadExtensionsEnabled(executionContext)) {
      V8Gamepad::installGamepadExtensions(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8GamepadButton::wrapperTypeInfo) {
    if (OriginTrials::gamepadExtensionsEnabled(executionContext)) {
      V8GamepadButton::installGamepadExtensions(
          isolate, world, v8::Local<v8::Object>(), prototypeObject,
          interfaceObject);
    }
  }
}

void installPendingConditionalFeatureForModules(
    const String& feature,
    const ScriptState* scriptState) {
  // TODO(iclelland): Generate all of this logic at compile-time, based on the
  // configuration of origin trial enabled attributes and interfaces in IDL
  // files. (crbug.com/615060)
  (*s_originalInstallPendingConditionalFeatureFunction)(feature, scriptState);
  v8::Local<v8::Object> globalInstanceObject;
  v8::Local<v8::Object> prototypeObject;
  v8::Local<v8::Function> interfaceObject;
  v8::Isolate* isolate = scriptState->isolate();
  const DOMWrapperWorld& world = scriptState->world();
  V8PerContextData* contextData = scriptState->perContextData();
  if (feature == "ForeignFetch") {
    if (contextData->getExistingConstructorAndPrototypeForType(
            &V8InstallEvent::wrapperTypeInfo, &prototypeObject,
            &interfaceObject)) {
      V8InstallEvent::installForeignFetch(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototypeObject, interfaceObject);
    }
    return;
  }
  if (feature == "ImageCapture") {
    globalInstanceObject = scriptState->context()->Global();
    V8WindowPartial::installImageCapture(isolate, world, globalInstanceObject,
                                         v8::Local<v8::Object>(),
                                         v8::Local<v8::Function>());
    return;
  }
  if (feature == "ServiceWorkerNavigationPreload") {
    globalInstanceObject = scriptState->context()->Global();
    V8WindowPartial::installServiceWorkerNavigationPreload(
        isolate, world, globalInstanceObject, v8::Local<v8::Object>(),
        v8::Local<v8::Function>());
    if (contextData->getExistingConstructorAndPrototypeForType(
            &V8FetchEvent::wrapperTypeInfo, &prototypeObject,
            &interfaceObject)) {
      V8FetchEvent::installServiceWorkerNavigationPreload(
          isolate, world, v8::Local<v8::Object>(), prototypeObject,
          interfaceObject);
    }
    if (contextData->getExistingConstructorAndPrototypeForType(
            &V8ServiceWorkerRegistration::wrapperTypeInfo, &prototypeObject,
            &interfaceObject)) {
      V8ServiceWorkerRegistration::installServiceWorkerNavigationPreload(
          isolate, world, v8::Local<v8::Object>(), prototypeObject,
          interfaceObject);
    }
    return;
  }
  if (feature == "WebShare") {
    if (contextData->getExistingConstructorAndPrototypeForType(
            &V8Navigator::wrapperTypeInfo, &prototypeObject,
            &interfaceObject)) {
      V8NavigatorPartial::installWebShare(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototypeObject, interfaceObject);
    }
    return;
  }
  if (feature == "WebUSB2") {
    globalInstanceObject = scriptState->context()->Global();
    V8WindowPartial::installWebUSB(isolate, world, globalInstanceObject,
                                   v8::Local<v8::Object>(),
                                   v8::Local<v8::Function>());
    if (contextData->getExistingConstructorAndPrototypeForType(
            &V8Navigator::wrapperTypeInfo, &prototypeObject,
            &interfaceObject)) {
      V8NavigatorPartial::installWebUSB(isolate, world, v8::Local<v8::Object>(),
                                        prototypeObject, interfaceObject);
    }
    return;
  }
  if (feature == "WebVR") {
    globalInstanceObject = scriptState->context()->Global();
    V8WindowPartial::installGamepadExtensions(
        isolate, world, globalInstanceObject, v8::Local<v8::Object>(),
        v8::Local<v8::Function>());
    V8WindowPartial::installWebVR(isolate, world, globalInstanceObject,
                                  v8::Local<v8::Object>(),
                                  v8::Local<v8::Function>());
    if (contextData->getExistingConstructorAndPrototypeForType(
            &V8Gamepad::wrapperTypeInfo, &prototypeObject, &interfaceObject)) {
      V8Gamepad::installGamepadExtensions(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototypeObject, interfaceObject);
    }
    if (contextData->getExistingConstructorAndPrototypeForType(
            &V8GamepadButton::wrapperTypeInfo, &prototypeObject,
            &interfaceObject)) {
      V8GamepadButton::installGamepadExtensions(
          isolate, world, v8::Local<v8::Object>(), prototypeObject,
          interfaceObject);
    }
    if (contextData->getExistingConstructorAndPrototypeForType(
            &V8Navigator::wrapperTypeInfo, &prototypeObject,
            &interfaceObject)) {
      V8NavigatorPartial::installWebVR(isolate, world, v8::Local<v8::Object>(),
                                       prototypeObject, interfaceObject);
    }
    return;
  }
}

void registerInstallConditionalFeaturesForModules() {
  registerInstallConditionalFeaturesForCore();
  s_originalInstallConditionalFeaturesFunction =
      setInstallConditionalFeaturesFunction(
          &installConditionalFeaturesForModules);
  s_originalInstallPendingConditionalFeatureFunction =
      setInstallPendingConditionalFeatureFunction(
          &installPendingConditionalFeatureForModules);
}

}  // namespace blink

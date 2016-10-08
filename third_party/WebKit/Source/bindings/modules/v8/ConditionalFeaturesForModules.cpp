// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/ConditionalFeaturesForModules.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8DedicatedWorkerGlobalScope.h"
#include "bindings/core/v8/V8Navigator.h"
#include "bindings/core/v8/V8SharedWorkerGlobalScope.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/core/v8/V8WorkerNavigator.h"
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
#include "core/origin_trials/OriginTrialContext.h"

namespace blink {

namespace {
InstallConditionalFeaturesFunction
    s_originalInstallConditionalFeaturesFunction = nullptr;
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
  OriginTrialContext* originTrialContext = OriginTrialContext::from(
      executionContext, OriginTrialContext::DontCreateIfNotExists);
  v8::Isolate* isolate = scriptState->isolate();
  const DOMWrapperWorld& world = scriptState->world();
  v8::Local<v8::Object> global = scriptState->context()->Global();
  if (wrapperTypeInfo == &V8Navigator::wrapperTypeInfo) {
    if (RuntimeEnabledFeatures::webBluetoothEnabled() ||
        (originTrialContext &&
         originTrialContext->isTrialEnabled("WebBluetooth"))) {
      V8NavigatorPartial::installWebBluetooth(isolate, world,
                                              v8::Local<v8::Object>(),
                                              prototypeObject, interfaceObject);
    }
    if (RuntimeEnabledFeatures::webShareEnabled() ||
        (originTrialContext &&
         originTrialContext->isTrialEnabled("WebShare"))) {
      V8NavigatorPartial::installWebShare(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototypeObject, interfaceObject);
    }
    if (RuntimeEnabledFeatures::webUSBEnabled() ||
        (originTrialContext && originTrialContext->isTrialEnabled("WebUSB"))) {
      V8NavigatorPartial::installWebUSB(isolate, world, v8::Local<v8::Object>(),
                                        prototypeObject, interfaceObject);
    }
    if (RuntimeEnabledFeatures::webVREnabled() ||
        (originTrialContext && originTrialContext->isTrialEnabled("WebVR"))) {
      V8NavigatorPartial::installWebVR(isolate, world, global, prototypeObject,
                                       interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8Window::wrapperTypeInfo) {
    if (RuntimeEnabledFeatures::webBluetoothEnabled() ||
        (originTrialContext &&
         originTrialContext->isTrialEnabled("WebBluetooth"))) {
      V8WindowPartial::installWebBluetooth(isolate, world, global,
                                           prototypeObject, interfaceObject);
    }
    if (RuntimeEnabledFeatures::webUSBEnabled() ||
        (originTrialContext && originTrialContext->isTrialEnabled("WebUSB"))) {
      V8WindowPartial::installWebUSB(isolate, world, global, prototypeObject,
                                     interfaceObject);
    }
    if (RuntimeEnabledFeatures::webVREnabled() ||
        (originTrialContext && originTrialContext->isTrialEnabled("WebVR"))) {
      V8WindowPartial::installWebVR(isolate, world, global, prototypeObject,
                                    interfaceObject);
    }
    if (RuntimeEnabledFeatures::gamepadExtensionsEnabled() ||
        (originTrialContext && originTrialContext->isTrialEnabled("WebVR"))) {
      V8WindowPartial::installGamepadExtensions(
          isolate, world, global, prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8ServiceWorkerGlobalScope::wrapperTypeInfo) {
    if (RuntimeEnabledFeatures::foreignFetchEnabled() ||
        (originTrialContext &&
         originTrialContext->isTrialEnabled("ForeignFetch"))) {
      V8ServiceWorkerGlobalScope::installForeignFetch(
          isolate, world, global, prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8InstallEvent::wrapperTypeInfo) {
    if (RuntimeEnabledFeatures::foreignFetchEnabled() ||
        (originTrialContext &&
         originTrialContext->isTrialEnabled("ForeignFetch"))) {
      V8InstallEvent::installForeignFetch(isolate, world,
                                          v8::Local<v8::Object>(),
                                          prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8Gamepad::wrapperTypeInfo) {
    if (RuntimeEnabledFeatures::gamepadExtensionsEnabled() ||
        (originTrialContext && originTrialContext->isTrialEnabled("WebVR"))) {
      V8Gamepad::installGamepadExtensions(isolate, world, global,
                                          prototypeObject, interfaceObject);
    }
  } else if (wrapperTypeInfo == &V8GamepadButton::wrapperTypeInfo) {
    if (RuntimeEnabledFeatures::gamepadExtensionsEnabled() ||
        (originTrialContext && originTrialContext->isTrialEnabled("WebVR"))) {
      V8GamepadButton::installGamepadExtensions(
          isolate, world, global, prototypeObject, interfaceObject);
    }
  }
}

void registerInstallConditionalFeaturesForModules() {
  s_originalInstallConditionalFeaturesFunction =
      setInstallConditionalFeaturesFunction(
          &installConditionalFeaturesForModules);
}

}  // namespace blink

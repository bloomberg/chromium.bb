// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Modulator.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"

namespace blink {

namespace {
const char kPerContextDataKey[] = "Modulator";

V8PerContextData* getPerContextData(LocalFrame* frame) {
  ScriptState* scriptState = toScriptStateForMainWorld(frame);
  if (!scriptState)
    return nullptr;
  return scriptState->perContextData();
}
}  // namespace

Modulator* Modulator::from(LocalFrame* frame) {
  return from(getPerContextData(frame));
}

Modulator* Modulator::from(V8PerContextData* perContextData) {
  if (!perContextData)
    return nullptr;
  return static_cast<Modulator*>(perContextData->getData(kPerContextDataKey));
}

Modulator::~Modulator() {}

void Modulator::setModulator(LocalFrame* frame, Modulator* modulator) {
  V8PerContextData* perContextData = getPerContextData(frame);
  DCHECK(perContextData);
  perContextData->addData(kPerContextDataKey, modulator);
}

void Modulator::clearModulator(LocalFrame* frame) {
  V8PerContextData* perContextData = getPerContextData(frame);
  DCHECK(perContextData);
  perContextData->clearData(kPerContextDataKey);
}

KURL Modulator::resolveModuleSpecifier(const String& moduleRequest,
                                       const KURL& baseURL) {
  // Step 1. Apply the URL parser to specifier. If the result is not failure,
  // return the result.
  KURL url(KURL(), moduleRequest);
  if (url.isValid())
    return url;

  // Step 2. If specifier does not start with the character U+002F SOLIDUS (/),
  // the two-character sequence U+002E FULL STOP, U+002F SOLIDUS (./), or the
  // three-character sequence U+002E FULL STOP, U+002E FULL STOP, U+002F SOLIDUS
  // (../), return failure and abort these steps.
  if (!moduleRequest.startsWith("/") && !moduleRequest.startsWith("./") &&
      !moduleRequest.startsWith("../"))
    return KURL();

  // Step 3. Return the result of applying the URL parser to specifier with
  // script's base URL as the base URL.
  DCHECK(baseURL.isValid());
  KURL absoluteURL(baseURL, moduleRequest);
  if (absoluteURL.isValid())
    return absoluteURL;

  return KURL();
}

}  // namespace blink

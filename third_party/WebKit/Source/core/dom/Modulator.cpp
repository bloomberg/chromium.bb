// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Modulator.h"

#include "bindings/core/v8/ScriptState.h"

namespace blink {

Modulator* Modulator::from(LocalFrame* frame) {
  ScriptState* scriptState = ScriptState::forMainWorld(frame);
  if (!scriptState)
    return nullptr;
  // TODO(kouhei): setModulator in V8PerContextData when we land ModulatorImpl.
  return scriptState->perContextData()->modulator();
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

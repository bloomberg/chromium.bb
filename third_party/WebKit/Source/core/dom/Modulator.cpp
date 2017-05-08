// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Modulator.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/dom/ModulatorImpl.h"
#include "core/frame/LocalFrame.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8PerContextData.h"

namespace blink {

namespace {
const char kPerContextDataKey[] = "Modulator";
}  // namespace

Modulator* Modulator::From(ScriptState* script_state) {
  if (!script_state)
    return nullptr;

  V8PerContextData* per_context_data = script_state->PerContextData();
  if (!per_context_data)
    return nullptr;

  Modulator* modulator =
      static_cast<Modulator*>(per_context_data->GetData(kPerContextDataKey));
  if (!modulator) {
    if (Document* document = ToDocument(ExecutionContext::From(script_state))) {
      modulator = ModulatorImpl::Create(script_state, document->Fetcher());
      Modulator::SetModulator(script_state, modulator);
    }
  }
  return modulator;
}

Modulator::~Modulator() {}

void Modulator::SetModulator(ScriptState* script_state, Modulator* modulator) {
  DCHECK(script_state);
  V8PerContextData* per_context_data = script_state->PerContextData();
  DCHECK(per_context_data);
  per_context_data->AddData(kPerContextDataKey, modulator);
}

void Modulator::ClearModulator(ScriptState* script_state) {
  DCHECK(script_state);
  V8PerContextData* per_context_data = script_state->PerContextData();
  DCHECK(per_context_data);
  per_context_data->ClearData(kPerContextDataKey);
}

KURL Modulator::ResolveModuleSpecifier(const String& module_request,
                                       const KURL& base_url) {
  // Step 1. Apply the URL parser to specifier. If the result is not failure,
  // return the result.
  KURL url(KURL(), module_request);
  if (url.IsValid())
    return url;

  // Step 2. If specifier does not start with the character U+002F SOLIDUS (/),
  // the two-character sequence U+002E FULL STOP, U+002F SOLIDUS (./), or the
  // three-character sequence U+002E FULL STOP, U+002E FULL STOP, U+002F SOLIDUS
  // (../), return failure and abort these steps.
  if (!module_request.StartsWith("/") && !module_request.StartsWith("./") &&
      !module_request.StartsWith("../"))
    return KURL();

  // Step 3. Return the result of applying the URL parser to specifier with
  // script's base URL as the base URL.
  DCHECK(base_url.IsValid());
  KURL absolute_url(base_url, module_request);
  if (absolute_url.IsValid())
    return absolute_url;

  return KURL();
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/script/Modulator.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/script/DocumentModulatorImpl.h"
#include "core/script/WorkerModulatorImpl.h"
#include "core/script/WorkletModulatorImpl.h"
#include "core/workers/WorkletGlobalScope.h"
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
  if (modulator)
    return modulator;
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsDocument()) {
    Document* document = ToDocument(execution_context);
    modulator =
        DocumentModulatorImpl::Create(script_state, document->Fetcher());
    Modulator::SetModulator(script_state, modulator);

    // See comment in LocalDOMWindow::modulator_ for this workaround.
    LocalDOMWindow* window = document->ExecutingWindow();
    window->SetModulator(modulator);
  } else if (execution_context->IsWorkletGlobalScope()) {
    modulator = WorkletModulatorImpl::Create(script_state);
    Modulator::SetModulator(script_state, modulator);

    // See comment in WorkerOrWorkletGlobalScope::modulator_ for this
    // workaround.
    ToWorkerOrWorkletGlobalScope(execution_context)->SetModulator(modulator);
  } else if (execution_context->IsWorkerGlobalScope()) {
    modulator = WorkerModulatorImpl::Create(script_state);
    Modulator::SetModulator(script_state, modulator);

    // See comment in WorkerOrWorkletGlobalScope::modulator_ for this
    // workaround.
    ToWorkerOrWorkletGlobalScope(execution_context)->SetModulator(modulator);
  } else {
    NOTREACHED();
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
                                       const KURL& base_url,
                                       String* failure_reason) {
  // https://html.spec.whatwg.org/multipage/webappapis.html#resolve-a-module-specifier

  // Step 1. "Apply the URL parser to specifier. If the result is not failure,
  // return the result." [spec text]
  KURL url(NullURL(), module_request);
  if (url.IsValid())
    return url;

  // Step 2. "If specifier does not start with the character U+002F SOLIDUS (/),
  // the two-character sequence U+002E FULL STOP, U+002F SOLIDUS (./), or the
  // three-character sequence U+002E FULL STOP, U+002E FULL STOP, U+002F SOLIDUS
  // (../), return failure and abort these steps." [spec text]
  if (!module_request.StartsWith("/") && !module_request.StartsWith("./") &&
      !module_request.StartsWith("../")) {
    if (failure_reason) {
      *failure_reason =
          "Relative references must start with either \"/\", \"./\", or "
          "\"../\".";
    }
    return KURL();
  }

  // Step 3. "Return the result of applying the URL parser to specifier with
  // script's base URL as the base URL." [spec text]
  DCHECK(base_url.IsValid());
  KURL absolute_url(base_url, module_request);
  if (absolute_url.IsValid())
    return absolute_url;

  if (failure_reason) {
    *failure_reason = "Invalid relative url or base scheme isn't hierarchical.";
  }
  return KURL();
}

}  // namespace blink

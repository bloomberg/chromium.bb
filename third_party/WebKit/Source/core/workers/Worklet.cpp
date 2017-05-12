// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/Worklet.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletGlobalScopeProxy.h"

namespace blink {

Worklet::Worklet(LocalFrame* frame)
    : ContextLifecycleObserver(frame->GetDocument()) {
  DCHECK(IsMainThread());
}

// Implementation of the first half of the "addModule(moduleURL, options)"
// algorithm:
// https://drafts.css-houdini.org/worklets/#dom-worklet-addmodule
ScriptPromise Worklet::addModule(ScriptState* script_state,
                                 const String& module_url,
                                 const WorkletOptions& options) {
  DCHECK(IsMainThread());
  if (!GetExecutionContext()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError,
                                           "This frame is already detached"));
  }

  // Step 1: "Let promise be a new promise."
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // Step 2: "Let worklet be the current Worklet."
  // |this| is the current Worklet.

  // Step 3: "Let moduleURLRecord be the result of parsing the moduleURL
  // argument relative to the relevant settings object of this."
  KURL module_url_record = GetExecutionContext()->CompleteURL(module_url);

  // Step 4: "If moduleURLRecord is failure, then reject promise with a
  // "SyntaxError" DOMException and return promise."
  if (!module_url_record.IsValid()) {
    resolver->Reject(DOMException::Create(
        kSyntaxError, "'" + module_url + "' is not a valid URL."));
    return promise;
  }

  // Step 5: "Return promise, and then continue running this algorithm in
  // parallel."
  // |kUnspecedLoading| is used here because this is a part of script module
  // loading.
  TaskRunnerHelper::Get(TaskType::kUnspecedLoading, script_state)
      ->PostTask(
          BLINK_FROM_HERE,
          WTF::Bind(&Worklet::FetchAndInvokeScript, WrapPersistent(this),
                    module_url_record, options, WrapPersistent(resolver)));
  return promise;
}

DEFINE_TRACE(Worklet) {
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink

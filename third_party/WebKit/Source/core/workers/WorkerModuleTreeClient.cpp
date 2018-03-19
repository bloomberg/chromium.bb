// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerModuleTreeClient.h"

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "core/script/ModuleScript.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerReportingProxy.h"

namespace blink {

WorkerModuleTreeClient::WorkerModuleTreeClient(Modulator* modulator)
    : modulator_(modulator) {}

// A partial implementation of the "Processing model" algorithm in the HTML
// WebWorker spec:
// https://html.spec.whatwg.org/multipage/workers.html#worker-processing-model
void WorkerModuleTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  if (!module_script) {
    // Step 11: ... "If the algorithm asynchronously completes with null, queue
    // a task to fire an event named error at worker, and abort these steps."
    // ...
    // TODO(nhiroki): Throw an ErrorEvent at the Worker object on the owner
    // Document.
    return;
  }

  // Step 11: ... "Otherwise, continue the rest of these steps after the
  // algorithm's asynchronous completion, with script being the asynchronous
  // completion value." ...

  ScriptValue error = modulator_->ExecuteModule(
      module_script, Modulator::CaptureEvalErrorFlag::kReport);
  WorkerGlobalScope* global_scope =
      ToWorkerGlobalScope(ExecutionContext::From(modulator_->GetScriptState()));
  global_scope->ReportingProxy().DidEvaluateModuleScript(error.IsEmpty());
}

void WorkerModuleTreeClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(modulator_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace blink

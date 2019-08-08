// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_module_tree_client.h"

#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"

namespace blink {

WorkerModuleTreeClient::WorkerModuleTreeClient(Modulator* modulator)
    : modulator_(modulator) {}

// A partial implementation of the "Processing model" algorithm in the HTML
// WebWorker spec:
// https://html.spec.whatwg.org/C/#worker-processing-model
void WorkerModuleTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  auto* execution_context =
      ExecutionContext::From(modulator_->GetScriptState());
  auto* worker_global_scope = To<WorkerGlobalScope>(execution_context);
  blink::WorkerReportingProxy& worker_reporting_proxy =
      worker_global_scope->ReportingProxy();

  if (!module_script) {
    // Step 12: "If the algorithm asynchronously completes with null, queue
    // a task to fire an event named error at worker, and return."
    worker_reporting_proxy.DidFailToFetchModuleScript();
    return;
  }
  worker_reporting_proxy.DidFetchScript(mojom::blink::kAppCacheNoCacheId);

  // Step 12: "Otherwise, continue the rest of these steps after the algorithm's
  // asynchronous completion, with script being the asynchronous completion
  // value."
  worker_global_scope->WorkerScriptFetchFinished(
      *module_script, base::nullopt /* v8_inspector::V8StackTraceId */);
}

void WorkerModuleTreeClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(modulator_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace blink

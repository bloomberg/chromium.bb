// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/worker_resource_fetcher_properties.h"

#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"

namespace blink {

WorkerResourceFetcherProperties::WorkerResourceFetcherProperties(
    WorkerOrWorkletGlobalScope& global_scope,
    scoped_refptr<WebWorkerFetchContext> web_context)
    : global_scope_(global_scope), web_context_(std::move(web_context)) {
  DCHECK(web_context_);
}

void WorkerResourceFetcherProperties::Trace(Visitor* visitor) {
  visitor->Trace(global_scope_);
  ResourceFetcherProperties::Trace(visitor);
}

mojom::ControllerServiceWorkerMode
WorkerResourceFetcherProperties::GetControllerServiceWorkerMode() const {
  return web_context_->IsControlledByServiceWorker();
}

bool WorkerResourceFetcherProperties::IsPaused() const {
  return global_scope_->IsContextPaused();
}

}  // namespace blink

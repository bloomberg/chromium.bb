// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerOrWorkletGlobalScope.h"

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/Deprecation.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/WorkerFetchContext.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/Functional.h"

namespace blink {

WorkerOrWorkletGlobalScope::WorkerOrWorkletGlobalScope(
    v8::Isolate* isolate,
    WorkerClients* worker_clients,
    WorkerReportingProxy& reporting_proxy)
    : worker_clients_(worker_clients),
      script_controller_(
          WorkerOrWorkletScriptController::Create(this, isolate)),
      reporting_proxy_(reporting_proxy),
      used_features_(static_cast<int>(WebFeature::kNumberOfFeatures)) {
  if (worker_clients_)
    worker_clients_->ReattachThread();
}

WorkerOrWorkletGlobalScope::~WorkerOrWorkletGlobalScope() = default;

void WorkerOrWorkletGlobalScope::CountFeature(WebFeature feature) {
  DCHECK(IsContextThread());
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, feature);
  DCHECK_GT(WebFeature::kNumberOfFeatures, feature);
  if (used_features_.QuickGet(static_cast<int>(feature)))
    return;
  used_features_.QuickSet(static_cast<int>(feature));
  ReportingProxy().CountFeature(feature);
}

void WorkerOrWorkletGlobalScope::CountDeprecation(WebFeature feature) {
  DCHECK(IsContextThread());
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, feature);
  DCHECK_GT(WebFeature::kNumberOfFeatures, feature);
  if (used_features_.QuickGet(static_cast<int>(feature)))
    return;
  used_features_.QuickSet(static_cast<int>(feature));

  // Adds a deprecation message to the console.
  DCHECK(!Deprecation::DeprecationMessage(feature).IsEmpty());
  AddConsoleMessage(
      ConsoleMessage::Create(kDeprecationMessageSource, kWarningMessageLevel,
                             Deprecation::DeprecationMessage(feature)));
  ReportingProxy().CountDeprecation(feature);
}

ResourceFetcher* WorkerOrWorkletGlobalScope::GetResourceFetcher() {
  DCHECK(RuntimeEnabledFeatures::OffMainThreadFetchEnabled());
  DCHECK(!IsMainThreadWorkletGlobalScope());
  if (resource_fetcher_)
    return resource_fetcher_;
  WorkerFetchContext* fetch_context = WorkerFetchContext::Create(*this);
  resource_fetcher_ = ResourceFetcher::Create(fetch_context);
  return resource_fetcher_;
}

bool WorkerOrWorkletGlobalScope::IsJSExecutionForbidden() const {
  return script_controller_->IsExecutionForbidden();
}

void WorkerOrWorkletGlobalScope::DisableEval(const String& error_message) {
  script_controller_->DisableEval(error_message);
}

bool WorkerOrWorkletGlobalScope::CanExecuteScripts(
    ReasonForCallingCanExecuteScripts) {
  return !IsJSExecutionForbidden();
}

void WorkerOrWorkletGlobalScope::Dispose() {
  DCHECK(script_controller_);
  script_controller_->Dispose();
  script_controller_.Clear();

  if (resource_fetcher_) {
    resource_fetcher_->StopFetching();
    resource_fetcher_->ClearContext();
  }
}

DEFINE_TRACE(WorkerOrWorkletGlobalScope) {
  visitor->Trace(resource_fetcher_);
  visitor->Trace(script_controller_);
  ExecutionContext::Trace(visitor);
}

}  // namespace blink

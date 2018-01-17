// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/LayoutWorkletGlobalScopeProxy.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/WTF.h"

namespace blink {

LayoutWorkletGlobalScopeProxy::LayoutWorkletGlobalScopeProxy(
    LocalFrame* frame,
    size_t global_scope_number) {
  DCHECK(IsMainThread());
  Document* document = frame->GetDocument();
  reporting_proxy_ =
      std::make_unique<MainThreadWorkletReportingProxy>(document);

  auto creation_params = std::make_unique<GlobalScopeCreationParams>(
      document->Url(), document->UserAgent(),
      document->GetContentSecurityPolicy()->Headers().get(),
      document->GetReferrerPolicy(), document->GetSecurityOrigin(),
      nullptr /* worker_clients */, document->AddressSpace(),
      OriginTrialContext::GetTokens(document).get(),
      nullptr /* worker_settings */, kV8CacheOptionsDefault);
  global_scope_ =
      LayoutWorkletGlobalScope::Create(frame, std::move(creation_params),
                                       *reporting_proxy_, global_scope_number);
}

void LayoutWorkletGlobalScopeProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    WorkletModuleResponsesMap* module_responses_map,
    network::mojom::FetchCredentialsMode credentials_mode,
    scoped_refptr<WebTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  global_scope_->FetchAndInvokeScript(
      module_url_record, module_responses_map, credentials_mode,
      std::move(outside_settings_task_runner), pending_tasks);
}

void LayoutWorkletGlobalScopeProxy::WorkletObjectDestroyed() {
  DCHECK(IsMainThread());
  // Do nothing.
}

void LayoutWorkletGlobalScopeProxy::TerminateWorkletGlobalScope() {
  DCHECK(IsMainThread());
  global_scope_->Terminate();
  // Nullify these fields to cut a potential reference cycle.
  global_scope_ = nullptr;
  reporting_proxy_.reset();
}

void LayoutWorkletGlobalScopeProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(global_scope_);
}

}  // namespace blink

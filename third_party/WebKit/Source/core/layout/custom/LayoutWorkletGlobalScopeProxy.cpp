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
#include "core/workers/WorkletModuleResponsesMap.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/WTF.h"

namespace blink {

LayoutWorkletGlobalScopeProxy* LayoutWorkletGlobalScopeProxy::From(
    WorkletGlobalScopeProxy* proxy) {
  return static_cast<LayoutWorkletGlobalScopeProxy*>(proxy);
}

LayoutWorkletGlobalScopeProxy::LayoutWorkletGlobalScopeProxy(
    LocalFrame* frame,
    WorkletModuleResponsesMap* module_responses_map,
    PendingLayoutRegistry* pending_layout_registry,
    size_t global_scope_number) {
  DCHECK(IsMainThread());
  Document* document = frame->GetDocument();
  reporting_proxy_ =
      std::make_unique<MainThreadWorkletReportingProxy>(document);

  auto creation_params = std::make_unique<GlobalScopeCreationParams>(
      document->Url(), document->UserAgent(),
      document->GetContentSecurityPolicy()->Headers().get(),
      document->GetReferrerPolicy(), document->GetSecurityOrigin(),
      document->IsSecureContext(), nullptr /* worker_clients */,
      document->AddressSpace(), OriginTrialContext::GetTokens(document).get(),
      base::UnguessableToken::Create(), nullptr /* worker_settings */,
      kV8CacheOptionsDefault, module_responses_map);
  global_scope_ = LayoutWorkletGlobalScope::Create(
      frame, std::move(creation_params), *reporting_proxy_,
      pending_layout_registry, global_scope_number);
}

void LayoutWorkletGlobalScopeProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    network::mojom::FetchCredentialsMode credentials_mode,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  global_scope_->FetchAndInvokeScript(module_url_record, credentials_mode,
                                      std::move(outside_settings_task_runner),
                                      pending_tasks);
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

CSSLayoutDefinition* LayoutWorkletGlobalScopeProxy::FindDefinition(
    const AtomicString& name) {
  DCHECK(IsMainThread());
  return global_scope_->FindDefinition(name);
}

void LayoutWorkletGlobalScopeProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(global_scope_);
}

}  // namespace blink

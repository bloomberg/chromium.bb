// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/worklet/worklet_thread_test_common.h"

#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_proxy_client.h"

namespace blink {

std::unique_ptr<AnimationAndPaintWorkletThread>
CreateAnimationAndPaintWorkletThread(
    Document* document,
    WorkerReportingProxy* reporting_proxy,
    AnimationWorkletProxyClient* proxy_client) {
  std::unique_ptr<AnimationAndPaintWorkletThread> thread =
      AnimationAndPaintWorkletThread::CreateForAnimationWorklet(
          *reporting_proxy);

  if (!proxy_client) {
    proxy_client = MakeGarbageCollected<AnimationWorkletProxyClient>(
        1, nullptr, /* mutator_dispatcher */
        nullptr,    /* mutator_runner */
        nullptr,    /* mutator_dispatcher */
        nullptr     /* mutator_runner */
    );
  }
  WorkerClients* clients = WorkerClients::Create();
  ProvideAnimationWorkletProxyClientTo(clients, proxy_client);

  thread->Start(
      std::make_unique<GlobalScopeCreationParams>(
          document->Url(), mojom::ScriptType::kModule,
          OffMainThreadWorkerScriptFetchOption::kEnabled, "Worklet",
          document->UserAgent(), nullptr /* web_worker_fetch_context */,
          Vector<CSPHeaderAndType>(), document->GetReferrerPolicy(),
          document->GetSecurityOrigin(), document->IsSecureContext(),
          document->GetHttpsState(), clients, document->AddressSpace(),
          OriginTrialContext::GetTokens(document).get(),
          base::UnguessableToken::Create(), nullptr /* worker_settings */,
          kV8CacheOptionsDefault,
          MakeGarbageCollected<WorkletModuleResponsesMap>()),
      base::nullopt, std::make_unique<WorkerDevToolsParams>(),
      ParentExecutionContextTaskRunners::Create());
  return thread;
}

}  // namespace blink

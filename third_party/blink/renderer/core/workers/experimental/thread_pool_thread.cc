// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/thread_pool_thread.h"

#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/workers/experimental/task_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_object_proxy_base.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {

namespace {

class ThreadPoolWorkerGlobalScope final : public WorkerGlobalScope {
 public:
  ThreadPoolWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params,
      WorkerThread* thread)
      : WorkerGlobalScope(std::move(creation_params),
                          thread,
                          CurrentTimeTicks()) {
    ReadyToRunClassicScript();
  }

  ~ThreadPoolWorkerGlobalScope() override = default;

  // EventTarget
  const AtomicString& InterfaceName() const override {
    // TODO(japhet): Replaces this with
    // EventTargetNames::ThreadPoolWorkerGlobalScope.
    return event_target_names::kDedicatedWorkerGlobalScope;
  }

  // WorkerGlobalScope
  void Initialize(const KURL& response_url,
                  network::mojom::ReferrerPolicy response_referrer_policy,
                  mojom::IPAddressSpace response_address_space,
                  const Vector<CSPHeaderAndType>& response_csp_headers,
                  const Vector<String>* response_origin_trial_tokens) override {
    InitializeURL(response_url);
    SetReferrerPolicy(response_referrer_policy);
    SetAddressSpace(response_address_space);

    // These should be called after SetAddressSpace() to correctly override the
    // address space by the "treat-as-public-address" CSP directive.
    InitContentSecurityPolicyFromVector(response_csp_headers);
    BindContentSecurityPolicyToExecutionContext();

    OriginTrialContext::AddTokens(this, response_origin_trial_tokens);

    // This should be called after OriginTrialContext::AddTokens() to install
    // origin trial features in JavaScript's global object.
    ScriptController()->PrepareForEvaluation();
  }
  void FetchAndRunClassicScript(
      const KURL& script_url,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      const v8_inspector::V8StackTraceId& stack_id) override {
    NOTREACHED();
  }
  void FetchAndRunModuleScript(
      const KURL& module_url_record,
      const FetchClientSettingsObjectSnapshot& outside_settings_object,
      WorkerResourceTimingNotifier& outside_resource_timing_notifier,
      network::mojom::FetchCredentialsMode) override {
    // TODO(japhet): Consider whether modules should be supported.
    NOTREACHED();
  }

  void ExceptionThrown(ErrorEvent*) override {}
};

}  // anonymous namespace

ThreadPoolThread::ThreadPoolThread(ExecutionContext* parent_execution_context,
                                   ThreadedObjectProxyBase& object_proxy,
                                   ThreadBackingPolicy backing_policy)
    : WorkerThread(object_proxy), backing_policy_(backing_policy) {
  DCHECK(parent_execution_context);
  worker_backing_thread_ = std::make_unique<WorkerBackingThread>(
      ThreadCreationParams(GetThreadType())
          .SetFrameOrWorkerScheduler(parent_execution_context->GetScheduler()));
}

WorkerOrWorkletGlobalScope* ThreadPoolThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  if (backing_policy_ == kWorker) {
    return MakeGarbageCollected<ThreadPoolWorkerGlobalScope>(
        std::move(creation_params), this);
  }
  return MakeGarbageCollected<TaskWorkletGlobalScope>(
      std::move(creation_params), this);
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletMessagingProxy.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/dom/Document.h"
#include "core/dom/SecurityContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "core/workers/WorkletGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"

namespace blink {

ThreadedWorkletMessagingProxy::ThreadedWorkletMessagingProxy(
    ExecutionContext* execution_context)
    : ThreadedMessagingProxyBase(execution_context), weak_ptr_factory_(this) {
  worklet_object_proxy_ = ThreadedWorkletObjectProxy::Create(
      weak_ptr_factory_.CreateWeakPtr(), GetParentFrameTaskRunners());
}

void ThreadedWorkletMessagingProxy::Initialize() {
  DCHECK(IsParentContextThread());
  if (AskedToTerminate())
    return;

  Document* document = ToDocument(GetExecutionContext());
  SecurityOrigin* starter_origin = document->GetSecurityOrigin();
  KURL script_url = document->Url();

  ContentSecurityPolicy* csp = document->GetContentSecurityPolicy();
  DCHECK(csp);

  WorkerThreadStartMode start_mode =
      GetWorkerInspectorProxy()->WorkerStartMode(document);
  std::unique_ptr<WorkerSettings> worker_settings =
      WTF::WrapUnique(new WorkerSettings(document->GetSettings()));

  // TODO(ikilpatrick): Decide on sensible a value for referrerPolicy.
  std::unique_ptr<WorkerThreadStartupData> startup_data =
      WorkerThreadStartupData::Create(
          script_url, document->UserAgent(), String(), nullptr, start_mode,
          csp->Headers().get(), /* referrerPolicy */ String(), starter_origin,
          nullptr, document->AddressSpace(),
          OriginTrialContext::GetTokens(document).get(),
          std::move(worker_settings), WorkerV8Settings::Default());

  InitializeWorkerThread(std::move(startup_data));
  GetWorkerInspectorProxy()->WorkerThreadCreated(document, GetWorkerThread(),
                                                 script_url);
}

void ThreadedWorkletMessagingProxy::EvaluateScript(
    const ScriptSourceCode& script_source_code) {
  TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedWorkletObjectProxy::EvaluateScript,
                          CrossThreadUnretained(worklet_object_proxy_.get()),
                          script_source_code.Source(), script_source_code.Url(),
                          CrossThreadUnretained(GetWorkerThread())));
}

void ThreadedWorkletMessagingProxy::TerminateWorkletGlobalScope() {
  TerminateGlobalScope();
}

}  // namespace blink

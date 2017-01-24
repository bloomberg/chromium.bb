// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletMessagingProxy.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/dom/Document.h"
#include "core/dom/SecurityContext.h"
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
    ExecutionContext* executionContext)
    : ThreadedMessagingProxyBase(executionContext), m_weakPtrFactory(this) {
  m_workletObjectProxy = ThreadedWorkletObjectProxy::create(
      m_weakPtrFactory.createWeakPtr(), getParentFrameTaskRunners());
}

void ThreadedWorkletMessagingProxy::initialize() {
  DCHECK(isParentContextThread());
  if (askedToTerminate())
    return;

  Document* document = toDocument(getExecutionContext());
  SecurityOrigin* starterOrigin = document->getSecurityOrigin();
  KURL scriptURL = document->url();

  ContentSecurityPolicy* csp = document->contentSecurityPolicy();
  DCHECK(csp);

  WorkerThreadStartMode startMode =
      workerInspectorProxy()->workerStartMode(document);
  std::unique_ptr<WorkerSettings> workerSettings =
      WTF::wrapUnique(new WorkerSettings(document->settings()));

  // TODO(ikilpatrick): Decide on sensible a value for referrerPolicy.
  std::unique_ptr<WorkerThreadStartupData> startupData =
      WorkerThreadStartupData::create(
          scriptURL, document->userAgent(), String(), nullptr, startMode,
          csp->headers().get(), /* referrerPolicy */ String(), starterOrigin,
          nullptr, document->addressSpace(),
          OriginTrialContext::getTokens(document).get(),
          std::move(workerSettings), WorkerV8Settings::Default());

  initializeWorkerThread(std::move(startupData));
  workerInspectorProxy()->workerThreadCreated(document, workerThread(),
                                              scriptURL);
}

void ThreadedWorkletMessagingProxy::evaluateScript(
    const ScriptSourceCode& scriptSourceCode) {
  postTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      crossThreadBind(&ThreadedWorkletObjectProxy::evaluateScript,
                      crossThreadUnretained(m_workletObjectProxy.get()),
                      scriptSourceCode.source(), scriptSourceCode.url(),
                      crossThreadUnretained(workerThread())));
}

void ThreadedWorkletMessagingProxy::terminateWorkletGlobalScope() {
  terminateGlobalScope();
}

}  // namespace blink

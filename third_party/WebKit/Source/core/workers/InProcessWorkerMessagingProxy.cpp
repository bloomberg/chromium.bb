/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/workers/InProcessWorkerMessagingProxy.h"

#include "core/dom/Document.h"
#include "core/dom/SecurityContext.h"
#include "core/events/ErrorEvent.h"
#include "core/events/MessageEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/InProcessWorkerBase.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "wtf/WTF.h"
#include <memory>

namespace blink {

InProcessWorkerMessagingProxy::InProcessWorkerMessagingProxy(
    InProcessWorkerBase* workerObject,
    WorkerClients* workerClients)
    : InProcessWorkerMessagingProxy(workerObject->getExecutionContext(),
                                    workerObject,
                                    workerClients) {
  DCHECK(m_workerObject);
}

InProcessWorkerMessagingProxy::InProcessWorkerMessagingProxy(
    ExecutionContext* executionContext,
    InProcessWorkerBase* workerObject,
    WorkerClients* workerClients)
    : ThreadedMessagingProxyBase(executionContext),
      m_workerObject(workerObject),
      m_workerClients(workerClients),
      m_weakPtrFactory(this) {
  m_workerObjectProxy = InProcessWorkerObjectProxy::create(
      m_weakPtrFactory.createWeakPtr(), getParentFrameTaskRunners());
}

InProcessWorkerMessagingProxy::~InProcessWorkerMessagingProxy() {
  DCHECK(!m_workerObject);
}

void InProcessWorkerMessagingProxy::startWorkerGlobalScope(
    const KURL& scriptURL,
    const String& userAgent,
    const String& sourceCode,
    ContentSecurityPolicy* contentSecurityPolicy,
    const String& referrerPolicy) {
  DCHECK(isParentContextThread());
  if (askedToTerminate()) {
    // Worker.terminate() could be called from JS before the thread was
    // created.
    return;
  }

  Document* document = toDocument(getExecutionContext());
  SecurityOrigin* starterOrigin = document->getSecurityOrigin();

  ContentSecurityPolicy* csp = contentSecurityPolicy
                                   ? contentSecurityPolicy
                                   : document->contentSecurityPolicy();
  DCHECK(csp);

  WorkerThreadStartMode startMode =
      workerInspectorProxy()->workerStartMode(document);
  std::unique_ptr<WorkerSettings> workerSettings =
      WTF::wrapUnique(new WorkerSettings(document->settings()));
  WorkerV8Settings workerV8Settings(WorkerV8Settings::Default());
  workerV8Settings.m_heapLimitMode =
      toIsolate(document)->IsHeapLimitIncreasedForDebugging()
          ? WorkerV8Settings::HeapLimitMode::IncreasedForDebugging
          : WorkerV8Settings::HeapLimitMode::Default;
  std::unique_ptr<WorkerThreadStartupData> startupData =
      WorkerThreadStartupData::create(
          scriptURL, userAgent, sourceCode, nullptr, startMode,
          csp->headers().get(), referrerPolicy, starterOrigin,
          m_workerClients.release(), document->addressSpace(),
          OriginTrialContext::getTokens(document).get(),
          std::move(workerSettings), workerV8Settings);

  initializeWorkerThread(std::move(startupData));
  workerInspectorProxy()->workerThreadCreated(document, workerThread(),
                                              scriptURL);
}

void InProcessWorkerMessagingProxy::postMessageToWorkerObject(
    PassRefPtr<SerializedScriptValue> message,
    MessagePortChannelArray channels) {
  DCHECK(isParentContextThread());
  if (!m_workerObject || askedToTerminate())
    return;

  MessagePortArray* ports =
      MessagePort::entanglePorts(*getExecutionContext(), std::move(channels));
  m_workerObject->dispatchEvent(
      MessageEvent::create(ports, std::move(message)));
}

void InProcessWorkerMessagingProxy::postMessageToWorkerGlobalScope(
    PassRefPtr<SerializedScriptValue> message,
    MessagePortChannelArray channels) {
  DCHECK(isParentContextThread());
  if (askedToTerminate())
    return;

  if (workerThread()) {
    // A message event is an activity and may initiate another activity.
    m_workerGlobalScopeHasPendingActivity = true;
    ++m_unconfirmedMessageCount;
    std::unique_ptr<WTF::CrossThreadClosure> task = crossThreadBind(
        &InProcessWorkerObjectProxy::processMessageFromWorkerObject,
        crossThreadUnretained(&workerObjectProxy()), std::move(message),
        WTF::passed(std::move(channels)),
        crossThreadUnretained(workerThread()));
    workerThread()->postTask(BLINK_FROM_HERE, std::move(task));
  } else {
    m_queuedEarlyTasks.push_back(
        WTF::makeUnique<QueuedTask>(std::move(message), std::move(channels)));
  }
}

void InProcessWorkerMessagingProxy::dispatchErrorEvent(
    const String& errorMessage,
    std::unique_ptr<SourceLocation> location,
    int exceptionId) {
  DCHECK(isParentContextThread());
  if (!m_workerObject)
    return;

  // We don't bother checking the askedToTerminate() flag here, because
  // exceptions should *always* be reported even if the thread is terminated.
  // This is intentionally different than the behavior in MessageWorkerTask,
  // because terminated workers no longer deliver messages (section 4.6 of the
  // WebWorker spec), but they do report exceptions.

  ErrorEvent* event =
      ErrorEvent::create(errorMessage, location->clone(), nullptr);
  if (m_workerObject->dispatchEvent(event) != DispatchEventResult::NotCanceled)
    return;

  postTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      crossThreadBind(&InProcessWorkerObjectProxy::processUnhandledException,
                      crossThreadUnretained(m_workerObjectProxy.get()),
                      exceptionId, crossThreadUnretained(workerThread())));
}

void InProcessWorkerMessagingProxy::workerThreadCreated() {
  DCHECK(isParentContextThread());
  ThreadedMessagingProxyBase::workerThreadCreated();

  // Worker initialization means a pending activity.
  m_workerGlobalScopeHasPendingActivity = true;

  DCHECK_EQ(0u, m_unconfirmedMessageCount);
  m_unconfirmedMessageCount = m_queuedEarlyTasks.size();
  for (auto& queuedTask : m_queuedEarlyTasks) {
    std::unique_ptr<WTF::CrossThreadClosure> task = crossThreadBind(
        &InProcessWorkerObjectProxy::processMessageFromWorkerObject,
        crossThreadUnretained(&workerObjectProxy()),
        queuedTask->message.release(),
        WTF::passed(std::move(queuedTask->channels)),
        crossThreadUnretained(workerThread()));
    workerThread()->postTask(BLINK_FROM_HERE, std::move(task));
  }
  m_queuedEarlyTasks.clear();
}

void InProcessWorkerMessagingProxy::parentObjectDestroyed() {
  DCHECK(isParentContextThread());

  // parentObjectDestroyed() is called in InProcessWorkerBase's destructor.
  // Thus it should be guaranteed that a weak pointer m_workerObject has been
  // cleared before this method gets called.
  DCHECK(!m_workerObject);

  ThreadedMessagingProxyBase::parentObjectDestroyed();
}

void InProcessWorkerMessagingProxy::confirmMessageFromWorkerObject() {
  DCHECK(isParentContextThread());
  if (askedToTerminate())
    return;
  DCHECK(m_workerGlobalScopeHasPendingActivity);
  DCHECK_GT(m_unconfirmedMessageCount, 0u);
  --m_unconfirmedMessageCount;
}

void InProcessWorkerMessagingProxy::pendingActivityFinished() {
  DCHECK(isParentContextThread());
  DCHECK(m_workerGlobalScopeHasPendingActivity);
  if (m_unconfirmedMessageCount > 0) {
    // Ignore the report because an inflight message event may initiate a
    // new activity.
    return;
  }
  m_workerGlobalScopeHasPendingActivity = false;
}

bool InProcessWorkerMessagingProxy::hasPendingActivity() const {
  DCHECK(isParentContextThread());
  if (askedToTerminate())
    return false;
  return m_workerGlobalScopeHasPendingActivity;
}

InProcessWorkerMessagingProxy::QueuedTask::QueuedTask(
    RefPtr<SerializedScriptValue> message,
    MessagePortChannelArray channels)
    : message(std::move(message)), channels(std::move(channels)) {}

InProcessWorkerMessagingProxy::QueuedTask::~QueuedTask() = default;

}  // namespace blink

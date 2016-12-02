// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletObjectProxy.h"

#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<ThreadedWorkletObjectProxy> ThreadedWorkletObjectProxy::create(
    const WeakPtr<ThreadedWorkletMessagingProxy>& messagingProxyWeakPtr) {
  DCHECK(messagingProxyWeakPtr);
  return wrapUnique(new ThreadedWorkletObjectProxy(messagingProxyWeakPtr));
}

ThreadedWorkletObjectProxy::~ThreadedWorkletObjectProxy() {
  DCHECK(m_messagingProxyWeakPtr);
}

void ThreadedWorkletObjectProxy::reportConsoleMessage(
    MessageSource source,
    MessageLevel level,
    const String& message,
    SourceLocation* location) {
  getParentFrameTaskRunners()
      ->get(TaskType::Internal)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&ThreadedWorkletMessagingProxy::reportConsoleMessage,
                          m_messagingProxyWeakPtr, source, level, message,
                          passed(location->clone())));
}

void ThreadedWorkletObjectProxy::postMessageToPageInspector(
    const String& message) {
  DCHECK(m_messagingProxyWeakPtr->getExecutionContext()->isDocument());
  // The TaskType of Inspector tasks need to be Unthrottled because they need to
  // run even on a suspended page.
  getParentFrameTaskRunners()
      ->get(TaskType::Unthrottled)
      ->postTask(BLINK_FROM_HERE,
                 crossThreadBind(
                     &ThreadedWorkletMessagingProxy::postMessageToPageInspector,
                     m_messagingProxyWeakPtr, message));
}

ParentFrameTaskRunners*
ThreadedWorkletObjectProxy::getParentFrameTaskRunners() {
  return m_messagingProxyWeakPtr->getParentFrameTaskRunners();
}

void ThreadedWorkletObjectProxy::didCloseWorkerGlobalScope() {
  getParentFrameTaskRunners()
      ->get(TaskType::Internal)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&ThreadedWorkletMessagingProxy::terminateGlobalScope,
                          m_messagingProxyWeakPtr));
}

void ThreadedWorkletObjectProxy::didTerminateWorkerThread() {
  // This will terminate the MessagingProxy.
  getParentFrameTaskRunners()
      ->get(TaskType::Internal)
      ->postTask(BLINK_FROM_HERE,
                 crossThreadBind(
                     &ThreadedWorkletMessagingProxy::workerThreadTerminated,
                     m_messagingProxyWeakPtr));
}

ThreadedWorkletObjectProxy::ThreadedWorkletObjectProxy(
    const WeakPtr<ThreadedWorkletMessagingProxy>& messagingProxyWeakPtr)
    : m_messagingProxyWeakPtr(messagingProxyWeakPtr) {}

}  // namespace blink

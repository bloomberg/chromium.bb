// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletObjectProxy.h"

#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<ThreadedWorkletObjectProxy> ThreadedWorkletObjectProxy::create(
    ThreadedWorkletMessagingProxy* messagingProxy) {
  DCHECK(messagingProxy);
  return wrapUnique(new ThreadedWorkletObjectProxy(messagingProxy));
}

void ThreadedWorkletObjectProxy::reportConsoleMessage(
    MessageSource source,
    MessageLevel level,
    const String& message,
    SourceLocation* location) {
  // TODO(nhiroki): Replace this with getParentFrameTaskRunners().
  // (https://crbug.com/667310)
  getExecutionContext()->postTask(
      BLINK_FROM_HERE, createCrossThreadTask(
                           &ThreadedWorkletMessagingProxy::reportConsoleMessage,
                           crossThreadUnretained(m_messagingProxy), source,
                           level, message, passed(location->clone())));
}

void ThreadedWorkletObjectProxy::postMessageToPageInspector(
    const String& message) {
  DCHECK(getExecutionContext()->isDocument());
  // TODO(nhiroki): Replace this with getParentFrameTaskRunners().
  // (https://crbug.com/667310)
  toDocument(getExecutionContext())
      ->postInspectorTask(
          BLINK_FROM_HERE,
          createCrossThreadTask(
              &ThreadedWorkletMessagingProxy::postMessageToPageInspector,
              crossThreadUnretained(m_messagingProxy), message));
}

ParentFrameTaskRunners*
ThreadedWorkletObjectProxy::getParentFrameTaskRunners() {
  DCHECK(m_messagingProxy);
  return m_messagingProxy->getParentFrameTaskRunners();
}

void ThreadedWorkletObjectProxy::didCloseWorkerGlobalScope() {
  // TODO(nhiroki): Replace this with getParentFrameTaskRunners().
  // (https://crbug.com/667310)
  getExecutionContext()->postTask(
      BLINK_FROM_HERE, createCrossThreadTask(
                           &ThreadedWorkletMessagingProxy::terminateGlobalScope,
                           crossThreadUnretained(m_messagingProxy)));
}

void ThreadedWorkletObjectProxy::didTerminateWorkerThread() {
  // This will terminate the MessagingProxy.
  // TODO(nhiroki): Replace this with getParentFrameTaskRunners().
  // (https://crbug.com/667310)
  getExecutionContext()->postTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(
          &ThreadedWorkletMessagingProxy::workerThreadTerminated,
          crossThreadUnretained(m_messagingProxy)));
}

ThreadedWorkletObjectProxy::ThreadedWorkletObjectProxy(
    ThreadedWorkletMessagingProxy* messagingProxy)
    : m_messagingProxy(messagingProxy) {}

ExecutionContext* ThreadedWorkletObjectProxy::getExecutionContext() const {
  DCHECK(m_messagingProxy);
  return m_messagingProxy->getExecutionContext();
}

}  // namespace blink

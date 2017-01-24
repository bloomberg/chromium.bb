// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedObjectProxyBase.h"

#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/ThreadedMessagingProxyBase.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

void ThreadedObjectProxyBase::countFeature(UseCounter::Feature feature) {
  getParentFrameTaskRunners()
      ->get(TaskType::UnspecedTimer)
      ->postTask(BLINK_FROM_HERE,
                 crossThreadBind(&ThreadedMessagingProxyBase::countFeature,
                                 messagingProxyWeakPtr(), feature));
}

void ThreadedObjectProxyBase::countDeprecation(UseCounter::Feature feature) {
  getParentFrameTaskRunners()
      ->get(TaskType::UnspecedTimer)
      ->postTask(BLINK_FROM_HERE,
                 crossThreadBind(&ThreadedMessagingProxyBase::countDeprecation,
                                 messagingProxyWeakPtr(), feature));
}

void ThreadedObjectProxyBase::reportConsoleMessage(MessageSource source,
                                                   MessageLevel level,
                                                   const String& message,
                                                   SourceLocation* location) {
  getParentFrameTaskRunners()
      ->get(TaskType::UnspecedTimer)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&ThreadedMessagingProxyBase::reportConsoleMessage,
                          messagingProxyWeakPtr(), source, level, message,
                          WTF::passed(location->clone())));
}

void ThreadedObjectProxyBase::postMessageToPageInspector(
    const String& message) {
  // The TaskType of Inspector tasks need to be Unthrottled because they need to
  // run even on a suspended page.
  getParentFrameTaskRunners()
      ->get(TaskType::Unthrottled)
      ->postTask(BLINK_FROM_HERE,
                 crossThreadBind(
                     &ThreadedMessagingProxyBase::postMessageToPageInspector,
                     messagingProxyWeakPtr(), message));
}

void ThreadedObjectProxyBase::didCloseWorkerGlobalScope() {
  getParentFrameTaskRunners()
      ->get(TaskType::UnspecedTimer)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&ThreadedMessagingProxyBase::terminateGlobalScope,
                          messagingProxyWeakPtr()));
}

void ThreadedObjectProxyBase::didTerminateWorkerThread() {
  // This will terminate the MessagingProxy.
  getParentFrameTaskRunners()
      ->get(TaskType::UnspecedTimer)
      ->postTask(
          BLINK_FROM_HERE,
          crossThreadBind(&ThreadedMessagingProxyBase::workerThreadTerminated,
                          messagingProxyWeakPtr()));
}

ParentFrameTaskRunners* ThreadedObjectProxyBase::getParentFrameTaskRunners() {
  return m_parentFrameTaskRunners.get();
}

ThreadedObjectProxyBase::ThreadedObjectProxyBase(
    ParentFrameTaskRunners* parentFrameTaskRunners)
    : m_parentFrameTaskRunners(parentFrameTaskRunners) {}

}  // namespace blink

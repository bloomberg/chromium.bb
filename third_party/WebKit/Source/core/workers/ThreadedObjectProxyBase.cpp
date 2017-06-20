// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedObjectProxyBase.h"

#include <memory>
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/ThreadedMessagingProxyBase.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

void ThreadedObjectProxyBase::CountFeature(WebFeature feature) {
  GetParentFrameTaskRunners()
      ->Get(TaskType::kUnspecedTimer)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(&ThreadedMessagingProxyBase::CountFeature,
                                 MessagingProxyWeakPtr(), feature));
}

void ThreadedObjectProxyBase::CountDeprecation(WebFeature feature) {
  GetParentFrameTaskRunners()
      ->Get(TaskType::kUnspecedTimer)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(&ThreadedMessagingProxyBase::CountDeprecation,
                                 MessagingProxyWeakPtr(), feature));
}

void ThreadedObjectProxyBase::ReportConsoleMessage(MessageSource source,
                                                   MessageLevel level,
                                                   const String& message,
                                                   SourceLocation* location) {
  GetParentFrameTaskRunners()
      ->Get(TaskType::kUnspecedTimer)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedMessagingProxyBase::ReportConsoleMessage,
                          MessagingProxyWeakPtr(), source, level, message,
                          WTF::Passed(location->Clone())));
}

void ThreadedObjectProxyBase::PostMessageToPageInspector(
    int session_id,
    const String& message) {
  // The TaskType of Inspector tasks need to be Unthrottled because they need to
  // run even on a suspended page.
  GetParentFrameTaskRunners()
      ->Get(TaskType::kUnthrottled)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(
                     &ThreadedMessagingProxyBase::PostMessageToPageInspector,
                     MessagingProxyWeakPtr(), session_id, message));
}

void ThreadedObjectProxyBase::DidCloseWorkerGlobalScope() {
  GetParentFrameTaskRunners()
      ->Get(TaskType::kUnspecedTimer)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedMessagingProxyBase::TerminateGlobalScope,
                          MessagingProxyWeakPtr()));
}

void ThreadedObjectProxyBase::DidTerminateWorkerThread() {
  // This will terminate the MessagingProxy.
  GetParentFrameTaskRunners()
      ->Get(TaskType::kUnspecedTimer)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedMessagingProxyBase::WorkerThreadTerminated,
                          MessagingProxyWeakPtr()));
}

ParentFrameTaskRunners* ThreadedObjectProxyBase::GetParentFrameTaskRunners() {
  return parent_frame_task_runners_.Get();
}

ThreadedObjectProxyBase::ThreadedObjectProxyBase(
    ParentFrameTaskRunners* parent_frame_task_runners)
    : parent_frame_task_runners_(parent_frame_task_runners) {}

}  // namespace blink

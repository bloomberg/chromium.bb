/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/workers/InProcessWorkerObjectProxy.h"

#include <memory>
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/MessageEvent.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"

namespace blink {

const double kDefaultIntervalInSec = 1;
const double kMaxIntervalInSec = 30;

std::unique_ptr<InProcessWorkerObjectProxy> InProcessWorkerObjectProxy::Create(
    InProcessWorkerMessagingProxy* messaging_proxy_weak_ptr,
    ParentFrameTaskRunners* parent_frame_task_runners) {
  DCHECK(messaging_proxy_weak_ptr);
  return WTF::WrapUnique(new InProcessWorkerObjectProxy(
      messaging_proxy_weak_ptr, parent_frame_task_runners));
}

InProcessWorkerObjectProxy::~InProcessWorkerObjectProxy() {}

void InProcessWorkerObjectProxy::PostMessageToWorkerObject(
    RefPtr<SerializedScriptValue> message,
    MessagePortChannelArray channels) {
  GetParentFrameTaskRunners()
      ->Get(TaskType::kPostedMessage)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(
                     &InProcessWorkerMessagingProxy::PostMessageToWorkerObject,
                     messaging_proxy_weak_ptr_, std::move(message),
                     WTF::Passed(std::move(channels))));
}

void InProcessWorkerObjectProxy::ProcessMessageFromWorkerObject(
    RefPtr<SerializedScriptValue> message,
    MessagePortChannelArray channels,
    WorkerThread* worker_thread) {
  WorkerGlobalScope* global_scope =
      ToWorkerGlobalScope(worker_thread->GlobalScope());
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*global_scope, std::move(channels));
  global_scope->DispatchEvent(MessageEvent::Create(ports, std::move(message)));

  GetParentFrameTaskRunners()
      ->Get(TaskType::kUnspecedTimer)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(
              &InProcessWorkerMessagingProxy::ConfirmMessageFromWorkerObject,
              messaging_proxy_weak_ptr_));

  StartPendingActivityTimer();
}

void InProcessWorkerObjectProxy::ProcessUnhandledException(
    int exception_id,
    WorkerThread* worker_thread) {
  WorkerGlobalScope* global_scope =
      ToWorkerGlobalScope(worker_thread->GlobalScope());
  global_scope->ExceptionUnhandled(exception_id);
}

void InProcessWorkerObjectProxy::ReportException(
    const String& error_message,
    std::unique_ptr<SourceLocation> location,
    int exception_id) {
  GetParentFrameTaskRunners()
      ->Get(TaskType::kUnspecedTimer)
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&InProcessWorkerMessagingProxy::DispatchErrorEvent,
                          messaging_proxy_weak_ptr_, error_message,
                          WTF::Passed(location->Clone()), exception_id));
}

void InProcessWorkerObjectProxy::DidCreateWorkerGlobalScope(
    WorkerOrWorkletGlobalScope* global_scope) {
  DCHECK(!worker_global_scope_);
  worker_global_scope_ = ToWorkerGlobalScope(global_scope);
  // This timer task should be unthrottled in order to prevent GC timing from
  // being delayed.
  // TODO(nhiroki): Consider making a special task type for GC.
  // (https://crbug.com/712504)
  timer_ = WTF::MakeUnique<TaskRunnerTimer<InProcessWorkerObjectProxy>>(
      TaskRunnerHelper::Get(TaskType::kUnthrottled, global_scope), this,
      &InProcessWorkerObjectProxy::CheckPendingActivity);
}

void InProcessWorkerObjectProxy::DidEvaluateWorkerScript(bool) {
  StartPendingActivityTimer();
}

void InProcessWorkerObjectProxy::WillDestroyWorkerGlobalScope() {
  timer_.reset();
  worker_global_scope_ = nullptr;
}

InProcessWorkerObjectProxy::InProcessWorkerObjectProxy(
    InProcessWorkerMessagingProxy* messaging_proxy_weak_ptr,
    ParentFrameTaskRunners* parent_frame_task_runners)
    : ThreadedObjectProxyBase(parent_frame_task_runners),
      messaging_proxy_weak_ptr_(messaging_proxy_weak_ptr),
      default_interval_in_sec_(kDefaultIntervalInSec),
      next_interval_in_sec_(kDefaultIntervalInSec),
      max_interval_in_sec_(kMaxIntervalInSec) {}

void InProcessWorkerObjectProxy::StartPendingActivityTimer() {
  if (timer_->IsActive()) {
    // Reset the next interval duration to check new activity state timely.
    // For example, a long-running activity can be cancelled by a message
    // event.
    next_interval_in_sec_ = kDefaultIntervalInSec;
    return;
  }
  timer_->StartOneShot(next_interval_in_sec_, BLINK_FROM_HERE);
  next_interval_in_sec_ =
      std::min(next_interval_in_sec_ * 1.5, max_interval_in_sec_);
}

void InProcessWorkerObjectProxy::CheckPendingActivity(TimerBase*) {
  bool has_pending_activity = V8GCController::HasPendingActivity(
      worker_global_scope_->GetThread()->GetIsolate(), worker_global_scope_);
  if (!has_pending_activity) {
    // Report all activities are done.
    GetParentFrameTaskRunners()
        ->Get(TaskType::kUnspecedTimer)
        ->PostTask(BLINK_FROM_HERE,
                   CrossThreadBind(
                       &InProcessWorkerMessagingProxy::PendingActivityFinished,
                       messaging_proxy_weak_ptr_));

    // Don't schedule a timer. It will be started again when a message event
    // is dispatched.
    next_interval_in_sec_ = default_interval_in_sec_;
    return;
  }

  // There is still a pending activity. Check it later.
  StartPendingActivityTimer();
}

CrossThreadWeakPersistent<ThreadedMessagingProxyBase>
InProcessWorkerObjectProxy::MessagingProxyWeakPtr() {
  return messaging_proxy_weak_ptr_;
}

}  // namespace blink

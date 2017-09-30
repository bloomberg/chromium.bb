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
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/MessageEvent.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/DedicatedWorkerMessagingProxy.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"

namespace blink {

std::unique_ptr<InProcessWorkerObjectProxy> InProcessWorkerObjectProxy::Create(
    DedicatedWorkerMessagingProxy* messaging_proxy_weak_ptr,
    ParentFrameTaskRunners* parent_frame_task_runners) {
  DCHECK(messaging_proxy_weak_ptr);
  return WTF::WrapUnique(new InProcessWorkerObjectProxy(
      messaging_proxy_weak_ptr, parent_frame_task_runners));
}

InProcessWorkerObjectProxy::~InProcessWorkerObjectProxy() {}

void InProcessWorkerObjectProxy::PostMessageToWorkerObject(
    RefPtr<SerializedScriptValue> message,
    Vector<MessagePortChannel> channels) {
  GetParentFrameTaskRunners()
      ->Get(TaskType::kPostedMessage)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(
                     &DedicatedWorkerMessagingProxy::PostMessageToWorkerObject,
                     messaging_proxy_weak_ptr_, std::move(message),
                     WTF::Passed(std::move(channels))));
}

void InProcessWorkerObjectProxy::ProcessMessageFromWorkerObject(
    RefPtr<SerializedScriptValue> message,
    Vector<MessagePortChannel> channels,
    WorkerThread* worker_thread) {
  WorkerGlobalScope* global_scope =
      ToWorkerGlobalScope(worker_thread->GlobalScope());
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*global_scope, std::move(channels));
  global_scope->DispatchEvent(MessageEvent::Create(ports, std::move(message)));
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
          CrossThreadBind(&DedicatedWorkerMessagingProxy::DispatchErrorEvent,
                          messaging_proxy_weak_ptr_, error_message,
                          WTF::Passed(location->Clone()), exception_id));
}

void InProcessWorkerObjectProxy::DidCreateWorkerGlobalScope(
    WorkerOrWorkletGlobalScope* global_scope) {
  DCHECK(!worker_global_scope_);
  worker_global_scope_ = ToWorkerGlobalScope(global_scope);
}

void InProcessWorkerObjectProxy::WillDestroyWorkerGlobalScope() {
  worker_global_scope_ = nullptr;
}

InProcessWorkerObjectProxy::InProcessWorkerObjectProxy(
    DedicatedWorkerMessagingProxy* messaging_proxy_weak_ptr,
    ParentFrameTaskRunners* parent_frame_task_runners)
    : ThreadedObjectProxyBase(parent_frame_task_runners),
      messaging_proxy_weak_ptr_(messaging_proxy_weak_ptr) {}

CrossThreadWeakPersistent<ThreadedMessagingProxyBase>
InProcessWorkerObjectProxy::MessagingProxyWeakPtr() {
  return messaging_proxy_weak_ptr_;
}

}  // namespace blink

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

#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "platform/CrossThreadFunctional.h"
#include "public/platform/WebTaskRunner.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<InProcessWorkerObjectProxy> InProcessWorkerObjectProxy::create(InProcessWorkerMessagingProxy* messagingProxy)
{
    DCHECK(messagingProxy);
    return wrapUnique(new InProcessWorkerObjectProxy(messagingProxy));
}

void InProcessWorkerObjectProxy::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message, std::unique_ptr<MessagePortChannelArray> channels)
{
    getParentFrameTaskRunners()->get(TaskType::PostedMessage)->postTask(BLINK_FROM_HERE, crossThreadBind(&InProcessWorkerMessagingProxy::postMessageToWorkerObject, crossThreadUnretained(m_messagingProxy), message, passed(std::move(channels))));
}

void InProcessWorkerObjectProxy::postTaskToMainExecutionContext(std::unique_ptr<ExecutionContextTask> task)
{
    // TODO(hiroshige,yuryu): Make this not use ExecutionContextTask and use
    // getParentFrameTaskRunners() instead.
    getExecutionContext()->postTask(BLINK_FROM_HERE, std::move(task));
}

void InProcessWorkerObjectProxy::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    getParentFrameTaskRunners()->get(TaskType::Internal)->postTask(BLINK_FROM_HERE, crossThreadBind(&InProcessWorkerMessagingProxy::confirmMessageFromWorkerObject, crossThreadUnretained(m_messagingProxy), hasPendingActivity));
}

void InProcessWorkerObjectProxy::reportPendingActivity(bool hasPendingActivity)
{
    getParentFrameTaskRunners()->get(TaskType::Internal)->postTask(BLINK_FROM_HERE, crossThreadBind(&InProcessWorkerMessagingProxy::reportPendingActivity, crossThreadUnretained(m_messagingProxy), hasPendingActivity));
}

void InProcessWorkerObjectProxy::reportException(const String& errorMessage, std::unique_ptr<SourceLocation> location, int exceptionId)
{
    getParentFrameTaskRunners()->get(TaskType::Internal)->postTask(BLINK_FROM_HERE, crossThreadBind(&InProcessWorkerMessagingProxy::reportException, crossThreadUnretained(m_messagingProxy), errorMessage, passed(location->clone()), exceptionId));
}

void InProcessWorkerObjectProxy::reportConsoleMessage(MessageSource source, MessageLevel level, const String& message, SourceLocation* location)
{
    getParentFrameTaskRunners()->get(TaskType::Internal)->postTask(BLINK_FROM_HERE, crossThreadBind(&InProcessWorkerMessagingProxy::reportConsoleMessage, crossThreadUnretained(m_messagingProxy), source, level, message, passed(location->clone())));
}

void InProcessWorkerObjectProxy::postMessageToPageInspector(const String& message)
{
    ExecutionContext* context = getExecutionContext();
    if (context->isDocument()) {
        // TODO(hiroshige): consider using getParentFrameTaskRunners() here
        // too.
        toDocument(context)->postInspectorTask(BLINK_FROM_HERE, createCrossThreadTask(&InProcessWorkerMessagingProxy::postMessageToPageInspector, crossThreadUnretained(m_messagingProxy), message));
    }
}

void InProcessWorkerObjectProxy::workerGlobalScopeClosed()
{
    getParentFrameTaskRunners()->get(TaskType::Internal)->postTask(BLINK_FROM_HERE, crossThreadBind(&InProcessWorkerMessagingProxy::terminateWorkerGlobalScope, crossThreadUnretained(m_messagingProxy)));
}

void InProcessWorkerObjectProxy::workerThreadTerminated()
{
    // This will terminate the MessagingProxy.
    getParentFrameTaskRunners()->get(TaskType::Internal)->postTask(BLINK_FROM_HERE, crossThreadBind(&InProcessWorkerMessagingProxy::workerThreadTerminated, crossThreadUnretained(m_messagingProxy)));
}

InProcessWorkerObjectProxy::InProcessWorkerObjectProxy(InProcessWorkerMessagingProxy* messagingProxy)
    : m_messagingProxy(messagingProxy)
{
}

ParentFrameTaskRunners* InProcessWorkerObjectProxy::getParentFrameTaskRunners()
{
    DCHECK(m_messagingProxy);
    return m_messagingProxy->getParentFrameTaskRunners();
}

ExecutionContext* InProcessWorkerObjectProxy::getExecutionContext()
{
    DCHECK(m_messagingProxy);
    return m_messagingProxy->getExecutionContext();
}

} // namespace blink

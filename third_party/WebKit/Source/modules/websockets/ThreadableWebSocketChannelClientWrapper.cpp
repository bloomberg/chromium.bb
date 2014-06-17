/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "config.h"
#include "modules/websockets/ThreadableWebSocketChannelClientWrapper.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/ExecutionContext.h"
#include "platform/CrossThreadCopier.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

ThreadableWebSocketChannelClientWrapper::ThreadableWebSocketChannelClientWrapper(WebSocketChannelClient* client)
    : m_client(client)
    , m_suspended(false)
{
}

ThreadableWebSocketChannelClientWrapper::~ThreadableWebSocketChannelClientWrapper()
{
}

PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> ThreadableWebSocketChannelClientWrapper::create(WebSocketChannelClient* client)
{
    return adoptRefWillBeNoop(new ThreadableWebSocketChannelClientWrapper(client));
}

void ThreadableWebSocketChannelClientWrapper::clearClient()
{
    m_client = 0;
}

void ThreadableWebSocketChannelClientWrapper::didConnect(const String& subprotocol, const String& extensions)
{
    m_pendingTasks.append(createCallbackTask(&didConnectCallback, this, subprotocol, extensions));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didReceiveMessage(const String& message)
{
    m_pendingTasks.append(createCallbackTask(&didReceiveMessageCallback, this, message));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
    m_pendingTasks.append(createCallbackTask(&didReceiveBinaryDataCallback, this, binaryData));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didConsumeBufferedAmount(unsigned long consumed)
{
    m_pendingTasks.append(createCallbackTask(&didConsumeBufferedAmountCallback, this, consumed));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didStartClosingHandshake()
{
    m_pendingTasks.append(createCallbackTask(&didStartClosingHandshakeCallback, this));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didClose(WebSocketChannelClient::ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    m_pendingTasks.append(createCallbackTask(&didCloseCallback, this, closingHandshakeCompletion, code, reason));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::didReceiveMessageError()
{
    m_pendingTasks.append(createCallbackTask(&didReceiveMessageErrorCallback, this));
    if (!m_suspended)
        processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::suspend()
{
    m_suspended = true;
}

void ThreadableWebSocketChannelClientWrapper::resume()
{
    m_suspended = false;
    processPendingTasks();
}

void ThreadableWebSocketChannelClientWrapper::processPendingTasks()
{
    if (m_suspended)
        return;
    Vector<OwnPtr<ExecutionContextTask> > tasks;
    tasks.swap(m_pendingTasks);
    for (Vector<OwnPtr<ExecutionContextTask> >::const_iterator iter = tasks.begin(); iter != tasks.end(); ++iter)
        (*iter)->performTask(0);
}

void ThreadableWebSocketChannelClientWrapper::didConnectCallback(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> wrapper, const String& subprotocol, const String& extensions)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didConnect(subprotocol, extensions);
}

void ThreadableWebSocketChannelClientWrapper::didReceiveMessageCallback(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> wrapper, const String& message)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didReceiveMessage(message);
}

void ThreadableWebSocketChannelClientWrapper::didReceiveBinaryDataCallback(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> wrapper, PassOwnPtr<Vector<char> > binaryData)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didReceiveBinaryData(binaryData);
}

void ThreadableWebSocketChannelClientWrapper::didConsumeBufferedAmountCallback(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> wrapper, unsigned long consumed)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didConsumeBufferedAmount(consumed);
}

void ThreadableWebSocketChannelClientWrapper::didStartClosingHandshakeCallback(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> wrapper)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didStartClosingHandshake();
}

void ThreadableWebSocketChannelClientWrapper::didCloseCallback(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> wrapper, WebSocketChannelClient::ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didClose(closingHandshakeCompletion, code, reason);
}

void ThreadableWebSocketChannelClientWrapper::didReceiveMessageErrorCallback(ExecutionContext* context, PassRefPtrWillBeRawPtr<ThreadableWebSocketChannelClientWrapper> wrapper)
{
    ASSERT_UNUSED(context, !context);
    if (wrapper->m_client)
        wrapper->m_client->didReceiveMessageError();
}

} // namespace WebCore

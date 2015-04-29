/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "core/loader/WorkerLoaderClientBridgeSyncHelper.h"

#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceResponse.h"
#include "public/platform/WebWaitableEvent.h"
#include "wtf/MainThread.h"
#include "wtf/OwnPtr.h"

namespace blink {

PassOwnPtr<WorkerLoaderClientBridgeSyncHelper> WorkerLoaderClientBridgeSyncHelper::create(ThreadableLoaderClient& client, PassOwnPtr<WebWaitableEvent> event)
{
    return adoptPtr(new WorkerLoaderClientBridgeSyncHelper(client, event));
}

WorkerLoaderClientBridgeSyncHelper::~WorkerLoaderClientBridgeSyncHelper()
{
    MutexLocker lock(m_lock);
    ASSERT(isMainThread());
    for (size_t i = 0; i < m_receivedData.size(); ++i)
        delete m_receivedData[i];
}

void WorkerLoaderClientBridgeSyncHelper::run()
{
    // This must be called only after m_event is signalled.
    MutexLocker lock(m_lock);
    ASSERT(m_done);
    for (size_t i = 0; i < m_clientTasks.size(); ++i)
        (*m_clientTasks[i])();
}

void WorkerLoaderClientBridgeSyncHelper::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    MutexLocker lock(m_lock);
    ASSERT(isMainThread());
    m_clientTasks.append(threadSafeBind(&ThreadableLoaderClient::didSendData, AllowCrossThreadAccess(&m_client), bytesSent, totalBytesToBeSent));
}

static void didReceiveResponseAdapter(ThreadableLoaderClient* client, unsigned long identifier, PassOwnPtr<CrossThreadResourceResponseData> responseData, PassOwnPtr<WebDataConsumerHandle> handle)
{
    OwnPtr<ResourceResponse> response(ResourceResponse::adopt(responseData));
    client->didReceiveResponse(identifier, *response, handle);
}

void WorkerLoaderClientBridgeSyncHelper::didReceiveResponse(unsigned long identifier, const ResourceResponse& response, PassOwnPtr<WebDataConsumerHandle> handle)
{
    MutexLocker lock(m_lock);
    ASSERT(isMainThread());
    m_clientTasks.append(threadSafeBind(&didReceiveResponseAdapter, AllowCrossThreadAccess(&m_client), identifier, response, handle));
}

void WorkerLoaderClientBridgeSyncHelper::didReceiveData(const char* data, unsigned dataLength)
{
    MutexLocker lock(m_lock);
    ASSERT(isMainThread());
    Vector<char>* buffer = new Vector<char>(dataLength);
    memcpy(buffer->data(), data, dataLength);
    m_receivedData.append(buffer);
    m_clientTasks.append(threadSafeBind(&ThreadableLoaderClient::didReceiveData, AllowCrossThreadAccess(&m_client), AllowCrossThreadAccess(static_cast<const char*>(buffer->data())), dataLength));
}

void WorkerLoaderClientBridgeSyncHelper::didDownloadData(int dataLength)
{
    MutexLocker lock(m_lock);
    ASSERT(isMainThread());
    m_clientTasks.append(threadSafeBind(&ThreadableLoaderClient::didDownloadData, AllowCrossThreadAccess(&m_client), dataLength));
}

void WorkerLoaderClientBridgeSyncHelper::didReceiveCachedMetadata(const char* data, int dataLength)
{
    MutexLocker lock(m_lock);
    ASSERT(isMainThread());
    Vector<char>* buffer = new Vector<char>(dataLength);
    memcpy(buffer->data(), data, dataLength);
    m_receivedData.append(buffer);
    m_clientTasks.append(threadSafeBind(&ThreadableLoaderClient::didReceiveCachedMetadata, AllowCrossThreadAccess(&m_client), AllowCrossThreadAccess(static_cast<const char*>(buffer->data())), dataLength));
}

void WorkerLoaderClientBridgeSyncHelper::didFinishLoading(unsigned long identifier, double finishTime)
{
    MutexLocker lock(m_lock);
    ASSERT(isMainThread());
    m_clientTasks.append(threadSafeBind(&ThreadableLoaderClient::didFinishLoading, AllowCrossThreadAccess(&m_client), identifier, finishTime));
    m_done = true;
    m_event->signal();
}

void WorkerLoaderClientBridgeSyncHelper::didFail(const ResourceError& error)
{
    MutexLocker lock(m_lock);
    ASSERT(isMainThread());
    m_clientTasks.append(threadSafeBind(&ThreadableLoaderClient::didFail, AllowCrossThreadAccess(&m_client), error));
    m_done = true;
    m_event->signal();
}

void WorkerLoaderClientBridgeSyncHelper::didFailAccessControlCheck(const ResourceError& error)
{
    MutexLocker lock(m_lock);
    ASSERT(isMainThread());
    m_clientTasks.append(threadSafeBind(&ThreadableLoaderClient::didFailAccessControlCheck, AllowCrossThreadAccess(&m_client), error));
    m_done = true;
    m_event->signal();
}

void WorkerLoaderClientBridgeSyncHelper::didFailRedirectCheck()
{
    MutexLocker lock(m_lock);
    ASSERT(isMainThread());
    m_clientTasks.append(threadSafeBind(&ThreadableLoaderClient::didFailRedirectCheck, AllowCrossThreadAccess(&m_client)));
    m_done = true;
    m_event->signal();
}

WorkerLoaderClientBridgeSyncHelper::WorkerLoaderClientBridgeSyncHelper(ThreadableLoaderClient& client, PassOwnPtr<WebWaitableEvent> event)
    : m_done(false)
    , m_client(client)
    , m_event(event)
{
    ASSERT(m_event);
}

} // namespace blink

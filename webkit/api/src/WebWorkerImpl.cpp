/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebWorkerImpl.h"

#include "DedicatedWorkerContext.h"
#include "DedicatedWorkerThread.h"
#include "GenericWorkerTask.h"
#include "KURL.h"
#include "MessageEvent.h"
#include "MessagePort.h"
#include "MessagePortChannel.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include "SubstituteData.h"
#include <wtf/MainThread.h>
#include <wtf/Threading.h>

#include "EmptyWebFrameClientImpl.h"
#include "PlatformMessagePortChannel.h"
#include "WebDataSourceImpl.h"
#include "WebFrameClient.h"
#include "WebMessagePortChannel.h"
#include "WebScreenInfo.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebView.h"
#include "WebWorkerClient.h"
// FIXME: webframe should eventually move to api/src too.
#include "webkit/glue/webframe_impl.h"

using namespace WebCore;

namespace WebKit {

#if ENABLE(WORKERS)

// Dummy WebViewDelegate - we only need it in Worker process to load a
// 'shadow page' which will initialize WebCore loader.
class WorkerWebFrameClient : public WebKit::EmptyWebFrameClient {
public:
    // Tell the loader to load the data into the 'shadow page' synchronously,
    // so we can grab the resulting Document right after load.
    virtual void didCreateDataSource(WebFrame* frame, WebDataSource* ds)
    {
        static_cast<WebDataSourceImpl*>(ds)->setDeferMainResourceDataLoad(false);
    }

    // Lazy allocate and leak this instance.
    static WorkerWebFrameClient* sharedInstance()
    {
        static WorkerWebFrameClient client;
        return &client;
    }

private:
    WorkerWebFrameClient()
    {
    }
};

WebWorker* WebWorker::create(WebWorkerClient* client)
{
    return new WebWorkerImpl(client);
}

// This function is called on the main thread to force to initialize some static
// values used in WebKit before any worker thread is started. This is because in
// our worker processs, we do not run any WebKit code in main thread and thus
// when multiple workers try to start at the same time, we might hit crash due
// to contention for initializing static values.
void initializeWebKitStaticValues()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        // Note that we have to pass a URL with valid protocol in order to follow
        // the path to do static value initializations.
        RefPtr<SecurityOrigin> origin =
            SecurityOrigin::create(KURL(ParsedURLString, "http://localhost"));
        origin.release();
    }
}

WebWorkerImpl::WebWorkerImpl(WebWorkerClient* client)
    : m_client(client)
    , m_webView(0)
    , m_askedToTerminate(false)
{
    initializeWebKitStaticValues();
}

WebWorkerImpl::~WebWorkerImpl()
{
    m_webView->close();
}

void WebWorkerImpl::postMessageToWorkerContextTask(WebCore::ScriptExecutionContext* context,
                                                   WebWorkerImpl* thisPtr,
                                                   const String& message,
                                                   PassOwnPtr<MessagePortChannelArray> channels)
{
    ASSERT(context->isWorkerContext());
    DedicatedWorkerContext* workerContext =
        static_cast<DedicatedWorkerContext*>(context);

    OwnPtr<MessagePortArray> ports =
        MessagePort::entanglePorts(*context, channels.release());
    RefPtr<SerializedScriptValue> serializedMessage =
        SerializedScriptValue::create(message);
    workerContext->dispatchEvent(MessageEvent::create(
        ports.release(), serializedMessage.release()));
    thisPtr->confirmMessageFromWorkerObject(workerContext->hasPendingActivity());
}

// WebWorker -------------------------------------------------------------------

void WebWorkerImpl::startWorkerContext(const WebURL& scriptUrl,
                                       const WebString& userAgent,
                                       const WebString& sourceCode)
{
    // Create 'shadow page'. This page is never displayed, it is used to proxy the
    // loading requests from the worker context to the rest of WebKit and Chromium
    // infrastructure.
    ASSERT(!m_webView);
    m_webView = WebView::create(0);
    m_webView->initializeMainFrame(WorkerWebFrameClient::sharedInstance());

    WebFrameImpl* webFrame = static_cast<WebFrameImpl*>(m_webView->mainFrame());

    // Construct substitute data source for the 'shadow page'. We only need it
    // to have same origin as the worker so the loading checks work correctly.
    CString content("");
    int len = static_cast<int>(content.length());
    RefPtr<SharedBuffer> buf(SharedBuffer::create(content.data(), len));
    SubstituteData substData(buf, String("text/html"), String("UTF-8"), KURL());
    ResourceRequest request(scriptUrl, CString());
    webFrame->frame()->loader()->load(request, substData, false);

    // This document will be used as 'loading context' for the worker.
    m_loadingDocument = webFrame->frame()->document();

    m_workerThread = DedicatedWorkerThread::create(scriptUrl, userAgent,
                                                   sourceCode, *this, *this);
    // Worker initialization means a pending activity.
    reportPendingActivity(true);
    m_workerThread->start();
}

void WebWorkerImpl::terminateWorkerContext()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;
    if (m_workerThread)
        m_workerThread->stop();
}

void WebWorkerImpl::postMessageToWorkerContext(const WebString& message,
                                               const WebMessagePortChannelArray& webChannels)
{
    OwnPtr<MessagePortChannelArray> channels;
    if (webChannels.size()) {
        channels = new MessagePortChannelArray(webChannels.size());
        for (size_t i = 0; i < webChannels.size(); ++i) {
            RefPtr<PlatformMessagePortChannel> platform_channel =
                PlatformMessagePortChannel::create(webChannels[i]);
            webChannels[i]->setClient(platform_channel.get());
            (*channels)[i] = MessagePortChannel::create(platform_channel);
        }
    }

    m_workerThread->runLoop().postTask(
        createCallbackTask(&postMessageToWorkerContextTask,
                           this, String(message), channels.release()));
}

void WebWorkerImpl::workerObjectDestroyed()
{
    // Worker object in the renderer was destroyed, perhaps a result of GC.
    // For us, it's a signal to start terminating the WorkerContext too.
    // FIXME: when 'kill a worker' html5 spec algorithm is implemented, it
    // should be used here instead of 'terminate a worker'.
    terminateWorkerContext();
}

void WebWorkerImpl::clientDestroyed()
{
    m_client = 0;
}

void WebWorkerImpl::dispatchTaskToMainThread(PassRefPtr<ScriptExecutionContext::Task> task)
{
    return callOnMainThread(invokeTaskMethod, task.releaseRef());
}

void WebWorkerImpl::invokeTaskMethod(void* param)
{
    ScriptExecutionContext::Task* task =
        static_cast<ScriptExecutionContext::Task*>(param);
    task->performTask(0);
    task->deref();
}

// WorkerObjectProxy -----------------------------------------------------------

void WebWorkerImpl::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message,
                                              PassOwnPtr<MessagePortChannelArray> channels)
{
    dispatchTaskToMainThread(createCallbackTask(&postMessageTask, this,
                                                message->toString(), channels));
}

void WebWorkerImpl::postMessageTask(ScriptExecutionContext* context,
                                    WebWorkerImpl* thisPtr,
                                    String message,
                                    PassOwnPtr<MessagePortChannelArray> channels)
{
    if (!thisPtr->m_client)
        return;

    WebMessagePortChannelArray webChannels(channels.get() ? channels->size() : 0);
    for (size_t i = 0; i < webChannels.size(); ++i) {
        webChannels[i] = (*channels)[i]->channel()->webChannelRelease();
        webChannels[i]->setClient(0);
    }

    thisPtr->m_client->postMessageToWorkerObject(message, webChannels);
}

void WebWorkerImpl::postExceptionToWorkerObject(const String& errorMessage,
                                                int lineNumber,
                                                const String& sourceURL)
{
    dispatchTaskToMainThread(createCallbackTask(&postExceptionTask, this,
                                                errorMessage, lineNumber,
                                                sourceURL));
}

void WebWorkerImpl::postExceptionTask(ScriptExecutionContext* context,
                                      WebWorkerImpl* thisPtr,
                                      const String& errorMessage,
                                      int lineNumber, const String& sourceURL)
{
    if (!thisPtr->m_client)
        return;

    thisPtr->m_client->postExceptionToWorkerObject(errorMessage,
                                                   lineNumber,
                                                   sourceURL);
}

void WebWorkerImpl::postConsoleMessageToWorkerObject(MessageDestination destination,
                                                     MessageSource source,
                                                     MessageType type,
                                                     MessageLevel level,
                                                     const String& message,
                                                     int lineNumber,
                                                     const String& sourceURL)
{
    dispatchTaskToMainThread(createCallbackTask(&postConsoleMessageTask, this,
                                                static_cast<int>(destination),
                                                static_cast<int>(source),
                                                static_cast<int>(type),
                                                static_cast<int>(level),
                                                message, lineNumber, sourceURL));
}

void WebWorkerImpl::postConsoleMessageTask(ScriptExecutionContext* context,
                                           WebWorkerImpl* thisPtr,
                                           int destination, int source,
                                           int type, int level,
                                           const String& message,
                                           int lineNumber,
                                           const String& sourceURL)
{
    if (!thisPtr->m_client)
        return;
    thisPtr->m_client->postConsoleMessageToWorkerObject(destination, source,
                                                        type, level, message,
                                                        lineNumber, sourceURL);
}

void WebWorkerImpl::confirmMessageFromWorkerObject(bool hasPendingActivity)
{
    dispatchTaskToMainThread(createCallbackTask(&confirmMessageTask, this,
                                                hasPendingActivity));
}

void WebWorkerImpl::confirmMessageTask(ScriptExecutionContext* context,
                                       WebWorkerImpl* thisPtr,
                                       bool hasPendingActivity)
{
    if (!thisPtr->m_client)
        return;
    thisPtr->m_client->confirmMessageFromWorkerObject(hasPendingActivity);
}

void WebWorkerImpl::reportPendingActivity(bool hasPendingActivity)
{
    dispatchTaskToMainThread(createCallbackTask(&reportPendingActivityTask,
                                                this, hasPendingActivity));
}

void WebWorkerImpl::reportPendingActivityTask(ScriptExecutionContext* context,
                                              WebWorkerImpl* thisPtr,
                                              bool hasPendingActivity)
{
    if (!thisPtr->m_client)
        return;
    thisPtr->m_client->reportPendingActivity(hasPendingActivity);
}

void WebWorkerImpl::workerContextDestroyed()
{
    dispatchTaskToMainThread(createCallbackTask(&workerContextDestroyedTask,
                                                this));
}

// WorkerLoaderProxy -----------------------------------------------------------

void WebWorkerImpl::postTaskToLoader(PassRefPtr<ScriptExecutionContext::Task> task)
{
    ASSERT(m_loadingDocument->isDocument());
    m_loadingDocument->postTask(task);
}

void WebWorkerImpl::postTaskForModeToWorkerContext(
    PassRefPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    m_workerThread->runLoop().postTaskForMode(task, mode);
}

void WebWorkerImpl::workerContextDestroyedTask(ScriptExecutionContext* context,
                                               WebWorkerImpl* thisPtr)
{
    if (thisPtr->m_client)
        thisPtr->m_client->workerContextDestroyed();
    // The lifetime of this proxy is controlled by the worker context.
    delete thisPtr;
}


#else

WebWorker* WebWorker::create(WebWorkerClient* client)
{
    return 0;
}

#endif // ENABLE(WORKERS)

} // namespace WebKit

/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "modules/webdatabase/chromium/DatabaseObserver.h"

#include "WebCommonWorkerClient.h"
#include "WebDatabase.h"
#include "WebDatabaseObserver.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebPermissionClient.h"
#include "WebSecurityOrigin.h"
#include "WebViewImpl.h"
#include "WebWorkerBase.h"
#include "WorkerAllowMainThreadBridgeBase.h"
#include "bindings/v8/WorkerScriptController.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/platform/CrossThreadCopier.h"
#include "core/workers/WorkerContext.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/webdatabase/DatabaseBackendBase.h"
#include "modules/webdatabase/DatabaseBackendContext.h"

using namespace WebKit;

namespace {

static const char allowDatabaseMode[] = "allowDatabaseMode";

// This class is used to route the result of the WebWorkerBase::allowDatabase
// call back to the worker context.
class AllowDatabaseMainThreadBridge : public WorkerAllowMainThreadBridgeBase {
public:
    static PassRefPtr<AllowDatabaseMainThreadBridge> create(WebCore::WorkerContext* workerContext, WebWorkerBase* webWorkerBase, const String& mode, WebFrame* frame, const String& name, const String& displayName, unsigned long estimatedSize)
    {
        return adoptRef(new AllowDatabaseMainThreadBridge(workerContext, webWorkerBase, mode, frame, name, displayName, estimatedSize));
    }

private:
    class AllowDatabaseParams : public AllowParams
    {
    public:
        AllowDatabaseParams(const String& mode, WebFrame* frame, const String& name, const String& displayName, unsigned long estimatedSize)
            : AllowParams(mode)
            , m_frame(WebCore::AllowCrossThreadAccess(frame))
            , m_name(name.isolatedCopy())
            , m_displayName(displayName.isolatedCopy())
            , m_estimatedSize(estimatedSize)
        {
        }
        WebCore::AllowCrossThreadAccessWrapper<WebFrame> m_frame;
        String m_name;
        String m_displayName;
        unsigned long m_estimatedSize;
    };

    AllowDatabaseMainThreadBridge(WebCore::WorkerContext* workerContext, WebWorkerBase* webWorkerBase, const String& mode, WebFrame* frame, const String& name, const String& displayName, unsigned long estimatedSize)
        : WorkerAllowMainThreadBridgeBase(workerContext, webWorkerBase)
    {
        postTaskToMainThread(
            adoptPtr(new AllowDatabaseParams(mode, frame, name, displayName, estimatedSize)));
    }

    virtual bool allowOnMainThread(WebCommonWorkerClient* commonClient, AllowParams* params)
    {
        ASSERT(isMainThread());
        AllowDatabaseParams* allowDBParams = static_cast<AllowDatabaseParams*>(params);
        return commonClient->allowDatabase(
            allowDBParams->m_frame.value(), allowDBParams->m_name, allowDBParams->m_displayName, allowDBParams->m_estimatedSize);
    }
};

bool allowDatabaseForWorker(WebFrame* frame, const WebString& name, const WebString& displayName, unsigned long estimatedSize)
{
    WebCore::WorkerScriptController* controller = WebCore::WorkerScriptController::controllerForContext();
    WebCore::WorkerContext* workerContext = controller->workerContext();
    WebCore::WorkerThread* workerThread = workerContext->thread();
    WebCore::WorkerRunLoop& runLoop = workerThread->runLoop();
    WebCore::WorkerLoaderProxy* workerLoaderProxy = &workerThread->workerLoaderProxy();

    // Create a unique mode just for this synchronous call.
    String mode = allowDatabaseMode;
    mode.append(String::number(runLoop.createUniqueId()));

    RefPtr<AllowDatabaseMainThreadBridge> bridge = AllowDatabaseMainThreadBridge::create(workerContext, workerLoaderProxy->toWebWorkerBase(), mode, frame, name, displayName, estimatedSize);

    // Either the bridge returns, or the queue gets terminated.
    if (runLoop.runInMode(workerContext, mode) == MessageQueueTerminated) {
        bridge->cancel();
        return false;
    }

    return bridge->result();
}

}

namespace WebCore {

bool DatabaseObserver::canEstablishDatabase(ScriptExecutionContext* scriptExecutionContext, const String& name, const String& displayName, unsigned long estimatedSize)
{
    ASSERT(scriptExecutionContext->isContextThread());
    ASSERT(scriptExecutionContext->isDocument() || scriptExecutionContext->isWorkerContext());
    if (scriptExecutionContext->isDocument()) {
        Document* document = static_cast<Document*>(scriptExecutionContext);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        if (!webFrame)
            return false;
        WebViewImpl* webView = webFrame->viewImpl();
        if (!webView)
            return false;
        if (webView->permissionClient())
            return webView->permissionClient()->allowDatabase(webFrame, name, displayName, estimatedSize);
    } else {
        WorkerContext* workerContext = static_cast<WorkerContext*>(scriptExecutionContext);
        WebWorkerBase* webWorker = static_cast<WebWorkerBase*>(workerContext->thread()->workerLoaderProxy().toWebWorkerBase());
        WebView* view = webWorker->view();
        if (!view)
            return false;
        return allowDatabaseForWorker(view->mainFrame(), name, displayName, estimatedSize);
    }

    return true;
}

void DatabaseObserver::databaseOpened(DatabaseBackendBase* database)
{
    ASSERT(database->databaseContext()->scriptExecutionContext()->isContextThread());
    WebDatabase::observer()->databaseOpened(WebDatabase(database));
}

void DatabaseObserver::databaseModified(DatabaseBackendBase* database)
{
    ASSERT(database->databaseContext()->scriptExecutionContext()->isContextThread());
    WebDatabase::observer()->databaseModified(WebDatabase(database));
}

void DatabaseObserver::databaseClosed(DatabaseBackendBase* database)
{
    ASSERT(database->databaseContext()->scriptExecutionContext()->isContextThread());
    WebDatabase::observer()->databaseClosed(WebDatabase(database));
}

void DatabaseObserver::reportOpenDatabaseResult(DatabaseBackendBase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportOpenDatabaseResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportChangeVersionResult(DatabaseBackendBase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportChangeVersionResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportStartTransactionResult(DatabaseBackendBase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportStartTransactionResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportCommitTransactionResult(DatabaseBackendBase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportCommitTransactionResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportExecuteStatementResult(DatabaseBackendBase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportExecuteStatementResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportVacuumDatabaseResult(DatabaseBackendBase* database, int sqliteErrorCode)
{
    WebDatabase::observer()->reportVacuumDatabaseResult(WebDatabase(database), sqliteErrorCode);
}

} // namespace WebCore

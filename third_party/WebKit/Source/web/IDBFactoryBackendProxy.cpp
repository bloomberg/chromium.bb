/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBFactoryBackendProxy.h"

#include "public/platform/WebIDBDatabase.h"
#include "public/platform/WebIDBDatabaseError.h"
#include "public/platform/WebIDBFactory.h"
#include "public/platform/WebVector.h"
#include "IDBDatabaseBackendProxy.h"
#include "WebFrameImpl.h"
#include "WebIDBCallbacksImpl.h"
#include "WebIDBDatabaseCallbacksImpl.h"
#include "WebKit.h"
#include "WebPermissionClient.h"
#include "WebSecurityOrigin.h"
#include "WebViewImpl.h"
#include "WebWorkerBase.h"
#include "WebWorkerClientImpl.h"
#include "WorkerAllowMainThreadBridgeBase.h"
#include "bindings/v8/WorkerScriptController.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/DOMError.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "weborigin/SecurityOrigin.h"


using namespace WebCore;

namespace WebKit {

PassRefPtr<IDBFactoryBackendInterface> IDBFactoryBackendProxy::create()
{
    return adoptRef(new IDBFactoryBackendProxy());
}

IDBFactoryBackendProxy::IDBFactoryBackendProxy()
{
    m_webIDBFactory = WebKit::Platform::current()->idbFactory();
}

IDBFactoryBackendProxy::~IDBFactoryBackendProxy()
{
}

static const char allowIndexedDBMode[] = "allowIndexedDBMode";

class AllowIndexedDBMainThreadBridge : public WorkerAllowMainThreadBridgeBase {
public:
    static PassRefPtr<AllowIndexedDBMainThreadBridge> create(WorkerGlobalScope* workerGlobalScope, WebWorkerBase* webWorkerBase, const String& mode, const String& name)
    {
        return adoptRef(new AllowIndexedDBMainThreadBridge(workerGlobalScope, webWorkerBase, mode, name));
    }

private:
    class AllowIDBParams : public AllowParams
    {
    public:
        AllowIDBParams(const String& mode, const String& name)
            : AllowParams(mode)
            , m_name(name.isolatedCopy())
        {
        }
        String m_name;
    };

    AllowIndexedDBMainThreadBridge(WorkerGlobalScope* workerGlobalScope, WebWorkerBase* webWorkerBase, const String& mode, const String& name)
        : WorkerAllowMainThreadBridgeBase(workerGlobalScope, webWorkerBase)
    {
        postTaskToMainThread(adoptPtr(new AllowIDBParams(mode, name)));
    }

    virtual bool allowOnMainThread(WebCommonWorkerClient* commonClient, AllowParams* params)
    {
        ASSERT(isMainThread());
        AllowIDBParams* allowIDBParams = static_cast<AllowIDBParams*>(params);
        return commonClient->allowIndexedDB(allowIDBParams->m_name);
    }
};

bool IDBFactoryBackendProxy::allowIndexedDB(ScriptExecutionContext* context, const String& name, const WebSecurityOrigin& origin, PassRefPtr<IDBCallbacks> callbacks)
{
    bool allowed;
    ASSERT_WITH_SECURITY_IMPLICATION(context->isDocument() || context->isWorkerGlobalScope());
    if (context->isDocument()) {
        Document* document = toDocument(context);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        WebViewImpl* webView = webFrame->viewImpl();
        // FIXME: webView->permissionClient() returns 0 in test_shell and content_shell http://crbug.com/137269
        allowed = !webView->permissionClient() || webView->permissionClient()->allowIndexedDB(webFrame, name, origin);
    } else {
        WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
        WebWorkerBase* webWorkerBase = static_cast<WebWorkerBase*>(workerGlobalScope->thread()->workerLoaderProxy().toWebWorkerBase());
        WorkerRunLoop& runLoop = workerGlobalScope->thread()->runLoop();

        String mode = allowIndexedDBMode;
        mode.append(String::number(runLoop.createUniqueId()));
        RefPtr<AllowIndexedDBMainThreadBridge> bridge = AllowIndexedDBMainThreadBridge::create(workerGlobalScope, webWorkerBase, mode, name);

        // Either the bridge returns, or the queue gets terminated.
        if (runLoop.runInMode(workerGlobalScope, mode) == MessageQueueTerminated) {
            bridge->cancel();
            return false;
        }
        allowed = bridge->result();
    }

    if (!allowed)
        callbacks->onError(WebIDBDatabaseError(UnknownError, "The user denied permission to access the database."));

    return allowed;
}

void IDBFactoryBackendProxy::getDatabaseNames(PassRefPtr<IDBCallbacks> prpCallbacks, const String& databaseIdentifier, ScriptExecutionContext* context)
{
    RefPtr<IDBCallbacks> callbacks(prpCallbacks);
    WebSecurityOrigin origin(context->securityOrigin());
    if (!allowIndexedDB(context, "Database Listing", origin, callbacks))
        return;

    m_webIDBFactory->getDatabaseNames(new WebIDBCallbacksImpl(callbacks), databaseIdentifier);
}

void IDBFactoryBackendProxy::open(const String& name, int64_t version, int64_t transactionId, PassRefPtr<IDBCallbacks> prpCallbacks, PassRefPtr<IDBDatabaseCallbacks> prpDatabaseCallbacks, const String& databaseIdentifier, ScriptExecutionContext* context)
{
    RefPtr<IDBCallbacks> callbacks(prpCallbacks);
    RefPtr<IDBDatabaseCallbacks> databaseCallbacks(prpDatabaseCallbacks);
    WebSecurityOrigin origin(context->securityOrigin());
    if (!allowIndexedDB(context, name, origin, callbacks))
        return;

    m_webIDBFactory->open(name, version, transactionId, new WebIDBCallbacksImpl(callbacks), new WebIDBDatabaseCallbacksImpl(databaseCallbacks), databaseIdentifier);
}

void IDBFactoryBackendProxy::deleteDatabase(const String& name, PassRefPtr<IDBCallbacks> prpCallbacks, const String& databaseIdentifier, ScriptExecutionContext* context)
{
    RefPtr<IDBCallbacks> callbacks(prpCallbacks);
    WebSecurityOrigin origin(context->securityOrigin());
    if (!allowIndexedDB(context, name, origin, callbacks))
        return;

    m_webIDBFactory->deleteDatabase(name, new WebIDBCallbacksImpl(callbacks), databaseIdentifier);
}

} // namespace WebKit

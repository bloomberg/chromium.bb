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

#include "config.h"
#include "WorkerFileSystemClient.h"

#include "WebWorkerBase.h"
#include "WorkerAllowMainThreadBridgeBase.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/platform/AsyncFileSystemCallbacks.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFileError.h"
#include "public/platform/WebFileSystem.h"
#include "public/platform/WebFileSystemType.h"
#include "wtf/MainThread.h"
#include "wtf/Threading.h"
#include "wtf/text/WTFString.h"

using namespace WebCore;

namespace WebKit {

namespace {

// This class is used to route the result of the WebWorkerBase::allowFileSystem
// call back to the worker context.
class AllowFileSystemMainThreadBridge : public WorkerAllowMainThreadBridgeBase {
public:
    static PassRefPtr<AllowFileSystemMainThreadBridge> create(WebCore::WorkerGlobalScope* workerGlobalScope, WebWorkerBase* webWorkerBase, const String& mode)
    {
        return adoptRef(new AllowFileSystemMainThreadBridge(workerGlobalScope, webWorkerBase, mode));
    }

private:
    AllowFileSystemMainThreadBridge(WebCore::WorkerGlobalScope* workerGlobalScope, WebWorkerBase* webWorkerBase, const String& mode)
        : WorkerAllowMainThreadBridgeBase(workerGlobalScope, webWorkerBase)
    {
        postTaskToMainThread(adoptPtr(new AllowParams(mode)));
    }

    virtual bool allowOnMainThread(WebCommonWorkerClient* commonClient, AllowParams*)
    {
        ASSERT(isMainThread());
        return commonClient->allowFileSystem();
    }
};

} // namespace

PassOwnPtr<FileSystemClient> WorkerFileSystemClient::create()
{
    return adoptPtr(static_cast<FileSystemClient*>(new WorkerFileSystemClient()));
}

WorkerFileSystemClient::~WorkerFileSystemClient()
{
}

bool WorkerFileSystemClient::allowFileSystem(ScriptExecutionContext* context)
{
    WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
    WebCore::WorkerThread* workerThread = workerGlobalScope->thread();
    WorkerRunLoop& runLoop = workerThread->runLoop();
    WebCore::WorkerLoaderProxy* workerLoaderProxy = &workerThread->workerLoaderProxy();

    String mode = "allowFileSystemMode";
    mode.append(String::number(runLoop.createUniqueId()));

    RefPtr<AllowFileSystemMainThreadBridge> bridge = AllowFileSystemMainThreadBridge::create(workerGlobalScope, workerLoaderProxy->toWebWorkerBase(), mode);

    // Either the bridge returns, or the queue gets terminated.
    // FIXME: This synchoronous execution should be replaced with better model.
    if (runLoop.runInMode(workerGlobalScope, mode) == MessageQueueTerminated) {
        bridge->cancel();
        return false;
    }

    return bridge->result();
}

WorkerFileSystemClient::WorkerFileSystemClient()
{
}

} // namespace WebKit

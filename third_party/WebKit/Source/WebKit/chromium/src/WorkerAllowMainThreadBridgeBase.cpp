/*
 * Copyright (C) 2010, 2013 Google Inc. All rights reserved.
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
#include "WorkerAllowMainThreadBridgeBase.h"

#include "wtf/MainThread.h"

namespace {

// Observes the worker context and notifies the bridge. This is kept
// separate from the bridge so that it's destroyed correctly by ~Observer.
class WorkerContextObserver : public WebCore::WorkerContext::Observer {
public:
    static PassOwnPtr<WorkerContextObserver> create(WebCore::WorkerContext* context, PassRefPtr<WebKit::WorkerAllowMainThreadBridgeBase> bridge)
    {
        return adoptPtr(new WorkerContextObserver(context, bridge));
    }

    // WorkerContext::Observer method.
    virtual void notifyStop()
    {
        if (!m_bridge)
            return;
        m_bridge->cancel();
    }

private:
    WorkerContextObserver(WebCore::WorkerContext* context, PassRefPtr<WebKit::WorkerAllowMainThreadBridgeBase> bridge)
        : WebCore::WorkerContext::Observer(context)
        , m_bridge(bridge)
    {
    }

    RefPtr<WebKit::WorkerAllowMainThreadBridgeBase> m_bridge;
};

}

namespace WebKit {

WorkerAllowMainThreadBridgeBase::WorkerAllowMainThreadBridgeBase(WebCore::WorkerContext* workerContext, WebWorkerBase* webWorkerBase)
    : m_webWorkerBase(webWorkerBase)
    , m_workerContextObserver(WorkerContextObserver::create(workerContext, this).leakPtr())
{
}

void WorkerAllowMainThreadBridgeBase::postTaskToMainThread(PassOwnPtr<AllowParams> params)
{
    WebWorkerBase::dispatchTaskToMainThread(
        createCallbackTask(&WorkerAllowMainThreadBridgeBase::allowTask, params, this));
}

// static
void WorkerAllowMainThreadBridgeBase::allowTask(WebCore::ScriptExecutionContext*, PassOwnPtr<AllowParams> params, PassRefPtr<WorkerAllowMainThreadBridgeBase> bridge)
{
    ASSERT(isMainThread());
    if (!bridge)
        return;
    MutexLocker locker(bridge->m_mutex);
    if (!bridge->m_webWorkerBase)
        return;
    WebCommonWorkerClient* commonClient = bridge->m_webWorkerBase->commonClient();
    if (!commonClient)
        return;
    bool allow = bridge->allowOnMainThread(commonClient, params.get());
    bridge->m_webWorkerBase->workerLoaderProxy()->postTaskForModeToWorkerContext(
        createCallbackTask(&didComplete, bridge, allow), params->m_mode.isolatedCopy());
}

// static
void WorkerAllowMainThreadBridgeBase::didComplete(WebCore::ScriptExecutionContext* context, PassRefPtr<WorkerAllowMainThreadBridgeBase> bridge, bool result)
{
    bridge->m_result = result;
}

} // namespace WebKit

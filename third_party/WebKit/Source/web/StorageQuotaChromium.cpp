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
#include "modules/quota/StorageQuota.h"

#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebStorageQuotaType.h"
#include "WebWorkerBase.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "modules/quota/StorageErrorCallback.h"
#include "modules/quota/StorageQuotaCallback.h"
#include "modules/quota/StorageUsageCallback.h"
#include "modules/quota/WebStorageQuotaCallbacksImpl.h"
#include "wtf/Threading.h"

using namespace WebKit;

namespace WebCore {

// FIXME: Implement this as StorageQuotaClient.
void StorageQuota::requestQuota(ScriptExecutionContext* scriptExecutionContext, unsigned long long newQuotaInBytes, PassRefPtr<StorageQuotaCallback> successCallback, PassRefPtr<StorageErrorCallback> errorCallback)
{
    ASSERT(scriptExecutionContext);
    WebStorageQuotaType storageType = static_cast<WebStorageQuotaType>(m_type);
    if (storageType != WebStorageQuotaTypeTemporary && storageType != WebStorageQuotaTypePersistent) {
        // Unknown storage type is requested.
        scriptExecutionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NotSupportedError));
        return;
    }
    if (scriptExecutionContext->isDocument()) {
        Document* document = toDocument(scriptExecutionContext);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        webFrame->client()->requestStorageQuota(webFrame, storageType, newQuotaInBytes, WebStorageQuotaCallbacksImpl::createLeakedPtr(successCallback, errorCallback));
    } else {
        // Requesting quota in Worker is not supported.
        scriptExecutionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NotSupportedError));
    }
}

} // namespace WebCore

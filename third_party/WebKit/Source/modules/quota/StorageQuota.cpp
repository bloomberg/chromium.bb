/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/dom/ExceptionCode.h"
#include "core/dom/ScriptExecutionContext.h"
#include "modules/quota/StorageErrorCallback.h"
#include "modules/quota/StorageUsageCallback.h"
#include "modules/quota/WebStorageQuotaCallbacksImpl.h"
#include "public/platform/Platform.h"
#include "public/platform/WebStorageQuotaCallbacks.h"
#include "public/platform/WebStorageQuotaType.h"
#include "weborigin/KURL.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

StorageQuota::StorageQuota(Type type)
    : m_type(type)
{
    ScriptWrappable::init(this);
}

void StorageQuota::queryUsageAndQuota(ScriptExecutionContext* scriptExecutionContext, PassRefPtr<StorageUsageCallback> successCallback, PassRefPtr<StorageErrorCallback> errorCallback)
{
    ASSERT(scriptExecutionContext);

    WebKit::WebStorageQuotaType storageType = static_cast<WebKit::WebStorageQuotaType>(m_type);
    if (storageType != WebKit::WebStorageQuotaTypeTemporary && storageType != WebKit::WebStorageQuotaTypePersistent) {
        // Unknown storage type is requested.
        scriptExecutionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NotSupportedError));
        return;
    }

    SecurityOrigin* securityOrigin = scriptExecutionContext->securityOrigin();
    if (securityOrigin->isUnique()) {
        scriptExecutionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NotSupportedError));
        return;
    }

    KURL storagePartition = KURL(KURL(), securityOrigin->toString());
    WebKit::Platform::current()->queryStorageUsageAndQuota(storagePartition, storageType, WebStorageQuotaCallbacksImpl::createLeakedPtr(successCallback, errorCallback));
}

StorageQuota::~StorageQuota()
{
}

} // namespace WebCore

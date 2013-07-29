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

#ifndef WebStorageQuotaCallbacksImpl_h
#define WebStorageQuotaCallbacksImpl_h

#include "modules/quota/StorageErrorCallback.h"
#include "modules/quota/StorageQuotaCallback.h"
#include "modules/quota/StorageUsageCallback.h"
#include "public/platform/WebStorageQuotaCallbacks.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class WebStorageQuotaCallbacksImpl : public WebKit::WebStorageQuotaCallbacks {
public:
    // The class is self-destructed and thus we have leakedPtr constructors.
    static WebStorageQuotaCallbacksImpl* createLeakedPtr(PassRefPtr<StorageUsageCallback> success, PassRefPtr<StorageErrorCallback> error)
    {
        OwnPtr<WebStorageQuotaCallbacksImpl> callbacks = adoptPtr(new WebStorageQuotaCallbacksImpl(success, error));
        return callbacks.leakPtr();
    }

    static WebStorageQuotaCallbacksImpl* createLeakedPtr(PassRefPtr<StorageQuotaCallback> success, PassRefPtr<StorageErrorCallback> error)
    {
        OwnPtr<WebStorageQuotaCallbacksImpl> callbacks = adoptPtr(new WebStorageQuotaCallbacksImpl(success, error));
        return callbacks.leakPtr();
    }

    virtual ~WebStorageQuotaCallbacksImpl();

    virtual void didQueryStorageUsageAndQuota(unsigned long long usageInBytes, unsigned long long quotaInBytes);
    virtual void didGrantStorageQuota(unsigned long long grantedQuotaInBytes);
    virtual void didFail(WebKit::WebStorageQuotaError);

private:
    WebStorageQuotaCallbacksImpl(PassRefPtr<StorageUsageCallback>, PassRefPtr<StorageErrorCallback>);
    WebStorageQuotaCallbacksImpl(PassRefPtr<StorageQuotaCallback>, PassRefPtr<StorageErrorCallback>);

    RefPtr<StorageUsageCallback> m_usageCallback;
    RefPtr<StorageQuotaCallback> m_quotaCallback;
    RefPtr<StorageErrorCallback> m_errorCallback;
};

} // namespace

#endif // WebStorageQuotaCallbacksImpl_h

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebStorageQuotaCallbacks.h"

#include "platform/StorageQuotaCallbacks.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class WebStorageQuotaCallbacksPrivate : public RefCounted<WebStorageQuotaCallbacksPrivate> {
public:
    static PassRefPtr<WebStorageQuotaCallbacksPrivate> create(const PassOwnPtr<StorageQuotaCallbacks>& callbacks)
    {
        return adoptRef(new WebStorageQuotaCallbacksPrivate(callbacks));
    }

    StorageQuotaCallbacks* callbacks() { return m_callbacks.get(); }

private:
    WebStorageQuotaCallbacksPrivate(const PassOwnPtr<StorageQuotaCallbacks>& callbacks) : m_callbacks(callbacks) { }
    OwnPtr<StorageQuotaCallbacks> m_callbacks;
};

WebStorageQuotaCallbacks::WebStorageQuotaCallbacks(const PassOwnPtr<StorageQuotaCallbacks>& callbacks)
{
    m_private = WebStorageQuotaCallbacksPrivate::create(callbacks);
}

void WebStorageQuotaCallbacks::reset()
{
    m_private.reset();
}

void WebStorageQuotaCallbacks::assign(const WebStorageQuotaCallbacks& other)
{
    m_private = other.m_private;
}

void WebStorageQuotaCallbacks::didQueryStorageUsageAndQuota(unsigned long long usageInBytes, unsigned long long quotaInBytes)
{
    ASSERT(!m_private.isNull());
    m_private->callbacks()->didQueryStorageUsageAndQuota(usageInBytes, quotaInBytes);
    m_private.reset();
}

void WebStorageQuotaCallbacks::didGrantStorageQuota(unsigned long long usageInBytes, unsigned long long grantedQuotaInBytes)
{
    ASSERT(!m_private.isNull());
    m_private->callbacks()->didGrantStorageQuota(usageInBytes, grantedQuotaInBytes);
    m_private.reset();
}

void WebStorageQuotaCallbacks::didFail(WebStorageQuotaError error)
{
    ASSERT(!m_private.isNull());
    m_private->callbacks()->didFail(error);
    m_private.reset();
}

} // namespace blink

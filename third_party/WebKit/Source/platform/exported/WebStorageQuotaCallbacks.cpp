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

class WebStorageQuotaCallbacksPrivate final : public RefCountedWillBeGarbageCollected<WebStorageQuotaCallbacksPrivate> {
public:
    static PassRefPtrWillBeRawPtr<WebStorageQuotaCallbacksPrivate> create(const PassOwnPtrWillBeRawPtr<StorageQuotaCallbacks>& callbacks)
    {
        return adoptRefWillBeNoop(new WebStorageQuotaCallbacksPrivate(callbacks));
    }

    void trace(Visitor*);

    StorageQuotaCallbacks* callbacks() { return m_callbacks.get(); }

private:
    WebStorageQuotaCallbacksPrivate(const PassOwnPtrWillBeRawPtr<StorageQuotaCallbacks>& callbacks) : m_callbacks(callbacks) { }
    OwnPtrWillBeMember<StorageQuotaCallbacks> m_callbacks;
};

void WebStorageQuotaCallbacksPrivate::trace(Visitor* visitor)
{
    visitor->trace(m_callbacks);
}

WebStorageQuotaCallbacks::WebStorageQuotaCallbacks(const PassOwnPtrWillBeRawPtr<StorageQuotaCallbacks>& callbacks)
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

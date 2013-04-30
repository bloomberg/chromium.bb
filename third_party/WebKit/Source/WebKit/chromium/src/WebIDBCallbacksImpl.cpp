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
#include "WebIDBCallbacksImpl.h"

#include <public/WebData.h>
#include <public/WebIDBCallbacks.h>
#include <public/WebIDBDatabase.h>
#include <public/WebIDBDatabaseError.h>
#include <public/WebIDBKey.h>
#include "IDBCursorBackendProxy.h"
#include "IDBDatabaseBackendProxy.h"
#include "modules/indexeddb/IDBCallbacks.h"
#include "modules/indexeddb/IDBDatabaseError.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBMetadata.h"

using namespace WebCore;

namespace WebKit {

WebIDBCallbacksImpl::WebIDBCallbacksImpl(PassRefPtr<IDBCallbacks> callbacks)
    : m_callbacks(callbacks)
{
}

WebIDBCallbacksImpl::~WebIDBCallbacksImpl()
{
}

void WebIDBCallbacksImpl::onError(const WebIDBDatabaseError& error)
{
    m_callbacks->onError(error);
}

void WebIDBCallbacksImpl::onSuccess(const WebVector<WebString>& webStringList)
{
    Vector<String> stringList;
    for (size_t i = 0; i < webStringList.size(); ++i)
        stringList.append(webStringList[i]);
    m_callbacks->onSuccess(stringList);
}

void WebIDBCallbacksImpl::onSuccess(WebIDBCursor* cursor, const WebIDBKey& key, const WebIDBKey& primaryKey, const WebData& value)
{
    m_callbacks->onSuccess(IDBCursorBackendProxy::create(adoptPtr(cursor)), key, primaryKey, value);
}

void WebIDBCallbacksImpl::onSuccess(WebIDBDatabase* webKitInstance, const WebIDBMetadata& metadata)
{
    if (m_databaseProxy) {
        m_callbacks->onSuccess(m_databaseProxy.release(), metadata);
        return;
    }
    RefPtr<IDBDatabaseBackendInterface> localDatabaseProxy = IDBDatabaseBackendProxy::create(adoptPtr(webKitInstance));
    m_callbacks->onSuccess(localDatabaseProxy.release(), metadata);
}

void WebIDBCallbacksImpl::onSuccess(const WebIDBKey& key)
{
    m_callbacks->onSuccess(key);
}

void WebIDBCallbacksImpl::onSuccess(const WebData& value)
{
    m_callbacks->onSuccess(value);
}

void WebIDBCallbacksImpl::onSuccess(const WebData& value, const WebIDBKey& key, const WebIDBKeyPath& keyPath)
{
    m_callbacks->onSuccess(value, key, keyPath);
}

void WebIDBCallbacksImpl::onSuccess(long long value)
{
    m_callbacks->onSuccess(value);
}

void WebIDBCallbacksImpl::onSuccess()
{
    m_callbacks->onSuccess();
}

void WebIDBCallbacksImpl::onSuccess(const WebIDBKey& key, const WebIDBKey& primaryKey, const WebData& value)
{
    m_callbacks->onSuccess(key, primaryKey, value);
}

void WebIDBCallbacksImpl::onBlocked(long long oldVersion)
{
    m_callbacks->onBlocked(oldVersion);
}

void WebIDBCallbacksImpl::onUpgradeNeeded(long long oldVersion, WebIDBDatabase* database, const WebIDBMetadata& metadata)
{
    m_databaseProxy = IDBDatabaseBackendProxy::create(adoptPtr(database));
    m_callbacks->onUpgradeNeeded(oldVersion, m_databaseProxy, metadata);
}

} // namespace WebKit

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "public/platform/WebIDBCallbacks.h"

#include "IDBCursorBackendProxy.h"
#include "IDBDatabaseBackendProxy.h"
#include "core/dom/DOMError.h"
#include "modules/indexeddb/IDBCallbacks.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "public/platform/WebData.h"
#include "public/platform/WebIDBDatabase.h"
#include "public/platform/WebIDBDatabaseError.h"
#include "public/platform/WebIDBKey.h"

using namespace WebCore;

namespace blink {

WebIDBCallbacks::WebIDBCallbacks(PassRefPtr<WebCore::IDBCallbacks> callbacks)
    : m_private(callbacks)
{
}

WebIDBCallbacks::~WebIDBCallbacks()
{
    m_private.reset();
    m_databaseProxy.reset();
}

void WebIDBCallbacks::onError(const WebIDBDatabaseError& error)
{
    m_private->onError(error);
}

void WebIDBCallbacks::onSuccess(const WebVector<WebString>& webStringList)
{
    Vector<String> stringList;
    for (size_t i = 0; i < webStringList.size(); ++i)
        stringList.append(webStringList[i]);
    m_private->onSuccess(stringList);
}

void WebIDBCallbacks::onSuccess(WebIDBCursor* cursor, const WebIDBKey& key, const WebIDBKey& primaryKey, const WebData& value)
{
    m_private->onSuccess(IDBCursorBackendProxy::create(adoptPtr(cursor)), key, primaryKey, value);
}

void WebIDBCallbacks::onSuccess(WebIDBDatabase* webKitInstance, const WebIDBMetadata& metadata)
{
    if (!m_databaseProxy.isNull()) {
        m_private->onSuccess(m_databaseProxy.get(), metadata);
        m_databaseProxy.reset();
        return;
    }
    RefPtr<IDBDatabaseBackendInterface> localDatabaseProxy = IDBDatabaseBackendProxy::create(adoptPtr(webKitInstance));
    m_private->onSuccess(localDatabaseProxy.release(), metadata);
}

void WebIDBCallbacks::onSuccess(const WebIDBKey& key)
{
    m_private->onSuccess(key);
}

void WebIDBCallbacks::onSuccess(const WebData& value)
{
    m_private->onSuccess(value);
}

void WebIDBCallbacks::onSuccess(const WebData& value, const WebIDBKey& key, const WebIDBKeyPath& keyPath)
{
    m_private->onSuccess(value, key, keyPath);
}

void WebIDBCallbacks::onSuccess(long long value)
{
    m_private->onSuccess(value);
}

void WebIDBCallbacks::onSuccess()
{
    m_private->onSuccess();
}

void WebIDBCallbacks::onSuccess(const WebIDBKey& key, const WebIDBKey& primaryKey, const WebData& value)
{
    m_private->onSuccess(key, primaryKey, value);
}

void WebIDBCallbacks::onBlocked(long long oldVersion)
{
    m_private->onBlocked(oldVersion);
}

void WebIDBCallbacks::onUpgradeNeeded(long long oldVersion, WebIDBDatabase* database, const WebIDBMetadata& metadata, DataLoss dataLoss, WebString dataLossMessage)
{
    m_databaseProxy = IDBDatabaseBackendProxy::create(adoptPtr(database));
    m_private->onUpgradeNeeded(oldVersion, m_databaseProxy.get(), metadata, dataLoss, dataLossMessage);
}

} // namespace blink

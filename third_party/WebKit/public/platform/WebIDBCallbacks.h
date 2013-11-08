/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebIDBCallbacks_h
#define WebIDBCallbacks_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebString.h"
#include "WebVector.h"

#if BLINK_IMPLEMENTATION
#include "wtf/PassRefPtr.h"
#endif

namespace WebCore {
class IDBCallbacks;
class IDBDatabaseBackendInterface;
}

namespace blink {

class WebData;
class WebIDBCursor;
class WebIDBDatabase;
class WebIDBDatabaseError;
class WebIDBIndex;
class WebIDBKey;
class WebIDBKeyPath;
struct WebIDBMetadata;

class BLINK_EXPORT WebIDBCallbacks {
public:
#if BLINK_IMPLEMENTATION
    explicit WebIDBCallbacks(PassRefPtr<WebCore::IDBCallbacks>);
#endif

    WebIDBCallbacks() { }
    virtual ~WebIDBCallbacks();

    enum DataLoss {
        DataLossNone = 0,
        DataLossTotal = 1
    };

    // For classes that follow the PImpl pattern, pass a const reference.
    // For the rest, pass ownership to the callee via a pointer.
    virtual void onError(const WebIDBDatabaseError&);
    virtual void onSuccess(const WebVector<WebString>&);
    virtual void onSuccess(WebIDBCursor*, const WebIDBKey&, const WebIDBKey& primaryKey, const WebData&);
    virtual void onSuccess(WebIDBDatabase*, const WebIDBMetadata&);
    virtual void onSuccess(const WebIDBKey&);
    virtual void onSuccess(const WebData&);
    virtual void onSuccess(const WebData&, const WebIDBKey&, const WebIDBKeyPath&);
    virtual void onSuccess(long long);
    virtual void onSuccess();
    virtual void onSuccess(const WebIDBKey&, const WebIDBKey& primaryKey, const WebData&);
    virtual void onBlocked(long long oldVersion);
    virtual void onUpgradeNeeded(long long oldVersion, WebIDBDatabase*, const WebIDBMetadata&, DataLoss, WebString dataLossMessage);

private:
    WebPrivatePtr<WebCore::IDBCallbacks> m_private;
    WebPrivatePtr<WebCore::IDBDatabaseBackendInterface> m_databaseProxy;
};

} // namespace blink

#endif // WebIDBCallbacks_h

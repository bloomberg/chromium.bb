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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "modules/indexeddb/IDBFactory.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/IDBBindingUtilities.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBHistograms.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IndexedDBClient.h"
#include "modules/indexeddb/WebIDBCallbacksImpl.h"
#include "modules/indexeddb/WebIDBDatabaseCallbacksImpl.h"
#include "platform/weborigin/DatabaseIdentifier.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebIDBFactory.h"

namespace WebCore {

static const char permissionDeniedErrorMessage[] = "The user denied permission to access the database.";

IDBFactory::IDBFactory(IndexedDBClient* permissionClient)
    : m_permissionClient(permissionClient)
{
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
    ScriptWrappable::init(this);
}

IDBFactory::~IDBFactory()
{
}

static bool isContextValid(ExecutionContext* context)
{
    ASSERT(context->isDocument() || context->isWorkerGlobalScope());
    if (context->isDocument()) {
        Document* document = toDocument(context);
        return document->frame() && document->page();
    }
    return true;
}

PassRefPtr<IDBRequest> IDBFactory::getDatabaseNames(ExecutionContext* context, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBFactory::getDatabaseNames");
    if (!isContextValid(context))
        return nullptr;
    if (!context->securityOrigin()->canAccessDatabase()) {
        exceptionState.throwSecurityError("access to the Indexed Database API is denied in this context.");
        return nullptr;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::createNull(), 0);

    if (!m_permissionClient->allowIndexedDB(context, "Database Listing")) {
        request->onError(DOMError::create(UnknownError, permissionDeniedErrorMessage));
        return request;
    }

    blink::Platform::current()->idbFactory()->getDatabaseNames(WebIDBCallbacksImpl::create(request).leakPtr(), createDatabaseIdentifierFromSecurityOrigin(context->securityOrigin()));
    return request;
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::open(ExecutionContext* context, const String& name, unsigned long long version, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBFactory::open");
    if (!version) {
        exceptionState.throwTypeError("The version provided must not be 0.");
        return nullptr;
    }
    return openInternal(context, name, version, exceptionState);
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::openInternal(ExecutionContext* context, const String& name, int64_t version, ExceptionState& exceptionState)
{
    blink::Platform::current()->histogramEnumeration("WebCore.IndexedDB.FrontEndAPICalls", IDBOpenCall, IDBMethodsMax);
    ASSERT(version >= 1 || version == IDBDatabaseMetadata::NoIntVersion);
    if (name.isNull()) {
        exceptionState.throwTypeError("The name provided must not be empty.");
        return nullptr;
    }
    if (!isContextValid(context))
        return nullptr;
    if (!context->securityOrigin()->canAccessDatabase()) {
        exceptionState.throwSecurityError("access to the Indexed Database API is denied in this context.");
        return nullptr;
    }

    RefPtr<IDBDatabaseCallbacks> databaseCallbacks = IDBDatabaseCallbacks::create();
    int64_t transactionId = IDBDatabase::nextTransactionId();
    RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(context, databaseCallbacks, transactionId, version);

    if (!m_permissionClient->allowIndexedDB(context, name)) {
        request->onError(DOMError::create(UnknownError, permissionDeniedErrorMessage));
        return request;
    }

    blink::Platform::current()->idbFactory()->open(name, version, transactionId, WebIDBCallbacksImpl::create(request).leakPtr(), WebIDBDatabaseCallbacksImpl::create(databaseCallbacks.release()).leakPtr(), createDatabaseIdentifierFromSecurityOrigin(context->securityOrigin()));
    return request;
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::open(ExecutionContext* context, const String& name, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBFactory::open");
    return openInternal(context, name, IDBDatabaseMetadata::NoIntVersion, exceptionState);
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::deleteDatabase(ExecutionContext* context, const String& name, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBFactory::deleteDatabase");
    blink::Platform::current()->histogramEnumeration("WebCore.IndexedDB.FrontEndAPICalls", IDBDeleteDatabaseCall, IDBMethodsMax);
    if (name.isNull()) {
        exceptionState.throwTypeError("The name provided must not be empty.");
        return nullptr;
    }
    if (!isContextValid(context))
        return nullptr;
    if (!context->securityOrigin()->canAccessDatabase()) {
        exceptionState.throwSecurityError("access to the Indexed Database API is denied in this context.");
        return nullptr;
    }

    RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(context, nullptr, 0, IDBDatabaseMetadata::DefaultIntVersion);

    if (!m_permissionClient->allowIndexedDB(context, name)) {
        request->onError(DOMError::create(UnknownError, permissionDeniedErrorMessage));
        return request;
    }

    blink::Platform::current()->idbFactory()->deleteDatabase(name, WebIDBCallbacksImpl::create(request).leakPtr(), createDatabaseIdentifierFromSecurityOrigin(context->securityOrigin()));
    return request;
}

short IDBFactory::cmp(ExecutionContext* context, const ScriptValue& firstValue, const ScriptValue& secondValue, ExceptionState& exceptionState)
{
    RefPtr<IDBKey> first = scriptValueToIDBKey(toIsolate(context), firstValue);
    RefPtr<IDBKey> second = scriptValueToIDBKey(toIsolate(context), secondValue);

    ASSERT(first);
    ASSERT(second);

    if (!first->isValid() || !second->isValid()) {
        exceptionState.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return 0;
    }

    return static_cast<short>(first->compare(second.get()));
}

} // namespace WebCore

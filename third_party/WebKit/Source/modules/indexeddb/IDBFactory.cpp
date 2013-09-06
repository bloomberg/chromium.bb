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

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/IDBBindingUtilities.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/PageGroup.h"
#include "core/platform/HistogramSupport.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBDatabaseCallbacksImpl.h"
#include "modules/indexeddb/IDBFactoryBackendInterface.h"
#include "modules/indexeddb/IDBHistograms.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "modules/indexeddb/IDBTracing.h"
#include "weborigin/DatabaseIdentifier.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

IDBFactory::IDBFactory(IDBFactoryBackendInterface* factory)
    : m_backend(factory)
{
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
    ScriptWrappable::init(this);
}

IDBFactory::~IDBFactory()
{
}

static bool isContextValid(ScriptExecutionContext* context)
{
    ASSERT(context->isDocument() || context->isWorkerGlobalScope());
    if (context->isDocument()) {
        Document* document = toDocument(context);
        return document->frame() && document->page();
    }
    return true;
}

PassRefPtr<IDBRequest> IDBFactory::getDatabaseNames(ScriptExecutionContext* context, ExceptionState& es)
{
    IDB_TRACE("IDBFactory::getDatabaseNames");
    if (!isContextValid(context))
        return 0;
    if (!context->securityOrigin()->canAccessDatabase()) {
        es.throwSecurityError(ExceptionMessages::failedToExecute("getDatabaseNames", "IDBFactory", "access to the Indexed Database API is denied in this context."));
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), 0);
    m_backend->getDatabaseNames(request, createDatabaseIdentifierFromSecurityOrigin(context->securityOrigin()), context);
    return request;
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext* context, const String& name, unsigned long long version, ExceptionState& es)
{
    IDB_TRACE("IDBFactory::open");
    if (!version) {
        es.throwTypeError();
        return 0;
    }
    return openInternal(context, name, version, es);
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::openInternal(ScriptExecutionContext* context, const String& name, int64_t version, ExceptionState& es)
{
    HistogramSupport::histogramEnumeration("WebCore.IndexedDB.FrontEndAPICalls", IDBOpenCall, IDBMethodsMax);
    ASSERT(version >= 1 || version == IDBDatabaseMetadata::NoIntVersion);
    if (name.isNull()) {
        es.throwTypeError();
        return 0;
    }
    if (!isContextValid(context))
        return 0;
    if (!context->securityOrigin()->canAccessDatabase()) {
        es.throwSecurityError(ExceptionMessages::failedToExecute("open", "IDBFactory", "access to the Indexed Database API is denied in this context."));
        return 0;
    }

    RefPtr<IDBDatabaseCallbacksImpl> databaseCallbacks = IDBDatabaseCallbacksImpl::create();
    int64_t transactionId = IDBDatabase::nextTransactionId();
    RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(context, databaseCallbacks, transactionId, version);
    m_backend->open(name, version, transactionId, request, databaseCallbacks, createDatabaseIdentifierFromSecurityOrigin(context->securityOrigin()), context);
    return request;
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext* context, const String& name, ExceptionState& es)
{
    IDB_TRACE("IDBFactory::open");
    return openInternal(context, name, IDBDatabaseMetadata::NoIntVersion, es);
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::deleteDatabase(ScriptExecutionContext* context, const String& name, ExceptionState& es)
{
    IDB_TRACE("IDBFactory::deleteDatabase");
    HistogramSupport::histogramEnumeration("WebCore.IndexedDB.FrontEndAPICalls", IDBDeleteDatabaseCall, IDBMethodsMax);
    if (name.isNull()) {
        es.throwTypeError();
        return 0;
    }
    if (!isContextValid(context))
        return 0;
    if (!context->securityOrigin()->canAccessDatabase()) {
        es.throwSecurityError(ExceptionMessages::failedToExecute("deleteDatabase", "IDBFactory", "access to the Indexed Database API is denied in this context."));
        return 0;
    }

    RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(context, 0, 0, IDBDatabaseMetadata::DefaultIntVersion);
    m_backend->deleteDatabase(name, request, createDatabaseIdentifierFromSecurityOrigin(context->securityOrigin()), context);
    return request;
}

short IDBFactory::cmp(ScriptExecutionContext* context, const ScriptValue& firstValue, const ScriptValue& secondValue, ExceptionState& es)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> first = scriptValueToIDBKey(&requestState, firstValue);
    RefPtr<IDBKey> second = scriptValueToIDBKey(&requestState, secondValue);

    ASSERT(first);
    ASSERT(second);

    if (!first->isValid() || !second->isValid()) {
        es.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return 0;
    }

    return static_cast<short>(first->compare(second.get()));
}

} // namespace WebCore

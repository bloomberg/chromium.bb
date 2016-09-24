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

#ifndef IDBObjectStore_h
#define IDBObjectStore_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "modules/indexeddb/IDBCursor.h"
#include "modules/indexeddb/IDBIndex.h"
#include "modules/indexeddb/IDBIndexParameters.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DOMStringList;
class IDBAny;
class ExceptionState;

class IDBObjectStore final : public GarbageCollectedFinalized<IDBObjectStore>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static IDBObjectStore* create(const IDBObjectStoreMetadata& metadata, IDBTransaction* transaction)
    {
        return new IDBObjectStore(metadata, transaction);
    }
    ~IDBObjectStore() { }
    DECLARE_TRACE();

    const IDBObjectStoreMetadata& metadata() const { return m_metadata; }
    const IDBKeyPath& idbKeyPath() const { return metadata().keyPath; }

    // Implement the IDBObjectStore IDL
    int64_t id() const { return metadata().id; }
    const String& name() const { return metadata().name; }
    void setName(const String& name, ExceptionState&);
    ScriptValue keyPath(ScriptState*) const;
    DOMStringList* indexNames() const;
    IDBTransaction* transaction() const { return m_transaction.get(); }
    bool autoIncrement() const { return metadata().autoIncrement; }

    IDBRequest* openCursor(ScriptState*, const ScriptValue& range, const String& direction, ExceptionState&);
    IDBRequest* openKeyCursor(ScriptState*, const ScriptValue& range, const String& direction, ExceptionState&);
    IDBRequest* get(ScriptState*, const ScriptValue& key, ExceptionState&);
    IDBRequest* getKey(ScriptState*, const ScriptValue& key, ExceptionState&);
    IDBRequest* getAll(ScriptState*, const ScriptValue& range, unsigned long maxCount, ExceptionState&);
    IDBRequest* getAll(ScriptState*, const ScriptValue& range, ExceptionState&);
    IDBRequest* getAllKeys(ScriptState*, const ScriptValue& range, unsigned long maxCount, ExceptionState&);
    IDBRequest* getAllKeys(ScriptState*, const ScriptValue& range, ExceptionState&);
    IDBRequest* add(ScriptState*, const ScriptValue&, const ScriptValue& key, ExceptionState&);
    IDBRequest* put(ScriptState*, const ScriptValue&, const ScriptValue& key, ExceptionState&);
    IDBRequest* deleteFunction(ScriptState*, const ScriptValue& key, ExceptionState&);
    IDBRequest* clear(ScriptState*, ExceptionState&);

    IDBIndex* createIndex(ScriptState* scriptState, const String& name, const StringOrStringSequence& keyPath, const IDBIndexParameters& options, ExceptionState& exceptionState)
    {
        return createIndex(scriptState, name, IDBKeyPath(keyPath), options, exceptionState);
    }
    IDBIndex* index(const String& name, ExceptionState&);
    void deleteIndex(const String& name, ExceptionState&);

    IDBRequest* count(ScriptState*, const ScriptValue& range, ExceptionState&);

    // Used by IDBCursor::update():
    IDBRequest* put(ScriptState*, WebIDBPutMode, IDBAny* source, const ScriptValue&, IDBKey*, ExceptionState&);

    // Used internally and by InspectorIndexedDBAgent:
    IDBRequest* openCursor(ScriptState*, IDBKeyRange*, WebIDBCursorDirection, WebIDBTaskType = WebIDBTaskTypeNormal);

    void markDeleted();
    bool isDeleted() const { return m_deleted; }
    void abort();
    void transactionFinished();

    void setMetadata(const IDBObjectStoreMetadata& metadata) { m_metadata = metadata; }

    // Used by IDBIndex::setName:
    bool containsIndex(const String& name) const
    {
        return findIndexId(name) != IDBIndexMetadata::InvalidId;
    }
    void indexRenamed(int64_t indexId, const String& newName);

    WebIDBDatabase* backendDB() const;

private:
    IDBObjectStore(const IDBObjectStoreMetadata&, IDBTransaction*);

    IDBIndex* createIndex(ScriptState*, const String& name, const IDBKeyPath&, const IDBIndexParameters&, ExceptionState&);
    IDBRequest* put(ScriptState*, WebIDBPutMode, IDBAny* source, const ScriptValue&, const ScriptValue& key, ExceptionState&);

    int64_t findIndexId(const String& name) const;

    IDBObjectStoreMetadata m_metadata;
    Member<IDBTransaction> m_transaction;
    bool m_deleted = false;

    // Caches the IDBIndex instances returned by the index() method.
    // The spec requires that an object store's index() returns the same
    // IDBIndex instance for a specific index, so this cache is necessary
    // for correctness.
    //
    // index() throws for completed/aborted transactions, so this is not used
    // after a transaction is finished, and can be cleared.
    using IDBIndexMap = HeapHashMap<String, Member<IDBIndex>>;
    IDBIndexMap m_indexMap;

    // Used to mark indexes created in an aborted upgrade transaction as
    // deleted.
    HeapHashSet<Member<IDBIndex>> m_createdIndexes;
};

} // namespace blink

#endif // IDBObjectStore_h

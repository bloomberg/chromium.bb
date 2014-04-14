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

#ifndef IDBIndex_h
#define IDBIndex_h

#include "bindings/v8/ScriptWrappable.h"
#include "modules/indexeddb/IDBCursor.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBRequest.h"
#include "public/platform/WebIDBCursor.h"
#include "public/platform/WebIDBDatabase.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionState;
class IDBObjectStore;

class IDBIndex : public ScriptWrappable, public RefCounted<IDBIndex> {
public:
    static PassRefPtr<IDBIndex> create(const IDBIndexMetadata& metadata, IDBObjectStore* objectStore, IDBTransaction* transaction)
    {
        return adoptRef(new IDBIndex(metadata, objectStore, transaction));
    }
    ~IDBIndex();

    // Implement the IDL
    const String& name() const { return m_metadata.name; }
    PassRefPtr<IDBObjectStore> objectStore() const { return m_objectStore; }
    ScriptValue keyPath(NewScriptState*) const;
    bool unique() const { return m_metadata.unique; }
    bool multiEntry() const { return m_metadata.multiEntry; }

    PassRefPtr<IDBRequest> openCursor(ExecutionContext*, const ScriptValue& key, const String& direction, ExceptionState&);
    PassRefPtr<IDBRequest> openKeyCursor(ExecutionContext*, const ScriptValue& range, const String& direction, ExceptionState&);
    PassRefPtr<IDBRequest> count(ExecutionContext*, const ScriptValue& range, ExceptionState&);
    PassRefPtr<IDBRequest> get(ExecutionContext*, const ScriptValue& key, ExceptionState&);
    PassRefPtr<IDBRequest> getKey(ExecutionContext*, const ScriptValue& key, ExceptionState&);

    void markDeleted() { m_deleted = true; }
    bool isDeleted() const;

    // Used internally and by InspectorIndexedDBAgent:
    PassRefPtr<IDBRequest> openCursor(ExecutionContext*, PassRefPtr<IDBKeyRange>, blink::WebIDBCursor::Direction);

    blink::WebIDBDatabase* backendDB() const;

private:
    IDBIndex(const IDBIndexMetadata&, IDBObjectStore*, IDBTransaction*);

    PassRefPtr<IDBRequest> getInternal(ExecutionContext*, const ScriptValue& key, ExceptionState&, bool keyOnly);

    IDBIndexMetadata m_metadata;
    RefPtr<IDBObjectStore> m_objectStore;
    RefPtr<IDBTransaction> m_transaction;
    bool m_deleted;
};

} // namespace WebCore

#endif // IDBIndex_h

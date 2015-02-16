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

#ifndef IDBAny_h
#define IDBAny_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "platform/SharedBuffer.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DOMStringList;
class IDBCursor;
class IDBCursorWithValue;
class IDBDatabase;
class IDBIndex;
class IDBObjectStore;
class WebBlobInfo;

// IDBAny is used for:
//  * source of IDBCursor (IDBObjectStore or IDBIndex)
//  * source of IDBRequest (IDBObjectStore, IDBIndex, IDBCursor, or null)
//  * result of IDBRequest (IDBDatabase, IDBCursor, DOMStringList, undefined, integer, key or value)
//
// This allows for lazy conversion to script values (via IDBBindingUtilities),
// and avoids the need for many dedicated union types.
//
// Values may be represented as just serialized data plus blob references
// (BufferType), or serialized data, blob references, and a key to be injected
// at a given path (BufferKeyAndKeyPathType).

class IDBAny : public GarbageCollectedFinalized<IDBAny> {
public:
    static IDBAny* createUndefined();
    static IDBAny* createNull();
    template<typename T>
    static IDBAny* create(T* idbObject)
    {
        return new IDBAny(idbObject);
    }
    static IDBAny* create(PassRefPtrWillBeRawPtr<DOMStringList> domStringList)
    {
        return new IDBAny(domStringList);
    }
    static IDBAny* create(int64_t value)
    {
        return new IDBAny(value);
    }
    static IDBAny* create(PassRefPtr<SharedBuffer> value, const Vector<WebBlobInfo>* blobInfo)
    {
        return new IDBAny(value, blobInfo);
    }
    static IDBAny* create(PassRefPtr<SharedBuffer> value, const Vector<WebBlobInfo>* blobInfo, IDBKey* key, const IDBKeyPath& keyPath)
    {
        return new IDBAny(value, blobInfo, key, keyPath);
    }
    ~IDBAny();
    DECLARE_TRACE();
    void contextWillBeDestroyed();

    enum Type {
        UndefinedType = 0,
        NullType,
        DOMStringListType,
        IDBCursorType,
        IDBCursorWithValueType,
        IDBDatabaseType,
        IDBIndexType,
        IDBObjectStoreType,
        IntegerType,
        KeyType,
        BufferType,
        BufferKeyAndKeyPathType,
    };

    Type type() const { return m_type; }
    // Use type() to figure out which one of these you're allowed to call.
    DOMStringList* domStringList() const;
    IDBCursor* idbCursor() const;
    IDBCursorWithValue* idbCursorWithValue() const;
    IDBDatabase* idbDatabase() const;
    IDBIndex* idbIndex() const;
    IDBObjectStore* idbObjectStore() const;
    SharedBuffer* buffer() const;
    const Vector<WebBlobInfo>* blobInfo() const;
    int64_t integer() const;
    const IDBKey* key() const;
    const IDBKeyPath& keyPath() const;

private:
    explicit IDBAny(Type);
    explicit IDBAny(PassRefPtrWillBeRawPtr<DOMStringList>);
    explicit IDBAny(IDBCursor*);
    explicit IDBAny(IDBDatabase*);
    explicit IDBAny(IDBIndex*);
    explicit IDBAny(IDBObjectStore*);
    explicit IDBAny(IDBKey*);
    IDBAny(PassRefPtr<SharedBuffer>, const Vector<WebBlobInfo>*);
    IDBAny(PassRefPtr<SharedBuffer>, const Vector<WebBlobInfo>*, IDBKey*, const IDBKeyPath&);
    explicit IDBAny(int64_t);

    const Type m_type;

    // Only one of the following should ever be in use at any given time, except that BufferType uses two and BufferKeyAndKeyPathType uses four.
    const RefPtrWillBeMember<DOMStringList> m_domStringList;
    const Member<IDBCursor> m_idbCursor;
    const Member<IDBDatabase> m_idbDatabase;
    const Member<IDBIndex> m_idbIndex;
    const Member<IDBObjectStore> m_idbObjectStore;
    const Member<IDBKey> m_idbKey;
    const IDBKeyPath m_idbKeyPath;
    const RefPtr<SharedBuffer> m_buffer;
    const Vector<WebBlobInfo>* m_blobInfo;
    const int64_t m_integer;
};

} // namespace blink

#endif // IDBAny_h

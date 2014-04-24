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

#include "bindings/v8/ScriptWrappable.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "platform/SharedBuffer.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DOMStringList;
class IDBCursor;
class IDBCursorWithValue;
class IDBDatabase;
class IDBIndex;
class IDBKeyPath;
class IDBObjectStore;
class IDBTransaction;

class IDBAny : public RefCounted<IDBAny> {
public:
    static PassRefPtr<IDBAny> createUndefined();
    static PassRefPtr<IDBAny> createNull();
    static PassRefPtr<IDBAny> createString(const String&);
    template<typename T>
    static PassRefPtr<IDBAny> create(T* idbObject)
    {
        return adoptRef(new IDBAny(idbObject));
    }
    template<typename T>
    static PassRefPtr<IDBAny> create(const T& idbObject)
    {
        return adoptRef(new IDBAny(idbObject));
    }
    template<typename T>
    static PassRefPtr<IDBAny> create(PassRefPtr<T> idbObject)
    {
        return adoptRef(new IDBAny(idbObject));
    }
    static PassRefPtr<IDBAny> create(int64_t value)
    {
        return adoptRef(new IDBAny(value));
    }
    static PassRefPtr<IDBAny> create(PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey> key, const IDBKeyPath& keyPath)
    {
        return adoptRef(new IDBAny(value, key, keyPath));
    }
    ~IDBAny();
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
        IDBTransactionType,
        BufferType,
        IntegerType,
        StringType,
        KeyPathType,
        KeyType,
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
    IDBTransaction* idbTransaction() const;
    SharedBuffer* buffer() const;
    int64_t integer() const;
    const String& string() const;
    const IDBKey* key() const;
    const IDBKeyPath& keyPath() const;

private:
    explicit IDBAny(Type);
    explicit IDBAny(PassRefPtr<DOMStringList>);
    explicit IDBAny(PassRefPtr<IDBCursor>);
    explicit IDBAny(PassRefPtr<IDBDatabase>);
    explicit IDBAny(PassRefPtr<IDBIndex>);
    explicit IDBAny(PassRefPtr<IDBObjectStore>);
    explicit IDBAny(PassRefPtr<IDBTransaction>);
    explicit IDBAny(PassRefPtr<IDBKey>);
    explicit IDBAny(const IDBKeyPath&);
    explicit IDBAny(const String&);
    explicit IDBAny(PassRefPtr<SharedBuffer>);
    explicit IDBAny(PassRefPtr<SharedBuffer>, PassRefPtr<IDBKey>, const IDBKeyPath&);
    explicit IDBAny(int64_t);

    const Type m_type;

    // Only one of the following should ever be in use at any given time.
    const RefPtr<DOMStringList> m_domStringList;
    const RefPtr<IDBCursor> m_idbCursor;
    const RefPtr<IDBDatabase> m_idbDatabase;
    const RefPtr<IDBIndex> m_idbIndex;
    const RefPtr<IDBObjectStore> m_idbObjectStore;
    const RefPtr<IDBTransaction> m_idbTransaction;
    const RefPtr<IDBKey> m_idbKey;
    const IDBKeyPath m_idbKeyPath;
    const RefPtr<SharedBuffer> m_buffer;
    const String m_string;
    const int64_t m_integer;
};

} // namespace WebCore

#endif // IDBAny_h

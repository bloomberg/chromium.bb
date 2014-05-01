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

namespace blink {

class WebBlobInfo;

}

namespace WebCore {

class DOMStringList;
class IDBCursor;
class IDBCursorWithValue;
class IDBDatabase;
class IDBIndex;
class IDBKeyPath;
class IDBObjectStore;
class IDBTransaction;

class IDBAny : public RefCountedWillBeGarbageCollectedFinalized<IDBAny> {
public:
    static PassRefPtrWillBeRawPtr<IDBAny> createUndefined();
    static PassRefPtrWillBeRawPtr<IDBAny> createNull();
    static PassRefPtrWillBeRawPtr<IDBAny> createString(const String&);
    template<typename T>
    static PassRefPtrWillBeRawPtr<IDBAny> create(T* idbObject)
    {
        return adoptRefWillBeNoop(new IDBAny(idbObject));
    }
    template<typename T>
    static PassRefPtrWillBeRawPtr<IDBAny> create(const T& idbObject)
    {
        return adoptRefWillBeNoop(new IDBAny(idbObject));
    }
    static PassRefPtrWillBeRawPtr<IDBAny> create(PassRefPtr<SharedBuffer> value, const Vector<blink::WebBlobInfo>* blobInfo)
    {
        return adoptRefWillBeNoop(new IDBAny(value, blobInfo));
    }
    template<typename T>
    static PassRefPtrWillBeRawPtr<IDBAny> create(PassRefPtr<T> idbObject)
    {
        return adoptRefWillBeNoop(new IDBAny(idbObject));
    }
    static PassRefPtrWillBeRawPtr<IDBAny> create(int64_t value)
    {
        return adoptRefWillBeNoop(new IDBAny(value));
    }
    static PassRefPtrWillBeRawPtr<IDBAny> create(PassRefPtr<SharedBuffer> value, const Vector<blink::WebBlobInfo>* blobInfo, PassRefPtrWillBeRawPtr<IDBKey> key, const IDBKeyPath& keyPath)
    {
        return adoptRefWillBeNoop(new IDBAny(value, blobInfo, key, keyPath));
    }
    ~IDBAny();
    void trace(Visitor*);
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
    const Vector<blink::WebBlobInfo>* blobInfo() const;
    int64_t integer() const;
    const String& string() const;
    const IDBKey* key() const;
    const IDBKeyPath& keyPath() const;

private:
    explicit IDBAny(Type);
    explicit IDBAny(PassRefPtr<DOMStringList>);
    explicit IDBAny(PassRefPtrWillBeRawPtr<IDBCursor>);
    explicit IDBAny(PassRefPtr<IDBDatabase>);
    explicit IDBAny(PassRefPtr<IDBIndex>);
    explicit IDBAny(PassRefPtrWillBeRawPtr<IDBObjectStore>);
    explicit IDBAny(PassRefPtr<IDBTransaction>);
    explicit IDBAny(PassRefPtrWillBeRawPtr<IDBKey>);
    explicit IDBAny(const IDBKeyPath&);
    explicit IDBAny(const String&);
    IDBAny(PassRefPtr<SharedBuffer>, const Vector<blink::WebBlobInfo>*);
    IDBAny(PassRefPtr<SharedBuffer>, const Vector<blink::WebBlobInfo>*, PassRefPtrWillBeRawPtr<IDBKey>, const IDBKeyPath&);
    explicit IDBAny(int64_t);

    const Type m_type;

    // Only one of the following should ever be in use at any given time, except that BufferType uses two and BufferKeyAndKeyPathType uses four.
    const RefPtr<DOMStringList> m_domStringList;
    const RefPtrWillBeMember<IDBCursor> m_idbCursor;
    const RefPtr<IDBDatabase> m_idbDatabase;
    const RefPtr<IDBIndex> m_idbIndex;
    const RefPtrWillBeMember<IDBObjectStore> m_idbObjectStore;
    const RefPtr<IDBTransaction> m_idbTransaction;
    const RefPtrWillBeMember<IDBKey> m_idbKey;
    const IDBKeyPath m_idbKeyPath;
    const RefPtr<SharedBuffer> m_buffer;
    const Vector<blink::WebBlobInfo>* m_blobInfo;
    const String m_string;
    const int64_t m_integer;
};

} // namespace WebCore

#endif // IDBAny_h

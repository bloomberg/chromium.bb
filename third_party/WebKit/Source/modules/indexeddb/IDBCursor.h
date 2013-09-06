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

#ifndef IDBCursor_h
#define IDBCursor_h

#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/ScriptWrappable.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/IndexedDB.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DOMRequestState;
class ExceptionState;
class IDBAny;
class IDBCallbacks;
class IDBCursorBackendInterface;
class IDBRequest;
class ScriptExecutionContext;
class SharedBuffer;

class IDBCursor : public ScriptWrappable, public WTF::RefCountedBase {
public:
    static const AtomicString& directionNext();
    static const AtomicString& directionNextUnique();
    static const AtomicString& directionPrev();
    static const AtomicString& directionPrevUnique();

    static IndexedDB::CursorDirection stringToDirection(const String& modeString, ExceptionState&);
    static const AtomicString& directionToString(unsigned short mode);

    static PassRefPtr<IDBCursor> create(PassRefPtr<IDBCursorBackendInterface>, IndexedDB::CursorDirection, IDBRequest*, IDBAny* source, IDBTransaction*);
    virtual ~IDBCursor();

    // Implement the IDL
    const String& direction() const { return directionToString(m_direction); }
    ScriptValue key(ScriptExecutionContext*);
    ScriptValue primaryKey(ScriptExecutionContext*);
    ScriptValue value(ScriptExecutionContext*);
    IDBAny* source() const { return m_source.get(); }

    PassRefPtr<IDBRequest> update(ScriptState*, ScriptValue&, ExceptionState&);
    void advance(unsigned long, ExceptionState&);
    void continueFunction(ScriptExecutionContext*, const ScriptValue& key, ExceptionState&);
    PassRefPtr<IDBRequest> deleteFunction(ScriptExecutionContext*, ExceptionState&);

    bool isKeyDirty() const { return m_keyDirty; }
    bool isPrimaryKeyDirty() const { return m_primaryKeyDirty; }
    bool isValueDirty() const { return m_valueDirty; }

    void continueFunction(PassRefPtr<IDBKey>, ExceptionState&);
    void postSuccessHandlerCallback();
    void close();
    void setValueReady(PassRefPtr<IDBKey>, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer> value);
    PassRefPtr<IDBKey> idbPrimaryKey() { return m_primaryKey; }
    IDBRequest* request() { return m_request.get(); }

    void deref()
    {
        if (derefBase())
            delete this;
        else if (hasOneRef())
            checkForReferenceCycle();
    }

protected:
    IDBCursor(PassRefPtr<IDBCursorBackendInterface>, IndexedDB::CursorDirection, IDBRequest*, IDBAny* source, IDBTransaction*);
    virtual bool isKeyCursor() const { return true; }

private:
    PassRefPtr<IDBObjectStore> effectiveObjectStore();

    void checkForReferenceCycle();
    bool isDeleted() const;

    RefPtr<IDBCursorBackendInterface> m_backend;
    RefPtr<IDBRequest> m_request;
    const IndexedDB::CursorDirection m_direction;
    RefPtr<IDBAny> m_source;
    RefPtr<IDBTransaction> m_transaction;
    bool m_gotValue;
    bool m_keyDirty;
    bool m_primaryKeyDirty;
    bool m_valueDirty;
    RefPtr<IDBKey> m_key;
    RefPtr<IDBKey> m_primaryKey;
    RefPtr<SharedBuffer> m_value;
};

} // namespace WebCore

#endif // IDBCursor_h

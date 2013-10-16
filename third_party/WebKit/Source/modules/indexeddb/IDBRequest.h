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

#ifndef IDBRequest_h
#define IDBRequest_h

#include "bindings/v8/DOMRequestState.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMError.h"
#include "core/dom/DOMStringList.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "core/events/ThreadLocalEventNames.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBCallbacks.h"
#include "modules/indexeddb/IDBCursor.h"

namespace WebCore {

class ExceptionState;
class IDBTransaction;
class SharedBuffer;

class IDBRequest : public ScriptWrappable, public IDBCallbacks, public EventTargetWithInlineData, public ActiveDOMObject {
    DEFINE_EVENT_TARGET_REFCOUNTING(IDBCallbacks);
public:
    static PassRefPtr<IDBRequest> create(ExecutionContext*, PassRefPtr<IDBAny> source, IDBTransaction*);
    static PassRefPtr<IDBRequest> create(ExecutionContext*, PassRefPtr<IDBAny> source, IDBDatabaseBackendInterface::TaskType, IDBTransaction*);
    virtual ~IDBRequest();

    PassRefPtr<IDBAny> result(ExceptionState&) const;
    PassRefPtr<DOMError> error(ExceptionState&) const;
    PassRefPtr<IDBAny> source() const;
    PassRefPtr<IDBTransaction> transaction() const;
    void preventPropagation() { m_preventPropagation = true; }

    // Defined in the IDL
    enum ReadyState {
        PENDING = 1,
        DONE = 2,
        EarlyDeath = 3
    };

    const String& readyState() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(success);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

    void markEarlyDeath();
    void setCursorDetails(IndexedDB::CursorType, IndexedDB::CursorDirection);
    void setPendingCursor(PassRefPtr<IDBCursor>);
    void abort();

    // IDBCallbacks
    virtual void onError(PassRefPtr<DOMError>);
    virtual void onSuccess(const Vector<String>&);
    virtual void onSuccess(PassRefPtr<IDBCursorBackendInterface>, PassRefPtr<IDBKey>, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer>);
    virtual void onSuccess(PassRefPtr<IDBKey>);
    virtual void onSuccess(PassRefPtr<SharedBuffer>);
    virtual void onSuccess(PassRefPtr<SharedBuffer>, PassRefPtr<IDBKey>, const IDBKeyPath&);
    virtual void onSuccess(int64_t);
    virtual void onSuccess();
    virtual void onSuccess(PassRefPtr<IDBKey>, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer>);

    // ActiveDOMObject
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void stop() OVERRIDE;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;
    virtual void uncaughtExceptionInEventHandler() OVERRIDE;

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtr<Event>) OVERRIDE;

    void transactionDidFinishAndDispatch();

    virtual void deref() OVERRIDE
    {
        if (derefBase())
            delete this;
        else if (hasOneRef())
            checkForReferenceCycle();
    }

    IDBDatabaseBackendInterface::TaskType taskType() { return m_taskType; }

    DOMRequestState* requestState() { return &m_requestState; }
    IDBCursor* getResultCursor();

protected:
    IDBRequest(ExecutionContext*, PassRefPtr<IDBAny> source, IDBDatabaseBackendInterface::TaskType, IDBTransaction*);
    void enqueueEvent(PassRefPtr<Event>);
    virtual bool shouldEnqueueEvent() const;
    void onSuccessInternal(PassRefPtr<SerializedScriptValue>);
    void onSuccessInternal(const ScriptValue&);

    RefPtr<IDBAny> m_result;
    RefPtr<DOMError> m_error;
    bool m_contextStopped;
    RefPtr<IDBTransaction> m_transaction;
    ReadyState m_readyState;
    bool m_requestAborted; // May be aborted by transaction then receive async onsuccess; ignore vs. assert.

private:
    void setResultCursor(PassRefPtr<IDBCursor>, PassRefPtr<IDBKey>, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer> value);
    void checkForReferenceCycle();

    RefPtr<IDBAny> m_source;
    const IDBDatabaseBackendInterface::TaskType m_taskType;

    bool m_hasPendingActivity;
    Vector<RefPtr<Event> > m_enqueuedEvents;

    // Only used if the result type will be a cursor.
    IndexedDB::CursorType m_cursorType;
    IndexedDB::CursorDirection m_cursorDirection;
    RefPtr<IDBCursor> m_pendingCursor;
    RefPtr<IDBKey> m_cursorKey;
    RefPtr<IDBKey> m_cursorPrimaryKey;
    RefPtr<SharedBuffer> m_cursorValue;
    bool m_didFireUpgradeNeededEvent;
    bool m_preventPropagation;

    DOMRequestState m_requestState;
};

} // namespace WebCore

#endif // IDBRequest_h

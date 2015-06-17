// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BodyStreamBuffer_h
#define BodyStreamBuffer_h

#include "core/dom/DOMException.h"
#include "modules/ModulesExport.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Heap.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/Deque.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DOMArrayBuffer;

class MODULES_EXPORT BodyStreamBuffer final : public GarbageCollectedFinalized<BodyStreamBuffer> {
public:
    class Observer : public GarbageCollectedFinalized<Observer> {
    public:
        virtual ~Observer() { }
        virtual void onWrite() = 0;
        virtual void onClose() = 0;
        virtual void onError() = 0;
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    };

    class Canceller : public GarbageCollected<Canceller> {
    public:
        virtual void cancel() = 0;
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    };

    class BlobHandleCreatorClient : public GarbageCollectedFinalized<BlobHandleCreatorClient> {
    public:
        virtual ~BlobHandleCreatorClient() { }
        virtual void didCreateBlobHandle(PassRefPtr<BlobDataHandle>) = 0;
        virtual void didFail(DOMException*) = 0;
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    };
    explicit BodyStreamBuffer(Canceller*);
    ~BodyStreamBuffer() { }

    PassRefPtr<DOMArrayBuffer> read();
    bool isClosed() const { return m_isClosed; }
    bool hasError() const { return m_exception; }
    DOMException* exception() const { return m_exception; }

    // Can't call after close() or error() was called.
    void write(PassRefPtr<DOMArrayBuffer>);
    // Can't call after close() or error() was called.
    void close();
    // Can't call after close() or error() was called.
    void error(DOMException*);
    void cancel() { m_canceller->cancel(); }

    // This function registers an observer so it fails and returns false when an
    // observer was already registered.
    bool readAllAndCreateBlobHandle(const String& contentType, BlobHandleCreatorClient*);

    // This function registers an observer so it fails and returns false when an
    // observer was already registered.
    bool startTee(BodyStreamBuffer* out1, BodyStreamBuffer* out2);

    // When an observer was registered this function fails and returns false.
    bool registerObserver(Observer*);
    void unregisterObserver();
    bool isObserverRegistered() const { return m_observer.get(); }
    DECLARE_TRACE();

    // Creates a BodyStreamBuffer from |handle| as the source.
    // On failure, BodyStreamBuffer::error() is called with a NetworkError
    // with |failureMessage|.
    static BodyStreamBuffer* create(PassOwnPtr<WebDataConsumerHandle> /* handle */, const String& failureMessage);

private:
    Deque<RefPtr<DOMArrayBuffer>> m_queue;
    bool m_isClosed;
    Member<DOMException> m_exception;
    Member<Observer> m_observer;
    Member<Canceller> m_canceller;
};

} // namespace blink

#endif // BodyStreamBuffer_h
